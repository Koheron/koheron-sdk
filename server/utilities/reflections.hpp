#ifndef __SERVER_UTILITIES_REFLECTIONS_HPP__
#define __SERVER_UTILITIES_REFLECTIONS_HPP__

#include <string_view>

// ----------------------------------------------------------
// Type name

template<class T>
consteval std::string_view type_name() {
    std::string_view p = __PRETTY_FUNCTION__;
    // e.g. "consteval std::string_view ... type_name() [with T = ns::Type; std::string_view = std::basic_string_view<char>]"
    constexpr std::string_view key = "T = ";
    auto from = p.find(key);

    if (from == std::string_view::npos) {
        return {};
    }

    from += key.size();
    auto to = p.find_first_of(";]", from); // stop before alias note or closing bracket
    if (to == std::string_view::npos) {
        to = p.size();
    }

    return p.substr(from, to - from);
}

// Keep only the last component after '::'
template<class T>
consteval std::string_view short_type_name() {
    auto s = type_name<T>();
    auto pos = s.rfind("::");
    return (pos == std::string_view::npos) ? s : s.substr(pos + 2);
}

// ----------------------------------------------------------
// Pointer member function (PMF) names

// Get "Class::method" from a pointer-to-member function at compile time
template<auto PMF>
consteval std::string_view pmf_full_name() {
    std::string_view p = __PRETTY_FUNCTION__;
    constexpr std::string_view key = "PMF = ";
    auto from = p.find(key);

    if (from == std::string_view::npos) {
        return {};
    }

    from += key.size();

    if (from < p.size() && p[from] == '&') { // GCC prints leading '&'
        ++from;
    }

    auto to = p.find(';', from); // GCC puts a ';' before template param dump

    if (to == std::string_view::npos) {
        to = p.find(']', from);
    }

    return p.substr(from, to - from);
}

consteval std::string_view pmf_unqualified(std::string_view full) {
    auto pos = full.rfind("::");
    return (pos == std::string_view::npos) ? full : full.substr(pos + 2);
}

consteval std::string_view pmf_class(std::string_view full) {
    auto pos = full.rfind("::");
    return (pos == std::string_view::npos) ? std::string_view{} : full.substr(0, pos);
}

// ----------------------------------------------------------
// Tests
//
// __PRETTY_FUNCTION__ is not a guaranteed stable interface.
// These tests aim to exercise the specific substrings we parse.

namespace test {

struct Foo {
    void f();
    int  g(int) const;
    void overloaded();
    void overloaded(int);
    template<class T> void templ();
    int  operator()(int) const;
};

struct Outer {
    struct Inner {
        void ping();
    };
};

} // namespace test

// --- type_name / short_type_name ---
static_assert(type_name<test::Foo>() == "test::Foo");
static_assert(short_type_name<test::Foo>() == "Foo");
static_assert(type_name<test::Outer::Inner>() == "test::Outer::Inner");
static_assert(short_type_name<test::Outer::Inner>() == "Inner");

// --- pmf_full_name basic (non-const) ---
static_assert(pmf_full_name<&test::Foo::f>().substr(0, /* "test::Foo::".size() */ 11) == "test::Foo::");
static_assert(pmf_unqualified(pmf_full_name<&test::Foo::f>()) == "f");
static_assert(pmf_class(pmf_full_name<&test::Foo::f>()) == "test::Foo");

// --- pmf_full_name const member ---
static_assert(pmf_unqualified(pmf_full_name<&test::Foo::g>()) == "g");
static_assert(pmf_class(pmf_full_name<&test::Foo::g>()) == "test::Foo");

// --- pmf_full_name for operator() ---

// Disambiguate operator() with an explicit PMF type
using CallConst = int (test::Foo::*)(int) const;

static_assert(
    pmf_unqualified(
        pmf_full_name<&test::Foo::operator()>()
    ) == "operator()"
);

static_assert(
    pmf_class(
        pmf_full_name<&test::Foo::operator()>()
    ) == "test::Foo"
);

// --- overloads: disambiguate with static_cast ---
static_assert(
    pmf_unqualified(
        pmf_full_name< static_cast<void (test::Foo::*)()>(&test::Foo::overloaded) >()
    ) == "overloaded"
);

static_assert(
    pmf_unqualified(
        pmf_full_name< static_cast<void (test::Foo::*)(int)>(&test::Foo::overloaded) >()
    ) == "overloaded"
);

static_assert(
    pmf_class(
        pmf_full_name< static_cast<void (test::Foo::*)(int)>(&test::Foo::overloaded) >()
    ) == "test::Foo"
);

// --- template member: use a template-id to form a PMF ---
static_assert(
    pmf_unqualified( pmf_full_name<&test::Foo::templ<int>>()) == "templ"
);

static_assert(
    pmf_class( pmf_full_name<&test::Foo::templ<int>>()) == "test::Foo"
);

// --- nested class member ---
static_assert(
    pmf_full_name<&test::Outer::Inner::ping>().substr(0,  /* "test::Outer::Inner::".size() */ 20)
    == "test::Outer::Inner::"
);

static_assert(
    pmf_unqualified(pmf_full_name<&test::Outer::Inner::ping>()) == "ping"
);

static_assert(
    pmf_class(pmf_full_name<&test::Outer::Inner::ping>()) == "test::Outer::Inner"
);

#endif // __SERVER_UTILITIES_REFLECTIONS_HPP__
