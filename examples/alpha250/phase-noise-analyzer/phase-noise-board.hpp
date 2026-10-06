#pragma once
#include "dds.hpp"
#include "server/hardware/memory_manager.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"
#include "boards/alpha250/drivers/ltc2157.hpp"
#include "server/runtime/driver_manager.hpp"
#include <scicpp/polynomials.hpp>

struct Alpha250PhaseNoiseBoard {
    void initialize_phase() { hw::get_memory<mem::control>().write_mask<reg::cordic, 3>(3); }
    void select_channel(uint32_t channel) {
        hw::get_memory<mem::control>().write_mask<reg::cordic, 16>((channel & 1) << 4);
    }
    uint32_t demodulated(uint32_t channel) {
        auto& status = hw::get_memory<mem::status>();
        return channel == 0 ? status.read<reg::demod0>() : status.read<reg::demod1>();
    }
    using Oscillator = Dds;
    static constexpr uint32_t max_phase_precision = 8;
    static constexpr uint32_t cic_rate_step = 2;
    static_assert(prm::phase_filter_width == 40);
    void set_phase_precision(uint32_t bits) {
        hw::get_memory<mem::control>().write<reg::phase_precision>(bits);
    }
    uint32_t phase_packet_status() {
        return hw::get_memory<mem::status>().read<reg::phase_packet>();
    }
    auto& clock() { return rt::get_driver<ClockGenerator>(); }
    Alpha250PhaseNoiseBoard() { clock().set_sampling_frequency(1); }
    double sampling_frequency() { return clock().get_adc_sampling_freq(); }
    uint32_t reference_clock() { return clock().get_reference_clock(); }
    double power_conversion(uint32_t channel, double frequency) {
        auto& adc = rt::get_driver<Ltc2157>();
        const double range = adc.get_input_voltage_range(channel);
        const double response = scicpp::polynomial::polyval(frequency, adc.tf_polynomial<double>(channel));
        return response * 22.0 * range * range / (50.0 * .001);
    }
};
