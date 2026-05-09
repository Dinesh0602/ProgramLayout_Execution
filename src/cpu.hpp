#pragma once
//
// CPU — facade that composes the four architectural pieces.
//
// External callers (the CLI driver, tests) only need to talk to this
// object: it owns the bus, the register file, and the control unit, and
// exposes the high-level operations the user actually wants — load a
// binary, run, dump memory.
//

#include "bus.hpp"
#include "control_unit.hpp"
#include "register_file.hpp"

#include <vector>

namespace tiny {

class CPU {
public:
    CPU() : ctrl_(bus_, regs_) {}

    // Wiring access — for tests and for the CLI driver.
    Bus&          bus()  { return bus_;  }
    RegisterFile& regs() { return regs_; }
    ControlUnit&  ctrl() { return ctrl_; }

    // ---- top-level operations ------------------------------------------
    void load(const std::vector<uint8_t>& bytes, uint16_t origin) {
        bus_.load_image(bytes, origin);
        regs_.set_pc(origin);
    }

    bool step()       { return ctrl_.step(); }
    void run_to_halt() { while (ctrl_.step()) {} }

    void dump(uint16_t from, uint16_t to) const {
        bus_.hex_dump(from, to);
    }

    void enable_trace(bool on) { ctrl_.enable_trace(on); }

private:
    Bus          bus_;
    RegisterFile regs_;
    ControlUnit  ctrl_;
};

} // namespace tiny
