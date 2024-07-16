// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_POLYNOMIALS_POLYNOMIAL
#define SCICPP_POLYNOMIALS_POLYNOMIAL

#include "scicpp/core/equal.hpp"
#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/numeric.hpp"
#include "scicpp/linalg/solve.hpp"
#include "scicpp/linalg/utils.hpp"
#include "scicpp/signal/convolve.hpp"

#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace scicpp::polynomial {

//---------------------------------------------------------------------------------
// Polyval
//---------------------------------------------------------------------------------

template <class T, class Array>
auto polyval(T &&x, const Array &coeffs) {
    if constexpr (meta::is_iterable_v<T>) {
        return map([&](auto v) { return polyval(v, coeffs); },
                   std::forward<T>(x));
    } else {
        using scalar_t = std::remove_reference_t<T>;
        static_assert(std::is_same_v<scalar_t, typename Array::value_type>);

        // https://en.wikipedia.org/wiki/Horner%27s_method
        auto res = scalar_t{0};

        std::for_each(coeffs.crbegin(), coeffs.crend(), [&](auto c) {
            res = std::fma(res, x, c);
        });

        return res;
    }
}

//---------------------------------------------------------------------------------
// polyadd
//---------------------------------------------------------------------------------

namespace detail {

template <class U, class V, class W>
void add_arrays(const U &P1, const V &P2, W &P) {
    using T = typename U::value_type;
    static_assert(std::is_same_v<typename U::value_type, T>);
    static_assert(std::is_same_v<typename W::value_type, T>);

    if (P1.size() <= P2.size()) {
        scicpp_require(P.size() >= P2.size());
        std::transform(
            P1.cbegin(), P1.cend(), P2.cbegin(), P.begin(), std::plus<T>());
        std::copy(P2.cbegin() + int(P1.size()),
                  P2.cend(),
                  P.begin() + int(P1.size()));
    } else {
        scicpp_require(P.size() >= P1.size());
        std::transform(
            P2.cbegin(), P2.cend(), P1.cbegin(), P.begin(), std::plus<T>());
        std::copy(P1.cbegin() + int(P2.size()),
                  P1.cend(),
                  P.begin() + int(P2.size()));
    }
}

} // namespace detail

template <typename T, std::size_t M, std::size_t N>
auto polyadd(const std::array<T, M> &P1, const std::array<T, N> &P2) {
    std::array<T, (M > N ? M : N)> P{};
    detail::add_arrays(P1, P2, P);
    return P;
}

template <typename T>
auto polyadd(const std::vector<T> &P1, const std::vector<T> &P2) {
    std::vector<T> P(P1.size() > P2.size() ? P1.size() : P2.size());
    detail::add_arrays(P1, P2, P);
    return P;
}

//---------------------------------------------------------------------------------
// polysub
//---------------------------------------------------------------------------------

namespace detail {

template <class U, class V, class W>
void sub_arrays(const U &P1, const V &P2, W &P) {
    using T = typename U::value_type;
    static_assert(std::is_same_v<typename U::value_type, T>);
    static_assert(std::is_same_v<typename W::value_type, T>);

    if (P1.size() <= P2.size()) {
        scicpp_require(P.size() >= P2.size());
        std::transform(
            P1.cbegin(), P1.cend(), P2.cbegin(), P.begin(), std::minus<T>());
        std::transform(P2.cbegin() + int(P1.size()),
                       P2.cend(),
                       P.begin() + int(P1.size()),
                       [](auto v) { return -v; });
    } else {
        scicpp_require(P.size() >= P1.size());
        std::transform(P2.cbegin(),
                       P2.cend(),
                       P1.cbegin(),
                       P.begin(),
                       [](auto v2, auto v1) { return v1 - v2; });
        std::copy(P1.cbegin() + int(P2.size()),
                  P1.cend(),
                  P.begin() + int(P2.size()));
    }
}

} // namespace detail

template <typename T, std::size_t M, std::size_t N>
auto polysub(const std::array<T, M> &P1, const std::array<T, N> &P2) {
    std::array<T, (M > N ? M : N)> P{};
    detail::sub_arrays(P1, P2, P);
    return P;
}

template <typename T>
auto polysub(const std::vector<T> &P1, const std::vector<T> &P2) {
    std::vector<T> P(P1.size() > P2.size() ? P1.size() : P2.size());
    detail::sub_arrays(P1, P2, P);
    return P;
}

//---------------------------------------------------------------------------------
// polymul
//---------------------------------------------------------------------------------

template <signal::ConvMethod method, class U, class V>
auto polymul(const U &P1, const V &P2) {
    return signal::convolve<method>(P1, P2);
}

// Specialization for the default convolution method
template <class U, class V>
auto polymul(const U &P1, const V &P2) {
    return polymul<signal::DIRECT>(P1, P2);
}

//---------------------------------------------------------------------------------
// polydiv
//---------------------------------------------------------------------------------

namespace detail {

// https://stackoverflow.com/questions/44770632/fft-division-for-fast-polynomial-division
// http://web.cs.iastate.edu/~cs577/handouts/polydivide.pdf

template <class U, class V>
constexpr auto polydiv_impl(const U &u, const V &v) {
    using T = typename U::value_type;
    static_assert(std::is_same_v<T, typename V::value_type>);

    auto tmp(u);
    const auto m = signed_size_t(u.size()) - 1;
    const auto n = signed_size_t(v.size()) - 1;
    const auto scale = T{1} / v[std::size_t(n)];

    for (signed_size_t k = m - n; k >= 0; --k) {
        const auto d = scale * tmp[std::size_t(n + k)];

        for (signed_size_t j = n + k - 1; j >= k; --j) {
            tmp[std::size_t(j)] -= d * v[std::size_t(j - k)];
        }
    }

    return tmp;
}

} // namespace detail

template <typename T, std::size_t M, std::size_t N>
constexpr auto polydiv(const std::array<T, M> &u, const std::array<T, N> &v) {
    static_assert(M > 0);
    static_assert(N > 0);

    if constexpr (N == 1) {
        using namespace scicpp::operators;
        return std::tuple{u / v[0], std::array<T, 1>{}};
    } else if constexpr (M < N) {
        return std::tuple{std::array<T, 1>{}, u};
    } else {
        const auto tmp = detail::polydiv_impl(u, v);

        std::array<T, M - N + 1> q{};
        std::array<T, N - 1> r{};
        std::move(tmp.begin(), tmp.begin() + r.size(), r.begin());
        std::move(tmp.begin() + r.size(), tmp.end(), q.begin());
        return std::tuple{q, r};
    }
}

template <typename T>
auto polydiv(const std::vector<T> &u, const std::vector<T> &v) {
    scicpp_require(!u.empty());
    scicpp_require(!v.empty());

    if (v.size() == 1) {
        using namespace scicpp::operators;
        return std::tuple{u / v[0], std::vector<T>(1, T{0})};
    }

    if (u.size() < v.size()) {
        return std::tuple{std::vector<T>(1, T{0}), u};
    }

    const auto tmp = detail::polydiv_impl(u, v);
    const auto len = signed_size_t(v.size()) - 1;

    return std::tuple{
        std::vector<T>(std::make_move_iterator(tmp.begin() + len),
                       std::make_move_iterator(tmp.end())),
        std::vector<T>(std::make_move_iterator(tmp.begin()),
                       std::make_move_iterator(tmp.begin() + len))};
}

//---------------------------------------------------------------------------------
// polymulx
//---------------------------------------------------------------------------------

template <typename T, std::size_t N>
auto polymulx(const std::array<T, N> &P) {
    std::array<T, N + 1> res{};
    std::copy(P.cbegin(), P.cend(), res.begin() + 1);
    return res;
}

template <typename T>
auto polymulx(const std::vector<T> &P) {
    std::vector<T> res(P.size() + 1);
    std::copy(P.cbegin(), P.cend(), res.begin() + 1);
    return res;
}

template <typename T>
auto polymulx(std::vector<T> &&P) {
    P.push_back(T{0});
    std::rotate(P.rbegin(), P.rbegin() + 1, P.rend());
    return std::move(P);
}

//---------------------------------------------------------------------------------
// polypow
//---------------------------------------------------------------------------------

// https://en.wikipedia.org/wiki/Exponentiation_by_squaring
// - Recursive implementation is used for fixed size arrays (std::array).
// - Iterative implementation is used for dynamic size arrays (std::vector).

template <std::size_t pow, typename T, std::size_t N>
auto polypow(const std::array<T, N> &P) {
    // Return a std::array<T, pow * (N - 1) + 1>
    if constexpr (pow == 0) {
        return std::array{T{1}};
    } else if constexpr (pow == 1) {
        return std::array{P};
    } else {
        if constexpr (pow % 2 == 0) {
            return polypow<pow / 2>(polymul(P, P));
        } else {
            return polymul(P, polypow<(pow - 1) / 2>(polymul(P, P)));
        }
    }
}

template <signal::ConvMethod method, typename T>
auto polypow(const std::vector<T> &P, std::size_t pow) {
    if (pow == 0) {
        return std::vector(1, T{1});
    } else if (pow == 1) {
        return std::vector<T>(P.cbegin(), P.cend());
    }

    std::vector<T> x(P.cbegin(), P.cend());
    std::vector<T> y(1, T(1));
    x.reserve(pow * (P.size() - 1) + 1);
    y.reserve(pow * (P.size() - 1) + 1);

    while (pow > 1) {
        if (pow % 2 == 1) {
            y = std::move(polymul<method>(x, y));
        }

        pow = pow / 2;
        x = std::move(polymul<method>(x, x));
    }

    return polymul(x, y);
}

// Specialization for the default convolution method (DIRECT)
template <typename T>
auto polypow(const std::vector<T> &P, std::size_t pow) {
    return polypow<signal::DIRECT>(P, pow);
}

//---------------------------------------------------------------------------------
// polyder
//---------------------------------------------------------------------------------

namespace detail {

template <typename T, std::size_t N>
constexpr auto polyder_once(const std::array<T, N> &P) {
    static_assert(N >= 1);
    std::array<T, N - 1> res{};

    for (std::size_t i = 0; i < res.size(); ++i) {
        res[i] = T(N - 1 - i) * P[i];
    }

    return res;
}

template <typename T>
void polyder_once(std::vector<T> &P) {
    scicpp_require(!P.empty());
    const auto N = P.size();

    for (std::size_t i = 0; i < (N - 1); ++i) {
        P[i] *= T(N - 1 - i);
    }

    P.resize(N - 1);
}

} // namespace detail

template <signed_size_t m, typename T, std::size_t N>
constexpr auto polyder(const std::array<T, N> &P) {
    static_assert(m >= 0);

    if constexpr (m == 0) {
        return std::array{P};
    } else if constexpr (m >= N) {
        return std::array<T, 0>{};
    } else {
        return polyder<m - 1>(detail::polyder_once(P));
    }
}

template <typename T, std::size_t N>
constexpr auto polyder(const std::array<T, N> &P) {
    return polyder<1>(P);
}

template <typename T>
auto polyder(const std::vector<T> &P, signed_size_t m = 1) {
    scicpp_require(m >= 0);

    if (m == 0) {
        return std::vector<T>(P);
    }

    if (m >= int(P.size())) {
        return std::vector<T>(0);
    }

    std::vector<T> res(P);

    while (m--) {
        detail::polyder_once(res);
    }

    return res;
}

//---------------------------------------------------------------------------------
// polyint
//---------------------------------------------------------------------------------

namespace detail {

template <typename T, std::size_t N>
constexpr auto polyint_once(const std::array<T, N> &P) {
    std::array<T, N + 1> res{};

    for (std::size_t i = 0; i < N; ++i) {
        res[i + 1] = P[i] / T(i + 1);
    }

    return res;
}

template <typename T>
void polyint_once(std::vector<T> &P) {
    const auto N = P.size();
    P.resize(N + 1);

    for (signed_size_t i = signed_size_t(N) - 1; i >= 0; --i) {
        P[std::size_t(i) + 1] = P[std::size_t(i)] / T(i + 1);
    }

    P[0] = T{0};
}

} // namespace detail

template <signed_size_t m, typename T, std::size_t N>
constexpr auto polyint(const std::array<T, N> &P) {
    static_assert(N > 0);
    static_assert(m >= 0);

    if constexpr (m == 0) {
        return std::array{P};
    } else {
        return polyint<m - 1>(detail::polyint_once(P));
    }
}

template <typename T, std::size_t N>
constexpr auto polyint(const std::array<T, N> &P) {
    return polyint<1>(P);
}

template <typename T>
auto polyint(const std::vector<T> &P, signed_size_t m = 1) {
    scicpp_require(!P.empty());
    scicpp_require(m >= 0);

    if (m == 0) {
        return std::vector<T>(P);
    }

    std::vector<T> res(P);
    res.reserve(P.size() + std::size_t(m));

    while (m--) {
        detail::polyint_once(res);
    }

    return res;
}

//---------------------------------------------------------------------------------
// polycompanion
//---------------------------------------------------------------------------------

template <typename T, std::size_t N>
auto polycompanion(const std::array<T, N> &P) {
    constexpr int deg = N - 1;
    Eigen::Matrix<T, deg, deg> res{};
    res.setZero();
    res.diagonal(-1).setOnes();
    res.col(deg - 1) = -linalg::to_eigen_array<deg>(P) / P[deg];
    return res;
}

template <typename T>
auto polycompanion(const std::vector<T> &P) {
    const int deg = int(P.size()) - 1;
    Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> res(deg, deg);
    res.setZero();
    res.diagonal(-1).setOnes();
    res.col(deg - 1) = -linalg::to_eigen_matrix(P, deg) / P[std::size_t(deg)];
    return res;
}

//---------------------------------------------------------------------------------
// polyroots
//---------------------------------------------------------------------------------

template <class Array>
auto polyroots(const Array &P) {
    return linalg::to_std_container(polycompanion(P).eigenvalues());
}

//---------------------------------------------------------------------------------
// polyvander
//---------------------------------------------------------------------------------

namespace detail {

template <class Matrix, class Vector>
void polyvander_filler(Matrix &res, const Vector &x) {
    static_assert(meta::is_eigen_matrix_v<Matrix>);

    res.col(0).setOnes();
    res.col(1) = linalg::to_eigen_array(x);

    for (int i = 2; i < res.cols(); ++i) {
        res.col(i) = res.col(i - 1).array() * res.col(1).array();
    }
}

} // namespace detail

template <int deg, typename T, std::size_t N>
auto polyvander(const std::array<T, N> &x) {
    Eigen::Matrix<T, int(N), deg + 1> res{};
    detail::polyvander_filler(res, x);
    return res;
}

template <typename T>
auto polyvander(const std::vector<T> &x, int deg) {
    Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> res(int(x.size()),
                                                         deg + 1);
    detail::polyvander_filler(res, x);
    return res;
}

//---------------------------------------------------------------------------------
// polyfit
//---------------------------------------------------------------------------------

template <int deg, typename T, std::size_t N>
auto polyfit(const std::array<T, N> &x, const std::array<T, N> &y) {
    return linalg::lstsq(polyvander<deg>(x), y);
}

template <typename T>
auto polyfit(const std::vector<T> &x, const std::vector<T> &y, int deg) {
    return linalg::lstsq(polyvander(x, deg), y);
}

//---------------------------------------------------------------------------------
// polytrim
//---------------------------------------------------------------------------------

template <typename T>
void polytrim(std::vector<T> &P, T tol = T{0}) {
    P.erase(
        std::find_if(P.crbegin(), P.crend(), [=](auto c) { return c > tol; })
            .base(),
        P.end());

    if (P.empty()) {
        P.push_back(T{0});
    }
}

//---------------------------------------------------------------------------------
// Polynomial class
//---------------------------------------------------------------------------------

template <typename T>
class Polynomial {
  public:
    using value_type = typename std::vector<T>::value_type;
    using size_type = typename std::vector<T>::size_type;

    explicit Polynomial(size_type deg) : m_coef(deg + 1, T{0}) {}

    explicit Polynomial(const std::vector<T> &coef) : m_coef(coef) {}
    explicit Polynomial(std::vector<T> &&coef) : m_coef(std::move(coef)) {}

    template <size_type N>
    explicit Polynomial(const std::array<T, N> &coef)
        : m_coef(coef.cbegin(), coef.cend()) {}

    template <class Iterator>
    Polynomial(Iterator first, Iterator last) : m_coef(first, last) {}

    Polynomial(std::initializer_list<T> l)
        : Polynomial(std::begin(l), std::end(l)) {}

    size_type degree() const { return m_coef.size() - 1; }

    const auto &data() const { return m_coef; }

    void mulx() { m_coef = polymulx(std::move(m_coef)); }

    void trim(T tol = T(0)) { polytrim(m_coef, tol); }

    T operator()(T x) const { return polyval(x, m_coef); }

    void reserve(size_type n_coefs) { m_coef.reserve(n_coefs); }

    Polynomial &operator+=(const Polynomial &rhs) {
        if (rhs.degree() > this->degree()) {
            const long old_size = long(m_coef.size());
            m_coef.resize(rhs.degree() + 1);
            std::transform(rhs.m_coef.begin(),
                           rhs.m_coef.begin() + old_size,
                           m_coef.begin(),
                           m_coef.begin(),
                           std::plus<T>());
            std::copy(rhs.m_coef.begin() + old_size,
                      rhs.m_coef.end(),
                      m_coef.begin() + old_size);
        } else {
            std::transform(rhs.m_coef.begin(),
                           rhs.m_coef.end(),
                           m_coef.begin(),
                           m_coef.begin(),
                           std::plus<T>());
        }

        return *this;
    }

    Polynomial &operator-=(const Polynomial &rhs) {
        if (rhs.degree() > this->degree()) {
            const auto old_size = m_coef.size();
            m_coef.resize(rhs.degree() + 1);

            for (size_type i = 0; i < old_size; ++i) {
                m_coef[i] -= rhs.m_coef[i];
            }

            for (size_type i = old_size; i < m_coef.size(); ++i) {
                m_coef[i] = -rhs.m_coef[i];
            }
        } else {
            for (size_type i = 0; i < rhs.degree() + 1; ++i) {
                m_coef[i] -= rhs.m_coef[i];
            }
        }

        return *this;
    }

    Polynomial &operator*=(T scalar) {
        for (auto &c : m_coef) {
            c *= scalar;
        }

        return *this;
    }

    Polynomial &operator*=(const Polynomial &rhs) {
        m_coef = std::move(polymul(m_coef, rhs.m_coef));
        return *this;
    }

    Polynomial operator+(const Polynomial &rhs) const {
        auto tmp(*this);
        tmp += rhs;
        return tmp;
    }

    Polynomial operator-(const Polynomial &rhs) const {
        auto tmp(*this);
        tmp -= rhs;
        return tmp;
    }

    Polynomial operator*(const Polynomial &rhs) const {
        auto tmp(*this);
        tmp *= rhs;
        return tmp;
    }

    bool scicpp_pure operator==(const Polynomial &rhs) const {
        return m_coef == rhs.m_coef;
    }

    bool scicpp_pure operator!=(const Polynomial &rhs) const {
        return m_coef != rhs.m_coef;
    }

    template <int rel_tol = 1>
    bool is_approx(const Polynomial &rhs) const {
        return almost_equal<rel_tol>(m_coef, rhs.m_coef);
    }

  private:
    std::vector<T> m_coef;

}; // class Polynomial

template <typename T>
Polynomial<T> operator*(const Polynomial<T> &P, T scalar) {
    auto tmp(P);
    tmp *= scalar;
    return tmp;
}

template <typename T>
Polynomial<T> operator*(T scalar, const Polynomial<T> &P) {
    return P * scalar;
}

//---------------------------------------------------------------------------------
// polyfromroots
//---------------------------------------------------------------------------------

// https://stackoverflow.com/questions/33594384/find-the-coefficients-of-the-polynomial-given-its-roots
template <class Array>
auto polyfromroots(const Array &roots) {
    using T = typename Array::value_type;

    auto P = Polynomial<T>({T{1}});
    auto P_ = Polynomial<T>({T{1}});
    P.reserve(roots.size() + 1);
    P_.reserve(roots.size());

    for (const auto &r : roots) {
        P_ = P * r;
        P.mulx();
        P -= P_;
    }

    return P;
}

} // namespace scicpp::polynomial

#endif // SCICPP_POLYNOMIALS_POLYNOMIAL