// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_SIGNAL_FILTER_DESIGN
#define SCICPP_SIGNAL_FILTER_DESIGN

#include "scicpp/core/maths.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/print.hpp"
#include "scicpp/core/stats.hpp"

#include <functional>
#include <tuple>
#include <utility>
#include <vector>

namespace scicpp::signal {

//---------------------------------------------------------------------------------
// unique_roots
//---------------------------------------------------------------------------------

enum class Rtype : int { MAX, MIN, AVG };

namespace detail {

// https://stackoverflow.com/questions/17074324/how-can-i-sort-two-vectors-in-the-same-way-with-criteria-that-uses-only-one-of

template <typename T>
auto sort_permutation(const std::vector<T> &vec) {
    std::vector<std::size_t> perm(vec.size());
    std::iota(perm.begin(), perm.end(), 0);
    std::sort(perm.begin(), perm.end(), [&](auto i, auto j) {
        return vec[i] <= vec[j];
    });
    return perm;
}

template <typename T>
auto apply_permutation(const std::vector<T> &vec,
                       const std::vector<std::size_t> &perm) {
    std::vector<T> sorted_vec(vec.size());
    std::transform(perm.begin(), perm.end(), sorted_vec.begin(), [&](auto i) {
        return vec[i];
    });
    return sorted_vec;
}

} // namespace detail

template <Rtype rtype, meta::Iterable Array, typename T>
auto unique_roots(const Array &p, T tol = 1E-3) {
    using Tp = typename Array::value_type;

    std::vector p_copy(p.cbegin(), p.cend());
    std::vector<Tp> uniq{};
    std::vector<std::size_t> mult{};

    for (const auto &x : p) {
        std::vector<Tp> tmp{};

        for (auto it = p_copy.begin(); it != p_copy.end();) {
            if (absolute(x - *it) < tol) {
                tmp.push_back(*it);
                it = p_copy.erase(it);
            } else {
                ++it;
            }
        }

        if (!tmp.empty()) {
            if constexpr (rtype == Rtype::MAX) {
                uniq.push_back(stats::amax(tmp));
            } else if constexpr (rtype == Rtype::MIN) {
                uniq.push_back(stats::amin(tmp));
            } else if constexpr (rtype == Rtype::AVG) {
                uniq.push_back(stats::mean(tmp));
            }

            mult.push_back(tmp.size());
        }
    }

    auto perm = detail::sort_permutation(uniq);
    auto sorted_mult = detail::apply_permutation(mult, perm);
    std::sort(uniq.begin(), uniq.end());
    return std::tuple{std::move(uniq), std::move(sorted_mult)};
}

} // namespace scicpp::signal

#endif // SCICPP_SIGNAL_FILTER_DESIGN