#pragma once
//
// TinyCPU assembler.
//
// Pipeline:
//   1. Lex   — split a source string into per-line token vectors.
//   2. Sizing — walk every line to assign a byte address to each label.
//   3. Emit  — walk the lines again, resolving labels and producing the
//              actual opcode + operand bytes.
//
// The mnemonic / encoding tables live in a separate translation unit
// (assembler.cpp) so adding a new opcode is a localised change.
//

#include "isa.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace tiny {

struct Program {
    std::vector<uint8_t> image;
    uint16_t             origin = 0x0000;
};

class Assembler {
public:
    Program assemble(const std::string& source);

private:
    // ----- representation of a tokenised line --------------------------
    struct LexedLine {
        size_t                   lineno = 0;
        std::string              label;     // empty when absent
        std::vector<std::string> tokens;    // [mnemonic, operand, operand, ...]
    };

    // ----- lexing ------------------------------------------------------
    static std::vector<LexedLine> lex(const std::string& source);
    static std::vector<std::string> tokenize_line(const std::string& s);

    // ----- numeric / register parsing ----------------------------------
    static bool parse_register(const std::string& s, uint8_t& out);
    static bool parse_number  (const std::string& s, uint16_t& out);
    static std::string upper  (const std::string& s);

    // ----- the two passes ----------------------------------------------
    void                 size_pass(const std::vector<LexedLine>& lines);
    std::vector<uint8_t> emit_pass(const std::vector<LexedLine>& lines);

    // ----- helpers used during emit ------------------------------------
    uint16_t resolve(const std::string& tok) const;

    // ----- state -------------------------------------------------------
    std::unordered_map<std::string, uint16_t> labels_;
    uint16_t                                  origin_ = 0x0000;
};

} // namespace tiny
