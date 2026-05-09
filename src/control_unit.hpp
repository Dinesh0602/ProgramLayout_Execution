#pragma once
//
// ControlUnit — drives the Fetch / Decode / Execute cycle.
//
// The control unit holds references to the three architectural pieces
// it coordinates (Bus, RegisterFile, ALU) and decides, for each fetched
// opcode, which sequence of operand reads, ALU calls, and writebacks to
// perform.
//
// Decoding is *table-driven*: a static array of {opcode, mnemonic,
// handler} entries is indexed by the opcode byte. Adding a new
// instruction is a one-line change in this table plus a new handler
// function — there is no central switch to grow.
//

#include "alu.hpp"
#include "bus.hpp"
#include "isa.hpp"
#include "register_file.hpp"

#include <array>
#include <cstdint>
#include <iostream>

namespace tiny {

class ControlUnit {
public:
    ControlUnit(Bus& bus, RegisterFile& rf)
        : bus_(bus), rf_(rf) {}

    // Run a single instruction. Returns false once HLT has fired.
    bool step();

    // Trace mode: print one disassembled line per executed instruction.
    void enable_trace(bool on, std::ostream* os = &std::cout) {
        trace_ = on;
        trace_os_ = os;
    }
    bool halted() const { return halted_; }

private:
    Bus&          bus_;
    RegisterFile& rf_;
    bool          halted_   = false;
    bool          trace_    = false;
    std::ostream* trace_os_ = &std::cout;

    // -------- fetch helpers ---------------------------------------------
    uint8_t  fetch8();
    uint16_t fetch16();

    // -------- handler signature ----------------------------------------
    //
    // Each opcode is implemented by a static method that takes the
    // ControlUnit and the address the instruction was fetched at (so
    // the trace line can show the right PC). The handler is responsible
    // for consuming all operand bytes and updating PC/registers/flags.
    //
    using Handler = void (*)(ControlUnit&, uint16_t /* op_pc */);

    struct Entry {
        uint8_t     opcode;
        const char* mnemonic;
        Handler     fn;
    };

    static const std::array<Entry, 26> dispatch_table_;
    static const Entry*                find_entry(uint8_t op);

    // Trace line emission.
    void emit_trace(uint16_t pc, const std::string& text) const;

    // -------- handlers --------------------------------------------------
    static void h_nop  (ControlUnit&, uint16_t);
    static void h_movi (ControlUnit&, uint16_t);
    static void h_ld   (ControlUnit&, uint16_t);
    static void h_st   (ControlUnit&, uint16_t);
    static void h_mov  (ControlUnit&, uint16_t);

    static void h_add  (ControlUnit&, uint16_t);
    static void h_sub  (ControlUnit&, uint16_t);
    static void h_and  (ControlUnit&, uint16_t);
    static void h_or   (ControlUnit&, uint16_t);
    static void h_xor  (ControlUnit&, uint16_t);
    static void h_cmp  (ControlUnit&, uint16_t);
    static void h_inc  (ControlUnit&, uint16_t);
    static void h_dec  (ControlUnit&, uint16_t);

    static void h_br   (ControlUnit&, uint16_t);
    static void h_beq  (ControlUnit&, uint16_t);
    static void h_bne  (ControlUnit&, uint16_t);
    static void h_bcs  (ControlUnit&, uint16_t);
    static void h_bcc  (ControlUnit&, uint16_t);
    static void h_bmi  (ControlUnit&, uint16_t);
    static void h_bpl  (ControlUnit&, uint16_t);

    static void h_push (ControlUnit&, uint16_t);
    static void h_pop  (ControlUnit&, uint16_t);

    static void h_call (ControlUnit&, uint16_t);
    static void h_ret  (ControlUnit&, uint16_t);

    static void h_hlt  (ControlUnit&, uint16_t);
};

} // namespace tiny
