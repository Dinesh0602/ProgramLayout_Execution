#pragma once
//
// RegisterFile — the architectural register state of the CPU.
//
// Holds the eight 8-bit general-purpose registers (R0..R7), the 16-bit
// program counter and stack pointer, and the four status flags.
// Everything that touches register state goes through this object, which
// keeps the rest of the emulator from poking at raw arrays.
//

#include "isa.hpp"

#include <array>
#include <cstdint>

namespace tiny {

struct Flags {
    bool Z = false;     // last result was zero
    bool N = false;     // bit 7 of last result was set
    bool C = false;     // arithmetic produced carry / borrow
    bool V = false;     // arithmetic produced signed overflow

    // Update Z and N from a result byte. Used by every op that produces
    // an 8-bit value.
    void update_zn(uint8_t v) {
        Z = (v == 0);
        N = ((v & 0x80) != 0);
    }

    // Pack into a single status byte (handy for printing).
    uint8_t pack() const {
        return (Z ? F_Z : 0) | (N ? F_N : 0) | (C ? F_C : 0) | (V ? F_V : 0);
    }
};

class RegisterFile {
public:
    RegisterFile() { reset(); }

    void reset() {
        gp_.fill(0);
        pc_    = 0x0000;
        sp_    = STACK_TOP;
        flags_ = Flags{};
    }

    // 8-bit GPR access.
    uint8_t  read(uint8_t r)  const { return (r < NUM_REGS) ? gp_[r] : 0; }
    void     write(uint8_t r, uint8_t v) { if (r < NUM_REGS) gp_[r] = v; }

    // PC / SP access.
    uint16_t pc() const  { return pc_; }
    void     set_pc(uint16_t v) { pc_ = v; }
    uint16_t advance_pc(uint16_t n = 1) { uint16_t old = pc_; pc_ += n; return old; }

    uint16_t sp() const  { return sp_; }
    void     set_sp(uint16_t v) { sp_ = v; }
    uint16_t push_sp() { return sp_--; }      // returns slot to write
    uint16_t pop_sp()  { return ++sp_; }      // returns slot to read

    // Flags.
    Flags&       flags()       { return flags_; }
    const Flags& flags() const { return flags_; }

private:
    std::array<uint8_t, NUM_REGS> gp_{};
    uint16_t pc_ = 0;
    uint16_t sp_ = STACK_TOP;
    Flags    flags_{};
};

} // namespace tiny
