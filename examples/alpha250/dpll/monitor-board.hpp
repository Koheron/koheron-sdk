#pragma once
#include "dpll.hpp"
#include "boards/alpha250/drivers/ltc2157.hpp"
#include <scicpp/polynomials.hpp>

// Policy adapter for the standard PNA core. Its initialization and channel
// selection address only the independent monitor hardware.
struct DpllMonitorBoard {
    using Oscillator = Dpll;
    static constexpr bool passive_monitor = true;
    static constexpr uint32_t max_phase_precision = 8;
    static constexpr uint32_t cic_rate_step = 2;
    DpllMonitorBoard() { (void)rt::get_driver<Dpll>(); }
    void initialize_phase() {}
    void select_channel(uint32_t channel) {
        hw::get_memory<mem::control>().write<reg::phase_sel>(channel);
    }
    uint32_t demodulated(uint32_t) {
        return hw::get_memory<mem::status>().read<reg::monitor_demod>();
    }
    void set_phase_precision(uint32_t bits) {
        hw::get_memory<mem::control>().write<reg::phase_precision>(bits);
    }
    uint32_t phase_packet_status() {
        return hw::get_memory<mem::status>().read<reg::phase_packet>();
    }
    double sampling_frequency() { return prm::adc_clk; }
    uint32_t reference_clock() const { return rt::get_driver<ClockGenerator>().get_reference_clock(); }
    uint64_t configuration_revision() const {
        std::shared_lock lock(dpll_monitor::controls_mutex);
        return 8 * dpll_monitor::controls_revision + reference_clock();
    }
    double power_conversion(uint32_t channel, double frequency) {
        auto& adc = rt::get_driver<Ltc2157>();
        const double range = adc.get_input_voltage_range(channel);
        const double response = scicpp::polynomial::polyval(frequency, adc.tf_polynomial<double>(channel));
        return response * 22.0 * range * range / (50.0 * .001);
    }
};
