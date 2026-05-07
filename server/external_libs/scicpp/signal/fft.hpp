// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_SIGNAL_FFT
#define SCICPP_SIGNAL_FFT

#include "scicpp/core/macros.hpp"
#include "scicpp/core/numeric.hpp"
#include "scicpp/core/range.hpp"
#include "scicpp/core/utils.hpp"

#include <algorithm>
#include <array>
#include <complex>
#include <concepts>
#include <cstdlib>
#include <ranges>
#include <span>
#include <unsupported/Eigen/FFT>
#include <utility>
#include <vector>

namespace scicpp::signal {

//---------------------------------------------------------------------------------
// FFT helper functions
//---------------------------------------------------------------------------------

template <meta::Iterable Array>
auto fftshift(Array &&a) {
    std::rotate(std::begin(a),
                std::begin(a) + signed_size_t(a.size() + 1) / 2,
                std::end(a));
    return std::move(a);
}

template <meta::Iterable Array>
auto fftshift(const Array &a) {
    auto res = utils::set_array(a);
    const auto offset = (signed_size_t(a.size() - 1) / 2) + 1;
    std::copy(std::cbegin(a) + offset, std::cend(a), std::begin(res));
    std::copy(std::cbegin(a),
              std::cbegin(a) + offset,
              std::begin(res) + signed_size_t(a.size()) / 2);
    return res;
}

template <meta::Iterable Array>
auto ifftshift(Array &&a) {
    std::rotate(std::begin(a),
                std::begin(a) + signed_size_t(a.size()) / 2,
                std::end(a));
    return std::move(a);
}

template <meta::Iterable Array>
auto ifftshift(const Array &a) {
    auto res = utils::set_array(a);
    const auto offset = (signed_size_t(a.size()) / 2);
    std::copy(std::cbegin(a) + offset, std::cend(a), std::begin(res));
    std::copy(std::cbegin(a),
              std::cbegin(a) + offset,
              std::begin(res) + signed_size_t(a.size() + 1) / 2);
    return res;
}

namespace detail {

using namespace scicpp::operators;

template <class Array, typename T = Array::value_type>
auto fftfreq_impl(Array &&res, T d) {
    scicpp_require(d > T{0});
    const auto N = signed_size_t(res.size());
    std::iota(std::begin(res), std::end(res), -T(N / 2));
    return ifftshift(std::forward<Array>(res) / (d * T(N)));
}

template <class Array, typename T = Array::value_type>
auto rfftfreq_impl(Array &&res, std::size_t N, T d) {
    scicpp_require(d > T{0});
    std::iota(std::begin(res), std::end(res), T{0});
    return std::forward<Array>(res) / (d * T(N));
}

} // namespace detail

template <std::size_t N, typename T>
auto fftfreq(T d = T{1}) {
    static_assert(N > 0);
    return detail::fftfreq_impl(std::array<T, N>{}, d);
}

template <typename T>
auto fftfreq(std::size_t n, T d = T{1}) {
    scicpp_require(n > 0);
    return detail::fftfreq_impl(std::vector<T>(n), d);
}

template <std::size_t N, typename T>
auto rfftfreq(T d = T{1}) {
    static_assert(N > 0);
    return detail::rfftfreq_impl(std::array<T, N / 2 + 1>{}, N, d);
}

template <typename T>
auto rfftfreq(std::size_t n, T d = T{1}) {
    scicpp_require(n > 0);
    return detail::rfftfreq_impl(std::vector<T>(n / 2 + 1), n, d);
}

namespace detail {

// https://rosettacode.org/wiki/Hamming_numbers#C.2B.2B11_For_Each_Generator
// Hamming like sequences Generator
//
// Nigel Galloway. August 13th., 2012
template <typename Integral, std::size_t n_primes>
class Hamming {
  private:
    std::array<Integral, n_primes> m_H, m_hp, m_hv;
    std::vector<Integral> m_x;

  public:
    bool operator!=(const Hamming & /*unused*/) const { return true; }
    Hamming begin() const { return *this; }
    Hamming end() const { return *this; }
    Integral operator*() const { return m_x.back(); }

    explicit Hamming(const std::array<Integral, n_primes> &pfs)
        : m_H(pfs), m_hp{}, m_hv({pfs}), m_x({1}) {}

    const Hamming &operator++() {
        for (std::size_t i = 0; i < n_primes; ++i) {
            for (; m_hv[i] <= m_x.back(); m_hv[i] = m_x[++m_hp[i]] * m_H[i]) {
            }
        }

        m_x.push_back(m_hv[0]);

        for (const auto &v : m_hv) {
            if (v < m_x.back()) {
                m_x.back() = v;
            }
        }

        return *this;
    }
};

template <typename Integral>
Integral next_ugly_number(Integral num) {
    for (const auto h : Hamming<Integral, 3>({2, 3, 5})) {
        if (h >= num) {
            return h;
        }
    }

    scicpp_unreachable;
    return 0;
}

} // namespace detail

template <typename Integral>
Integral next_fast_len(Integral n) {
    return detail::next_ugly_number(n);
}

template <meta::Iterable Array>
auto zero_padding(const Array &v, std::size_t new_size) {
    using InT = typename Array::value_type;
    using T = std::remove_cv_t<InT>;
    auto res = zeros<T>(new_size);
    std::copy(std::begin(v),
              std::begin(v) + signed_size_t(std::min(new_size, v.size())),
              std::begin(res));
    return res;
}

template <typename T>
auto zero_padding(std::vector<T> &&v, std::size_t new_size) {
    v.resize(new_size, T{0});
    return std::move(v);
}

//---------------------------------------------------------------------------------
// FFTs
//---------------------------------------------------------------------------------

namespace detail {
template <class R>
concept ResizableContiguousRange =
    std::ranges::contiguous_range<R> && requires(R r, std::size_t n) {
        r.resize(n);
        r.data();
    };
} // namespace detail

template <std::contiguous_iterator It,
          std::sized_sentinel_for<It> S,
          detail::ResizableContiguousRange CplxVector>
void fft_into(It first, S last, CplxVector &dst) {
    using T = std::ranges::range_value_t<CplxVector>::value_type;

    const auto src_size = std::distance(first, last);
    scicpp_require(src_size != 0);

    dst.resize(static_cast<std::size_t>(src_size));
    Eigen::FFT<T> fft_engine;
    fft_engine.fwd(dst.data(), std::to_address(first), src_size);
}

template <std::ranges::contiguous_range Array,
          detail::ResizableContiguousRange CplxVector>
void fft_into(const Array &x, CplxVector &dst) {
    fft_into(std::cbegin(x), std::cend(x), dst);
}

template <std::ranges::contiguous_range Array,
          detail::ResizableContiguousRange CplxVector>
auto fft(const Array &x, CplxVector &&dst) {
    scicpp_require(!std::ranges::empty(x));

    if (std::ranges::size(x) == 1) {
        dst.resize(1);
        dst[0] = *std::cbegin(x);
    } else {
        fft_into(x, dst);
    }

    return std::move(dst);
}

template <std::ranges::contiguous_range Array>
auto fft(const Array &x) {
    using Tarr = std::ranges::range_value_t<Array>;

    if constexpr (meta::is_complex_v<Tarr>) {
        using T = Tarr::value_type;
        std::vector<std::complex<T>> y;
        return fft(x, std::move(y));
    } else {
        std::vector<std::complex<Tarr>> y;
        return fft(x, std::move(y));
    }
}

template <class InputIt, class CplxVector>
void rfft_inplace(InputIt first, InputIt last, CplxVector &dst) {
    using T = typename CplxVector::value_type::value_type;

    const auto src_size = std::distance(first, last);
    scicpp_require(src_size != 0);

    dst.resize(std::size_t(src_size) / 2 + 1);
    Eigen::FFT<T> fft_engine;
    fft_engine.SetFlag(Eigen::FFT<T>::HalfSpectrum);
    fft_engine.fwd(dst.data(), &*first, src_size);
}

template <class Array, class CplxVector>
void rfft_inplace(const Array &x, CplxVector &dst) {
    rfft_inplace(std::cbegin(x), std::cend(x), dst);
}

template <class Array, class CplxVector>
auto rfft(const Array &x, CplxVector &&dst) {
    scicpp_require(!x.empty());

    if (x.size() == 1) {
        dst.resize(1);
        dst[0] = x[0];
    } else {
        rfft_inplace(x, dst);
    }

    return std::move(dst);
}

template <class Array>
auto rfft(const Array &x) {
    using T = typename Array::value_type;
    std::vector<std::complex<T>> y;
    return rfft(x, std::move(y));
}

// TODO Iterators interface for ifft

template <typename T>
auto ifft(const std::vector<std::complex<T>> &y, int n = -1) {
    Eigen::FFT<T> fft_engine;
    std::vector<std::complex<T>> x;

    if (int(y.size()) == n || n < 0) {
        fft_engine.inv(x, y);
    } else {
        fft_engine.inv(x, zero_padding(y, std::size_t(n)));
    }

    return x;
}

namespace detail {

template <typename T>
auto to_complex(const std::vector<T> &y, std::size_t n) {
    std::vector<std::complex<T>> x(n, std::complex<T>(T{0}, T{0}));

    std::transform(std::cbegin(y),
                   std::cbegin(y) + int(std::min(n, y.size())),
                   std::begin(x),
                   [](auto v) { return std::complex(v, T{0}); });

    return x;
}

} // namespace detail

template <typename T>
auto ifft(const std::vector<T> &y, int n = -1) {
    Eigen::FFT<T> fft_engine;
    std::vector<std::complex<T>> x;
    const auto size = n < 0 ? y.size() : std::size_t(n);
    fft_engine.inv(x, detail::to_complex(y, size));
    return x;
}

template <typename T>
auto irfft(const std::vector<std::complex<T>> &y, int n = -1) {
    Eigen::FFT<T> fft_engine;
    std::vector<T> x;

    if (int(y.size()) == n) {
        fft_engine.inv(x, y);
    } else {
        const auto size = n < 0 ? 2 * (y.size() - 1) : std::size_t(n);
        fft_engine.inv(x, zero_padding(y, size));
    }

    return x;
}

} // namespace scicpp::signal

#endif // SCICPP_SIGNAL_FFT
