// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_CORE_PRINT
#define SCICPP_CORE_PRINT

#include "scicpp/core/functional.hpp"
#include "scicpp/core/macros.hpp"
#include "scicpp/core/maths.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/stats.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace scicpp {

struct PrintOptions {
    std::string separator{" "};
    int precision{8};
    // Total number of array elements which trigger summarization rather than full repr
    std::size_t threshold{1000};
    // Number of characters per line for the purpose of inserting line breaks
    int linewidth{75};
    // Number of array items in summary at beginning and end of each dimension
    std::size_t edgeitems{3};
    // If true, always print floating point numbers using fixed point notation
    bool suppress{false};
    // String representation of floating point not-a-number
    std::string nanstr{"nan"};
    // String representation of floating point infinity
    std::string infstr{"inf"};
};

namespace detail {

struct ElementFormater {
    ElementFormater(const PrintOptions &prtopts, bool fixed, int width)
        : m_prtopts(prtopts), m_fixed(fixed), m_width(width) {
        if (fixed) {
            m_ss << std::fixed;
        } else {
            m_ss << std::scientific;
        }

        m_ss << std::setprecision(m_prtopts.precision);
        m_ss << std::setfill(' ');
    }

    void reset() {
        // https://stackoverflow.com/questions/7623650/resetting-a-stringstream
        m_ss.str(std::string());
        m_ss.clear();

        if (m_width >= 0) {
            m_ss << std::setw(m_width);
        }
    }

    template <typename Tuple>
    void print_tuple(Tuple tup) {
        m_ss << "(";
        const auto print_tuple_element = [&](auto x) {
            print_scalar_value(x);
            m_ss << ", ";
        };
        std::apply([&](auto... x) { (print_tuple_element(x), ...); },
                   meta::subtuple<1>(tup));
        print_scalar_value(std::get<std::tuple_size_v<Tuple> - 1>(tup));
        m_ss << ")";
    }

    template <typename T>
    bool is_negative(T value) const {
        return (std::fpclassify(value) == FP_ZERO &&
                std::signbit(value)) // Negative zero
               || value < T(0);
    }

    template <typename T>
    void print_complex(const std::complex<T> &z) {
        print_scalar_value(z.real());

        if (is_negative(z.imag())) {
            m_ss << "-";
            print_scalar_value(-z.imag());
        } else {
            m_ss << "+";
            print_scalar_value(z.imag());
        }

        m_ss << "j";
    }

    template <typename T>
    void print_scalar_value(T value) {
        if (units::isnan(value)) {
            m_ss << m_prtopts.nanstr;
        } else if (units::isinf(value)) {
            if (units::signbit(value)) {
                m_ss << "-" << m_prtopts.infstr;
            } else {
                m_ss << m_prtopts.infstr;
            }
        } else {
            m_ss << units::value(value);
        }
    }

    template <typename T>
    auto print_element(T value) {
        reset();

        if constexpr (meta::is_complex_v<T>) {
            print_complex(units::value(value));
        } else if constexpr (meta::is_std_tuple_v<T> ||
                             meta::is_std_pair_v<T>) {
            print_tuple(value);
        } else {
            print_scalar_value(value);
        }
    }

    auto static remove_trailling_zeros(std::string &str) {
        if (str != "0") {
            const auto first = str.find_last_not_of('0') + 1;
            str.replace(first, str.size(), str.size() - first, ' ');
        }
    }

    auto str() const {
        if (m_fixed) {
            auto str = m_ss.str();
            remove_trailling_zeros(str);
            return str;
        } else {
            return m_ss.str();
        }
    }

    const PrintOptions &m_prtopts;
    bool m_fixed;
    int m_width;

    std::ostringstream m_ss;
}; // class ElementFormater

template <class Array>
int get_width([[maybe_unused]] const Array &A, int fixed) {
    using T = typename Array::value_type;
    if constexpr (meta::is_std_tuple_v<T>) {
        return -1;
    } else if constexpr (meta::is_complex_v<T>) {
        if (!fixed) {
            return 15;
        } else {
            return 12;
        }
    } else {
        if (!fixed) {
            return 12;
        } else {
            return 12;
        }
    }
}

template <class Array>
bool use_scientific_notation(const Array &A) {
    // https://numpy.org/doc/stable/reference/generated/numpy.set_printoptions.html
    // Scientific notation is used when absolute value of the smallest number is < 1e-4
    // or the ratio of the maximum absolute value to the minimum is > 1e3.

    using T = typename Array::value_type;

    if constexpr (meta::is_complex_v<T>) {
        return use_scientific_notation(real(A)) ||
               use_scientific_notation(imag(A));
    } else if constexpr (meta::is_std_tuple_v<T> || meta::is_std_pair_v<T>) {
        return true;
    } else if constexpr (std::is_integral_v<T>) {
        return false;
    } else {
        const auto fabs = vectorize([](auto x) { return units::fabs(x); });
        const auto filtered_array = filter(fabs(A), [](auto x) {
            return filters::not_zero(x) && !units::isinf(x);
        });

        if (filtered_array.empty()) {
            return false;
        }

        const auto min = units::value(stats::amin(filtered_array));
        const auto max = units::value(stats::amax(filtered_array));
        return (min < 1E-4) || ((max / min) > 1E3);
    }
}

template <class Array>
auto get_edgeitems(const Array &A, std::size_t num_edgeitems) {
    return std::array{std::vector(A.cbegin(), A.cbegin() + int(num_edgeitems)),
                      std::vector(A.cend() - int(num_edgeitems), A.cend())};
}

template <class Stream, class Array>
int print_array_elements(Stream &stream,
                         const Array &A,
                         const PrintOptions &prtopts,
                         ElementFormater &fmter,
                         int pos) {
    const auto *sep = "";
    std::size_t sep_size = 0;

    for (const auto &a : A) {
        fmter.print_element(a);
        const auto str = fmter.str();
        stream << sep;
        pos += int(sep_size + str.size());

        if (pos >= prtopts.linewidth) {
            stream << "\n " << str;
            pos = 1 + int(str.size());
        } else {
            stream << str;
        }

        sep = prtopts.separator.c_str();
        sep_size = prtopts.separator.size();
    }

    return pos;
}

} // namespace detail

template <class Stream, class Array, meta::enable_if_iterable<Array> = 0>
void print(Stream &stream, const Array &A, const PrintOptions &prtopts) {
    if (A.empty()) {
        stream << "[]\n";
        return;
    }

    stream << "[";
    int pos = 1;

    if (A.size() <= prtopts.threshold) {
        const auto fixed =
            prtopts.suppress || !detail::use_scientific_notation(A);
        const auto width = detail::get_width(A, fixed);
        detail::ElementFormater fmter(prtopts, fixed, width);
        detail::print_array_elements(stream, A, prtopts, fmter, pos);
    } else {
        scicpp_require(A.size() > 2 * prtopts.edgeitems);

        const auto edgeitems = detail::get_edgeitems(A, prtopts.edgeitems);
        const auto scientific =
            detail::use_scientific_notation(std::get<0>(edgeitems)) ||
            detail::use_scientific_notation(std::get<1>(edgeitems));
        const auto fixed = prtopts.suppress || !scientific;
        const auto width = detail::get_width(A, fixed);
        detail::ElementFormater fmter(prtopts, fixed, width);
        pos = detail::print_array_elements(
            stream, std::get<0>(edgeitems), prtopts, fmter, pos);
        stream << prtopts.separator << "..." << prtopts.separator;
        pos += 3 + 2 * int(prtopts.separator.size());
        detail::print_array_elements(
            stream, std::get<1>(edgeitems), prtopts, fmter, pos);
    }

    stream << "]\n";
}

template <class Stream, class T>
void print(Stream &stream, const T &A) {
    if constexpr (meta::is_iterable_v<T>) {
        print(stream, A, PrintOptions{});
    } else {
        detail::ElementFormater fmter(PrintOptions{}, false, 12);
        fmter.print_element(A);
        stream << fmter.str() << "\n";
    }
}

template <class T>
void print(const T &A) {
    print(std::cout, A);
}

template <class Array, meta::enable_if_iterable<Array> = 0>
auto array2string(const Array &A,
                  const PrintOptions &prtopts = PrintOptions{}) {
    std::ostringstream ss;
    print(ss, A, prtopts);
    return ss.str();
}

} // namespace scicpp

#endif // SCICPP_CORE_PRINT