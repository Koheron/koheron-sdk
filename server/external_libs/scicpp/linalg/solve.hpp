// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_LINALG_SOLVE
#define SCICPP_LINALG_SOLVE

#include "scicpp/linalg/utils.hpp"

#include <Eigen/Dense>
#include <type_traits>

namespace scicpp::linalg {

template <class EigenMatrix, class StdContainer>
auto lstsq(const EigenMatrix &A, const StdContainer &b) {
    static_assert(std::is_same_v<typename EigenMatrix::value_type,
                                 typename StdContainer::value_type>);

    return to_std_container(
        A.fullPivHouseholderQr().solve(to_eigen_matrix(b)).eval());
}

} // namespace scicpp::linalg

#endif // SCICPP_LINALG_SOLVE
