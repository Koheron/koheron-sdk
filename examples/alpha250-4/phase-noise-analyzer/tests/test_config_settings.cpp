#include "server/runtime/config_ini.hpp"
#include <cassert>
#include <cmath>

int main() {
    rt::cfg::ini config;
    rt::cfg::set(config, "PhaseNoiseAnalyzer", "tracking_enabled", true);
    rt::cfg::set(config, "PhaseNoiseAnalyzer", "tracking_bandwidth", 0.1f);
    rt::cfg::set(config, "PhaseNoiseAnalyzer", "cic_rate", 67);
    assert(rt::cfg::get<bool>(config, "PhaseNoiseAnalyzer", "tracking_enabled", false));
    assert(std::abs(rt::cfg::get<float>(config, "PhaseNoiseAnalyzer", "tracking_bandwidth", 0.f) - 0.1f) < 1e-7f);
    assert(rt::cfg::get<int>(config, "PhaseNoiseAnalyzer", "cic_rate", 0) == 67);
    for (const auto* text : {" true ", "yes", "ON", "1"}) {
        bool value = false;
        assert(rt::cfg::parse_to<bool>(text, value) && value);
    }
    for (const auto* text : {" false ", "no", "OFF", "0"}) {
        bool value = true;
        assert(rt::cfg::parse_to<bool>(text, value) && !value);
    }
    double value = 0.;
    assert(rt::cfg::parse_to<double>("  0.10000000149011612  ", value));
    assert(std::abs(value - 0.10000000149011612) < 1e-16);
    int integer = 0;
    assert(rt::cfg::parse_to<int>(" -12345 ", integer) && integer == -12345);
    assert(!rt::cfg::parse_to<double>("0.1junk", value));
}
