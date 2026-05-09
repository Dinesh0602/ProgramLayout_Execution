# Program Layout & Execution — TinyCPU recursion demo

This is the **second project** in the TinyCPU series. The first project
designed and built TinyCPU itself (registers, ALU, control unit, bus,
MMIO, assembler, three sample programs). This project takes that CPU,
extends its ISA with `CALL` and `RET`, writes a small recursive C
program (`factorial`), hand-translates it for TinyCPU, and uses it to
demonstrate **how a program is laid out in memory, how function calls
are handled, and how recursion is carried out**.

## Required deliverables

| Required item                                          | Where it lives                                                  |
| ------------------------------------------------------ | --------------------------------------------------------------- |
| Recursive C function (`factorial`) and `main` driver   | [programs/factorial.c](programs/factorial.c)                    |
| TinyCPU port of the C program                          | [programs/factorial.asm](programs/factorial.asm)                |
| Memory-layout / function-call / recursion writeup      | [docs/PROGRAM_LAYOUT.md](docs/PROGRAM_LAYOUT.md)                |
| Project report with team contributions                 | [docs/project_report.md](docs/project_report.md)                |
| TinyCPU emulator + assembler (with CALL/RET added)     | [src/](src/)                                                    |

---

## Build

You need a C++17 compiler (`g++` or `clang++`) and GNU Make.

```bash
make
```

This produces:

- `./tinycpu` — the emulator + assembler binary
- `build/factorial.bin` — the assembled image of `programs/factorial.asm`

---

## Run the recursive factorial

```bash
./tinycpu run build/factorial.bin
```

Expected output:

```
3! = 6
```

To watch the recursion physically unfold on the stack, use `--trace`:

```bash
./tinycpu run build/factorial.bin --trace
```

Pipe through `grep` to highlight just the function-linkage events:

```bash
./tinycpu run build/factorial.bin --trace 2>&1 | grep -E 'CALL|RET'
```

You'll see three `CALL 0x003b` lines on the way down (one per recursive
level) and three matching `RET ->` lines on the way back — that's the
recursion in action.

---

## What's in the box

```
SimpleCPU/
├── Makefile                 # build emulator + assemble all .asm
├── README.md                # this file
├── docs/
│   ├── PROGRAM_LAYOUT.md    # memory layout / CALL+RET / recursion walkthrough
│   └── project_report.md    # project report + team contributions
├── programs/
│   ├── factorial.c          # recursive factorial in C (the source)
│   └── factorial.asm        # TinyCPU port (the demo program)
└── src/
    ├── isa.hpp / .cpp       # opcodes (incl. CALL=0x70, RET=0x71), reg codes, MMIO map
    ├── register_file.hpp    # R0..R7, PC, SP, Flags
    ├── alu.hpp / .cpp       # add/sub/and/or/xor/cmp/inc/dec helpers
    ├── bus.hpp / .cpp       # 64 KiB RAM + cycle counter + MMIO routing
    ├── control_unit.hpp/.cpp# Fetch/Decode/Execute via a dispatch table
    ├── cpu.hpp              # facade composing the four components
    ├── assembler.hpp / .cpp # two-pass assembler (labels, numeric + char literals)
    └── main.cpp             # CLI: asm / run / dump
```

After `make` you'll also see `tinycpu` and `build/`.

---

## CLI quick reference

| Command                                                            | Purpose                                              |
| ------------------------------------------------------------------ | ---------------------------------------------------- |
| `tinycpu asm  <src.asm> -o <out.bin>`                              | Assemble source to a flat binary.                    |
| `tinycpu run  <bin> [--origin 0x..] [--trace] [--dump-after F:T]`  | Load, run, optionally trace and dump memory.         |
| `tinycpu dump <bin> [--origin 0x..] [F:T]`                         | Hex-dump a binary as if it were loaded.              |

## Make targets

| Target        | What it does                                                            |
| ------------- | ----------------------------------------------------------------------- |
| `make`        | Build `./tinycpu` and assemble every `.asm` under `programs/`.          |
| `make run`    | Build if needed, run `factorial`.                                       |
| `make trace`  | Run `factorial` with `--trace`.                                         |
| `make debug`  | Run with `--trace --dump-after 0x0000:0x009F`.                          |
| `make clean`  | Remove `build/` and `./tinycpu`.                                        |
