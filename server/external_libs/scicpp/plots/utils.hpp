// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2023 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_PLOTS_UTILS
#define SCICPP_PLOTS_UTILS

#include "scicpp/core/macros.hpp"

namespace scicpp::plots {

//---------------------------------------------------------------------------------
// Matplotlib markers to Gnuplot point types
//---------------------------------------------------------------------------------

[[nodiscard]] scicpp_pure inline auto marker(char c) {
    switch (c) {
    case '.':
        return 0;
    case '+':
        return 1;
    case 'x':
        return 2;
    case 's':
        return 5;
    case 'o':
        return 7;
    case '^':
        return 9;
    case 'v':
        return 11;
    case 'D':
        return 13;
    default: // Unknown symbol
        return 0;
    }
}

} // namespace scicpp::plots

#endif // SCICPP_PLOTS_UTILS