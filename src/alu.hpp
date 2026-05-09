#pragma once
//
// ALU — pure-function arithmetic and logic operations.
//
// The ALU is intentionally stateless: every entry point takes the
// operands and a reference to the architectural Flags, performs the
// operation, returns the 8-bit result, and updates the flags it is
// supposed to update according to the ISA's flag-semantics table.
//
// The control unit is what *decides* which ALU op to call; the ALU just
// computes.
//

#include "register_file.hpp"

#include <cstdint>

namespace tiny {

namespace alu {

// Arithmetic — set Z, N, C, V.
uint8_t op_add(uint8_t a, uint8_t b, Flags& f);
uint8_t op_sub(uint8_t a, uint8_t b, Flags& f);

// Comparison — same math as op_sub, no result returned (caller discards).
void    op_cmp(uint8_t a, uint8_t b, Flags& f);

// Bitwise logic — set Z and N, clear V, leave C alone.
uint8_t op_and(uint8_t a, uint8_t b, Flags& f);
uint8_t op_or (uint8_t a, uint8_t b, Flags& f);
uint8_t op_xor(uint8_t a, uint8_t b, Flags& f);

// Single-operand counters — set Z and N, clear V, leave C alone.
uint8_t op_inc(uint8_t a, Flags& f);
uint8_t op_dec(uint8_t a, Flags& f);

// "Plain assignment" flag effect — used by data-movement ops (MOVI, LD,
// MOV, POP) that need Z/N reflecting the moved byte and V cleared.
void    update_flags_for_move(uint8_t v, Flags& f);

} // namespace alu
} // namespace tiny
