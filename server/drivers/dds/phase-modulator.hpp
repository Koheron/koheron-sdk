#ifndef KOHERON_DDS_PHASE_MODULATOR_HPP
#define KOHERON_DDS_PHASE_MODULATOR_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <thread>

namespace dds_pm {

enum class Waveform : uint32_t {
    sine, square, pulse, triangle, up_ramp, down_ramp,
    uniform_noise, gaussian_noise, prbs, bpsk
};

struct Settings {
    uint64_t carrier_increment = 0;
    uint64_t carrier_phase = 0;
    uint64_t modulation_increment = 0;
    uint64_t modulation_phase = 0;
    uint64_t deviation = 0;
    uint64_t duty = 0;
    uint32_t seed = 1;
    Waveform waveform = Waveform::sine;
    bool output_enabled = false;
    bool pm_enabled = false;
};

// Memory needs the SDK read_reg<uint32_t>/write_reg<uint32_t> interface.
// No board dependency: all phase quantities are native integer words.
template<class Memory>
class Controller {
public:
    explicit Controller(Memory& memory) : memory_(memory) {
        if (read(0) != 0x504d0001) { error_ = "DDS PM IP identifier mismatch"; return; }
        const auto precision = read(8);
        phase_width_ = precision & 0xff;
        prbs_width_ = precision >> 24;
        capabilities_ = read(4);
        if (phase_width_ < 32 || phase_width_ > 48 ||
            (prbs_width_ != 7 && prbs_width_ != 15 && prbs_width_ != 23 && prbs_width_ != 31)) {
            error_ = "Unsupported DDS PM precision";
            return;
        }
        valid_ = true;
    }

    uint32_t phase_width() const { return phase_width_; }
    uint32_t capabilities() const { return capabilities_; }
    const char* error() const { return error_; }

    bool configure(const Settings& settings, bool restart_carrier = false,
                   bool restart_modulation = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!valid_) return false;
        const uint64_t turn = uint64_t{1} << phase_width_;
        const auto shape = static_cast<uint32_t>(settings.waveform);
        if (settings.carrier_increment >= turn || settings.carrier_phase >= turn ||
            settings.modulation_increment >= turn || settings.modulation_phase >= turn ||
            settings.deviation > turn || settings.duty > turn || shape > 9) {
            error_ = "DDS PM setting outside native word range"; return false;
        }
        if (settings.pm_enabled && !(capabilities_ & (uint32_t{1} << shape))) {
            error_ = "PM waveform is disabled in this FPGA build"; return false;
        }
        if ((shape == 6 || shape == 7) && settings.seed == 0) {
            error_ = "Noise seed must be nonzero"; return false;
        }
        if (shape == 8 && !(settings.seed & ((uint32_t{1} << prbs_width_) - 1))) {
            error_ = "PRBS seed must be nonzero within the configured order"; return false;
        }

        if (!wait_idle()) return false;
        word(0x20, settings.carrier_increment);
        word(0x28, settings.carrier_phase);
        word(0x30, settings.modulation_increment);
        word(0x38, settings.modulation_phase);
        word(0x40, settings.deviation);
        word(0x48, settings.duty);
        write(0x50, settings.seed);
        write(0x54, (shape << 8) | (settings.output_enabled ? 1u : 0u) |
                    (settings.pm_enabled ? 2u : 0u));
        // Publish all shadow words before issuing the commit on ARM as well.
        std::atomic_thread_fence(std::memory_order_seq_cst);
        write(0x10, 1u | (restart_carrier ? 2u : 0u) | (restart_modulation ? 4u : 0u));
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (!wait_idle()) return false;
        error_ = "";
        return true;
    }

private:
    uint32_t read(uint32_t offset) { return memory_.template read_reg<uint32_t>(offset); }
    void write(uint32_t offset, uint32_t value) { memory_.template write_reg<uint32_t>(offset, value); }
    void word(uint32_t offset, uint64_t value) {
        write(offset, static_cast<uint32_t>(value));
        write(offset + 4, static_cast<uint32_t>(value >> 32));
    }
    bool wait_idle() {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (read(0x0c) & 1u) {
            if (std::chrono::steady_clock::now() >= deadline) {
                error_ = "DDS PM commit timeout: check the sample clock"; return false;
            }
            std::this_thread::yield();
        }
        return true;
    }
    Memory& memory_;
    std::mutex mutex_;
    uint32_t phase_width_ = 0, prbs_width_ = 0, capabilities_ = 0;
    bool valid_ = false;
    const char* error_ = "";
};
}
#endif
