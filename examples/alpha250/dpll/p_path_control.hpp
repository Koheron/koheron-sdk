#ifndef KOHERON_DPLL_P_PATH_CONTROL_HPP
#define KOHERON_DPLL_P_PATH_CONTROL_HPP

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace dpll_p {
// Hardware projects onto the captured reference without doing an angle
// calculation. A fixed reference amplitude is intentional for this prototype.
inline bool calibrate(uint32_t snapshot, int32_t& cx, int32_t& cy) {
    const double i = static_cast<int16_t>(snapshot & 0xffffU);
    const double q = static_cast<int16_t>(snapshot >> 16);
    const double norm = i*i + q*q;
    if (norm < 4096.0) return false;
    constexpr double scale = 8192.0 / 3.14159265358979323846 * 262144.0;
    const double x = std::round(-q * scale / norm);
    const double y = std::round(i * scale / norm);
    // Signed Q6.18 fits one DSP input. A0>=64 bounds either coefficient
    // by 8192/(pi*64), below 64, without losing fractional precision.
    if (x < -16777216.0 || x > 16777215.0 ||
        y < -16777216.0 || y > 16777215.0) return false;
    cx = static_cast<int32_t>(x); cy = static_cast<int32_t>(y);
    return true;
}

// Serialized by the Dpll RPC lock. Capture is averaged over 64 ADC clocks and
// acknowledged before reading its coherent I/Q word. Other channel bits survive.
template<class Control, class Status>
class Paths {
  public:
    Paths(Control& control, Status& status, uint32_t command, uint32_t coefficients,
          uint32_t status_offset, uint32_t snapshots, uint32_t integrators)
        : control_(control), status_(status), command_(command), coefficients_(coefficients),
          status_offset_(status_offset), snapshots_(snapshots), integrators_(integrators) {}

    // 0=success, -1=invalid argument, -2=acknowledgement timeout,
    // -3=fast accumulator disabled or insufficient calibration amplitude.
    int32_t select(uint32_t channel, uint32_t mode) {
        if (channel >= 2 || mode > 1) return -1;
        uint32_t request = control_.read_reg(command_);
        const uint32_t mode_bit = 1U << channel;
        if (!mode) {
            write(request & ~mode_bit);
            return wait(channel, 1U, 0U) ? 0 : -2;
        }
        if ((control_.read_reg(integrators_ + 4*channel) & 5U) != 5U) return -3;
        if (status_.read_reg(status_offset_ + 4*channel) & 1U) return 0;
        const uint32_t capture_bit = 1U << (8+channel);
        // Recover an outstanding capture after a server restart or timeout.
        if (!wait(channel, 4U, (request & capture_bit) ? 4U : 0U)) return -2;
        request = (request & ~mode_bit) ^ capture_bit;
        write(request);
        if (!wait(channel, 4U, (request & capture_bit) ? 4U : 0U)) return -2;
        int32_t cx = 0, cy = 0;
        if (!calibrate(status_.read_reg(snapshots_ + 4*channel), cx, cy)) return -3;
        control_.write_reg(coefficients_ + 8*channel, static_cast<uint32_t>(cx));
        control_.write_reg(coefficients_ + 8*channel + 4, static_cast<uint32_t>(cy));
        write(request | mode_bit);
        if (wait(channel, 1U, 1U)) return 0;
        // Do not leave an unacknowledged Fast request armed for later recovery.
        write(request & ~mode_bit);
        return -2;
    }

  private:
    void write(uint32_t request) {
        std::atomic_thread_fence(std::memory_order_seq_cst);
        control_.write_reg(command_, request);
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }
    bool wait(uint32_t channel, uint32_t mask, uint32_t value) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        do {
            if ((status_.read_reg(status_offset_ + 4*channel) & mask) == value) return true;
        } while (std::chrono::steady_clock::now() < deadline);
        return false;
    }
    Control& control_;
    Status& status_;
    uint32_t command_, coefficients_, status_offset_, snapshots_, integrators_;
};
} // namespace dpll_p
#endif
