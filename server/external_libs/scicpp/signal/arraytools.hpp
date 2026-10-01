// SPDX-License-Identifier: MIT

#ifndef SCICPP_SIGNAL_ARRAYTOOLS
#define SCICPP_SIGNAL_ARRAYTOOLS

#include "scicpp/core/macros.hpp"
#include "scicpp/core/manips.hpp"
#include "scicpp/core/meta.hpp"
#include "scicpp/core/range.hpp"
#include "scicpp/core/units/quantity.hpp"

#include <vector>

namespace scicpp::signal {

// ----------------------------------------------------------------------------
// Boundary extension functions
// ----------------------------------------------------------------------------

template <meta::Iterable Array,
          typename DiffTp = typename Array::difference_type>
auto odd_ext(const Array &x, DiffTp n) {
    using T = typename Array::value_type;
    using raw_t = units::representation_t<T>;
    using namespace operators;

    const auto size = DiffTp(x.size());
    scicpp_require(n <= size - 1);

    if (n < 1) {
        return std::vector(x.cbegin(), x.cend());
    }

    const auto left_end = x[0] * raw_t(2);
    auto left_ext = slice_array(x, n, DiffTp(0), DiffTp(-1));
    const auto right_end = x.back() * raw_t(2);
    auto right_ext = slice_array(x, DiffTp(-2), -(n + 2), DiffTp(-1));

    return (left_end - left_ext) | x | (right_end - right_ext);
}

template <meta::Iterable Array,
          typename DiffTp = typename Array::difference_type>
auto even_ext(const Array &x, DiffTp n) {
    using namespace operators;

    scicpp_require(n <= DiffTp(x.size()) - 1);

    if (n < 1) {
        return std::vector(x.cbegin(), x.cend());
    }

    return slice_array(x, n, DiffTp(0), DiffTp(-1)) | x |
           slice_array(x, DiffTp(-2), -(n + 2), DiffTp(-1));
}

template <meta::Iterable Array,
          typename DiffTp = typename Array::difference_type>
auto const_ext(const Array &x, DiffTp n) {
    using T = typename Array::value_type;
    using raw_t = units::representation_t<T>;
    using namespace operators;

    scicpp_require(!x.empty());

    if (n < 1) {
        return std::vector(x.cbegin(), x.cend());
    }

    const auto padding = std::size_t(n);

    return (ones<raw_t>(padding) * x[0]) | x |
           (ones<raw_t>(padding) * x.back());
}

template <meta::Iterable Array,
          typename DiffTp = typename Array::difference_type>
auto zero_ext(const Array &x, DiffTp n) {
    using T = typename Array::value_type;
    using namespace operators;

    if (n < 1) {
        return std::vector(x.cbegin(), x.cend());
    }

    const auto padding = std::size_t(n);

    if (x.empty()) {
        return zeros<T>(2 * padding);
    }

    return zeros<T>(padding) | x | zeros<T>(padding);
}

} // namespace scicpp::signal

#endif // SCICPP_SIGNAL_ARRAYTOOLS
