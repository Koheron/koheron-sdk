#pragma once
#include "server/runtime/driver_manager.hpp"
#include "redpitaya_adc_calibration.hpp"

struct RedPitayaFftBoard {
    double sampling_frequency() { return prm::adc_clk; }
    auto& calibration() { return rt::get_driver<RedPitayaAdcCalibration>(); }
};
