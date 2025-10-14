#include "server/runtime/runtime.hpp"

// Can either include all drivers using:
// #include <drivers.hpp>
// or only the drivers used by the app (lighter dependencies)
#include "boards/alpha250/drivers/clock-generator.hpp"
#include "boards/alpha250/drivers/ltc2157.hpp"
#include "boards/alpha250/drivers/ad9747.hpp"
#include "boards/alpha250/drivers/precision-adc.hpp"
#include "boards/alpha250/drivers/precision-dac.hpp"
#include "examples/alpha250/fft/fft.hpp"

#include <chrono>
#include <thread>

int main() {
    rt::Runtime rt;
    auto& ctx = rt.context();

    ctx.log<INFO>("Initialize");
    // You may also call ctx.get<Common>().init();
    auto& clkgen = ctx.get<ClockGenerator>();
    ctx.get<GpioExpander>();
    auto& ltc2157 = ctx.get<Ltc2157>();
    auto& ad9747 = ctx.get<Ad9747>();
    auto& precisiondac = ctx.get<PrecisionDac>();
    ctx.get<PrecisionAdc>();

    clkgen.init();
    ltc2157.init();
    ad9747.init();
    precisiondac.init();

    rt.systemd_notify_ready();

    auto& fft = ctx.get<FFT>();
    ctx.logf<INFO>("FFT size = {} points", fft.get_fft_size());

    while (true) {
        using namespace std::chrono_literals;

        auto data = fft.get_adc_raw_data(10);
        ctx.logf<INFO>("FFT data = {}, {}", data[0], data[1]);
        std::this_thread::sleep_for(10ms);
    }

    return 0;
}