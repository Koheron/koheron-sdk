#ifndef KOHERON_DDS_PHASE_MODULATOR_HPP
#define KOHERON_DDS_PHASE_MODULATOR_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
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

    bool operator==(const Settings&) const = default;
};

// Engineering units for ordinary use. A configure_signal call enables a tone;
// phase modulation is opt-in. Native Settings remains available for exact words.
struct SignalSettings {
    long double carrier_hz = 10'000'000.0L;
    long double carrier_phase_deg = 0.0L;
    long double modulation_hz = 1'000.0L;
    long double modulation_phase_deg = 0.0L;
    long double deviation_deg = 30.0L;
    long double duty = 0.5L;
    uint32_t seed = 1;
    Waveform waveform = Waveform::sine;
    bool output_enabled = true;
    bool pm_enabled = false;
};

struct ChannelInfo {
    uint32_t phase_width = 0;
    uint32_t modulation_width = 0;
    uint32_t lut_bits = 0;
    uint32_t prbs_width = 0;
    uint32_t output_width = 0;
    uint32_t capabilities = 0;

    bool operator==(const ChannelInfo&) const = default;
};

enum class Error : uint32_t {
    none, identifier_mismatch, invalid_channel_count, invalid_metadata,
    inconsistent_channels, invalid_channel, out_of_range, unavailable_waveform,
    invalid_sample_rate, invalid_frequency, invalid_phase, invalid_deviation, invalid_duty,
    invalid_noise_seed, invalid_prbs_seed, pending_commit_timeout, commit_timeout
};

struct Result {
    Error code = Error::none;
    explicit operator bool() const { return code == Error::none; }
    const char* message() const {
        switch (code) {
            case Error::none: return "";
            case Error::identifier_mismatch: return "DDS PM IP identifier mismatch";
            case Error::invalid_channel_count: return "DDS PM channel count must be 1 or 2";
            case Error::invalid_metadata: return "Unsupported DDS PM metadata";
            case Error::inconsistent_channels: return "DDS PM channel metadata does not match";
            case Error::invalid_channel: return "DDS PM channel is absent";
            case Error::out_of_range: return "DDS PM setting outside native word range";
            case Error::unavailable_waveform: return "PM waveform is disabled in this FPGA build";
            case Error::invalid_sample_rate: return "Sample rate must be finite and positive";
            case Error::invalid_frequency: return "Frequency must be finite, nonnegative and below Nyquist";
            case Error::invalid_phase: return "Phase must be finite";
            case Error::invalid_deviation: return "Deviation must be finite and between 0 and 360 degrees";
            case Error::invalid_duty: return "Duty must be finite and between 0 and 1";
            case Error::invalid_noise_seed: return "Noise seed must be nonzero";
            case Error::invalid_prbs_seed: return "PRBS seed must be nonzero within the configured order";
            case Error::pending_commit_timeout: return "Previous DDS PM commit is still pending: check the sample clock";
            case Error::commit_timeout: return "DDS PM commit acknowledgement timed out; settings may still apply when the sample clock resumes";
            default: return "Unknown DDS PM error";
        }
    }
};

// One controller owns the integrated IP's entire 8 KiB register region.
// Memory supplies the SDK read_reg<uint32_t>/write_reg<uint32_t> interface.
// Native words and optional sampling-rate-based engineering units are supported.
// Use one Controller per region: its locks serialize configurations per bank.
template<class Memory>
class Controller {
public:
    explicit Controller(Memory& memory,
                        std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
        : memory_(memory), timeout_(timeout) {
        if (read(0, id) != identifier) { fail_initialization(Error::identifier_mismatch); return; }
        const auto count = read(0, channel_count_register);
        if (count < 1 || count > max_channels) { fail_initialization(Error::invalid_channel_count); return; }
        // Discover the count before accessing bank 1: absent-bank MMIO faults.
        for (uint32_t channel = 0; channel < count; ++channel) {
            if (channel != 0 && read(channel, id) != identifier) {
                fail_initialization(Error::identifier_mismatch); return;
            }
            const auto precision_word = read(channel, precision);
            auto& info = info_[channel];
            info.phase_width = precision_word & 0xff;
            info.modulation_width = (precision_word >> 8) & 0xff;
            info.lut_bits = (precision_word >> 16) & 0xff;
            info.prbs_width = precision_word >> 24;
            info.capabilities = read(channel, capabilities_register);
            info.output_width = read(channel, output_width_register);
            if (info.phase_width < 32 || info.phase_width > 48 ||
                info.modulation_width < 16 || info.modulation_width > 24 ||
                info.lut_bits < 8 || info.lut_bits > 18 ||
                info.output_width < 12 || info.output_width > 24 ||
                (info.prbs_width != 7 && info.prbs_width != 15 && info.prbs_width != 23 && info.prbs_width != 31) ||
                (info.capabilities & ~source_mask) != 0) {
                fail_initialization(Error::invalid_metadata); return;
            }
            if (read(channel, channel_count_register) != count || (channel != 0 && info != info_[0])) {
                fail_initialization(Error::inconsistent_channels); return;
            }
        }
        channel_count_ = count;
    }

    Controller(Memory& memory, long double sample_rate,
               std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
        : Controller(memory, timeout) {
        sample_rate_ = sample_rate;
        if (valid() && (!std::isfinite(sample_rate) || sample_rate <= 0.0L)) {
            channel_count_ = 0;
            fail_initialization(Error::invalid_sample_rate);
        }
    }

    bool valid() const { return channel_count_ != 0; }
    uint32_t channel_count() const { return channel_count_; }
    Result initialization_result() const { return {initialization_error_}; }
    ChannelInfo channel_info(uint32_t channel = 0) const {
        return channel < channel_count_ ? info_[channel] : ChannelInfo{};
    }
    uint32_t phase_width(uint32_t channel = 0) const { return channel_info(channel).phase_width; }
    uint32_t capabilities(uint32_t channel = 0) const { return channel_info(channel).capabilities; }
    uint64_t full_turn(uint32_t channel = 0) const {
        return channel < channel_count_ ? uint64_t{1} << info_[channel].phase_width : 0;
    }
    Settings default_settings(uint32_t channel = 0) const {
        Settings settings;
        settings.duty = full_turn(channel) / 2;
        return settings;
    }
    // Compatibility accessor. For a particular call, use its returned Result:
    // another thread may have completed a later operation by the time this runs.
    const char* error() const { return Result{last_error_.load(std::memory_order_relaxed)}.message(); }

    Result configure(uint32_t channel, const Settings& settings,
                     bool restart_carrier = false, bool restart_modulation = false) {
        if (!valid()) return finish(initialization_error_);
        if (channel >= channel_count_) return finish(Error::invalid_channel);
        std::lock_guard<std::mutex> lock(mutex_[channel]);
        return configure_locked(channel, settings, restart_carrier, restart_modulation,
                                std::chrono::steady_clock::now() + timeout_);
    }

    Result configure(const Settings& settings, bool restart_carrier = false,
                     bool restart_modulation = false) {
        return configure(0, settings, restart_carrier, restart_modulation);
    }

    Result configure_signal(uint32_t channel, const SignalSettings& signal) {
        return configure_signal(channel, signal, sample_rate_);
    }
    Result set_carrier_frequency(uint32_t channel, long double hz) {
        return set_carrier_frequency(channel, hz, sample_rate_);
    }
    Result set_modulation_frequency(uint32_t channel, long double hz) {
        return set_modulation_frequency(channel, hz, sample_rate_);
    }

    Result configure_signal(uint32_t channel, const SignalSettings& signal, long double sample_rate,
                            bool restart_carrier = true, bool restart_modulation = true) {
        if (!valid()) return finish(initialization_error_);
        if (channel >= channel_count_) return finish(Error::invalid_channel);
        auto settings = default_settings(channel);
        Result result;
        if (!(result = frequency_word(signal.carrier_hz, sample_rate, full_turn(channel), settings.carrier_increment)) ||
            !(result = frequency_word(signal.modulation_hz, sample_rate, full_turn(channel), settings.modulation_increment)) ||
            !(result = phase_word(signal.carrier_phase_deg, full_turn(channel), settings.carrier_phase)) ||
            !(result = phase_word(signal.modulation_phase_deg, full_turn(channel), settings.modulation_phase)) ||
            !(result = deviation_word(signal.deviation_deg, full_turn(channel), settings.deviation))) {
            return finish(result.code);
        }
        if (!std::isfinite(signal.duty) || signal.duty < 0.0L || signal.duty > 1.0L) return finish(Error::invalid_duty);
        settings.duty = static_cast<uint64_t>(std::round(signal.duty * static_cast<long double>(full_turn(channel))));
        settings.seed = signal.seed;
        settings.waveform = signal.waveform;
        settings.output_enabled = signal.output_enabled;
        settings.pm_enabled = signal.pm_enabled;
        return configure(channel, settings, restart_carrier, restart_modulation);
    }

    // Read the last committed shadow settings without caching stale software state.
    // An outstanding commit must acknowledge before this snapshot is returned.
    Result get_settings(uint32_t channel, Settings& settings) {
        if (!valid()) return finish(initialization_error_);
        if (channel >= channel_count_) return finish(Error::invalid_channel);
        std::lock_guard<std::mutex> lock(mutex_[channel]);
        if (!wait_idle(channel, std::chrono::steady_clock::now() + timeout_)) return finish(Error::pending_commit_timeout);
        settings = read_settings(channel);
        return finish(Error::none);
    }

    Result set_carrier_increment(uint32_t channel, uint64_t word) {
        return update(channel, [word](Settings& settings) { settings.carrier_increment = word; return Result{}; });
    }
    Result set_modulation_increment(uint32_t channel, uint64_t word) {
        return update(channel, [word](Settings& settings) { settings.modulation_increment = word; return Result{}; });
    }
    Result set_phase_word(uint32_t channel, uint64_t word) {
        return update(channel, [word](Settings& settings) { settings.carrier_phase = word; return Result{}; });
    }
    Result set_deviation_word(uint32_t channel, uint64_t word) {
        return update(channel, [word](Settings& settings) { settings.deviation = word; return Result{}; });
    }
    Result set_output_enabled(uint32_t channel, bool enabled) {
        return update(channel, [enabled](Settings& settings) {
            settings.output_enabled = enabled;
            return Result{};
        });
    }
    Result mute(uint32_t channel = 0) { return set_output_enabled(channel, false); }
    Result restart(uint32_t channel = 0, bool carrier = true, bool modulation = true) {
        return update(channel, [](Settings&) { return Result{}; }, carrier, modulation);
    }
    Result set_pm_enabled(uint32_t channel, bool enabled) {
        return update(channel, [enabled](Settings& settings) {
            settings.pm_enabled = enabled;
            return Result{};
        });
    }
    Result set_carrier_frequency(uint32_t channel, long double hz, long double sample_rate) {
        return update(channel, [&](Settings& settings) {
            return frequency_word(hz, sample_rate, full_turn(channel), settings.carrier_increment);
        });
    }
    Result set_modulation_frequency(uint32_t channel, long double hz, long double sample_rate) {
        return update(channel, [&](Settings& settings) {
            return frequency_word(hz, sample_rate, full_turn(channel), settings.modulation_increment);
        });
    }
    Result set_phase(uint32_t channel, long double degrees) {
        return update(channel, [&](Settings& settings) {
            return phase_word(degrees, full_turn(channel), settings.carrier_phase);
        });
    }
    Result set_deviation(uint32_t channel, long double degrees) {
        return update(channel, [&](Settings& settings) {
            return deviation_word(degrees, full_turn(channel), settings.deviation);
        });
    }

private:
    static Result frequency_word(long double hz, long double sample_rate, uint64_t turn, uint64_t& word) {
        if (!std::isfinite(sample_rate) || sample_rate <= 0.0L) return {Error::invalid_sample_rate};
        if (!std::isfinite(hz) || hz < 0.0L || hz >= sample_rate / 2.0L) return {Error::invalid_frequency};
        word = static_cast<uint64_t>(std::round(hz / sample_rate * static_cast<long double>(turn)));
        return {};
    }
    static Result phase_word(long double degrees, uint64_t turn, uint64_t& word) {
        if (!std::isfinite(degrees)) return {Error::invalid_phase};
        const auto wrapped = std::fmod(degrees, 360.0L);
        const auto rounded = std::round(wrapped / 360.0L * static_cast<long double>(turn));
        // Round signed phase before wrapping, matching half-away-from-zero
        // conversion for negative settings as well as positive settings.
        word = rounded < 0.0L ? (turn-static_cast<uint64_t>(-rounded)) & (turn-1)
                              : static_cast<uint64_t>(rounded) & (turn-1);
        return {};
    }
    static Result deviation_word(long double degrees, uint64_t turn, uint64_t& word) {
        if (!std::isfinite(degrees) || degrees < 0.0L || degrees > 360.0L) return {Error::invalid_deviation};
        word = static_cast<uint64_t>(std::round(degrees / 360.0L * static_cast<long double>(turn)));
        return {};
    }
    Result configure_locked(uint32_t channel, const Settings& settings, bool restart_carrier,
                            bool restart_modulation, std::chrono::steady_clock::time_point deadline) {
        const auto turn = full_turn(channel);
        const auto shape = static_cast<uint32_t>(settings.waveform);
        if (settings.carrier_increment >= turn || settings.carrier_phase >= turn ||
            settings.modulation_increment >= turn || settings.modulation_phase >= turn ||
            settings.deviation > turn || settings.duty > turn || shape > 9) {
            return finish(Error::out_of_range);
        }
        if (settings.pm_enabled && !(info_[channel].capabilities & (uint32_t{1} << shape))) {
            return finish(Error::unavailable_waveform);
        }
        if ((shape == 6 || shape == 7) && settings.seed == 0) return finish(Error::invalid_noise_seed);
        if (shape == 8 && !(settings.seed & ((uint32_t{1} << info_[channel].prbs_width) - 1))) {
            return finish(Error::invalid_prbs_seed);
        }

        if (!wait_idle(channel, deadline)) return finish(Error::pending_commit_timeout);
        word(channel, carrier_increment, settings.carrier_increment);
        word(channel, carrier_phase, settings.carrier_phase);
        word(channel, modulation_increment, settings.modulation_increment);
        word(channel, modulation_phase, settings.modulation_phase);
        word(channel, deviation, settings.deviation);
        word(channel, duty, settings.duty);
        write(channel, seed, settings.seed);
        write(channel, control, (shape << 8) | (settings.output_enabled ? 1u : 0u) |
                                (settings.pm_enabled ? 2u : 0u));
        // Order MMIO shadow writes before commit, including on ARM.
        std::atomic_thread_fence(std::memory_order_seq_cst);
        write(channel, command, 1u | (restart_carrier ? 2u : 0u) | (restart_modulation ? 4u : 0u));
        std::atomic_thread_fence(std::memory_order_seq_cst);
        if (!wait_idle(channel, deadline)) return finish(Error::commit_timeout);
        return finish(Error::none);
    }

    static constexpr uint32_t identifier = 0x504d0001;
    static constexpr uint32_t max_channels = 2;
    static constexpr uint32_t bank_stride = 0x1000;
    static constexpr uint32_t source_mask = 0x3ff;
    enum Register : uint32_t {
        id = 0x00, capabilities_register = 0x04, precision = 0x08,
        status = 0x0c, command = 0x10, output_width_register = 0x18,
        channel_count_register = 0x1c, carrier_increment = 0x20,
        carrier_phase = 0x28, modulation_increment = 0x30, modulation_phase = 0x38,
        deviation = 0x40, duty = 0x48, seed = 0x50, control = 0x54
    };
    template<class Change>
    Result update(uint32_t channel, Change change, bool carrier = false, bool modulation = false) {
        if (!valid()) return finish(initialization_error_);
        if (channel >= channel_count_) return finish(Error::invalid_channel);
        std::lock_guard<std::mutex> lock(mutex_[channel]);
        const auto deadline = std::chrono::steady_clock::now() + timeout_;
        if (!wait_idle(channel, deadline)) return finish(Error::pending_commit_timeout);
        auto settings = read_settings(channel);
        const auto result = change(settings);
        if (!result) return finish(result.code);
        return configure_locked(channel, settings, carrier, modulation, deadline);
    }
    uint64_t read_word(uint32_t channel, uint32_t offset) {
        const auto low = read(channel, offset);
        const auto high = read(channel, offset+4);
        return (uint64_t{high} << 32) | low;
    }
    Settings read_settings(uint32_t channel) {
        Settings settings;
        settings.carrier_increment = read_word(channel, carrier_increment);
        settings.carrier_phase = read_word(channel, carrier_phase);
        settings.modulation_increment = read_word(channel, modulation_increment);
        settings.modulation_phase = read_word(channel, modulation_phase);
        settings.deviation = read_word(channel, deviation);
        settings.duty = read_word(channel, duty);
        settings.seed = read(channel, seed);
        const auto value = read(channel, control);
        settings.waveform = static_cast<Waveform>((value >> 8) & 0xf);
        settings.output_enabled = value & 1u;
        settings.pm_enabled = value & 2u;
        return settings;
    }
    Result finish(Error code) {
        last_error_.store(code, std::memory_order_relaxed);
        return {code};
    }
    void fail_initialization(Error code) {
        initialization_error_ = code;
        finish(code);
    }
    uint32_t read(uint32_t channel, uint32_t offset) {
        return memory_.template read_reg<uint32_t>(channel * bank_stride + offset);
    }
    void write(uint32_t channel, uint32_t offset, uint32_t value) {
        memory_.template write_reg<uint32_t>(channel * bank_stride + offset, value);
    }
    void word(uint32_t channel, uint32_t offset, uint64_t value) {
        write(channel, offset, static_cast<uint32_t>(value));
        write(channel, offset + 4, static_cast<uint32_t>(value >> 32));
    }
    bool wait_idle(uint32_t channel, std::chrono::steady_clock::time_point deadline) {
        while (read(channel, status) & 1u) {
            if (std::chrono::steady_clock::now() >= deadline) return false;
            // Waiting on a stopped clock must not consume a CPU core.
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        return true;
    }
    Memory& memory_;
    const std::chrono::milliseconds timeout_;
    long double sample_rate_ = 0.0L;
    std::array<std::mutex, max_channels> mutex_;
    std::array<ChannelInfo, max_channels> info_{};
    uint32_t channel_count_ = 0;
    Error initialization_error_ = Error::none;
    std::atomic<Error> last_error_{Error::none};
};
}
#endif
