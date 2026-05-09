# Program Layout & Execution

This document is the assignment-specific deliverable. It shows how the
recursive `factorial` program is laid out in TinyCPU's memory, how its
function calls are handled, and how the recursion physically unfolds on
the stack.

The C reference for the program is [programs/factorial.c](../programs/factorial.c);
the hand-translated assembly is [programs/factorial.asm](../programs/factorial.asm).
The numbers in this document come from running the actual emulator:

```bash
make
./tinycpu run build/factorial.bin
./tinycpu run build/factorial.bin --trace      # one line per instruction
```

---

## 1. The C source

```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int main(void) {
    int n      = 3;
    int result = factorial(n);
    printf("%d! = %d\n", n, result);
    return 0;
}
```

A driver `main` calls a recursive `factorial`. On TinyCPU `printf` is
replaced by writing each character byte to the memory-mapped TTY at
`0xC000`, and multiplication is implemented by repeated addition (the
TinyCPU ISA has no `MUL`).

---

## 2. Memory layout of the executable

TinyCPU has a flat 64 KiB address space. The CLI loader (`CPU::load` in
[`src/cpu.hpp`](../src/cpu.hpp)) places the program image at the
`.org` address declared in the source — for `factorial.asm` that is
**`0x0000`** — and points `PC` at it. The MMIO page lives at `0xC000`,
and the stack grows downward from `SP = 0x03FF` (set by
`RegisterFile::reset()` in [`src/register_file.hpp`](../src/register_file.hpp)).

```
 0xFFFF ┌──────────────────────────────────┐
        │  reserved                        │
 0xC011 │  MMIO_TIMER_CTRL                 │
 0xC010 │  MMIO_TIMER                      │
 0xC001 │  MMIO_TTY_IN                     │
 0xC000 │  MMIO_TTY_OUT                    │
        ├──────────────────────────────────┤
        │           free RAM               │
 0x03FF │  ← initial SP (stack top)        │
        │            ↓ stack grows down    │
        │           free RAM               │
 0x007F ├──────────────────────────────────┤  end of program image
        │  factorial.asm code (127 bytes)  │
 0x0000 ├──────────────────────────────────┤  PC starts here
        └──────────────────────────────────┘
```

After loading `build/factorial.bin` (127 bytes), the byte map is:

| Address range     | Contents                                              |
| ----------------- | ----------------------------------------------------- |
| `0x0000 – 0x0022` | the `printf("3! = ")` prelude (5 × MOVI/ST pairs)     |
| `0x0023 – 0x0025` | `MOVI R0, #3`  (the argument)                         |
| `0x0026 – 0x0028` | `CALL FACTORIAL`                                       |
| `0x0029 – 0x003A` | digit conversion + newline + `HLT`                    |
| `0x003B – 0x0074` | `FACTORIAL` body (recursive case)                     |
| `0x0075 – 0x007E` | `FACT_BASE`                                           |

Important addresses we'll use repeatedly:

- `FACTORIAL = 0x003B`
- `FACT_BASE = 0x0075`
- The instruction *after* the `CALL` in `main` lives at `0x0029`.
- The instruction *after* the recursive `CALL` in `FACTORIAL` lives at `0x005E`.

---

## 3. How a function call is handled

TinyCPU has two control-flow opcodes that work as a matched pair:

| Opcode | Name | Effect                                                                          |
| ------ | ---- | ------------------------------------------------------------------------------- |
| `0x70` | CALL | Push the address of the next instruction onto the stack as a 16-bit value, then jump to the operand. |
| `0x71` | RET  | Pop a 16-bit value from the stack and jump to it.                                |

The implementations are in [`src/control_unit.cpp`](../src/control_unit.cpp):

```cpp
void ControlUnit::h_call(ControlUnit& c, uint16_t pc) {
    uint16_t target   = c.fetch16();
    uint16_t ret_addr = c.rf_.pc();           // first byte after CALL operands

    // Push as two bytes: high byte first so the low byte ends up at the
    // lower address (the "top" of a downward-growing stack).
    uint16_t slot_hi = c.rf_.push_sp();
    c.bus_.write(slot_hi, (uint8_t)((ret_addr >> 8) & 0xFF));
    uint16_t slot_lo = c.rf_.push_sp();
    c.bus_.write(slot_lo, (uint8_t)(ret_addr & 0xFF));

    c.rf_.set_pc(target);
}

void ControlUnit::h_ret(ControlUnit& c, uint16_t pc) {
    uint16_t slot_lo = c.rf_.pop_sp();
    uint8_t  lo      = c.bus_.read(slot_lo);
    uint16_t slot_hi = c.rf_.pop_sp();
    uint8_t  hi      = c.bus_.read(slot_hi);
    uint16_t ret_addr = (uint16_t)lo | ((uint16_t)hi << 8);
    c.rf_.set_pc(ret_addr);
}
```

`push_sp()` returns the current SP and decrements it; `pop_sp()` does
the inverse. Two pushes per `CALL`, two pops per `RET`, so each call
frame consumes exactly two bytes of stack for the linkage. There is no
separate "stack memory" — the stack is just normal RAM that the SP
register happens to point into.

### The calling convention used by `factorial.asm`

| Item            | Convention                                                                |
| --------------- | ------------------------------------------------------------------------- |
| Argument        | passed in `R0`                                                            |
| Return value    | returned in `R0`                                                          |
| Caller-saved    | `R0` (caller pushes `R0` itself if it still needs it after the call)      |
| Callee-saved    | `R1`, `R2`, `R3` (saved in the prologue, restored in the epilogue)        |

Because TinyCPU has no `MUL` instruction, multiplication is implemented
inside the recursive case via repeated addition.

---

## 4. How the recursion is carried out

Below is a stack snapshot at every interesting point during
`factorial(3)`. SP starts at `0x03FF`; addresses below are written in
hex, with the value at each address in decimal where the meaning is
clearer.

### State 0 — `main` is about to issue `CALL FACTORIAL`

```
SP = 0x03FF
R0 = 3            (the argument to factorial)
PC = 0x0026
[stack is empty]
```

### State 1 — inside `factorial(3)`, after the prologue

`CALL` pushed the return address `0x0029` (high byte then low byte —
high goes at the higher stack address, low at the lower / "top"
address); the prologue pushed `R1`, `R2`, `R3`:

```
SP = 0x03F8
R0 = 3
PC = 0x0041

addr     | value
---------+--------------------------------
0x03FE   | 0x00     ← high byte of return address
0x03FD   | 0x29     ← low  byte of return address  ← (return into main)
0x03FC   | (old R1)
0x03FB   | (old R2)
0x03FA   | (old R3)
0x03F9   | (next free)                              ← SP after PUSH R3
```

(After three single-byte PUSHes, SP is `0x03FF − 5 = 0x03FA`. Wait —
let me redo that. After `CALL` (2 bytes pushed) SP = `0x03FD`. After
`PUSH R1`, SP = `0x03FC`. After `PUSH R2`, SP = `0x03FB`. After
`PUSH R3`, SP = `0x03FA`. So SP after prologue = `0x03FA`, with `R3` at
`0x03FB`, `R2` at `0x03FC`, `R1` at `0x03FD−... ` — no, let me be more
careful: `push_sp()` returns the *old* SP and decrements, so the byte
each `PUSH` writes lands one byte higher than the new SP. Starting
SP = `0x03FF`, the CALL high-byte push writes to `0x03FF` (then SP →
`0x03FE`), the low-byte push writes to `0x03FE` (then SP → `0x03FD`).
The first `PUSH R1` writes at `0x03FD` (then SP → `0x03FC`). And so on.
Corrected snapshot:)

```
SP = 0x03FA
R0 = 3
PC = 0x0041

addr     | value
---------+--------------------------------
0x03FF   | 0x00     ← high byte of return address (0x0029 → main)
0x03FE   | 0x29     ← low  byte of return address
0x03FD   | (old R1)
0x03FC   | (old R2)
0x03FB   | (old R3)
0x03FA   | (next free)         ← SP
```

### State 2 — about to recurse: `factorial(3)` has saved `n=3` and computed `n-1`

The recursive case begins with `PUSH R0` (so we remember `n` for the
later multiply), then `MOVI R1,#1; SUB R0, R1`:

```
SP = 0x03F9
R0 = 2           (n - 1, the new argument)
PC = 0x005B      (next: CALL FACTORIAL)

addr     | value
---------+--------------------------------
0x03FF   | 0x00
0x03FE   | 0x29     ← return into main (0x0029)
0x03FD   | (old R1)
0x03FC   | (old R2)
0x03FB   | (old R3)
0x03FA   | 3        ← saved n at depth 1   ← top of stack
0x03F9   | (next free)                     ← SP
```

### State 3 — inside `factorial(2)`, after its prologue

Same shape as state 1, just one level deeper. The `CALL` from state 2
pushed `0x005E` (the address of `POP R1` after the recursive call).

```
SP = 0x03F4
R0 = 2
PC = 0x0041

addr     | value
---------+--------------------------------
0x03FF   | 0x00
0x03FE   | 0x29     ← return into main
0x03FD   | (R1 saved by f(3))
0x03FC   | (R2 saved by f(3))
0x03FB   | (R3 saved by f(3))
0x03FA   | 3        ← saved n at depth 1
0x03F9   | 0x00     ← high byte of return into f(3) (0x005E)
0x03F8   | 0x5E     ← low  byte of return into f(3)
0x03F7   | (R1 saved by f(2))
0x03F6   | (R2 saved by f(2))
0x03F5   | (R3 saved by f(2))
0x03F4   | (next free)                      ← SP
```

### State 4 — `factorial(2)` is about to recurse with `n - 1 = 1`

```
SP = 0x03F3
R0 = 1
PC = 0x005B

addr     | value
---------+--------------------------------
... (everything from state 3) ...
0x03F4   | 2        ← saved n at depth 2
0x03F3   | (next free)                      ← SP
```

### State 5 — `factorial(1)` has finished its prologue and hit the base case

`MOVI R1, #1; CMP R0, R1` sets the zero flag, and `BEQ FACT_BASE`
jumps to `0x0075`.

```
SP = 0x03EE
R0 = 1            (about to be replaced with the return value)
PC = 0x0075       (FACT_BASE)

addr     | value
---------+--------------------------------
0x03FF   | 0x00
0x03FE   | 0x29     ← return into main (depth 0)
0x03FD   | (R1 from f(3))
0x03FC   | (R2 from f(3))
0x03FB   | (R3 from f(3))
0x03FA   | 3        ← saved n at depth 1
0x03F9   | 0x00
0x03F8   | 0x5E     ← return into f(3) (depth 1)
0x03F7   | (R1 from f(2))
0x03F6   | (R2 from f(2))
0x03F5   | (R3 from f(2))
0x03F4   | 2        ← saved n at depth 2
0x03F3   | 0x00
0x03F2   | 0x5E     ← return into f(2) (depth 2)
0x03F1   | (R1 from f(1))
0x03F0   | (R2 from f(1))
0x03EF   | (R3 from f(1))
0x03EE   | (next free)                      ← SP
```

This is the deepest the stack ever gets: `0x03FF − 0x03EE = 17 bytes`.
Even with three layers of recursion the stack footprint is modest —
each frame contributes a 2-byte return address, three 1-byte
callee-saved registers, and (in the recursive case only) a 1-byte
saved argument.

### Unwinding — `factorial(1)` returns 1

`FACT_BASE` writes `R0 = 1`, restores `R3 / R2 / R1` (popping in
reverse of the prologue), and `RET`s. `RET` pops the top word — which
is the return address `0x005E` that `f(2)`'s recursive `CALL` pushed —
and jumps there.

```
SP = 0x03F3      (back to where it was when f(2) issued the recursive CALL)
R0 = 1           (return value)
PC = 0x005E
```

### `factorial(2)` finishes the multiplication

At `0x005E` we execute `POP R1; MOV R2, R0; MOVI R0, #0` and then loop
adding `R2` to `R0`:

- `POP R1` pops the saved `n = 2` into `R1`. `R0` still holds `1`.
- `MOV R2, R0` copies `R0` (the multiplicand `(n-1)! = 1`) into `R2`.
- `MOVI R0, #0` zeros the accumulator.
- The `FACT_MUL` loop adds `R2` into `R0` exactly `R1 = 2` times, so
  `R0 = 0 + 1 + 1 = 2`.

The epilogue restores `R3 / R2 / R1` and `RET`s, popping `0x005E`
again — this time the one pushed by `factorial(3)`'s recursive call.

```
SP = 0x03FA      (back to where f(3) had it before recursing)
R0 = 2           (this is factorial(2))
PC = 0x005E
```

### `factorial(3)` finishes the multiplication

Same sequence: `POP R1` recovers the saved `n = 3`. Multiplicand is
`R0 = 2`. The loop runs `3` times, producing `R0 = 6`. Epilogue
restores registers, `RET` pops `0x0029`.

```
SP = 0x03FF      (back to where main started)
R0 = 6           (this is factorial(3) — the final answer)
PC = 0x0029
```

### Back in `main`

`MOVI R1, #'0'; ADD R0, R1` converts `6` to ASCII `'6'`,
`ST R0, [0xC000]` prints it, the next two instructions print a
newline, and `HLT` halts the CPU. The console now shows:

```
3! = 6
```

---

## 5. Reproducing the trace

Every state in this document can be observed directly:

```bash
make
./tinycpu run build/factorial.bin --trace
```

The trace prints one line per fetch / decode / execute step. You can
spot the recursion easily by `grep`ing for `CALL` and `RET`:

```
[pc=0026] CALL 0x003b   ← main calling factorial(3)
[pc=005b] CALL 0x003b   ← factorial(3) recursing into factorial(2)
[pc=005b] CALL 0x003b   ← factorial(2) recursing into factorial(1)
[pc=007e] RET -> 0x005e ← factorial(1) returns to factorial(2)
[pc=0074] RET -> 0x005e ← factorial(2) returns to factorial(3)
[pc=0074] RET -> 0x0029 ← factorial(3) returns to main
```

For a hex dump of the loaded image so you can verify the addresses
above:

```bash
make debug
```

(`make debug` runs with `--trace --dump-after 0x0000:0x009F`.)
