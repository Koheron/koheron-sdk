// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_UNITS_QUANTITY
#define SCICPP_CORE_UNITS_QUANTITY

#include "scicpp/core/meta.hpp"
#include "scicpp/core/units/arithmetic.hpp"

#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdio>
#include <numeric>
#include <ratio>
#include <tuple>
#include <type_traits>
#include <utility>

namespace scicpp::units {

// http://web.mit.edu/2.25/www/pdf/DA_unified.pdf

// ----------------------------------------------------------------------------
// Dimension
// ----------------------------------------------------------------------------

// Rational of primary dimensions to a given inverse power.
//
// Let q1, ..., qN and r1, ..., rN be primary dimensions (prime numbers),
// a dimension can be represented as:
// Ratio ^ (1 / Root)
// where Ratio = (q1^p1 x ... x qN^pN) / (r1^p1' x ... x rN^pN'),
// and Root is an integer (for example 2 for sqrt).

template <typename Ratio, intmax_t Root = 1>
using dimension = arithmetic::root_ratio<Ratio, Root>;

template <typename Dim, intmax_t Root>
using dimension_root = arithmetic::root_ratio_root<Dim, Root>;

template <typename Dim1, typename Dim2>
using dimension_multiply = arithmetic::root_ratio_multiply<Dim1, Dim2>;

template <typename Dim1, typename Dim2>
using dimension_divide = arithmetic::root_ratio_divide<Dim1, Dim2>;

template <typename Dim, intmax_t N>
using dimension_power = arithmetic::root_ratio_power<Dim, N>;

// ----------------------------------------------------------------------------
// Scale
// ----------------------------------------------------------------------------

template <typename Ratio, intmax_t Root = 1>
using scale = arithmetic::root_ratio<Ratio, Root>;

template <typename Scale, intmax_t Root>
using scale_root = arithmetic::root_ratio_root<Scale, Root>;

template <typename Scale1, typename Scale2>
using scale_multiply = arithmetic::root_ratio_multiply<Scale1, Scale2>;

template <typename Scale1, typename Scale2>
using scale_divide = arithmetic::root_ratio_divide<Scale1, Scale2>;

template <typename Scale, intmax_t N>
using scale_power = arithmetic::root_ratio_power<Scale, N>;

// ----------------------------------------------------------------------------
// Quantity
// ----------------------------------------------------------------------------

// Represents a physical quantity.
// T is the underlying representation type.
// Dim is the dimension of the quantity, that is the rational of primary dimensions.
// Scale is a rational for scaling the quantity.
// Offset mainly for temperature units conversions (ex. K <-> Celsius).

template <typename T, typename Dim, typename Scale, typename Offset>
struct quantity;

// is_quantity

namespace detail {

template <class T>
struct is_quantity : std::false_type {};

template <typename T, typename Dim, typename Scale, typename Offset>
struct is_quantity<quantity<T, Dim, Scale, Offset>> : std::true_type {};

} // namespace detail

template <class T>
constexpr bool is_quantity_v = detail::is_quantity<T>::value;

template <class T>
using enable_if_is_quantity = std::enable_if_t<is_quantity_v<T>, int>;

template <class T>
using disable_if_is_quantity = std::enable_if_t<!is_quantity_v<T>, int>;

// common_quantity

template <typename T,
          typename Dim,
          typename Scale1,
          typename Scale2,
          typename Offset1,
          typename Offset2>
struct common_quantity {
    static_assert(std::is_same_v<Offset1, Offset2>,
                  "Do not add/subtract units with different offset, this is "
                  "confusing. Use an explicit call to quantity_cast.");

    using CommonScale = arithmetic::common_root_ratio_t<Scale1, Scale2>;
    using type = quantity<T, Dim, CommonScale, Offset1>;
};

template <typename T,
          typename Dim,
          typename Scale1,
          typename Scale2,
          typename Offset1,
          typename Offset2>
using common_quantity_t =
    typename common_quantity<T, Dim, Scale1, Scale2, Offset1, Offset2>::type;

// quantity_cast

template <typename ToQty,
          typename T,
          typename Dim,
          typename Scale,
          typename Offset,
          enable_if_is_quantity<ToQty> = 0>
constexpr auto quantity_cast(const quantity<T, Dim, Scale, Offset> &qty) {
    static_assert(std::is_same_v<Dim, typename ToQty::dim>,
                  "Cannot cast to a quantity with different dimension");

    using to_rep_t = typename ToQty::value_type;
    using rep_t = std::common_type_t<T, to_rep_t>;

    using QtyScale = scale_divide<Scale, typename ToQty::scal>;
    constexpr auto qty_scale = arithmetic::eval<rep_t>(QtyScale());

    using OffsetDiff = std::ratio_subtract<Offset, typename ToQty::offset>;
    constexpr auto offset_diff =
        rep_t(OffsetDiff::num) / rep_t(OffsetDiff::den);

    return ToQty(static_cast<to_rep_t>(
        qty_scale * static_cast<rep_t>(qty.value()) + offset_diff));
}

template <typename T,
          typename Dim,
          typename Scale,
          typename Offset = std::ratio<0>>
struct quantity {
  private:
    template <typename Scale2>
    static constexpr bool is_harmonic =
        (std::ratio_divide<Scale2, Scale>::den == 1);

    template <typename T2, typename Scale2>
    static constexpr bool is_implicitly_convertible =
        std::is_floating_point_v<T> ||
        (!std::is_floating_point_v<T2> && is_harmonic<Scale2>);

    template <typename T2>
    static constexpr bool is_convertible_rep =
        std::is_convertible_v<const T2 &, T> ||
        (std::is_floating_point_v<T> && !std::is_floating_point_v<T2>);

  public:
    static_assert(meta::is_ratio_v<Offset>);
    static_assert(Offset::den != 0);

    using value_type = T;
    using dim = Dim;
    using scal = Scale;
    using offset = Offset;

    // Constructors, destructors, copy

    constexpr quantity() = default;

    quantity(const quantity &) = default;

    template <typename T2, std::enable_if_t<is_convertible_rep<T2>, int> = 0>
    explicit constexpr quantity(const T2 &value)
        : m_value(static_cast<T>(value)) {}

    // We follow std::chrono and only allow implicit conversion
    // between units if this does not result in a loss in precision.
    // Else quantity_cast must be explicitly called.
    template <typename T2,
              typename Scale2,
              typename Offset2,
              std::enable_if_t<is_implicitly_convertible<T2, Scale2>, int> = 0>
    constexpr quantity(const quantity<T2, Dim, Scale2, Offset2> &qty)
        : m_value(quantity_cast<quantity>(qty).value()) {}

    ~quantity() = default;

    quantity &operator=(const quantity &) = default;

    // Comparisons

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator==(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        constexpr auto lhs_factor =
            T(Scale::num * RhsScale::den * Offset::den * RhsOffset::den);
        constexpr auto lhs_offset =
            T(Offset::num * RhsScale::den * Scale::den * RhsOffset::den);
        constexpr auto rhs_factor =
            T(RhsScale::num * Scale::den * RhsOffset::den * Offset::den);
        constexpr auto rhs_offset =
            T(RhsOffset::num * Scale::den * RhsScale::den * Offset::den);

        return lhs_factor * m_value + lhs_offset ==
               rhs_factor * rhs.value() + rhs_offset;
    }

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator!=(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        return !(*this == rhs);
    }

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator<(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        constexpr auto lhs_factor =
            T(Scale::num * RhsScale::den * Offset::den * RhsOffset::den);
        constexpr auto lhs_offset =
            T(Offset::num * RhsScale::den * Scale::den * RhsOffset::den);
        constexpr auto rhs_factor =
            T(RhsScale::num * Scale::den * RhsOffset::den * Offset::den);
        constexpr auto rhs_offset =
            T(RhsOffset::num * Scale::den * RhsScale::den * Offset::den);

        return lhs_factor * m_value + lhs_offset <
               rhs_factor * rhs.value() + rhs_offset;
    }

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator<=(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        return !(rhs < *this);
    }

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator>(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        return rhs < *this;
    }

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator>=(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        return !(*this < rhs);
    }

    // Arithmetic

    constexpr auto &operator+=(const quantity &rhs) {
        m_value += rhs.m_value;
        return *this;
    }

    constexpr auto &operator-=(const quantity &rhs) {
        m_value -= rhs.m_value;
        return *this;
    }

    constexpr auto &operator*=(const T &rhs) {
        m_value *= rhs;
        return *this;
    }

    constexpr auto &operator/=(const T &rhs) {
        m_value /= rhs;
        return *this;
    }

    constexpr auto operator+() const { return *this; }
    constexpr auto operator-() const { return quantity(-m_value); }

    constexpr const quantity operator++(int) { return quantity(m_value++); }

    constexpr quantity &operator++() {
        ++m_value;
        return *this;
    }

    constexpr const quantity operator--(int) { return quantity(m_value--); }

    constexpr quantity &operator--() {
        --m_value;
        return *this;
    }

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator+(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        using cq =
            common_quantity_t<T, Dim, Scale, RhsScale, Offset, RhsOffset>;
        return cq(cq(*this).value() + cq(rhs).value());
    }

    template <typename RhsScale, typename RhsOffset>
    constexpr auto
    operator-(const quantity<T, Dim, RhsScale, RhsOffset> &rhs) const {
        using cq =
            common_quantity_t<T, Dim, Scale, RhsScale, Offset, RhsOffset>;
        return cq(cq(*this).value() - cq(rhs).value());
    }

    template <typename RhsDim, typename RhsScale>
    constexpr auto operator*(const quantity<T, RhsDim, RhsScale> &rhs) const {
        using DimMul = dimension_multiply<Dim, RhsDim>;
        using ScalMul = scale_multiply<Scale, RhsScale>;
        return quantity<T, DimMul, ScalMul>(m_value * rhs.value());
    }

    template <typename RhsDim, typename RhsScale>
    constexpr auto operator/(const quantity<T, RhsDim, RhsScale> &rhs) const {
        using DimDiv = dimension_divide<Dim, RhsDim>;
        using ScalDiv = scale_divide<Scale, RhsScale>;
        return quantity<T, DimDiv, ScalDiv>(m_value / rhs.value());
    }

    constexpr auto inv() const {
        using DimInv = dimension_divide<dimension<std::ratio<1>>, Dim>;
        using ScaleInv = scale_divide<scale<std::ratio<1>>, Scale>;
        return quantity<T, DimInv, ScaleInv>(T{1} / m_value);
    }

    constexpr auto value() const { return m_value; }

    // Return the value of a quantity in the base unit
    constexpr auto eval() const {
        constexpr auto factor = arithmetic::eval<T>(Scale());
        constexpr auto off = T(Offset::num) / T(Offset::den);
        return factor * m_value + off;
    }

  private:
    T m_value;
};

template <typename T1,
          typename T2,
          typename Dim,
          typename Scale,
          typename Offset,
          meta::disable_if_iterable<T1> = 0,
          meta::disable_if_complex<T1> = 0>
constexpr auto operator*(T1 factor,
                         const quantity<T2, Dim, Scale, Offset> &rhs) {
    using T = decltype(std::declval<T1>() * std::declval<T2>());
    return quantity<T, Dim, Scale, Offset>(T(factor) * T(rhs.value()));
}

template <typename T1,
          typename T2,
          typename Dim,
          typename Scale,
          typename Offset,
          meta::disable_if_iterable<T2> = 0,
          meta::disable_if_complex<T2> = 0>
constexpr auto operator*(const quantity<T1, Dim, Scale, Offset> &rhs,
                         T2 factor) {
    return factor * rhs;
}

template <typename T1,
          typename T2,
          typename Dim,
          typename Scale,
          typename Offset,
          meta::disable_if_iterable<T1> = 0>
constexpr auto operator/(T1 factor,
                         const quantity<T2, Dim, Scale, Offset> &rhs) {
    return factor * rhs.inv();
}

template <typename T1,
          typename T2,
          typename Dim,
          typename Scale,
          typename Offset,
          meta::disable_if_iterable<T2> = 0,
          meta::disable_if_complex<T2> = 0>
constexpr auto operator/(const quantity<T1, Dim, Scale, Offset> &rhs,
                         T2 factor) {
    return rhs * (T2{1} / factor);
}

template <typename T>
constexpr auto value(T x) {
    if constexpr (meta::is_complex_v<std::decay_t<T>>) {
        return std::complex(value(x.real()), value(x.imag()));
    } else if constexpr (is_quantity_v<T>) {
        return x.value();
    } else {
        return x;
    }
}

template <intmax_t Root, typename T>
auto root(T x) {
    static_assert(Root > 0);

    if constexpr (is_quantity_v<T>) {
        using rept_t = typename T::value_type;
        using DimRoot = dimension_root<typename T::dim, Root>;
        using ScalRoot = scale_root<typename T::scal, Root>;
        return quantity<rept_t, DimRoot, ScalRoot>(
            std::pow(value(x), rept_t{1} / rept_t(Root)));
    } else {
        return std::pow(x, T{1} / T(Root));
    }
}

// To debug
template <typename T, typename Dim, typename Scale, typename Offset>
void print(const quantity<T, Dim, Scale, Offset> &q) {
    std::printf("%f x (%li / %li) + (%li / %li) = %f [%li, %li]\n",
                q.value(),
                Scale::num,
                Scale::den,
                Offset::num,
                Offset::den,
                q.value() * (T(Scale::num) / T(Scale::den)) +
                    T(Offset::num) / T(Offset::den),
                Dim::num,
                Dim::den);
}

// representation_t: Get the representation type of a quantity

namespace detail {

template <class T>
struct representation_type_impl {
    using type = T;
};

template <typename T, typename Dim, typename Scale, typename Offset>
struct representation_type_impl<quantity<T, Dim, Scale, Offset>> {
    using type = T;
};

template <typename T, typename Dim, typename Scale, typename Offset>
struct representation_type_impl<std::complex<quantity<T, Dim, Scale, Offset>>> {
    using type = std::complex<T>;
};

} // namespace detail

template <class T>
using representation_t = typename detail::representation_type_impl<T>::type;

// is_same_dimension
template <class Qty1, class Qty2>
constexpr bool is_same_dimension =
    std::is_same_v<typename Qty1::dim, typename Qty2::dim>;

// Quantities type operations

template <typename Quantity1, typename Quantity2>
using quantity_multiply =
    decltype(std::declval<Quantity1>() * std::declval<Quantity2>());

template <typename Quantity1, typename Quantity2>
using quantity_divide =
    decltype(std::declval<Quantity1>() / std::declval<Quantity2>());

template <typename Quantity>
using quantity_invert = decltype(std::declval<Quantity>().inv());

template <intmax_t Root, typename Quantity>
using quantity_root = decltype(root<Root>(std::declval<Quantity>()));

// ----------------------------------------------------------------------------
// base_dimension
// ----------------------------------------------------------------------------

template <intmax_t PrimeNumber>
using base_dimension = dimension<std::ratio<PrimeNumber>>;

namespace detail {

template <std::size_t N>
struct dimensional_system_impl {
    template <std::size_t... I>
    static constexpr auto
    make_base_dimension_tuple(std::index_sequence<I...> /*unused*/) {
        constexpr auto primes = arithmetic::prime_list<N>().values();
        return std::make_tuple(base_dimension<std::get<I>(primes)>()...);
    }

    using type =
        decltype(make_base_dimension_tuple(std::make_index_sequence<N>{}));
};

} // namespace detail

template <std::size_t N>
using dimensional_system = typename detail::dimensional_system_impl<N>::type;

template <std::size_t I, typename DimSyst>
using get_base_dimension = std::tuple_element_t<I, DimSyst>;

// TODO Append new dimension to dimensional_system

// ----------------------------------------------------------------------------
// base_quantity
// ----------------------------------------------------------------------------

template <typename T,
          typename BaseDim,
          typename Scale,
          typename Offset = std::ratio<0>>
using base_quantity = quantity<T, BaseDim, Scale, Offset>;

template <std::size_t I,
          typename DimSyst,
          typename T,
          typename Scale,
          typename Offset = std::ratio<0>>
using get_base_quantity =
    base_quantity<T, get_base_dimension<I, DimSyst>, Scale, Offset>;

// ----------------------------------------------------------------------------
// Specializations for std::complex
// ----------------------------------------------------------------------------

// Operator *

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator*(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    using ret_t =
        std::complex<quantity_multiply<quantity<T1, Dim1, Scale1, Offset1>,
                                       quantity<T1, Dim1, Scale1, Offset1>>>;
    return ret_t(value(factor) * value(rhs));
}

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          typename T2,
          typename Dim2,
          typename Scale2,
          typename Offset2,
          meta::disable_if_iterable<T2> = 0>
constexpr auto
operator*(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const std::complex<quantity<T2, Dim2, Scale2, Offset2>> &factor) {
    using ret_t =
        std::complex<quantity_multiply<quantity<T1, Dim1, Scale1, Offset1>,
                                       quantity<T2, Dim2, Scale2, Offset2>>>;
    return ret_t(value(factor) * value(rhs));
}

template <typename Tp,
          typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          disable_if_is_quantity<Tp> = 0,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator*(const Tp &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    return std::complex<quantity<T1, Dim1, Scale1, Offset1>>(value(factor) *
                                                             rhs);
}

template <typename Tp,
          typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          disable_if_is_quantity<Tp> = 0,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator*(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const Tp &factor) {
    return std::complex<quantity<T1, Dim1, Scale1, Offset1>>(value(rhs) *
                                                             factor);
}

template <typename Qty,
          typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          enable_if_is_quantity<Qty> = 0,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator*(const Qty &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    return std::complex<
        quantity_multiply<Qty, quantity<T1, Dim1, Scale1, Offset1>>>(
        value(factor) * value(rhs));
}

template <typename Qty,
          typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          enable_if_is_quantity<Qty> = 0,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator*(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const Qty &factor) {
    return std::complex<
        quantity_multiply<Qty, quantity<T1, Dim1, Scale1, Offset1>>>(
        value(factor) * value(rhs));
}

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator*(const quantity<T1, Dim1, Scale1, Offset1> &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    return std::complex<quantity_multiply<quantity<T1, Dim1, Scale1, Offset1>,
                                          quantity<T1, Dim1, Scale1, Offset1>>>(
        value(factor) * value(rhs));
}

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator*(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const quantity<T1, Dim1, Scale1, Offset1> &factor) {
    return std::complex<quantity_multiply<quantity<T1, Dim1, Scale1, Offset1>,
                                          quantity<T1, Dim1, Scale1, Offset1>>>(
        value(factor) * value(rhs));
}

// Operator /

template <typename Tp,
          typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          disable_if_is_quantity<Tp> = 0,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator/(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const Tp &factor) {
    return std::complex<quantity<T1, Dim1, Scale1, Offset1>>(value(rhs) /
                                                             factor);
}

template <typename Tp,
          typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          disable_if_is_quantity<Tp> = 0,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator/(const Tp &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    return std::complex<quantity_invert<quantity<T1, Dim1, Scale1, Offset1>>>(
        rhs / value(factor));
}

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator/(const quantity<T1, Dim1, Scale1, Offset1> &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    return std::complex<quantity_divide<quantity<T1, Dim1, Scale1, Offset1>,
                                        quantity<T1, Dim1, Scale1, Offset1>>>(
        value(rhs) / value(factor));
}

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator/(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const quantity<T1, Dim1, Scale1, Offset1> &factor) {
    return std::complex<quantity_divide<quantity<T1, Dim1, Scale1, Offset1>,
                                        quantity<T1, Dim1, Scale1, Offset1>>>(
        value(rhs) / value(factor));
}

template <typename Qty,
          typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          enable_if_is_quantity<Qty> = 0,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator/(const Qty &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    return std::complex<
        quantity_divide<Qty, quantity<T1, Dim1, Scale1, Offset1>>>(
        value(rhs) / value(factor));
}

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          meta::disable_if_iterable<T1> = 0>
constexpr auto
operator/(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &factor) {
    using ret_t =
        std::complex<quantity_divide<quantity<T1, Dim1, Scale1, Offset1>,
                                     quantity<T1, Dim1, Scale1, Offset1>>>;
    return ret_t(value(rhs) / value(factor));
}

template <typename T1,
          typename Dim1,
          typename Scale1,
          typename Offset1,
          typename T2,
          typename Dim2,
          typename Scale2,
          typename Offset2,
          meta::disable_if_iterable<T2> = 0>
constexpr auto
operator/(const std::complex<quantity<T1, Dim1, Scale1, Offset1>> &rhs,
          const std::complex<quantity<T2, Dim2, Scale2, Offset2>> &factor) {
    using ret_t =
        std::complex<quantity_divide<quantity<T1, Dim1, Scale1, Offset1>,
                                     quantity<T2, Dim2, Scale2, Offset2>>>;
    return ret_t(value(rhs) / value(factor));
}

} // namespace scicpp::units

#endif // SCICPP_CORE_UNITS_QUANTITY