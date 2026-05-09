#include "isa.hpp"

//
// Lookup tables for mnemonics and per-instruction sizes. Kept here (not
// inline in isa.hpp) so the assembler, the disassembler, and the trace
// path all share the same source of truth.
//

namespace tiny {

const char* reg_name(uint8_t r) {
    static const char* names[NUM_REGS] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7"
    };
    return (r < NUM_REGS) ? names[r] : "??";
}

const char* mnemonic_for(uint8_t op) {
    switch (op) {
        case OP_NOP:  return "NOP";
        case OP_MOVI: return "MOVI";
        case OP_LD:   return "LD";
        case OP_ST:   return "ST";
        case OP_MOV:  return "MOV";

        case OP_ADD:  return "ADD";
        case OP_SUB:  return "SUB";
        case OP_AND:  return "AND";
        case OP_OR:   return "OR";
        case OP_XOR:  return "XOR";
        case OP_CMP:  return "CMP";
        case OP_INC:  return "INC";
        case OP_DEC:  return "DEC";

        case OP_BR:   return "BR";
        case OP_BEQ:  return "BEQ";
        case OP_BNE:  return "BNE";
        case OP_BCS:  return "BCS";
        case OP_BCC:  return "BCC";
        case OP_BMI:  return "BMI";
        case OP_BPL:  return "BPL";

        case OP_PUSH: return "PUSH";
        case OP_POP:  return "POP";

        case OP_CALL: return "CALL";
        case OP_RET:  return "RET";

        case OP_HLT:  return "HLT";

        default:      return "???";
    }
}

uint8_t instruction_size(uint8_t op) {
    switch (op) {
        // 1-byte
        case OP_NOP:
        case OP_HLT:
        case OP_RET:
            return 1;

        // 2-byte (single register)
        case OP_INC: case OP_DEC:
        case OP_PUSH: case OP_POP:
            return 2;

        // 3-byte (Rd + imm8 / Rd + Rs / branch / CALL)
        case OP_MOVI:
        case OP_MOV:
        case OP_ADD: case OP_SUB:
        case OP_AND: case OP_OR:  case OP_XOR:
        case OP_CMP:
        case OP_BR:  case OP_BEQ: case OP_BNE:
        case OP_BCS: case OP_BCC:
        case OP_BMI: case OP_BPL:
        case OP_CALL:
            return 3;

        // 4-byte (Rd/Rs + abs16)
        case OP_LD:
        case OP_ST:
            return 4;

        default:
            return 0;
    }
}

} // namespace tiny
