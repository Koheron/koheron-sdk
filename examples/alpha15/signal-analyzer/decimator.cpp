#include "./decimator.hpp"

#include "server/context/context.hpp"

#include <utility>
#include <tuple>
#include <scicpp/core.hpp>

namespace sci = scicpp;
namespace sig = scicpp::signal;
namespace win = scicpp::signal::windows;

Decimator::Decimator(Context& ctx_)
: ctx(ctx_)
, analyzer0(ctx_)
, analyzer1(ctx_)
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
        ctx.log<ERROR>("Decimator: Invalid window index\n");
        return;
    }
}

void Decimator::start_acquisition() {
    analyzer0.start_acquisition();
    analyzer1.start_acquisition();
}
