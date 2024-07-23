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
#include <cstdlib>
#include <unsupported/Eigen/FFT>
#include <utility>
#include <vector>

namespace scicpp::signal {

//---------------------------------------------------------------------------------
// FFT helper functions
//---------------------------------------------------------------------------------

template <class Array>
auto fftshift(Array &&a) {
    static_assert(meta::is_iterable_v<Array>);
    std::rotate(
        a.begin(), a.begin() + signed_size_t(a.size() + 1) / 2, a.end());
    return std::move(a);
}

template <class Array>
auto fftshift(const Array &a) {
    static_assert(meta::is_iterable_v<Array>);
    auto res = utils::set_array(a);
    const auto offset = (signed_size_t(a.size() - 1) / 2) + 1;
    std::copy(a.cbegin() + offset, a.cend(), res.begin());
    std::copy(a.cbegin(),
              a.cbegin() + offset,
              res.begin() + signed_size_t(a.size()) / 2);
    return res;
}

template <class Array>
auto ifftshift(Array &&a) {
    static_assert(meta::is_iterable_v<Array>);
    std::rotate(a.begin(), a.begin() + signed_size_t(a.size()) / 2, a.end());
    return std::move(a);
}

template <class Array>
auto ifftshift(const Array &a) {
    static_assert(meta::is_iterable_v<Array>);
    auto res = utils::set_array(a);
    const auto offset = (signed_size_t(a.size()) / 2);
    std::copy(a.cbegin() + offset, a.cend(), res.begin());
    std::copy(a.cbegin(),
              a.cbegin() + offset,
              res.begin() + signed_size_t(a.size() + 1) / 2);
    return res;
}

namespace detail {

using namespace scicpp::operators;

template <class Array, typename T = typename Array::value_type>
auto fftfreq_impl(Array &&res, T d) {
    scicpp_require(d > T{0});
    const auto N = signed_size_t(res.size());
    std::iota(res.begin(), res.end(), -T(N / 2));
    return ifftshift(std::forward<Array>(res) / (d * T(N)));
}

template <class Array, typename T = typename Array::value_type>
auto rfftfreq_impl(Array &&res, std::size_t N, T d) {
    scicpp_require(d > T{0});
    std::iota(res.begin(), res.end(), T{0});
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

template <typename Array>
auto zero_padding(const Array &v, std::size_t new_size) {
    static_assert(meta::is_iterable_v<Array>);

    using T = typename Array::value_type;
    auto res = zeros<T>(new_size);
    std::copy(v.cbegin(),
              v.cbegin() + signed_size_t(std::min(new_size, v.size())),
              res.begin());
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

template <class InputIt, class CplxVector>
void fft_inplace(InputIt first, InputIt last, CplxVector &dst) {
    using T = typename CplxVector::value_type::value_type;

    const auto src_size = std::distance(first, last);
    scicpp_require(src_size != 0);

    dst.resize(std::size_t(src_size));
    Eigen::FFT<T> fft_engine;
    fft_engine.fwd(dst.data(), &*first, src_size);
}

template <class Array, class CplxVector>
void fft_inplace(const Array &x, CplxVector &dst) {
    fft_inplace(x.cbegin(), x.cend(), dst);
}

template <class Array, class CplxVector>
auto fft(const Array &x, CplxVector &&dst) {
    scicpp_require(!x.empty());

    if (x.size() == 1) {
        dst.resize(1);
        dst[0] = x[0];
    } else {
        fft_inplace(x, dst);
    }

    return std::move(dst);
}

template <class Array>
auto fft(const Array &x) {
    using Tarr = typename Array::value_type;

    if constexpr (meta::is_complex_v<Tarr>) {
        using T = typename Tarr::value_type;
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
    rfft_inplace(x.cbegin(), x.cend(), dst);
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

    std::transform(y.cbegin(),
                   y.cbegin() + int(std::min(n, y.size())),
                   x.begin(),
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
