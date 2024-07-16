// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

// Compile time arithmetic functions.
// Mostly implements root_ratio used for dimensional analysis.

#ifndef SCICPP_CORE_UNITS_ARITHMETIC
#define SCICPP_CORE_UNITS_ARITHMETIC

#include "scicpp/core/meta.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <ratio>
#include <utility>

namespace scicpp::arithmetic {

//---------------------------------------------------------------------------------
// Compile-time log2
// https://hbfs.wordpress.com/2016/03/22/log2-with-c-metaprogramming/
//---------------------------------------------------------------------------------

constexpr inline intmax_t ct_log2(intmax_t num) {
    return (num < 2) ? 1 : 1 + ct_log2(intmax_t(uintmax_t(num) >> 1));
}

//---------------------------------------------------------------------------------
// Compile-time power
// https://stackoverflow.com/questions/27270541/is-there-no-built-in-way-to-compute-a-power-at-compile-time-in-c
//---------------------------------------------------------------------------------

template <typename T>
constexpr T power(T a, intmax_t n) {
    if (n == 0) {
        return T{1};
    }

    const auto p = power(a, n / 2);
    return p * p * (n % 2 == 0 ? T{1} : a);
}

//---------------------------------------------------------------------------------
// Compile-time root
// Use Newton-Raphson method, generalizing
// https://stackoverflow.com/questions/8622256/in-c11-is-sqrt-defined-as-constexpr
//---------------------------------------------------------------------------------

namespace detail {

template <intmax_t N, typename T>
constexpr T root_newton_raphson(T x, T curr, T prev) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (curr == prev) {
        return curr;
    }
#pragma GCC diagnostic pop

    return root_newton_raphson<N>(
        x, (T{N - 1} * curr + x / power(curr, N - 1)) / T{N}, curr);
}

} // namespace detail

template <intmax_t N, typename T>
constexpr T ct_root(T x) {
    if (x < T{0}) {
        return std::numeric_limits<T>::quiet_NaN();
    }

    if constexpr (N == 1) {
        return x;
    } else {
        return detail::root_newton_raphson<N>(x, x, T{0});
    }
}

//---------------------------------------------------------------------------------
// Compile-time prime factor decomposition
//---------------------------------------------------------------------------------

template <intmax_t n>
struct prime_factors {
    using prime_t = intmax_t;
    using pow_t = intmax_t;

    // https://stackoverflow.com/questions/11924249/finding-prime-factors
    constexpr prime_factors() {
        intmax_t z = 2;
        intmax_t num = n;

        while (z * z <= num) {
            if (num % z == 0) {
                append(z);
                num /= z;
            } else {
                z++;
            }
        }

        if (num > 1) {
            append(num);
        }
    }

    constexpr auto values() const { return factors; }

  private:
    constexpr void append(prime_t prime) {
        std::size_t i = 0;

        for (; i < factors.size(); ++i) {
            if (factors[i].first == 0) {
                break;
            }

            if (factors[i].first == prime) {
                ++factors[i].second;
                return;
            }
        }

        // prime not in array
        factors[i].first = prime;
        factors[i].second = 1;
    }

    // There are at most log2(n) prime factors
    std::array<std::pair<prime_t, pow_t>, std::size_t(ct_log2(n))> factors{};
};

//---------------------------------------------------------------------------------
// Root of a ratio
//---------------------------------------------------------------------------------

// Root of a rational number (Q/P)^(1/R)
// where
// - Q/P is an irreducible fraction
// - Q and P are not perfect Rth power

template <typename Ratio, intmax_t Root = 1>
struct root_ratio {
    static_assert(meta::is_ratio_v<Ratio>);
    static_assert(Ratio::num > 0);
    static_assert(Ratio::den > 0);
    static_assert(Root > 0);

    using ratio = Ratio;

    static constexpr auto num = Ratio::num;
    static constexpr auto den = Ratio::den;
    static constexpr auto root = Root;
};

template <typename T, typename Ratio, intmax_t Root = 1>
constexpr auto eval(root_ratio<Ratio, Root> /* unused */) {
    return ct_root<Root>(T(Ratio::num) / T(Ratio::den));
}

namespace detail {

template <typename PrimeFactors>
constexpr bool is_exact_root(PrimeFactors factors, intmax_t root) {
    for (const auto &factor : factors) {
        if (factor.second % root != 0) {
            return false;
        }
    }

    return true;
}

template <typename PrimeFactors>
constexpr auto compute_root(PrimeFactors factors, intmax_t root) {
    intmax_t res = 1;

    for (const auto &factor : factors) {
        res *= power(factor.first, factor.second / root);
    }

    return res;
}

template <typename RR, intmax_t Root = 1>
constexpr auto root_ratio_root_impl() {
    static_assert(Root > 0);

    constexpr auto Q = RR::num;
    constexpr auto P = RR::den;
    constexpr auto R = RR::root * Root;

    // If Q and P are exact roots of R (Q=q^R and P=p^R),
    // we can reduce the dimension to a ratio only:
    // (Q/P)^(1/R) = (q^R / p^R)^(1/R) = q/p
    constexpr auto q_factors = prime_factors<Q>().values();
    constexpr auto p_factors = prime_factors<P>().values();

    if constexpr (is_exact_root(q_factors, R) && is_exact_root(p_factors, R)) {
        return root_ratio<std::ratio<compute_root(q_factors, R),
                                     compute_root(p_factors, R)>>{};
    } else {
        return root_ratio<std::ratio<Q, P>, R>{};
    }
}

template <typename RR1, typename RR2>
constexpr auto root_ratio_multiply_impl() {
    constexpr auto R1 = RR1::root;
    constexpr auto R2 = RR2::root;

    if constexpr (R1 == 1 && R2 == 1) {
        return root_ratio<
            std::ratio_multiply<typename RR1::ratio, typename RR2::ratio>>{};
    } else {
        constexpr auto Q1 = RR1::num;
        constexpr auto Q2 = RR2::num;
        constexpr auto P1 = RR1::den;
        constexpr auto P2 = RR2::den;

        // Reduce under common root:
        // (Q1/P1)^(1/R1) x (Q2/P2)^(1/R2) = (Q/P)^(1/R)
        // where:
        // Q = Q1^r2 x Q2^r1
        // P = P1^r2 x P2^r1
        // R = G r1 r2
        // and
        // G = gcd(R1, R2),
        // Ri = G ri

        constexpr auto G = std::gcd(R1, R2);
        constexpr auto r1 = R1 / G;
        constexpr auto r2 = R2 / G;

        constexpr auto Q = power(Q1, r2) * power(Q2, r1);
        constexpr auto P = power(P1, r2) * power(P2, r1);
        constexpr auto R = G * r1 * r2;

        // Compute the root to reduce the root_ratio if exact root
        return root_ratio_root_impl<root_ratio<std::ratio<Q, P>, R>>();
    }
}

template <typename RR1, typename RR2>
constexpr auto root_ratio_divide_impl() {
    using RR2Inv = root_ratio<std::ratio<RR2::den, RR2::num>, RR2::root>;
    return root_ratio_multiply_impl<RR1, RR2Inv>();
}

template <typename RR, intmax_t N>
constexpr auto root_ratio_power_impl() {
    if constexpr (N == 0) {
        return root_ratio<std::ratio<1>>{};
    } else {
        return root_ratio_multiply_impl<
            RR,
            decltype(root_ratio_power_impl<RR, N - 1>())>();
    }
}

} // namespace detail

template <typename RR, intmax_t Root = 1>
using root_ratio_root = decltype(detail::root_ratio_root_impl<RR, Root>());

template <typename RR1, typename RR2>
using root_ratio_multiply =
    decltype(detail::root_ratio_multiply_impl<RR1, RR2>());

template <typename RR1, typename RR2>
using root_ratio_divide = decltype(detail::root_ratio_divide_impl<RR1, RR2>());

template <typename RR, intmax_t N>
using root_ratio_power = decltype(detail::root_ratio_power_impl<RR, N>());

template <typename RR1, typename RR2>
struct common_root_ratio {
    static constexpr auto Q1_R2 = power(RR1::num, RR2::root);
    static constexpr auto Q2_R1 = power(RR2::num, RR1::root);
    static constexpr auto P1_R2 = power(RR1::den, RR2::root);
    static constexpr auto P2_R1 = power(RR2::den, RR1::root);
    static constexpr auto R = RR1::root * RR2::root;

    static constexpr auto G_Q = std::gcd(Q1_R2, Q2_R1);
    static constexpr auto G_P = std::gcd(P1_R2, P2_R1);

    static constexpr auto Q = G_Q;
    static constexpr auto P = (P1_R2 / G_P) * P2_R1;

    using type = root_ratio_root<root_ratio<std::ratio<Q, P>, R>>;
};

template <typename RR1, typename RR2>
using common_root_ratio_t = typename common_root_ratio<RR1, RR2>::type;

//---------------------------------------------------------------------------------
// is_prime, next_prime
// https://stackoverflow.com/questions/30052316/find-next-prime-number-algorithm
//---------------------------------------------------------------------------------

constexpr inline bool is_prime(intmax_t number) {
    if (number == 2 || number == 3) {
        return true;
    }

    if (number % 2 == 0 || number % 3 == 0) {
        return false;
    }

    intmax_t divisor = 6;

    while (divisor * divisor - 2 * divisor + 1 <= number) {
        if ((number % (divisor - 1) == 0) || (number % (divisor + 1) == 0)) {
            return false;
        }

        divisor += 6;
    }

    return true;
}

constexpr inline intmax_t next_prime(intmax_t a) {
    while (!is_prime(++a)) {
    }
    return a;
}

template <std::size_t N, intmax_t First = 1>
struct prime_list {
    constexpr prime_list() {
        ids[0] = First;

        for (std::size_t i = 1; i < N; ++i) {
            ids[i] = next_prime(ids[i - 1]);
        }
    }

    constexpr auto values() const { return ids; }

    std::array<intmax_t, N> ids{};
};

} // namespace scicpp::arithmetic

#endif // SCICPP_CORE_UNITS_ARITHMETIC