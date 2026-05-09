#include "assembler.hpp"
#include "cpu.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

//
// TinyCPU CLI driver.
//
// Subcommands:
//   asm  <source.asm>   -o <out.bin>
//       Assemble source -> flat binary image. The binary is just the
//       opcode/operand stream that should be loaded at the program's
//       origin (set by `.org`, default 0x0000).
//
//   run  <program.bin>  [--origin 0x..] [--trace] [--dump-after F:T]
//       Load image -> point PC at origin -> step until HLT.
//       --trace prints one disassembled line per instruction.
//       --dump-after prints a hex dump after the program halts.
//
//   dump <program.bin>  [--origin 0x..] [F:T]
//       Load image and print a hex dump of memory[F:T].
//

using namespace tiny;

// ---- file helpers ----------------------------------------------------------

namespace {

std::string slurp(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("cannot open: " + path);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

void spit(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("cannot write: " + path);
    ofs.write((const char*)bytes.data(), (std::streamsize)bytes.size());
}

std::vector<uint8_t> read_image(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) throw std::runtime_error("cannot open: " + path);
    return std::vector<uint8_t>{
        std::istreambuf_iterator<char>(ifs),
        std::istreambuf_iterator<char>()
    };
}

// Parse "from:to" where each side is a number in any C base. Throws on
// malformed input.
void parse_range(const std::string& s, uint16_t& from, uint16_t& to) {
    auto p = s.find(':');
    if (p == std::string::npos)
        throw std::runtime_error("range must be of the form FROM:TO");
    from = (uint16_t)std::stoul(s.substr(0, p), nullptr, 0);
    to   = (uint16_t)std::stoul(s.substr(p + 1), nullptr, 0);
}

void usage() {
    std::cout
        << "TinyCPU CLI:\n"
        << "  tinycpu asm  <source.asm> -o <out.bin>\n"
        << "  tinycpu run  <program.bin> [--origin 0x..] [--trace] [--dump-after F:T]\n"
        << "  tinycpu dump <program.bin> [--origin 0x..] [F:T]\n";
}

} // namespace

// ---- subcommands -----------------------------------------------------------

static int do_asm(int argc, char** argv) {
    if (argc < 5) { usage(); return 1; }

    std::string in  = argv[2];
    std::string out;
    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) out = argv[++i];
    }
    if (out.empty()) {
        std::cerr << "asm: -o <out.bin> is required\n";
        return 1;
    }

    try {
        Assembler asmblr;
        Program p = asmblr.assemble(slurp(in));
        spit(out, p.image);
        std::cout << "assembled " << in << " -> " << out
                  << "  (" << p.image.size() << " bytes @ 0x"
                  << std::hex << p.origin << std::dec << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "asm error: " << e.what() << "\n";
        return 2;
    }
    return 0;
}

static int do_run(int argc, char** argv) {
    if (argc < 3) { usage(); return 1; }

    std::string bin = argv[2];
    uint16_t    origin     = 0x0000;
    bool        trace      = false;
    uint16_t    dump_from  = 0;
    uint16_t    dump_to    = 0;
    bool        do_dump    = false;

    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--origin" && i + 1 < argc) {
            origin = (uint16_t)std::stoul(argv[++i], nullptr, 0);
        } else if (a == "--trace") {
            trace = true;
        } else if (a == "--dump-after" && i + 1 < argc) {
            parse_range(argv[++i], dump_from, dump_to);
            do_dump = true;
        }
    }

    try {
        auto bytes = read_image(bin);
        CPU  cpu;
        cpu.enable_trace(trace);
        cpu.load(bytes, origin);
        cpu.run_to_halt();
        if (do_dump) cpu.dump(dump_from, dump_to);
    } catch (const std::exception& e) {
        std::cerr << "run error: " << e.what() << "\n";
        return 2;
    }
    return 0;
}

static int do_dump(int argc, char** argv) {
    if (argc < 3) { usage(); return 1; }

    std::string bin = argv[2];
    uint16_t    origin    = 0x0000;
    uint16_t    dump_from = 0;
    uint16_t    dump_to   = 0x00FF;

    for (int i = 3; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--origin" && i + 1 < argc) {
            origin = (uint16_t)std::stoul(argv[++i], nullptr, 0);
        } else if (a.find(':') != std::string::npos) {
            parse_range(a, dump_from, dump_to);
        }
    }

    try {
        auto bytes = read_image(bin);
        CPU  cpu;
        cpu.load(bytes, origin);
        cpu.dump(dump_from, dump_to);
    } catch (const std::exception& e) {
        std::cerr << "dump error: " << e.what() << "\n";
        return 2;
    }
    return 0;
}

// ---- entry point -----------------------------------------------------------

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }
    std::string cmd = argv[1];
    if (cmd == "asm")  return do_asm(argc, argv);
    if (cmd == "run")  return do_run(argc, argv);
    if (cmd == "dump") return do_dump(argc, argv);
    usage();
    return 1;
}
