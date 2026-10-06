#ifndef KOHERON_DPLL_GAIN_CONTROL_HPP
#define KOHERON_DPLL_GAIN_CONTROL_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>

namespace dpll_gain {

inline constexpr std::array<int64_t, 16> mantissas{
    2048, 2139, 2233, 2332, 2435, 2543, 2656, 2774,
    2896, 3025, 3158, 3298, 3444, 3597, 3756, 3922
};

// The legacy signed integer range is retained for both APIs. A gain command
// is a magnitude, not the effective PI/I2/I3 gain after their output slices.
inline constexpr int64_t minimum = int64_t(std::numeric_limits<int32_t>::min()) * 2048;
inline constexpr int64_t maximum = int64_t(std::numeric_limits<int32_t>::max()) * 2048;

inline bool geometric(int32_t sign, uint32_t step, int64_t& coefficient) {
    if (sign < -1 || sign > 1 || step > 496) return false;
    coefficient = sign * (mantissas[step % 16] << (step / 16));
    return coefficient >= minimum && coefficient <= maximum;
}

// Calls are serialized by the Dpll driver's RPC lock. Each transaction writes
// two 32-bit data words before changing the request toggle, and waits for the
// hardware acknowledgement before reusing the data registers. It never writes
// the active bank. No assumption about host write speed enters the data path.
template<class Control, class Status>
class Tables {
  public:
    Tables(Control& control, Status& status, uint32_t command, uint32_t data,
           uint32_t ack, uint32_t banks, uint32_t coefficients)
        : control_(control), status_(status), command_(command), data_(data),
          ack_(ack), banks_(banks), coefficients_(coefficients) {}

    bool synchronize() {
        request_ = control_.read_reg(command_);
        return wait(false);
    }

    bool program(uint32_t channel, uint32_t gain, int64_t coefficient) {
        if (channel >= 2 || gain >= 4 || coefficient < minimum || coefficient > maximum)
            return false;
        if (!synchronize()) return false;
        const uint32_t target = 4 * channel + gain;
        const uint32_t bank = 1U ^ ((status_.read_reg(banks_) >> target) & 1U);
        const uint32_t selection = (target << 6) | (bank << 5);
        for (uint32_t top = 0; top < 2; ++top) {
            for (uint32_t address = 0; address < 16; ++address) {
                const int64_t factor = top && address >= 8 ? int64_t(address) - 16 : address;
                if (!transact(selection | (top << 4) | address, coefficient * factor))
                    return false;
            }
        }
        // The hardware records this coefficient at the bank-switch edge.
        return transact(selection | (1U << 9), coefficient);
    }

    int64_t coefficient(uint32_t channel, uint32_t gain) {
        const uint32_t offset = coefficients_ + 8 * (4 * channel + gain);
        const uint64_t low = status_.read_reg(offset);
        const uint64_t high = status_.read_reg(offset + 4);
        return static_cast<int64_t>((high << 32) | low);
    }

  private:
    bool transact(uint32_t command, int64_t data) {
        const uint64_t bits = static_cast<uint64_t>(data);
        control_.write_reg(data_, uint32_t(bits));
        control_.write_reg(data_ + 4, uint32_t(bits >> 32));
        std::atomic_thread_fence(std::memory_order_seq_cst);
        request_ = ((request_ ^ 0x80000000U) & 0x80000000U) | command;
        control_.write_reg(command_, request_);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return wait(true);
    }

    bool wait(bool require_success) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        do {
            const uint32_t ack = status_.read_reg(ack_);
            if ((ack & ~0x40000000U) == request_)
                return !require_success || !(ack & 0x40000000U);
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }

    Control& control_;
    Status& status_;
    uint32_t command_, data_, ack_, banks_, coefficients_;
    uint32_t request_ = 0;
};
} // namespace dpll_gain
#endif
