#include "bus.hpp"

#include <iomanip>

//
// Bus implementation: the MMIO routing happens up front, so reads and
// writes to "real" RAM are just an array index in the fast path.
//

namespace tiny {

uint8_t Bus::read(uint16_t addr) const {
    switch (addr) {
        case MMIO_TTY_IN:
            return tty_in_ ? tty_in_() : 0;

        case MMIO_TIMER:
            return (uint8_t)(cycles_ & 0xFF);

        default:
            return ram_[addr];
    }
}

void Bus::write(uint16_t addr, uint8_t v) {
    switch (addr) {
        case MMIO_TTY_OUT:
            if (tty_out_) tty_out_(v);
            return;

        case MMIO_TIMER_CTRL:
            // Bit 0: any 1 here clears the cycle counter.
            if (v & 0x01) cycles_ = 0;
            return;

        default:
            ram_[addr] = v;
            return;
    }
}

uint16_t Bus::read16(uint16_t addr) const {
    uint8_t lo = read(addr);
    uint8_t hi = read((uint16_t)(addr + 1));
    return (uint16_t)lo | ((uint16_t)hi << 8);
}

void Bus::load_image(const std::vector<uint8_t>& bytes, uint16_t origin) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        size_t a = (size_t)origin + i;
        if (a < MEM_BYTES) ram_[a] = bytes[i];
    }
}

void Bus::hex_dump(uint16_t from, uint16_t to, std::ostream& os) const {
    for (uint32_t a = from; a <= to; a += 16) {
        os << std::hex << std::setfill('0');
        os << std::setw(4) << a << ": ";
        for (uint32_t i = 0; i < 16 && a + i <= to; ++i) {
            os << std::setw(2) << (int)ram_[a + i] << " ";
        }
        os << "\n";
    }
}

} // namespace tiny
