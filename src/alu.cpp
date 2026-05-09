#include "alu.hpp"

//
// ALU implementation. Each function does its arithmetic at a wider type,
// so we can read carry / overflow off the result before truncating to
// 8 bits.
//

namespace tiny {
namespace alu {

uint8_t op_add(uint8_t a, uint8_t b, Flags& f) {
    uint16_t wide = (uint16_t)a + (uint16_t)b;
    uint8_t  res  = (uint8_t)wide;

    f.update_zn(res);
    f.C = (wide & 0x100) != 0;                          // carry out of bit 7
    f.V = (~(a ^ b) & (a ^ res) & 0x80) != 0;           // signed overflow
    return res;
}

uint8_t op_sub(uint8_t a, uint8_t b, Flags& f) {
    int16_t wide = (int16_t)a - (int16_t)b;
    uint8_t res  = (uint8_t)wide;

    f.update_zn(res);
    f.C = (wide < 0);                                   // borrow occurred
    f.V = ((a ^ b) & (a ^ res) & 0x80) != 0;
    return res;
}

void op_cmp(uint8_t a, uint8_t b, Flags& f) {
    (void)op_sub(a, b, f);                              // discard result, keep flags
}

uint8_t op_and(uint8_t a, uint8_t b, Flags& f) {
    uint8_t r = a & b;
    f.update_zn(r); f.V = false;
    return r;
}

uint8_t op_or(uint8_t a, uint8_t b, Flags& f) {
    uint8_t r = a | b;
    f.update_zn(r); f.V = false;
    return r;
}

uint8_t op_xor(uint8_t a, uint8_t b, Flags& f) {
    uint8_t r = a ^ b;
    f.update_zn(r); f.V = false;
    return r;
}

uint8_t op_inc(uint8_t a, Flags& f) {
    uint8_t r = (uint8_t)(a + 1);
    f.update_zn(r); f.V = false;
    return r;
}

uint8_t op_dec(uint8_t a, Flags& f) {
    uint8_t r = (uint8_t)(a - 1);
    f.update_zn(r); f.V = false;
    return r;
}

void update_flags_for_move(uint8_t v, Flags& f) {
    f.update_zn(v);
    f.V = false;
}

} // namespace alu
} // namespace tiny
