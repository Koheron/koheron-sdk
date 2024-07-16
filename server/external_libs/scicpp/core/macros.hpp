// SPDX-License-Identifier: MIT
// Copyright (c) 2019-2021 Thomas Vanderbruggen <th.vanderbruggen@gmail.com>

#ifndef SCICPP_MACROS
#define SCICPP_MACROS

#include <cassert>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace scicpp {

#ifdef NDEBUG
#define scicpp_unreachable __builtin_unreachable()
#else
#define scicpp_unreachable assert(false)
#endif

//https://blog.regehr.org/archives/1096
#ifdef NDEBUG
#define scicpp_require(expr)                                                   \
    ((expr) ? static_cast<void>(0) : __builtin_unreachable())
#else
#define scicpp_require(expr) assert(expr)
#endif

#define scicpp_pure __attribute__((pure))
#define scicpp_const __attribute__((const))

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

// Define a signed integer for sizes
using signed_size_t = std::make_signed_t<std::size_t>;

} // namespace scicpp

#endif // SCICPP_MACROS