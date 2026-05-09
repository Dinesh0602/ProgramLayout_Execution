#pragma once
//
// Bus — owns physical memory, the on-chip cycle counter, and the routing
// of memory-mapped I/O.
//
// Every architectural memory access must go through Bus::read / write.
// The control unit, the assembler-loader, and the dump command all hit
// it here. That single chokepoint is what gives us a clean place to
// dispatch MMIO addresses to host devices (TTY in/out, timer) without
// the rest of the emulator needing to know about them.
//
// Devices are wired in via std::function callbacks rather than being
// hardcoded — this leaves the bus open for new MMIO devices in the
// future without touching its source.
//

#include "isa.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

namespace tiny {

class Bus {
public:
    Bus() { ram_.fill(0); }

    // -------- I/O wiring ---------------------------------------------------
    // These default to "console out goes to stdout" and "console in always
    // reports no key". Tests can swap them out.
    using OutFn = std::function<void(uint8_t)>;
    using InFn  = std::function<uint8_t()>;

    void on_tty_out(OutFn f) { tty_out_ = std::move(f); }
    void on_tty_in (InFn  f) { tty_in_  = std::move(f); }

    // -------- bus access ---------------------------------------------------
    // 8-bit access (the architectural primitive).
    uint8_t  read (uint16_t addr) const;
    void     write(uint16_t addr, uint8_t v);

    // 16-bit little-endian convenience (used for fetch16).
    uint16_t read16(uint16_t addr) const;

    // -------- clock --------------------------------------------------------
    // The control unit calls tick() once per retired instruction. The
    // cycle count is exposed via the MMIO_TIMER port.
    void     tick()                     { ++cycles_; }
    uint64_t cycles() const             { return cycles_; }
    void     reset_cycles()             { cycles_ = 0; }

    // -------- helpers ------------------------------------------------------
    void     load_image(const std::vector<uint8_t>& bytes, uint16_t origin);
    void     hex_dump(uint16_t from, uint16_t to, std::ostream& os = std::cout) const;

private:
    std::array<uint8_t, MEM_BYTES> ram_{};
    uint64_t                       cycles_ = 0;

    OutFn tty_out_ = [](uint8_t c) { std::cout << (char)c; };
    InFn  tty_in_  = []() -> uint8_t { return 0; };
};

} // namespace tiny
