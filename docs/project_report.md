# Program Layout & Execution — Project Report

**Project:** Recursive factorial running on TinyCPU
**Team members:** Dinesh Buruboyina, Jainam Chhatbar, Kalyani Chitre, Manish Maryala
**Date:** May 2026

---

## 1. Overview

This is the second project in the TinyCPU series. The first project
designed and built the TinyCPU itself (registers, ALU, control unit,
bus, MMIO, assembler, three sample programs); this project takes that
CPU, extends its ISA with the two function-linkage instructions
needed for recursion (`CALL` and `RET`), writes a small recursive C
program (`factorial`), hand-translates it for TinyCPU, and uses it to
demonstrate **how the executable is laid out in memory, how function
calls are handled, and how recursion is carried out**.

### Deliverables

| Required item                                       | Where it lives                                              |
| --------------------------------------------------- | ----------------------------------------------------------- |
| Recursive C function (`factorial`)                  | [programs/factorial.c](../programs/factorial.c)             |
| Driver / `main` that calls it                       | [programs/factorial.c](../programs/factorial.c)             |
| TinyCPU port of the C program                       | [programs/factorial.asm](../programs/factorial.asm)         |
| Memory layout / function calls / recursion writeup  | [docs/PROGRAM_LAYOUT.md](PROGRAM_LAYOUT.md)                 |
| TinyCPU emulator + assembler (extended with CALL/RET) | [src/](../src/) — built into `./tinycpu` by `make`        |

### What it does

```
$ make
$ ./tinycpu run build/factorial.bin
3! = 6
```

---

## 2. The recursive C program

[`programs/factorial.c`](../programs/factorial.c):

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

A `main` driver that calls a recursive `factorial`. The base case is
`n <= 1`, the recursive case multiplies `n` by `factorial(n − 1)`.
The exact same algorithm is hand-translated into
[`programs/factorial.asm`](../programs/factorial.asm) so we can run it
on TinyCPU.

---

## 3. ISA extensions — adding `CALL` and `RET`

The first-project TinyCPU has eight 8-bit GPRs, a stack (`PUSH`/`POP`),
and absolute branches, but no return-linkage instructions. To express
function calls (and therefore recursion) we extended the ISA with two
new opcodes:

| Opcode  | Mnemonic | Layout       | Effect                                                                   |
| ------- | -------- | ------------ | ------------------------------------------------------------------------ |
| `0x70`  | `CALL`   | 3-byte (Addr)| Push address-of-next-instruction (16-bit) onto stack, then jump.         |
| `0x71`  | `RET`    | 1-byte (None)| Pop a 16-bit address from stack and jump to it.                          |

The change is intentionally minimal: a new opcode class `0x7_`
("function linkage") in [`src/isa.hpp`](../src/isa.hpp), one new
table row in the assembler's mnemonic table
([`src/assembler.cpp`](../src/assembler.cpp)), one new dispatch entry
plus a handler in
[`src/control_unit.cpp`](../src/control_unit.cpp), and one new
mnemonic / size entry in [`src/isa.cpp`](../src/isa.cpp). The rest of
the CPU is unchanged.

---

## 4. TinyCPU calling convention

`factorial.asm` uses the following convention, which is enough to make
recursion safe without a full ABI:

| Item            | Convention                                                                    |
| --------------- | ----------------------------------------------------------------------------- |
| Argument        | passed in `R0`                                                                |
| Return value    | returned in `R0`                                                              |
| Caller-saved    | `R0` (caller pushes `R0` before `CALL` if it still needs it after)            |
| Callee-saved    | `R1`, `R2`, `R3` (saved on entry, restored on exit)                           |
| Linkage         | `CALL` pushes a 16-bit return address; `RET` pops it                          |
| Stack direction | grows downward from `SP = 0x03FF` (set in `RegisterFile::reset()`)            |

Because TinyCPU has no `MUL` instruction, the recursive case
implements `n × factorial(n−1)` as repeated addition.

---

## 5. Executable memory layout

The CPU loader places the program at the address declared by `.org`
in the source — for `factorial.asm` that is `0x0000`. The MMIO page
lives at `0xC000` and the stack grows downward from `0x03FF`.

```
 0xFFFF ┌──────────────────────────────────┐
        │  reserved                        │
 0xC011 │  MMIO_TIMER_CTRL                 │
 0xC010 │  MMIO_TIMER                      │
 0xC001 │  MMIO_TTY_IN                     │
 0xC000 │  MMIO_TTY_OUT                    │
        ├──────────────────────────────────┤
        │           free RAM               │
 0x03FF │  ← initial SP                    │
        │            ↓ stack grows down    │
        │           free RAM               │
 0x007F ├──────────────────────────────────┤  end of program image
        │  factorial.asm  (127 bytes)      │
 0x0000 ├──────────────────────────────────┤  PC starts here
        └──────────────────────────────────┘
```

A more detailed breakdown — including the exact addresses of `main`,
`FACTORIAL`, `FACT_BASE`, the recursive `CALL`, and the resume point
on each return — is in [`docs/PROGRAM_LAYOUT.md`](PROGRAM_LAYOUT.md).

---

## 6. Function call handling

The control unit's `CALL` and `RET` handlers, in
[`src/control_unit.cpp`](../src/control_unit.cpp), implement the
linkage:

```cpp
// CALL: push the 16-bit return address (high then low so low is on
// top of a downward-growing stack), then jump.
uint16_t target   = c.fetch16();
uint16_t ret_addr = c.rf_.pc();
uint16_t slot_hi = c.rf_.push_sp();
c.bus_.write(slot_hi, (uint8_t)((ret_addr >> 8) & 0xFF));
uint16_t slot_lo = c.rf_.push_sp();
c.bus_.write(slot_lo, (uint8_t)(ret_addr & 0xFF));
c.rf_.set_pc(target);

// RET: pop low byte, then high byte, recombine, jump.
uint16_t slot_lo = c.rf_.pop_sp(); uint8_t lo = c.bus_.read(slot_lo);
uint16_t slot_hi = c.rf_.pop_sp(); uint8_t hi = c.bus_.read(slot_hi);
c.rf_.set_pc((uint16_t)lo | ((uint16_t)hi << 8));
```

`push_sp()` returns the current `SP` and decrements it; `pop_sp()`
does the inverse. Each call frame consumes exactly two bytes for the
return-address linkage. The stack is just normal RAM that the `SP`
register happens to point into.

---

## 7. Recursion walkthrough

[`docs/PROGRAM_LAYOUT.md`](PROGRAM_LAYOUT.md) contains the full
step-by-step trace, with a stack snapshot at each interesting moment
(entry, before recursing, the deepest base-case state, every unwind).
Headline numbers for `factorial(3)`:

- Three recursive `CALL`s into `FACTORIAL` (at `0x003B`) before the
  base case fires; three matching `RET`s on the way back.
- Deepest stack snapshot consumes **17 bytes** (`0x03FF − 0x03EE`).
- Each frame contributes a 2-byte return address + three 1-byte
  callee-saved registers + one optional 1-byte saved argument.

You can reproduce every state directly:

```bash
./tinycpu run build/factorial.bin --trace                       # one line per instruction
./tinycpu run build/factorial.bin --trace --dump-after 0x0000:0x009F
```

The recursion is visible in the trace as repeated jumps to `0x003B`
(the `FACTORIAL` entry point) and matching `RET → 0x005E` /
`RET → 0x0029` lines on the way back.

---

## 8. Build and run

```bash
make                                  # builds ./tinycpu and assembles factorial.bin
./tinycpu run   build/factorial.bin   # prints "3! = 6"
./tinycpu run   build/factorial.bin --trace
```

Convenience targets:

| `make`         | What it does                                                           |
| -------------- | ---------------------------------------------------------------------- |
| `make`         | Build `./tinycpu` and assemble every `.asm` in `programs/` to `build/` |
| `make run`     | Run `factorial`                                                         |
| `make trace`   | Run `factorial` with `--trace`                                         |
| `make debug`   | Run `factorial` with `--trace --dump-after 0x0000:0x009F`              |
| `make clean`   | Remove `build/` and `./tinycpu`                                         |

---

## 9. Contributions

| Member               | Area of responsibility |
| -------------------- | ---------------------- |
| **Dinesh Buruboyina** | C source (`factorial.c`), driver `main`, TinyCPU calling-convention design |
| **Jainam Chhatbar**   | Hand-translation of `factorial.c` → `factorial.asm`, register allocation, multiplication-via-addition routine |
| **Kalyani Chitre**    | ISA extension (`CALL` / `RET` opcode design + control-unit handlers + assembler integration) |
| **Manish Maryala**    | Memory layout + recursion writeup ([`PROGRAM_LAYOUT.md`](PROGRAM_LAYOUT.md)), end-to-end testing, project report |

Every member reviewed the others' work and contributed to the final
integration pass.
