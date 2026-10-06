#include "../gain_control.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

struct Hardware {
    std::array<uint32_t, 24> control{};
    std::array<uint32_t, 20> status{};
    int64_t products[8][2][32]{};
    uint32_t filled[8][2]{};
    uint32_t pending = 0;
    uint64_t pending_data = 0;
    int delay = 0, writes = 0, commits = 0;
    bool reject = false, stall = false;

    void write(uint32_t offset, uint32_t value) {
        assert(!delay); // The payload cannot change before acknowledgement.
        control.at(offset / 4) = value;
        if (offset != 84) return;
        assert((value ^ status[2]) & 0x80000000U);
        pending = value;
        pending_data = uint64_t(control[22]) | (uint64_t(control[23]) << 32);
        delay = 3;
    }

    uint32_t read(uint32_t offset) {
        if (delay && !stall && --delay == 0) {
            const unsigned target = (pending >> 6) & 7;
            const unsigned bank = (pending >> 5) & 1;
            const unsigned address = pending & 31;
            const int64_t data = static_cast<int64_t>(pending_data);
            if (reject) {
                status[2] = pending | 0x40000000U;
                reject = false;
            } else {
                if (pending & 512) {
                    assert(filled[target][bank] == 0xffffffffU);
                    for (unsigned a = 0; a < 32; ++a) {
                        int factor = int(a & 15);
                        if (a >= 24) factor -= 16;
                        assert(products[target][bank][a] == data * factor);
                    }
                    status[3] = (status[3] & ~(1U << target)) | (bank << target);
                    status[4 + 2 * target] = uint32_t(pending_data);
                    status[5 + 2 * target] = uint32_t(pending_data >> 32);
                    ++commits;
                } else {
                    assert(bank != ((status[3] >> target) & 1));
                    products[target][bank][address] = data;
                    filled[target][bank] |= 1U << address;
                    ++writes;
                }
                status[2] = pending;
            }
        }
        return status.at(offset / 4);
    }
};
struct Control {
    Hardware& hw;
    uint32_t read_reg(uint32_t offset) { return hw.control.at(offset / 4); }
    void write_reg(uint32_t offset, uint32_t value) { hw.write(offset, value); }
};
struct Status {
    Hardware& hw;
    uint32_t read_reg(uint32_t offset) { return hw.read(offset); }
};

int main() {
    Hardware hw;
    Control control{hw};
    Status status{hw};
    using Tables = dpll_gain::Tables<Control, Status>;
    Tables tables(control, status, 84, 88, 8, 12, 16);
    unsigned cases = 0;
    double worst_error = 0;
    for (unsigned channel = 0; channel < 2; ++channel) {
        for (unsigned gain = 0; gain < 4; ++gain) {
            for (unsigned step = 0; step <= 496; ++step) {
                for (int sign : {-1, 1}) {
                    int64_t coefficient = 0;
                    const bool valid = dpll_gain::geometric(sign, step, coefficient);
                    if (step == 496 && sign == 1) { assert(!valid); continue; }
                    assert(valid);
                    worst_error = std::max(worst_error,
                        std::abs(double(coefficient) / (sign * 2048 * std::exp2(step / 16.0)) - 1));
                    assert(tables.program(channel, gain, coefficient));
                    assert(tables.coefficient(channel, gain) == coefficient);
                    ++cases;
                }
            }
            for (int32_t integer : {INT32_MIN, INT32_MAX, -1234567, -3, -1, 0, 1, 3, 1234567}) {
                assert(tables.program(channel, gain, int64_t(integer) * 2048));
                assert(tables.coefficient(channel, gain) == int64_t(integer) * 2048);
                ++cases;
            }
        }
    }
    assert(worst_error < 0.000204);
    assert(hw.writes == 32 * int(cases) && hw.commits == int(cases));
    const auto before = hw.control;
    assert(!tables.program(2, 0, 0));
    assert(!tables.program(0, 4, 0));
    assert(!tables.program(0, 0, dpll_gain::maximum + 1));
    assert(!tables.program(0, 0, dpll_gain::minimum - 1));
    int64_t coefficient = 0;
    assert(!dpll_gain::geometric(2, 0, coefficient));
    assert(!dpll_gain::geometric(1, 497, coefficient));
    assert(hw.control == before);

    const auto old_gain = tables.coefficient(0, 0);
    hw.reject = true;
    assert(!tables.program(0, 0, 2048));
    assert(tables.coefficient(0, 0) == old_gain);
    assert(tables.program(0, 0, -2139)); // A rejected command is recoverable.
    hw.stall = true;
    assert(!tables.program(0, 0, 2048));
    hw.stall = false;
    // Recreate the host object after a timeout/server restart. It must drain
    // the in-flight transaction and discover the current bank from hardware.
    Tables restarted(control, status, 84, 88, 8, 12, 16);
    assert(restarted.program(0, 0, 2332));
    assert(restarted.coefficient(0, 0) == 2332);
    std::cout << "Gain control checks passed: " << cases
              << " gains, all channels, exact integer compatibility, acknowledgement, restart and failure recovery\n";
}
