#pragma once
//
// TinyCPU — Instruction Set Architecture (machine-readable form).
//
// This header is the single source of truth for everything that defines
// the *architecture*: how many registers, where I/O sits in the address
// space, and which byte values represent which instructions. The
// assembler, the disassembler, and the control unit's dispatch table all
// consume this file so that a change here propagates everywhere.
//
// The ISA is grouped into five classes by opcode prefix:
//
//   0x0_  data movement   (NOP, MOVI, LD, ST, MOV)
//   0x2_  arithmetic/logic (ADD, SUB, AND, OR, XOR, CMP, INC, DEC)
//   0x4_  branches         (BR, BEQ, BNE, BCS, BCC, BMI, BPL)
//   0x6_  stack            (PUSH, POP)
//   0x7_  function linkage (CALL, RET)
//   0x8_  system           (HLT)
//
// All I/O is memory-mapped — there are no dedicated I/O instructions.
// To print a byte, store it to MMIO_TTY_OUT; to sample the timer, load
// from MMIO_TIMER.
//

#include <cstddef>
#include <cstdint>

namespace tiny {

// =============================================================================
// Address space
// =============================================================================
constexpr size_t   MEM_BYTES        = 65536;        // full 64 KiB

// MMIO page lives at 0xC000 (instead of the usual 0xFF.. window).
constexpr uint16_t MMIO_TTY_OUT     = 0xC000;       // write -> putc on host
constexpr uint16_t MMIO_TTY_IN      = 0xC001;       // read  -> getc (0 if none)
constexpr uint16_t MMIO_TIMER       = 0xC010;       // low byte of cycle counter
constexpr uint16_t MMIO_TIMER_CTRL  = 0xC011;       // bit 0: write 1 to reset

// Stack convention: SP starts at 0x03FF and grows downward.
constexpr uint16_t STACK_TOP        = 0x03FF;

// =============================================================================
// Register file
// =============================================================================
//
// Eight general-purpose 8-bit registers, addressed by a 3-bit code that
// is encoded directly into instruction operand bytes.
//
constexpr uint8_t  NUM_REGS         = 8;

enum RegId : uint8_t {
    R0 = 0, R1 = 1, R2 = 2, R3 = 3,
    R4 = 4, R5 = 5, R6 = 6, R7 = 7
};

// =============================================================================
// Status flags (held in the architectural Flags register)
// =============================================================================
enum FlagBit : uint8_t {
    F_Z = 1 << 0,   // zero
    F_N = 1 << 1,   // negative (bit 7 of result set)
    F_C = 1 << 2,   // carry / borrow
    F_V = 1 << 3    // signed overflow
};

// =============================================================================
// Opcodes (the byte values are part of the ISA — do not reorder)
// =============================================================================
enum Op : uint8_t {

    // ---- 0x0_  data movement -----------------------------------------------
    OP_NOP   = 0x00,                // 1 byte
    OP_MOVI  = 0x01,                // MOVI Rd, #imm8        (3 bytes)
    OP_LD    = 0x02,                // LD   Rd, [abs16]      (4 bytes)
    OP_ST    = 0x03,                // ST   Rs, [abs16]      (4 bytes)
    OP_MOV   = 0x04,                // MOV  Rd, Rs           (3 bytes)

    // ---- 0x2_  arithmetic / logic ------------------------------------------
    OP_ADD   = 0x20,                // ADD  Rd, Rs           (3 bytes)
    OP_SUB   = 0x21,                // SUB  Rd, Rs
    OP_AND   = 0x22,                // AND  Rd, Rs
    OP_OR    = 0x23,                // OR   Rd, Rs
    OP_XOR   = 0x24,                // XOR  Rd, Rs
    OP_CMP   = 0x25,                // CMP  Rd, Rs           (flags only)
    OP_INC   = 0x26,                // INC  Rd               (2 bytes)
    OP_DEC   = 0x27,                // DEC  Rd               (2 bytes)

    // ---- 0x4_  branches ----------------------------------------------------
    OP_BR    = 0x40,                // BR   abs16            (3 bytes; unconditional)
    OP_BEQ   = 0x41,                // BEQ  abs16            (Z == 1)
    OP_BNE   = 0x42,                // BNE  abs16            (Z == 0)
    OP_BCS   = 0x43,                // BCS  abs16            (C == 1)
    OP_BCC   = 0x44,                // BCC  abs16            (C == 0)
    OP_BMI   = 0x45,                // BMI  abs16            (N == 1)
    OP_BPL   = 0x46,                // BPL  abs16            (N == 0)

    // ---- 0x6_  stack -------------------------------------------------------
    OP_PUSH  = 0x60,                // PUSH Rs               (2 bytes)
    OP_POP   = 0x61,                // POP  Rd               (2 bytes)

    // ---- 0x7_  function linkage --------------------------------------------
    //
    // CALL pushes the address of the next instruction onto the stack as
    // a 16-bit value (high byte first, then low byte, so the low byte
    // ends up at the lower address — the standard "top" of a downward-
    // growing stack). RET pops the same 16-bit value and jumps to it.
    //
    // These are the building blocks that turn a flat instruction stream
    // into something that can express functions and recursion.
    //
    OP_CALL  = 0x70,                // CALL abs16            (3 bytes)
    OP_RET   = 0x71,                // RET                   (1 byte)

    // ---- 0x8_  system ------------------------------------------------------
    OP_HLT   = 0x80                 // HLT                   (1 byte)
};

// =============================================================================
// Mnemonic lookup helpers (for trace / disassembler).
// =============================================================================
const char* mnemonic_for(uint8_t op);
const char* reg_name(uint8_t r);
uint8_t     instruction_size(uint8_t op);   // 1, 2, 3, or 4; 0 if unknown

} // namespace tiny
