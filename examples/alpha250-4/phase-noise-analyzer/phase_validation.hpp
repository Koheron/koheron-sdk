#ifndef ALPHA250_4_PHASE_VALIDATION_HPP
#define ALPHA250_4_PHASE_VALIDATION_HPP

#include <array>
#include <scicpp/core.hpp>

template<std::size_t Samples, typename Phase, std::size_t N>
bool phase_block_valid(const std::array<Phase, N>& phase) {
    static_assert(Samples > 1 && Samples <= N);
    using namespace scicpp::operators;
    constexpr auto max_sample_jump = 0.5f * scicpp::pi<Phase>;
    std::array<Phase, Samples - 1> differences{};
    for (std::size_t i = 1; i < Samples; ++i) {
        differences[i - 1] = phase[i] - phase[i - 1];
        if (scicpp::absolute(differences[i - 1]) > max_sample_jump) return false;
    }

    // A constant phase slope is a frequency offset, rather than an impulse.
    // Compare peaks against the noise about that slope so tracking can lock.
    const auto residual = differences - scicpp::stats::mean(differences);
    const auto rms = scicpp::stats::std(residual);
    const auto peak = scicpp::stats::amax(scicpp::absolute(residual));
    return rms <= Phase{0.0f} || peak / rms <= scicpp::units::dimensionless<float>{12.0f};
}

#endif
