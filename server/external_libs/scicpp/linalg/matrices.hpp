// SPDX-License-Identifier: MIT
// Copyright (c) 2024 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_LINALG_MATRICES
#define SCICPP_LINALG_MATRICES

#include "scicpp/core/macros.hpp"
#include "scicpp/core/manips.hpp"
#include "scicpp/linalg/utils.hpp"

#include <Eigen/Dense>
#include <array>
#include <vector>

namespace scicpp::linalg {

// ----------------------------------------------------------------------------
// eye
// ----------------------------------------------------------------------------

template <typename T, std::size_t N>
auto scicpp_pure eye(signed_size_t k = 0) {
    Eigen::Matrix<T, N, N> res{};
    res.setZero();
    res.diagonal(k).setOnes();
    return res;
}

template <typename T>
auto eye(std::size_t n, signed_size_t k = 0) {
    Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic> res(n, n);
    res.setZero();
    res.diagonal(k).setOnes();
    return res;
}

// ----------------------------------------------------------------------------
// companion
//
// See also polynomial::polycompanion
// ----------------------------------------------------------------------------

template <typename T, std::size_t N>
auto companion(const std::array<T, N> &v) {
    static_assert(N > 1);

    constexpr int deg = N - 1;
    auto res = eye<T, deg>(-1);
    res.row(0) =
        -to_eigen_matrix(slice_array(v, 1, int(v.size())), deg).transpose() /
        v[0];
    return res;
}

template <typename T>
auto companion(const std::vector<T> &v) {
    scicpp_require(v.size() > 1);

    const auto deg = int(v.size()) - 1;
    auto res = eye<T>(std::size_t(deg), -1);
    res.row(0) =
        -to_eigen_matrix(slice_array(v, 1, int(v.size())), deg).transpose() /
        v[0];
    return res;
}

} // namespace scicpp::linalg

#endif // SCICPP_LINALG_MATRICES