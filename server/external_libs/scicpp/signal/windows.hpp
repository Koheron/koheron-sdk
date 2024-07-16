// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_SIGNAL_WINDOWS
#define SCICPP_SIGNAL_WINDOWS

#include "scicpp/core/constants.hpp"
#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/maths.hpp"
#include "scicpp/core/numeric.hpp"
#include "scicpp/core/range.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <numeric>
#include <vector>

namespace scicpp::signal::windows {

enum Symmetry : int { Symmetric, Periodic };

namespace detail {

// Window functions are symmetric.
// So we compute only one half and mirror it to the upper
// region of the vector.

template <class Array, typename Func>
void symmetric_filler(Array &w, Func f) {
    const auto half_len = signed_size_t(w.size()) / 2;
    std::iota(w.begin() + half_len, w.end(), half_len);
    std::transform(w.cbegin() + half_len, w.cend(), w.begin() + half_len, f);
    std::reverse_copy(w.cbegin() + half_len, w.cend(), w.begin());
}

template <typename T, std::size_t M, Symmetry sym, typename FillerFunc>
auto build_window_array(FillerFunc f) {
    if constexpr (sym == Symmetric) {
        std::array<T, M> w{};
        f(w);
        return w;
    } else { // sym == Periodic
        std::array<T, M + 1> w{};
        f(w);
        return *reinterpret_cast<std::array<T, M> *>(w.data());
    }
}

template <typename T, Symmetry sym, typename FillerFunc>
auto build_window_vector(std::size_t M, FillerFunc f) {
    if constexpr (sym == Symmetric) {
        std::vector<T> w(M);
        f(w);
        return w;
    } else { // sym == Periodic
        std::vector<T> w(M + 1);
        f(w);
        w.resize(M);
        return w;
    }
}

} // namespace detail

//---------------------------------------------------------------------------------
// Polynomial windows
//---------------------------------------------------------------------------------

template <typename T, std::size_t M, Symmetry = Symmetric>
auto boxcar() {
    return ones<M, T>();
}

template <typename T, Symmetry = Symmetric>
auto boxcar(std::size_t M) {
    return ones<T>(M);
}

namespace detail {

template <class Array>
constexpr void bartlett_filler(Array &w) {
    using T = typename Array::value_type;
    const auto scaling = -T{2} / T(w.size() - 1);
    symmetric_filler(w, [&](auto i) { return std::fma(scaling, T(i), T{2}); });
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
constexpr auto bartlett() {
    return detail::build_window_array<T, M, sym>(
        [](auto &w) { detail::bartlett_filler(w); });
}

template <typename T, Symmetry sym = Symmetric>
auto bartlett(std::size_t M) {
    return detail::build_window_vector<T, sym>(
        M, [](auto &w) { detail::bartlett_filler(w); });
}

//---------------------------------------------------------------------------------
// Cosine window
//---------------------------------------------------------------------------------

namespace detail {

template <class Array>
void cosine_filler(Array &w) {
    if (!w.empty()) {
        using T = typename Array::value_type;
        const T scaling = pi<T> / T(w.size());
        symmetric_filler(
            w, [&](std::size_t i) { return std::sin(scaling * (T(i) + 0.5)); });
    }
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto cosine() {
    return detail::build_window_array<T, M, sym>(
        [](auto &w) { detail::cosine_filler(w); });
}

template <typename T, Symmetry sym = Symmetric>
auto cosine(std::size_t M) {
    return detail::build_window_vector<T, sym>(
        M, [](auto &w) { detail::cosine_filler(w); });
}

namespace detail {

template <class Array>
void bohman_filler(Array &w) {
    if (!w.empty()) {
        using T = typename Array::value_type;
        const auto step = T{2} / T(w.size() - 1);
        symmetric_filler(w, [=](std::size_t i) {
            const auto x = T(i) * step - T{1};
            return (T{1} - x) * std::cos(pi<T> * x) +
                   std::sin(pi<T> * x) / pi<T>;
        });
    }
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto bohman() {
    return detail::build_window_array<T, M, sym>(
        [](auto &w) { detail::bohman_filler(w); });
}

template <typename T, Symmetry sym = Symmetric>
auto bohman(std::size_t M) {
    return detail::build_window_vector<T, sym>(
        M, [](auto &w) { detail::bohman_filler(w); });
}

//---------------------------------------------------------------------------------
// Cosine sum windows
//---------------------------------------------------------------------------------

namespace detail {

template <class Array,
          std::size_t n_weights,
          typename T = typename Array::value_type>
void general_cosine(Array &w, const std::array<T, n_weights> &a) {
    const auto scaling = T{2} * pi<T> / T(w.size() - 1);

    symmetric_filler(w, [&](std::size_t i) {
        auto tmp = T{0};
        auto sign = -T{1};
        std::size_t j = 0;

        for (const auto &c : a) {
            sign *= -T{1};
            tmp += sign * c * std::cos(scaling * T(i * j));
            j++;
        }

        return tmp;
    });
}

} // namespace detail

template <typename T, Symmetry sym = Symmetric, std::size_t n_weights>
auto general_cosine(std::size_t M, const std::array<T, n_weights> &a) {
    return detail::build_window_vector<T, sym>(
        M, [&](auto &w) { detail::general_cosine(w, a); });
}

template <typename T,
          std::size_t M,
          Symmetry sym = Symmetric,
          std::size_t n_weights>
auto general_cosine(const std::array<T, n_weights> &a) {
    return detail::build_window_array<T, M, sym>(
        [&](auto &w) { detail::general_cosine(w, a); });
}

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto general_hamming(T alpha) {
    return general_cosine<T, M, sym>(std::array{alpha, T{1} - alpha});
}

template <typename T, Symmetry sym = Symmetric>
auto general_hamming(std::size_t M, T alpha) {
    return general_cosine<T, sym>(M, std::array{alpha, T{1} - alpha});
}

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto hann() {
    return general_hamming<T, M, sym>(T{0.5});
}

template <typename T, Symmetry sym = Symmetric>
auto hann(std::size_t M) {
    return general_hamming<T, sym>(M, T{0.5});
}

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto hamming() {
    return general_hamming<T, M, sym>(T{0.54});
}

template <typename T, Symmetry sym = Symmetric>
auto hamming(std::size_t M) {
    return general_hamming<T, sym>(M, T{0.54});
}

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto blackman() {
    return general_cosine<T, M, sym>(std::array{0.42, 0.50, 0.08});
}

template <typename T, Symmetry sym = Symmetric>
auto blackman(std::size_t M) {
    return general_cosine<T, sym>(M, std::array{0.42, 0.50, 0.08});
}

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto nuttall() {
    return general_cosine<T, M, sym>(
        std::array{0.3635819, 0.4891775, 0.1365995, 0.0106411});
}

template <typename T, Symmetry sym = Symmetric>
auto nuttall(std::size_t M) {
    return general_cosine<T, sym>(
        M, std::array{0.3635819, 0.4891775, 0.1365995, 0.0106411});
}

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto blackmanharris() {
    return general_cosine<T, M, sym>(
        std::array{0.35875, 0.48829, 0.14128, 0.01168});
}

template <typename T, Symmetry sym = Symmetric>
auto blackmanharris(std::size_t M) {
    return general_cosine<T, sym>(
        M, std::array{0.35875, 0.48829, 0.14128, 0.01168});
}

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto flattop() {
    return general_cosine<T, M, sym>(std::array{
        0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368});
}

template <typename T, Symmetry sym = Symmetric>
auto flattop(std::size_t M) {
    return general_cosine<T, sym>(
        M,
        std::array{
            0.21557895, 0.41663158, 0.277263158, 0.083578947, 0.006947368});
}

//---------------------------------------------------------------------------------
// Gaussian
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename T = typename Array::value_type>
void gaussian_filler(Array &w, T sigma) {
    const T shift = w.size() % 2 == 0 ? T{0.5} : T{0};
    const T i0 = T(w.size() / 2) - shift;
    const T scaling = -T{1} / (T{2} * sigma * sigma);

    symmetric_filler(w, [=](std::size_t i) {
        const T n = T(i) - i0;
        return std::exp(scaling * n * n);
    });
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto gaussian(T sigma) {
    return detail::build_window_array<T, M, sym>(
        [&](auto &w) { detail::gaussian_filler(w, sigma); });
}

template <typename T, Symmetry sym = Symmetric>
auto gaussian(std::size_t M, T sigma) {
    return detail::build_window_vector<T, sym>(
        M, [&](auto &w) { detail::gaussian_filler(w, sigma); });
}

//---------------------------------------------------------------------------------
// General Gaussian
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename T = typename Array::value_type>
void general_gaussian_filler(Array &w, T p, T sigma) {
    const T shift = w.size() % 2 == 0 ? T{0.5} : T{0};
    const T i0 = T(w.size() / 2) - shift;

    symmetric_filler(w, [=](std::size_t i) {
        const T n = T(i) - i0;
        return std::exp(-0.5 * std::pow(n / sigma, T{2} * p));
    });
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto general_gaussian(T p, T sigma) {
    return detail::build_window_array<T, M, sym>(
        [&](auto &w) { detail::general_gaussian_filler(w, p, sigma); });
}

template <typename T, Symmetry sym = Symmetric>
auto general_gaussian(std::size_t M, T p, T sigma) {
    return detail::build_window_vector<T, sym>(
        M, [&](auto &w) { detail::general_gaussian_filler(w, p, sigma); });
}

//---------------------------------------------------------------------------------
// Kaiser
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename T = typename Array::value_type>
void kaiser_filler(Array &w, T beta) {
    scicpp_require(beta >= T{0});

    const auto alpha = 0.5 * T(w.size() - 1);
    const auto i0_beta = std::cyl_bessel_i(0, beta);

    symmetric_filler(w, [=](std::size_t i) {
        const auto r = (T(i) - alpha) / alpha;
        return std::cyl_bessel_i(0, beta * std::sqrt(T{1} - r * r)) / i0_beta;
    });
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto kaiser(T beta) {
    return detail::build_window_array<T, M, sym>(
        [&](auto &w) { detail::kaiser_filler(w, std::abs(beta)); });
}

template <typename T, Symmetry sym = Symmetric>
auto kaiser(std::size_t M, T beta) {
    return detail::build_window_vector<T, sym>(
        M, [&](auto &w) { detail::kaiser_filler(w, std::abs(beta)); });
}

//---------------------------------------------------------------------------------
// Parzen
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename T = typename Array::value_type>
void parzen_filler(Array &w) {
    const auto N = signed_size_t(w.size());

    symmetric_filler(w, [=](std::size_t i) {
        const auto n = signed_size_t(i) - (N - 1) / 2;
        const auto nabs = (n >= 0) ? n : -n;

        const T shift = (N % 2 == 0) ? T{0.5} : T{0};
        const T i0 = T(N / 2) - shift;
        const T A = T(2) * T(abs(T(i) - i0)) / T(N);
        const T B = T(1) - A;

        if (nabs <= N / 4) {
            return T(1) - T(6) * A * A * B;
        } else {
            return T(2) * B * B * B;
        }
    });
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto parzen() {
    return detail::build_window_array<T, M, sym>(
        [&](auto &w) { detail::parzen_filler(w); });
}

template <typename T, Symmetry sym = Symmetric>
auto parzen(std::size_t M) {
    return detail::build_window_vector<T, sym>(
        M, [&](auto &w) { detail::parzen_filler(w); });
}

//---------------------------------------------------------------------------------
// Lanczos
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename T = typename Array::value_type>
void lanczos_filler(Array &w) {
    symmetric_filler(w, [=](std::size_t i) {
        return sinc(T(1) - T(2 * i) / T(w.size() - 1));
    });
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto lanczos() {
    return detail::build_window_array<T, M, sym>(
        [&](auto &w) { detail::lanczos_filler(w); });
}

template <typename T, Symmetry sym = Symmetric>
auto lanczos(std::size_t M) {
    return detail::build_window_vector<T, sym>(
        M, [&](auto &w) { detail::lanczos_filler(w); });
}

//---------------------------------------------------------------------------------
// Tukey
//---------------------------------------------------------------------------------

namespace detail {

template <class Array, typename T = typename Array::value_type>
void tukey_filler(Array &w, T alpha) {
    const auto N = signed_size_t(w.size());
    auto width = signed_size_t((T(1) - alpha) * T(N / 2));

    if ((N % 2 == 0) && (width == 0)) {
        width = 1;
    }

    symmetric_filler(w, [=](std::size_t i) {
        const auto n = signed_size_t(i) - (N - 1) / 2;

        if (n <= width) {
            return T(1);
        } else {
            const auto A = T(2) / alpha;
            return T(0.5) *
                   (T(1) + std::cos(pi<T> * (T(1) - A + A * T(i) / T(N - 1))));
        }
    });
}

} // namespace detail

template <typename T, std::size_t M, Symmetry sym = Symmetric>
auto tukey(T alpha = 0.5) {
    if (alpha <= T(0)) {
        return boxcar<T, M>();
    }

    if (alpha >= T(1)) {
        return hann<T, M>();
    }

    return detail::build_window_array<T, M, sym>(
        [&](auto &w) { detail::tukey_filler(w, alpha); });
}

template <typename T, Symmetry sym = Symmetric>
auto tukey(std::size_t M, T alpha = 0.5) {
    if (alpha <= T(0)) {
        return boxcar<T>(M);
    }

    if (alpha >= T(1)) {
        return hann<T>(M);
    }

    return detail::build_window_vector<T, sym>(
        M, [&](auto &w) { detail::tukey_filler(w, alpha); });
}

//---------------------------------------------------------------------------------
// get_window
//---------------------------------------------------------------------------------

enum Window : std::size_t {
    Boxcar,
    Bartlett,
    Cosine,
    Hann,
    Hamming,
    Blackman,
    Nuttall,
    Blackmanharris,
    Flattop,
    Bohman,
    Parzen,
    Lanczos
};

template <Window win, std::size_t N, typename T = double>
auto get_window() {
    switch (win) {
    case Boxcar:
        return boxcar<T, N>();
    case Bartlett:
        return bartlett<T, N>();
    case Cosine:
        return cosine<T, N>();
    case Hann:
        return hann<T, N>();
    case Hamming:
        return hamming<T, N>();
    case Blackman:
        return blackman<T, N>();
    case Nuttall:
        return nuttall<T, N>();
    case Blackmanharris:
        return blackmanharris<T, N>();
    case Flattop:
        return flattop<T, N>();
    case Bohman:
        return bohman<T, N>();
    case Parzen:
        return parzen<T, N>();
    case Lanczos:
        return lanczos<T, N>();
    default:
        scicpp_unreachable;
    }
}

template <typename T = double>
auto get_window(Window win, std::size_t N) {
    switch (win) {
    case Boxcar:
        return boxcar<T>(N);
    case Bartlett:
        return bartlett<T>(N);
    case Cosine:
        return cosine<T>(N);
    case Hann:
        return hann<T>(N);
    case Hamming:
        return hamming<T>(N);
    case Blackman:
        return blackman<T>(N);
    case Nuttall:
        return nuttall<T>(N);
    case Blackmanharris:
        return blackmanharris<T>(N);
    case Flattop:
        return flattop<T>(N);
    case Bohman:
        return bohman<T>(N);
    case Parzen:
        return parzen<T>(N);
    case Lanczos:
        return lanczos<T>(N);
    default:
        scicpp_unreachable;
    }
}

//---------------------------------------------------------------------------------
// window utilities
//---------------------------------------------------------------------------------

// S1 = (Sum_i w_i)^2
template <typename Array>
auto s1(const Array &window) {
    return std::norm(sum(window));
}

// S2 = Sum_i w_i^2
template <typename Array>
auto s2(const Array &window) {
    using T = typename Array::value_type;
    return std::get<0>(
        reduce(window, [](auto r, auto v) { return r + std::norm(v); }, T{0}));
}

// https://fr.mathworks.com/help/signal/ref/enbw.html
template <typename Array, typename T = typename Array::value_type>
auto enbw(const Array &window, T fs = T{1}) {
    return fs * s2(window) / s1(window);
}

} // namespace scicpp::signal::windows

#endif // SCICPP_SIGNAL_WINDOWS
