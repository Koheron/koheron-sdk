// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_UNITS_UTILITIES
#define SCICPP_CORE_UNITS_UTILITIES

#if SCICPP_HAS_UNITS

#include "scicpp/core/units/quantity.hpp"

#include <array>
#include <complex>
#include <deque>
#include <list>
#include <optional>
#include <span>
#include <tuple>
#include <variant>
#include <vector>

namespace scicpp::units {

// ----------------------------------------------------------------------------
// strip_units
// ----------------------------------------------------------------------------

namespace detail {

// --- cv/ref copier ---
template <class From, class To>
struct copy_cvref;
template <class From, class To>
using copy_cvref_t = typename copy_cvref<From, To>::type;

template <class From, class To>
struct copy_cvref {
    using U0 = std::remove_reference_t<To>;
    using U1 =
        std::conditional_t<std::is_const_v<std::remove_reference_t<From>>,
                           const U0,
                           U0>;
    using U2 =
        std::conditional_t<std::is_volatile_v<std::remove_reference_t<From>>,
                           volatile U1,
                           U1>;
    using type = std::conditional_t<
        std::is_lvalue_reference_v<From>,
        U2 &,
        std::conditional_t<std::is_rvalue_reference_v<From>, U2 &&, U2>>;
};

// copy cv (ignoring references) from From to To
template <class From, class To>
struct copy_cv_qual {
    using F = std::remove_reference_t<From>;
    using C = std::conditional_t<std::is_const_v<F>, const To, To>;
    using V = std::conditional_t<std::is_volatile_v<F>, volatile C, C>;
    using type = V;
};

template <class From, class To>
using copy_cv_qual_t = typename copy_cv_qual<From, To>::type;

// primary (after decay)
template <class T, class = void>
struct strip_units_base {
    using type = T;
};

// quantity detection on the *decayed* type
template <class T>
struct strip_units_base<
    T,
    std::void_t<typename scicpp::units::representation_t<T>>> {
    using type = typename scicpp::units::representation_t<T>;
};

// ---- container / algebraic types (operate on decayed, recurse) ----
template <class T, std::size_t N>
struct strip_units_base<std::array<T, N>> {
    using type =
        std::array<typename strip_units_base<std::remove_cvref_t<T>>::type, N>;
};

template <class T>
struct strip_units_base<std::complex<T>> {
    using type =
        std::complex<typename strip_units_base<std::remove_cvref_t<T>>::type>;
};

template <class T, class Alloc>
struct strip_units_base<std::vector<T, Alloc>> {
    using U = typename strip_units_base<std::remove_cvref_t<T>>::type;
    using A2 = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;
    using type = std::vector<U, A2>;
};

template <class T, class Alloc>
struct strip_units_base<std::deque<T, Alloc>> {
    using U = typename strip_units_base<std::remove_cvref_t<T>>::type;
    using A2 = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;
    using type = std::deque<U, A2>;
};

template <class T, class Alloc>
struct strip_units_base<std::list<T, Alloc>> {
    using U = typename strip_units_base<std::remove_cvref_t<T>>::type;
    using A2 = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;
    using type = std::list<U, A2>;
};

template <class... Ts>
struct strip_units_base<std::variant<Ts...>> {
    using type = std::variant<copy_cv_qual_t<
        Ts,
        typename strip_units_base<std::remove_cvref_t<Ts>>::type>...>;
};

template <class T>
struct strip_units_base<std::optional<T>> {
    using U = typename strip_units_base<std::remove_cvref_t<T>>::type;
    using type = std::optional<U>;
};

template <class A, class B>
struct strip_units_base<std::pair<A, B>> {
    using type =
        std::pair<typename strip_units_base<std::remove_cvref_t<A>>::type,
                  typename strip_units_base<std::remove_cvref_t<B>>::type>;
};

template <class... Ts>
struct strip_units_base<std::tuple<Ts...>> {
    using type = std::tuple<copy_cv_qual_t<
        Ts,
        typename strip_units_base<std::remove_cvref_t<Ts>>::type>...>;
};

template <class T, std::size_t Extent>
struct strip_units_base<std::span<T, Extent>> {
    using U0 = typename strip_units_base<std::remove_cvref_t<T>>::type;
    using Uc = std::conditional_t<std::is_const_v<T>, const U0, U0>;
    using Uv = std::conditional_t<std::is_volatile_v<T>, volatile Uc, Uc>;
    using type = std::span<Uv, Extent>;
};

template <class T>
struct strip_units_base<std::reference_wrapper<T>> {
    using U = typename strip_units_base<std::remove_cvref_t<T>>::type;
    using type = std::reference_wrapper<U>;
};

template <class T>
struct strip_units_base<std::initializer_list<T>> {
    using type = std::initializer_list<
        typename strip_units_base<std::remove_cvref_t<T>>::type>;
};

// ---- facade that preserves cv/ref of the *original* T ----

template <class T>
struct strip_units {
    using decayed = std::remove_cvref_t<T>;
    using base = typename strip_units_base<decayed>::type;
    using type = copy_cvref_t<T, base>;
};

} // namespace detail

template <class T>
using strip_units_t = typename detail::strip_units<T>::type;

} // namespace scicpp::units

#else // !SCICPP_HAS_UNITS

namespace scicpp::units {

template <class T>
using strip_units = T;

} // namespace scicpp::units

#endif // SCICPP_HAS_UNITS

#endif // SCICPP_CORE_UNITS_UTILITIES