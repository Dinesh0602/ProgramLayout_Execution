#include "control_unit.hpp"

#include <iomanip>
#include <sstream>
#include <string>

//
// ControlUnit implementation.
//
// The hot path is `step()`: it fetches one opcode byte, hands the
// dispatch table the work of finding the right handler, calls the
// handler, then advances the cycle counter on the bus. Every handler
// is a small, focused function that stays close to the architectural
// rules — no big switch, no fall-throughs.
//

namespace tiny {

// ---------------------------------------------------------------------------
// Fetch helpers
// ---------------------------------------------------------------------------
uint8_t ControlUnit::fetch8() {
    uint16_t pc = rf_.advance_pc(1);
    return bus_.read(pc);
}

uint16_t ControlUnit::fetch16() {
    uint16_t pc = rf_.pc();
    rf_.advance_pc(2);
    return bus_.read16(pc);
}

// ---------------------------------------------------------------------------
// Trace
// ---------------------------------------------------------------------------
void ControlUnit::emit_trace(uint16_t pc, const std::string& text) const {
    if (!trace_ || !trace_os_) return;
    auto& os = *trace_os_;
    auto saved = os.flags();
    os << "[pc=" << std::hex << std::setfill('0') << std::setw(4) << pc << "] "
       << text << "\n";
    os.flags(saved);
}

// ---------------------------------------------------------------------------
// Dispatch table
// ---------------------------------------------------------------------------
const std::array<ControlUnit::Entry, 26> ControlUnit::dispatch_table_ = {{
    { OP_NOP,  "NOP",  &ControlUnit::h_nop  },
    { OP_MOVI, "MOVI", &ControlUnit::h_movi },
    { OP_LD,   "LD",   &ControlUnit::h_ld   },
    { OP_ST,   "ST",   &ControlUnit::h_st   },
    { OP_MOV,  "MOV",  &ControlUnit::h_mov  },

    { OP_ADD,  "ADD",  &ControlUnit::h_add  },
    { OP_SUB,  "SUB",  &ControlUnit::h_sub  },
    { OP_AND,  "AND",  &ControlUnit::h_and  },
    { OP_OR,   "OR",   &ControlUnit::h_or   },
    { OP_XOR,  "XOR",  &ControlUnit::h_xor  },
    { OP_CMP,  "CMP",  &ControlUnit::h_cmp  },
    { OP_INC,  "INC",  &ControlUnit::h_inc  },
    { OP_DEC,  "DEC",  &ControlUnit::h_dec  },

    { OP_BR,   "BR",   &ControlUnit::h_br   },
    { OP_BEQ,  "BEQ",  &ControlUnit::h_beq  },
    { OP_BNE,  "BNE",  &ControlUnit::h_bne  },
    { OP_BCS,  "BCS",  &ControlUnit::h_bcs  },
    { OP_BCC,  "BCC",  &ControlUnit::h_bcc  },
    { OP_BMI,  "BMI",  &ControlUnit::h_bmi  },
    { OP_BPL,  "BPL",  &ControlUnit::h_bpl  },

    { OP_PUSH, "PUSH", &ControlUnit::h_push },
    { OP_POP,  "POP",  &ControlUnit::h_pop  },

    { OP_CALL, "CALL", &ControlUnit::h_call },
    { OP_RET,  "RET",  &ControlUnit::h_ret  },

    { OP_HLT,  "HLT",  &ControlUnit::h_hlt  },

    // One spare slot kept here so the table size matches the declared
    // std::array<Entry, 26>. Initialised to a benign no-op.
    { 0xFF,    "?",    &ControlUnit::h_nop  }
}};

const ControlUnit::Entry* ControlUnit::find_entry(uint8_t op) {
    for (const auto& e : dispatch_table_) {
        if (e.opcode == op) return &e;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Step (fetch / decode / execute)
// ---------------------------------------------------------------------------
bool ControlUnit::step() {
    if (halted_) return false;

    uint16_t op_pc = rf_.pc();
    uint8_t  op    = fetch8();

    const Entry* e = find_entry(op);
    if (!e || op == 0xFF) {
        std::cerr << "illegal opcode 0x" << std::hex << (int)op
                  << " at 0x" << std::setw(4) << std::setfill('0') << op_pc
                  << "\n";
        halted_ = true;
        return false;
    }

    e->fn(*this, op_pc);
    bus_.tick();
    return !halted_;
}

// ===========================================================================
// Handler implementations
// ===========================================================================
//
// Each handler:
//   1. consumes the opcode's operand bytes via fetch8/fetch16,
//   2. performs its architectural effect via RegisterFile / Bus / ALU,
//   3. emits a trace line if tracing is enabled.
//

namespace {
inline std::string reg_str(uint8_t r) { return reg_name(r); }
inline std::string hex16 (uint16_t v) {
    std::ostringstream os;
    os << "0x" << std::hex << std::setw(4) << std::setfill('0') << v;
    return os.str();
}
} // namespace

// ---------- 0x0_  data movement -----------------------------------------

void ControlUnit::h_nop(ControlUnit& c, uint16_t pc) {
    c.emit_trace(pc, "NOP");
}

void ControlUnit::h_movi(ControlUnit& c, uint16_t pc) {
    uint8_t r   = c.fetch8();
    uint8_t imm = c.fetch8();
    c.rf_.write(r, imm);
    alu::update_flags_for_move(imm, c.rf_.flags());
    c.emit_trace(pc, "MOVI " + reg_str(r) + ", #" + std::to_string((int)imm));
}

void ControlUnit::h_ld(ControlUnit& c, uint16_t pc) {
    uint8_t  r    = c.fetch8();
    uint16_t addr = c.fetch16();
    uint8_t  v    = c.bus_.read(addr);
    c.rf_.write(r, v);
    alu::update_flags_for_move(v, c.rf_.flags());
    c.emit_trace(pc, "LD " + reg_str(r) + ", [" + hex16(addr) + "]");
}

void ControlUnit::h_st(ControlUnit& c, uint16_t pc) {
    uint8_t  r    = c.fetch8();
    uint16_t addr = c.fetch16();
    uint8_t  v    = c.rf_.read(r);
    c.bus_.write(addr, v);
    c.emit_trace(pc, "ST " + reg_str(r) + ", [" + hex16(addr) + "]");
}

void ControlUnit::h_mov(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8();
    uint8_t s = c.fetch8();
    uint8_t v = c.rf_.read(s);
    c.rf_.write(d, v);
    alu::update_flags_for_move(v, c.rf_.flags());
    c.emit_trace(pc, "MOV " + reg_str(d) + ", " + reg_str(s));
}

// ---------- 0x2_  arithmetic / logic ------------------------------------

void ControlUnit::h_add(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8(), s = c.fetch8();
    uint8_t r = alu::op_add(c.rf_.read(d), c.rf_.read(s), c.rf_.flags());
    c.rf_.write(d, r);
    c.emit_trace(pc, "ADD " + reg_str(d) + ", " + reg_str(s));
}

void ControlUnit::h_sub(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8(), s = c.fetch8();
    uint8_t r = alu::op_sub(c.rf_.read(d), c.rf_.read(s), c.rf_.flags());
    c.rf_.write(d, r);
    c.emit_trace(pc, "SUB " + reg_str(d) + ", " + reg_str(s));
}

void ControlUnit::h_and(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8(), s = c.fetch8();
    uint8_t r = alu::op_and(c.rf_.read(d), c.rf_.read(s), c.rf_.flags());
    c.rf_.write(d, r);
    c.emit_trace(pc, "AND " + reg_str(d) + ", " + reg_str(s));
}

void ControlUnit::h_or(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8(), s = c.fetch8();
    uint8_t r = alu::op_or(c.rf_.read(d), c.rf_.read(s), c.rf_.flags());
    c.rf_.write(d, r);
    c.emit_trace(pc, "OR " + reg_str(d) + ", " + reg_str(s));
}

void ControlUnit::h_xor(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8(), s = c.fetch8();
    uint8_t r = alu::op_xor(c.rf_.read(d), c.rf_.read(s), c.rf_.flags());
    c.rf_.write(d, r);
    c.emit_trace(pc, "XOR " + reg_str(d) + ", " + reg_str(s));
}

void ControlUnit::h_cmp(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8(), s = c.fetch8();
    alu::op_cmp(c.rf_.read(d), c.rf_.read(s), c.rf_.flags());
    c.emit_trace(pc, "CMP " + reg_str(d) + ", " + reg_str(s));
}

void ControlUnit::h_inc(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8();
    uint8_t r = alu::op_inc(c.rf_.read(d), c.rf_.flags());
    c.rf_.write(d, r);
    c.emit_trace(pc, "INC " + reg_str(d));
}

void ControlUnit::h_dec(ControlUnit& c, uint16_t pc) {
    uint8_t d = c.fetch8();
    uint8_t r = alu::op_dec(c.rf_.read(d), c.rf_.flags());
    c.rf_.write(d, r);
    c.emit_trace(pc, "DEC " + reg_str(d));
}

// ---------- 0x4_  branches ----------------------------------------------

void ControlUnit::h_br(ControlUnit& c, uint16_t pc) {
    uint16_t addr = c.fetch16();
    c.rf_.set_pc(addr);
    c.emit_trace(pc, "BR " + hex16(addr));
}

void ControlUnit::h_beq(ControlUnit& c, uint16_t pc) {
    uint16_t addr = c.fetch16();
    if (c.rf_.flags().Z) c.rf_.set_pc(addr);
    c.emit_trace(pc, "BEQ " + hex16(addr));
}

void ControlUnit::h_bne(ControlUnit& c, uint16_t pc) {
    uint16_t addr = c.fetch16();
    if (!c.rf_.flags().Z) c.rf_.set_pc(addr);
    c.emit_trace(pc, "BNE " + hex16(addr));
}

void ControlUnit::h_bcs(ControlUnit& c, uint16_t pc) {
    uint16_t addr = c.fetch16();
    if (c.rf_.flags().C) c.rf_.set_pc(addr);
    c.emit_trace(pc, "BCS " + hex16(addr));
}

void ControlUnit::h_bcc(ControlUnit& c, uint16_t pc) {
    uint16_t addr = c.fetch16();
    if (!c.rf_.flags().C) c.rf_.set_pc(addr);
    c.emit_trace(pc, "BCC " + hex16(addr));
}

void ControlUnit::h_bmi(ControlUnit& c, uint16_t pc) {
    uint16_t addr = c.fetch16();
    if (c.rf_.flags().N) c.rf_.set_pc(addr);
    c.emit_trace(pc, "BMI " + hex16(addr));
}

void ControlUnit::h_bpl(ControlUnit& c, uint16_t pc) {
    uint16_t addr = c.fetch16();
    if (!c.rf_.flags().N) c.rf_.set_pc(addr);
    c.emit_trace(pc, "BPL " + hex16(addr));
}

// ---------- 0x6_  stack -------------------------------------------------

void ControlUnit::h_push(ControlUnit& c, uint16_t pc) {
    uint8_t  s   = c.fetch8();
    uint16_t slot = c.rf_.push_sp();
    c.bus_.write(slot, c.rf_.read(s));
    c.emit_trace(pc, "PUSH " + reg_str(s));
}

void ControlUnit::h_pop(ControlUnit& c, uint16_t pc) {
    uint8_t  d   = c.fetch8();
    uint16_t slot = c.rf_.pop_sp();
    uint8_t  v   = c.bus_.read(slot);
    c.rf_.write(d, v);
    alu::update_flags_for_move(v, c.rf_.flags());
    c.emit_trace(pc, "POP " + reg_str(d));
}

// ---------- 0x7_  function linkage --------------------------------------

void ControlUnit::h_call(ControlUnit& c, uint16_t pc) {
    // 1. Read the target address (3-byte instruction: op + lo + hi).
    uint16_t target = c.fetch16();

    // 2. The address PC is now pointing at is the instruction *after*
    //    CALL, which is exactly the address we want to return to.
    uint16_t ret_addr = c.rf_.pc();

    // 3. Push it as two bytes. We push the high byte first so that, on
    //    a downward-growing stack, the low byte ends up at the lower
    //    address (the "top"). RET will pop in the reverse order.
    uint16_t slot_hi = c.rf_.push_sp();
    c.bus_.write(slot_hi, (uint8_t)((ret_addr >> 8) & 0xFF));
    uint16_t slot_lo = c.rf_.push_sp();
    c.bus_.write(slot_lo, (uint8_t)(ret_addr & 0xFF));

    // 4. Jump to the target.
    c.rf_.set_pc(target);
    c.emit_trace(pc, "CALL " + hex16(target));
}

void ControlUnit::h_ret(ControlUnit& c, uint16_t pc) {
    // RET reverses what CALL did: pop low byte, then high byte, and
    // set PC to the recombined address.
    uint16_t slot_lo = c.rf_.pop_sp();
    uint8_t  lo      = c.bus_.read(slot_lo);
    uint16_t slot_hi = c.rf_.pop_sp();
    uint8_t  hi      = c.bus_.read(slot_hi);

    uint16_t ret_addr = (uint16_t)lo | ((uint16_t)hi << 8);
    c.rf_.set_pc(ret_addr);
    c.emit_trace(pc, "RET -> " + hex16(ret_addr));
}

// ---------- 0x8_  system ------------------------------------------------

void ControlUnit::h_hlt(ControlUnit& c, uint16_t pc) {
    c.halted_ = true;
    c.emit_trace(pc, "HLT");
}

} // namespace tiny
