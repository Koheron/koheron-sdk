/// Modulation driver
///
/// (c) Koheron

#ifndef __DRIVERS_MODULATION_HPP__
#define __DRIVERS_MODULATION_HPP__

#include "memory.hpp"

#include <atomic>
#include <cmath>
#include <context.hpp>

class Modulation
{
  public:
    Modulation(Context& ctx_)
    :ctx(ctx_)
    , dac_map(ctx.mm.get<mem::dac>())
    {
    }

    // Modulation

    auto get_modulation_status() {
        for (uint32_t channel = 0; channel < 2; channel++) {
            if (modulation_changed[channel]) {
                set_dac_waveform(channel);
                modulation_changed[channel] = false;
            }
        }
        return std::make_tuple(
            waveform_type[0],
            waveform_type[1],
            dac_amplitude[0],
            dac_amplitude[1],
            dac_frequency[0],
            dac_frequency[1],
            dac_offset[0],
            dac_offset[1]
        );
    }

    void set_dac_buffer(uint32_t channel, const std::array<uint32_t, prm::wfm_size/2>& arr) {
        dac_map.set_ptr(arr.data(), prm::wfm_size/2, channel);
    }

    void set_waveform_type(uint32_t channel, uint32_t wfm_type) {
        waveform_type[channel] = wfm_type;
        modulation_changed[channel] = true;
    }

    void set_dac_amplitude(uint32_t channel, float amplitude_value) {
        dac_amplitude[channel] = amplitude_value;
        modulation_changed[channel] = true;
    }

    void set_dac_frequency(uint32_t channel, float frequency_value) {
        dac_frequency[channel] = frequency_value;
        modulation_changed[channel] = true;
    }

    void set_dac_offset(uint32_t channel, float offset_value) {
        dac_offset[channel] = offset_value;
        modulation_changed[channel] = true;
    }

    auto get_float_dac_waveform(uint32_t channel) {
        std::array<float, prm::wfm_size> dac_data;
        switch (waveform_type[channel]) {
            // Sine wave
            case 0: {
                for (uint32_t i = 0; i < prm::wfm_size; i++) {
                    dac_data[i] = dac_amplitude[channel] * std::cos(2 * float(M_PI) * i * dac_frequency[channel] / prm::sampling_rate) + dac_offset[channel];
                }
                break;
            }
            // Triangular wave
            case 1: {
                auto period = uint32_t(prm::sampling_rate / dac_frequency[channel]);
                for (uint32_t i = 0; i < prm::wfm_size; i++) {
                    dac_data[i] = dac_amplitude[channel] * ( 4 * (std::abs((i % period) - period/2.0F)/period) - 1) + dac_offset[channel];
                }
                break;
            }
            // Square wave
            case 2: {
                for (uint32_t i = 0; i < prm::wfm_size; i++) {
                    dac_data[i] = dac_amplitude[channel] * (1 - 2*(int(2*i * dac_frequency[channel] / prm::sampling_rate) % 2)) + dac_offset[channel];
                }
                break;
            }
            default:
                break;
        }
        return dac_data;
    }

    void set_dac_waveform(uint32_t channel) {
        auto dac_data = get_float_dac_waveform(channel);
        std::array<uint32_t, prm::wfm_size/2> arr;
        for (uint32_t i = 0; i < prm::wfm_size/2; i++) {
            arr[i] = uint32_t(8192 * dac_data[2*i + 1] + 8192) % 16384 + 8192 +
                     ((uint32_t(8192 * dac_data[2*i] + 8192) % 16384 + 8192) << 16);
        }
        dac_map.set_ptr(arr.data(), prm::wfm_size/2, channel);
    }

    auto& get_dac_buffer(uint32_t channel) {
        return dac_map.read_array<uint32_t, prm::wfm_size/2>(channel);
    }

  private:

    bool avg_on;
    uint32_t n_avg_min;
    uint32_t num_average;
    uint32_t waveform_type[2] = {0,0};
    float dac_amplitude[2] = {0,0};
    float dac_frequency[2] = {0,0};
    float dac_offset[2] = {0,0};

    std::array<std::atomic<bool>,2> modulation_changed;

    Context& ctx;
    Memory<mem::dac>& dac_map;

};

#endif // __DRIVERS_MODULATION_HPP__
