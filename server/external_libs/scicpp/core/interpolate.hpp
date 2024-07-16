// SPDX-License-Identifier: MIT
// Copyright (c) 2020 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_INTERPOLATE
#define SCICPP_CORE_INTERPOLATE

#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/linalg/utils.hpp"

#include <Eigen/Core>
#include <algorithm>
#include <unsupported/Eigen/Splines>
#include <utility>

namespace scicpp::interpolate {

// Interpolation with degree zero doesn't work
enum InterpKind : int { /* ZERO = 0, */ SLINEAR = 1, QUADRATIC = 2, CUBIC = 3 };

namespace detail {

// https://stackoverflow.com/questions/29822041/eigen-spline-interpolation-how-to-get-spline-y-value-at-arbitray-point-x

template <InterpKind kind>
class SplineFunction {
  public:
    template <class EigenVector>
    SplineFunction(const EigenVector &x, const EigenVector &y)
        : x_min(x.minCoeff()), x_max(x.maxCoeff()),
          // Spline fitting here. X values are scaled down to [0, 1] for this.
          m_spline(Eigen::SplineFitting<Eigen::Spline<double, 1>>::Interpolate(
              y.transpose(),
              std::min<int>(int(x.rows() - 1), kind),
              scaled_values(x))) {
        scicpp_require(x.size() == y.size());
        scicpp_require(x_max > x_min);
    }

    template <typename T>
    auto operator()(T x) const {
        // x values need to be scaled down in extraction as well.
        return m_spline(scaled_value(x))(0);
    }

  private:
    template <typename T>
    auto scaled_value(T x) const {
        return (x - T(x_min)) / (T(x_max) - T(x_min));
    }

    template <class EigenVector>
    auto scaled_values(const EigenVector &v) const {
        return v.unaryExpr([this](double x) { return scaled_value(x); })
            .transpose();
    }

    double x_min, x_max;
    Eigen::Spline<double, 1> m_spline;
};

} // namespace detail

template <InterpKind kind = SLINEAR>
struct interp1d {
    template <typename Array1, typename Array2>
    interp1d(const Array1 &x, const Array2 &y)
        : s(linalg::to_eigen_array(x), linalg::to_eigen_array(y)) {}

    template <typename T>
    auto operator()(T &&x) const {
        if constexpr (meta::is_iterable_v<T>) {
            return map(s, std::forward<T>(x));
        } else {
            return s(x);
        }
    }

  private:
    detail::SplineFunction<kind> s;
};

} // namespace scicpp::interpolate

#endif // SCICPP_CORE_INTERPOLATE
