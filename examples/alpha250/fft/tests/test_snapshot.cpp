#include "examples/alpha250/fft/fft.hpp"
#include <type_traits>
#include <utility>
// The network layer must own the result after the driver's mutex is released.
using Spectrum = std::array<float, prm::fft_size / 2>;
static_assert(std::is_same_v<decltype(std::declval<FFT&>().read_psd()), Spectrum>);
static_assert(std::is_same_v<decltype(std::declval<FFT&>().read_psd_raw()), Spectrum>);
static_assert(std::tuple_size_v<decltype(std::declval<FFT&>().get_control_parameters())> == 8);
