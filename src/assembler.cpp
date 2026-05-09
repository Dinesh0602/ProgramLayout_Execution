#include "assembler.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

//
// Assembler implementation.
//
// Encoding is data-driven: a small table maps each mnemonic to the
// shape of its operand list, the opcode byte, and its size. The two
// passes share that table so they cannot disagree.
//

namespace tiny {

// ---------------------------------------------------------------------------
// Operand-shape descriptors
// ---------------------------------------------------------------------------
//
// `Shape` describes the assembly-level operand layout for one mnemonic.
// The size_pass uses the size; the emit_pass uses the encoder to produce
// bytes. Keeping both in the same row guarantees they agree.
//

namespace {

enum class Shape : uint8_t {
    None,        // mnemonic                   1 byte  (NOP, HLT)
    R,           // mnemonic Rd                2 bytes (INC, DEC, PUSH, POP)
    R_imm,       // mnemonic Rd, #imm8         3 bytes (MOVI)
    R_R,         // mnemonic Rd, Rs            3 bytes (MOV, ADD, ..., CMP)
    R_addr,      // mnemonic R, abs16 / label  4 bytes (LD, ST)
    Addr         // mnemonic abs16 / label     3 bytes (BR, BEQ, ...)
};

struct OpDef {
    const char* mnemonic;
    uint8_t     opcode;
    Shape       shape;
    uint8_t     size;
};

// The full instruction table. Mnemonics are stored upper-case so the
// assembler can be case-insensitive at the source level.
constexpr OpDef OPS[] = {
    { "NOP",  OP_NOP,  Shape::None,  1 },
    { "MOVI", OP_MOVI, Shape::R_imm, 3 },
    { "LD",   OP_LD,   Shape::R_addr,4 },
    { "ST",   OP_ST,   Shape::R_addr,4 },
    { "MOV",  OP_MOV,  Shape::R_R,   3 },

    { "ADD",  OP_ADD,  Shape::R_R,   3 },
    { "SUB",  OP_SUB,  Shape::R_R,   3 },
    { "AND",  OP_AND,  Shape::R_R,   3 },
    { "OR",   OP_OR,   Shape::R_R,   3 },
    { "XOR",  OP_XOR,  Shape::R_R,   3 },
    { "CMP",  OP_CMP,  Shape::R_R,   3 },
    { "INC",  OP_INC,  Shape::R,     2 },
    { "DEC",  OP_DEC,  Shape::R,     2 },

    { "BR",   OP_BR,   Shape::Addr,  3 },
    { "BEQ",  OP_BEQ,  Shape::Addr,  3 },
    { "BNE",  OP_BNE,  Shape::Addr,  3 },
    { "BCS",  OP_BCS,  Shape::Addr,  3 },
    { "BCC",  OP_BCC,  Shape::Addr,  3 },
    { "BMI",  OP_BMI,  Shape::Addr,  3 },
    { "BPL",  OP_BPL,  Shape::Addr,  3 },

    { "PUSH", OP_PUSH, Shape::R,     2 },
    { "POP",  OP_POP,  Shape::R,     2 },

    { "CALL", OP_CALL, Shape::Addr,  3 },
    { "RET",  OP_RET,  Shape::None,  1 },

    { "HLT",  OP_HLT,  Shape::None,  1 }
};

const OpDef* find_op(const std::string& mnem_upper) {
    for (const auto& d : OPS) {
        if (mnem_upper == d.mnemonic) return &d;
    }
    return nullptr;
}

inline std::string trim(const std::string& s) {
    size_t i = 0, j = s.size();
    while (i < j && std::isspace((unsigned char)s[i]))     ++i;
    while (j > i && std::isspace((unsigned char)s[j - 1])) --j;
    return s.substr(i, j - i);
}

} // namespace

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------
std::string Assembler::upper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return r;
}

bool Assembler::parse_register(const std::string& s, uint8_t& out) {
    std::string u = upper(s);
    if (u.size() == 2 && u[0] == 'R' && u[1] >= '0' && u[1] <= '7') {
        out = (uint8_t)(u[1] - '0');
        return true;
    }
    return false;
}

bool Assembler::parse_number(const std::string& s, uint16_t& out) {
    std::string t = s;
    if (!t.empty() && t[0] == '#') t = t.substr(1);

    // Character literal: 'X' or '\n' etc. (3 or 4 chars, quoted).
    if (t.size() >= 3 && t.front() == '\'' && t.back() == '\'') {
        std::string inner = t.substr(1, t.size() - 2);
        if (inner.size() == 1) {
            out = (uint16_t)(uint8_t)inner[0];
            return true;
        }
        if (inner.size() == 2 && inner[0] == '\\') {
            char c = inner[1];
            uint8_t v;
            switch (c) {
                case 'n':  v = '\n'; break;
                case 't':  v = '\t'; break;
                case 'r':  v = '\r'; break;
                case '0':  v = '\0'; break;
                case '\\': v = '\\'; break;
                case '\'': v = '\''; break;
                case '"':  v = '"';  break;
                default:   return false;
            }
            out = v;
            return true;
        }
        return false;
    }

    try {
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
            out = (uint16_t)std::stoul(t, nullptr, 16);
            return true;
        }
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'b' || t[1] == 'B')) {
            out = (uint16_t)std::stoul(t.substr(2), nullptr, 2);
            return true;
        }
        if (!t.empty() && (std::isdigit((unsigned char)t[0]) || t[0] == '-')) {
            out = (uint16_t)std::stoi(t);
            return true;
        }
    } catch (...) { /* fall through */ }
    return false;
}

// ---------------------------------------------------------------------------
// Lexing
// ---------------------------------------------------------------------------
std::vector<std::string> Assembler::tokenize_line(const std::string& s) {
    std::vector<std::string> toks;
    std::string cur;
    bool        in_dq = false;      // inside "double quotes"
    bool        in_sq = false;      // inside 'single quotes'

    auto flush = [&]() {
        if (!cur.empty()) { toks.push_back(cur); cur.clear(); }
    };

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (!in_dq && !in_sq && c == ';') break;            // comment
        if (!in_sq && c == '"') {
            cur.push_back(c);
            in_dq = !in_dq;
            continue;
        }
        if (!in_dq && c == '\'') {
            cur.push_back(c);
            in_sq = !in_sq;
            continue;
        }
        if (!in_dq && !in_sq && (std::isspace((unsigned char)c) || c == ',')) {
            flush();
        } else {
            cur.push_back(c);
        }
    }
    flush();
    return toks;
}

std::vector<Assembler::LexedLine> Assembler::lex(const std::string& source) {
    std::vector<LexedLine> out;
    std::istringstream iss(source);
    std::string raw;
    size_t lineno = 0;

    while (std::getline(iss, raw)) {
        ++lineno;
        std::string s = trim(raw);
        if (s.empty() || s[0] == ';') continue;

        LexedLine ln;
        ln.lineno = lineno;

        // Optional leading "label:". Only consider colons that appear
        // before the first ';' (otherwise a colon inside a trailing
        // comment, e.g. "MOVI R0, #3   ; arg: n = 3", would be wrongly
        // taken as a label separator).
        size_t scan_end = s.find(';');
        if (scan_end == std::string::npos) scan_end = s.size();
        size_t colon = s.find(':');
        if (colon != std::string::npos && colon < scan_end) {
            ln.label = trim(s.substr(0, colon));
            s        = trim(s.substr(colon + 1));
        }

        if (!s.empty()) {
            ln.tokens = tokenize_line(s);
        }
        if (!ln.label.empty() || !ln.tokens.empty()) {
            out.push_back(std::move(ln));
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Pass 1 — sizing (assign addresses to labels)
// ---------------------------------------------------------------------------
void Assembler::size_pass(const std::vector<LexedLine>& lines) {
    labels_.clear();
    origin_      = 0x0000;
    uint16_t pc  = origin_;

    for (const auto& ln : lines) {
        if (!ln.label.empty()) {
            labels_[upper(ln.label)] = pc;
        }
        if (ln.tokens.empty()) continue;

        const std::string mnem = upper(ln.tokens[0]);

        // Directives are sized by their content.
        if (mnem == ".ORG") {
            if (ln.tokens.size() < 2)
                throw std::runtime_error(".org needs an address");
            uint16_t v;
            if (!parse_number(ln.tokens[1], v))
                throw std::runtime_error(".org expects a numeric address");
            pc      = v;
            origin_ = v;
            continue;
        }
        if (mnem == ".BYTE") {
            for (size_t i = 1; i < ln.tokens.size(); ++i) {
                const std::string& t = ln.tokens[i];
                if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
                    pc += (uint16_t)(t.size() - 2);    // chars inside quotes
                } else {
                    pc += 1;
                }
            }
            continue;
        }
        if (mnem == ".WORD") {
            pc += (uint16_t)((ln.tokens.size() - 1) * 2);
            continue;
        }

        // Real instruction.
        const OpDef* def = find_op(mnem);
        if (!def) throw std::runtime_error("unknown mnemonic: " + ln.tokens[0]);
        pc += def->size;
    }
}

// ---------------------------------------------------------------------------
// Pass 2 — emitting bytes
// ---------------------------------------------------------------------------
uint16_t Assembler::resolve(const std::string& tok) const {
    // Optional [..] brackets — `LD R0, [0xC000]` and `LD R0, 0xC000` are
    // both accepted.
    std::string body = tok;
    if (body.size() >= 2 && body.front() == '[' && body.back() == ']') {
        body = body.substr(1, body.size() - 2);
    }
    uint16_t v;
    auto u = upper(body);
    auto it = labels_.find(u);
    if (it != labels_.end()) return it->second;
    if (parse_number(body, v)) return v;
    throw std::runtime_error("unresolved symbol or invalid number: " + tok);
}

std::vector<uint8_t> Assembler::emit_pass(const std::vector<LexedLine>& lines) {
    std::vector<uint8_t> out;
    uint16_t pc = origin_;

    auto put8  = [&](uint8_t b)  { out.push_back(b); ++pc; };
    auto put16 = [&](uint16_t w) {
        out.push_back((uint8_t)(w & 0xFF));
        out.push_back((uint8_t)((w >> 8) & 0xFF));
        pc = (uint16_t)(pc + 2);
    };

    for (const auto& ln : lines) {
        if (ln.tokens.empty()) continue;
        const std::string mnem = upper(ln.tokens[0]);

        // ---- directives ------------------------------------------------
        if (mnem == ".ORG") {
            uint16_t v;
            if (!parse_number(ln.tokens[1], v))
                throw std::runtime_error(".org expects a numeric address");
            if (v < pc) throw std::runtime_error(".org cannot move backwards");
            while (pc < v) put8(0x00);
            continue;
        }
        if (mnem == ".BYTE") {
            for (size_t i = 1; i < ln.tokens.size(); ++i) {
                const std::string& t = ln.tokens[i];
                if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
                    for (size_t k = 1; k + 1 < t.size(); ++k)
                        put8((uint8_t)t[k]);
                } else {
                    uint16_t v;
                    if (!parse_number(t, v))
                        throw std::runtime_error(".byte expects numbers or a quoted string");
                    put8((uint8_t)v);
                }
            }
            continue;
        }
        if (mnem == ".WORD") {
            for (size_t i = 1; i < ln.tokens.size(); ++i) {
                put16(resolve(ln.tokens[i]));
            }
            continue;
        }

        // ---- real instruction -----------------------------------------
        const OpDef* def = find_op(mnem);
        if (!def) throw std::runtime_error("unknown mnemonic: " + ln.tokens[0]);

        const auto& toks = ln.tokens;

        switch (def->shape) {
            case Shape::None: {
                if (toks.size() != 1)
                    throw std::runtime_error(std::string(def->mnemonic) + " takes no operands");
                put8(def->opcode);
                break;
            }
            case Shape::R: {
                if (toks.size() != 2)
                    throw std::runtime_error(std::string(def->mnemonic) + " expects one register");
                uint8_t r;
                if (!parse_register(toks[1], r))
                    throw std::runtime_error(std::string(def->mnemonic) + ": bad register");
                put8(def->opcode);
                put8(r);
                break;
            }
            case Shape::R_imm: {
                if (toks.size() != 3)
                    throw std::runtime_error(std::string(def->mnemonic) + " expects Rd, #imm8");
                uint8_t  r;
                uint16_t v;
                if (!parse_register(toks[1], r))
                    throw std::runtime_error(std::string(def->mnemonic) + ": bad register");
                if (!parse_number(toks[2], v))
                    throw std::runtime_error(std::string(def->mnemonic) + ": bad immediate");
                put8(def->opcode);
                put8(r);
                put8((uint8_t)v);
                break;
            }
            case Shape::R_R: {
                if (toks.size() != 3)
                    throw std::runtime_error(std::string(def->mnemonic) + " expects Rd, Rs");
                uint8_t d, s;
                if (!parse_register(toks[1], d) || !parse_register(toks[2], s))
                    throw std::runtime_error(std::string(def->mnemonic) + ": bad register");
                put8(def->opcode);
                put8(d);
                put8(s);
                break;
            }
            case Shape::R_addr: {
                if (toks.size() != 3)
                    throw std::runtime_error(std::string(def->mnemonic) + " expects R, addr");
                uint8_t r;
                if (!parse_register(toks[1], r))
                    throw std::runtime_error(std::string(def->mnemonic) + ": bad register");
                uint16_t a = resolve(toks[2]);
                put8(def->opcode);
                put8(r);
                put16(a);
                break;
            }
            case Shape::Addr: {
                if (toks.size() != 2)
                    throw std::runtime_error(std::string(def->mnemonic) + " expects an address");
                uint16_t a = resolve(toks[1]);
                put8(def->opcode);
                put16(a);
                break;
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
Program Assembler::assemble(const std::string& source) {
    auto lines = lex(source);
    size_pass(lines);

    Program p;
    p.image  = emit_pass(lines);
    p.origin = origin_;
    return p;
}

} // namespace tiny
