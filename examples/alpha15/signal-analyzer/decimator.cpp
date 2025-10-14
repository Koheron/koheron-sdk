#include "./decimator.hpp"

#include "server/runtime/syslog.hpp"

#include <utility>
#include <tuple>
#include <scicpp/core.hpp>

namespace sci = scicpp;
namespace sig = scicpp::signal;
namespace win = scicpp::signal::windows;

Decimator::Decimator()
: analyzer0()
, analyzer1()
{
    set_fft_window(1);
    start_acquisition();
}

void Decimator::set_fft_window(uint32_t window_id) {
    switch (window_id) {
      case 0:
        analyzer0.set_window<win::Boxcar>();
        analyzer1.set_window<win::Boxcar>();
        break;
      case 1:
        analyzer0.set_window<win::Hann>();
        analyzer1.set_window<win::Hann>();
        break;
      case 2:
        analyzer0.set_window<win::Flattop>();
        analyzer1.set_window<win::Flattop>();
        break;
      case 3:
        analyzer0.set_window<win::Blackmanharris>();
        analyzer1.set_window<win::Blackmanharris>();
        break;
      default:
        rt::print<ERROR>("Decimator: Invalid window index\n");
        return;
    }
}

void Decimator::start_acquisition() {
    analyzer0.start_acquisition();
    analyzer1.start_acquisition();
}
