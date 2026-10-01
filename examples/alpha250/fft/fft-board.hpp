#pragma once
#include "server/runtime/driver_manager.hpp"
#include "boards/alpha250/drivers/clock-generator.hpp"
#include "boards/alpha250/drivers/ltc2157.hpp"

struct Alpha250FftBoard {
    double sampling_frequency() { return rt::get_driver<ClockGenerator>().get_adc_sampling_freq(); }
    auto& calibration() { return rt::get_driver<Ltc2157>(); }
};
