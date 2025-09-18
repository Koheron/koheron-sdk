/// Serialize and deserialize binary buffers
///
/// (c) Koheron

#ifndef __SERIALIZER_DESERIALIZER_HPP__
#define __SERIALIZER_DESERIALIZER_HPP__

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <vector>
#include <string>
#include <string_view>
#include <complex>
#include <tuple>
#include <type_traits>
#include <span>
#include <ranges>

namespace koheron {

//------------------------------------------------------------------------------
// endian helpers (big-endian wire format)
//------------------------------------------------------------------------------

constexpr bool host_is_be = (std::endian::native == std::endian::big);

constexpr uint16_t bswap16(uint16_t x) noexcept { return __builtin_bswap16(x); }
constexpr uint32_t bswap32(uint32_t x) noexcept { return __builtin_bswap32(x); }
constexpr uint64_t bswap64(uint64_t x) noexcept { return __builtin_bswap64(x); }

template<class T>
constexpr T to_be(T v) noexcept {
    if constexpr (sizeof(T) == 1) return v;
    else if constexpr (sizeof(T) == 2) return host_is_be ? v : static_cast<T>(bswap16(static_cast<uint16_t>(v)));
    else if constexpr (sizeof(T) == 4) return host_is_be ? v : static_cast<T>(bswap32(static_cast<uint32_t>(v)));
    else if constexpr (sizeof(T) == 8) return host_is_be ? v : static_cast<T>(bswap64(static_cast<uint64_t>(v)));
    else static_assert(sizeof(T) <= 8, "Unsupported size");
}
template<class T>
constexpr T from_be(T v) noexcept { return to_be(v); }

//------------------------------------------------------------------------------
// size_of<T>: byte size on the wire for scalar-ish types
//------------------------------------------------------------------------------

template<class T, std::size_t N = 1>
inline constexpr std::size_t size_of = sizeof(T) * N;

template<class T> struct is_std_complex : std::false_type {};
template<class U> struct is_std_complex<std::complex<U>> : std::true_type {};
template<class T> inline constexpr bool is_std_complex_v = is_std_complex<std::remove_cv_t<T>>::value;

template<> inline constexpr std::size_t size_of<std::complex<float>>  = 2 * sizeof(float);
template<> inline constexpr std::size_t size_of<std::complex<double>> = 2 * sizeof(double);

//------------------------------------------------------------------------------
// extract<T>/append<T> in big-endian, using bit_cast and generic code
//------------------------------------------------------------------------------

template<class T> requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
inline T extract(const char* p) {
    if constexpr (sizeof(T) == 1) return static_cast<unsigned char>(p[0]);
    T v{};
    std::memcpy(&v, p, sizeof(T));
    return from_be(v);
}

template<class T> requires (std::is_integral_v<T> && std::is_signed_v<T>)
inline T extract(const char* p) {
    using U = std::make_unsigned_t<T>;
    U u = extract<U>(p);
    return static_cast<T>(u);
}

template<class T> requires (std::is_floating_point_v<T>)
inline T extract(const char* p) {
    if constexpr (sizeof(T) == 4) {
        auto u = extract<uint32_t>(p);
        return std::bit_cast<float>(u);
    } else {
        auto u = extract<uint64_t>(p);
        return std::bit_cast<double>(u);
    }
}

template<class T> requires is_std_complex_v<T>
inline T extract(const char* p) {
    using V = typename T::value_type;
    V r = extract<V>(p);
    V i = extract<V>(p + sizeof(V));
    return T{r,i};
}

template<>
inline bool extract<bool>(const char* p) {
    return static_cast<unsigned char>(p[0]) == 1;
}

// ---- append ----

template<class T> requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
inline void append(unsigned char* p, T v) {
    if constexpr (sizeof(T) == 1) {
        p[0] = static_cast<unsigned char>(v);
        return;
    }

    v = to_be(v);
    std::memcpy(p, &v, sizeof(T));
}

template<class T> requires (std::is_integral_v<T> && std::is_signed_v<T>)
inline void append(unsigned char* p, T v) {
    using U = std::make_unsigned_t<T>;
    append<U>(p, static_cast<U>(v));
}

template<class T> requires (std::is_floating_point_v<T>)
inline void append(unsigned char* p, T v) {
    if constexpr (sizeof(T) == 4) {
        append<uint32_t>(p, std::bit_cast<uint32_t>(static_cast<float>(v)));
    } else {
        append<uint64_t>(p, std::bit_cast<uint64_t>(static_cast<double>(v)));
    }
}

template<class T> requires is_std_complex_v<T>
inline void append(unsigned char* p, T v) {
    using V = typename T::value_type;
    append<V>(p, v.real());
    append<V>(p + sizeof(V), v.imag());
}

template<>
inline void append<bool>(unsigned char* p, bool v) {
    p[0] = v ? 1u : 0u;
}

//------------------------------------------------------------------------------
// tuple serialization / deserialization (constexpr-friendly)
//------------------------------------------------------------------------------

namespace detail {

template<std::size_t pos, class T0, class... Ts>
inline std::tuple<T0, Ts...> deser_fold(const char* buff) {
    if constexpr (sizeof...(Ts) == 0) {
        return std::tuple<T0>{ extract<T0>(buff + pos) };
    } else {
        return std::tuple_cat(
            std::tuple<T0>{ extract<T0>(buff + pos) },
            deser_fold<pos + size_of<T0>, Ts...>(buff)
        );
    }
}

} // namespace detail

template<std::size_t pos, class... Ts>
inline std::tuple<Ts...> deserialize(const char* buff) {
    if constexpr (sizeof...(Ts) == 0) return {};
    return detail::deser_fold<pos, Ts...>(buff);
}

template<class... Ts>
constexpr std::size_t required_buffer_size() {
    return (size_of<Ts> + ... + 0u);
}

// Serialize tuple => std::array<uint8_t, N>
namespace detail {
template<std::size_t buff_pos, std::size_t I, class... Ts>
inline void serialize_tuple(const std::tuple<Ts...>& t, unsigned char* buff) {
    if constexpr (I < sizeof...(Ts)) {
        using T = std::tuple_element_t<I, std::tuple<Ts...>>;
        append<T>(buff + buff_pos, std::get<I>(t));
        serialize_tuple<buff_pos + size_of<T>, I + 1, Ts...>(t, buff);
    }
}
} // namespace detail

template<class... Ts>
inline std::array<unsigned char, required_buffer_size<Ts...>()>
serialize(const std::tuple<Ts...>& t) {
    std::array<unsigned char, required_buffer_size<Ts...>()> arr{};
    detail::serialize_tuple<0, 0, Ts...>(t, arr.data());
    return arr;
}

template<class T> struct is_std_tuple : std::false_type {};
template<class... U> struct is_std_tuple<std::tuple<U...>> : std::true_type {};
template<class T> inline constexpr bool is_std_tuple_v = is_std_tuple<std::remove_cv_t<T>>::value;

template<class... Ts>
requires ( (!is_std_tuple_v<std::remove_cvref_t<Ts>>) && ... )
inline std::array<unsigned char, required_buffer_size<std::remove_cvref_t<Ts>...>()>
serialize(Ts&&... ts) {
    auto t = std::tuple<std::remove_cvref_t<Ts>...>(std::forward<Ts>(ts)...);
    return serialize(t);
}

//------------------------------------------------------------------------------
// Dynamic command builder (replaces DynamicSerializer) with C++20 concepts
//------------------------------------------------------------------------------

// concepts
template<class T>
concept ContiguousContainer =
    requires(T t) {
        typename T::value_type;
        { t.data() } -> std::convertible_to<const typename T::value_type*>;
        { t.size() } -> std::convertible_to<std::size_t>;
    };

template<class T>
concept StdArray = requires {
    typename T::value_type;
    std::tuple_size<T>::value; // std::array satisfies
};

template<class T>
concept CStrLike =
    // char* / const char*
    (std::is_pointer_v<std::remove_reference_t<T>> &&
     std::is_same_v<std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>, char>) ||
    // char[N] / const char[N]
    (std::is_array_v<std::remove_reference_t<T>> &&
     std::is_same_v<std::remove_cv_t<std::remove_extent_t<std::remove_reference_t<T>>>, char>);

template<class T>
concept ScalarLike = (std::is_scalar_v<std::remove_reference_t<T>> && !std::is_pointer_v<std::remove_reference_t<T>>) || is_std_complex_v<T>;

// packer that batches scalars, flushing before contiguous payloads
struct CommandBuilder {
    std::vector<unsigned char>* out = nullptr;

    void reset_into(std::vector<unsigned char>& dst) noexcept {
        out = &dst;
        out->clear();
    }

    // Append N bytes and return a pointer to them
    unsigned char* append_raw(std::size_t n) {
        auto old = out->size();
        out->resize(old + n);
        return out->data() + old;
    }

    template<class T> requires ((std::is_integral_v<T> && std::is_unsigned_v<T>))
    void push_scalar(T v) {
        if constexpr (sizeof(T) == 1) {
            *append_raw(1) = static_cast<unsigned char>(v);
        } else {
            v = to_be(v);
            std::memcpy(append_raw(sizeof(T)), &v, sizeof(T));
        }
    }

    template<class T> requires ((std::is_integral_v<T> && std::is_signed_v<T>))
    void push_scalar(T v) { push_scalar<std::make_unsigned_t<T>>(static_cast<std::make_unsigned_t<T>>(v)); }

    template<class T> requires (std::is_floating_point_v<T>)
    void push_scalar(T v) {
        if constexpr (sizeof(T) == 4) {
            push_scalar<std::uint32_t>(std::bit_cast<std::uint32_t>(static_cast<float>(v)));
        } else {
            push_scalar<std::uint64_t>(std::bit_cast<std::uint64_t>(static_cast<double>(v)));
        }
    }

    template<class T> requires is_std_complex_v<T>
    void push_scalar(const T& v) {
        using V = typename T::value_type;
        push_scalar<V>(v.real());
        push_scalar<V>(v.imag());
    }

    // header (length filled in finalize)
    void write_header(std::uint16_t class_id, std::uint16_t func_id) {
        auto p = append_raw(4 + 2 + 2);
        // 4 bytes placeholder
        append<uint16_t>(p + 4, class_id);
        append<uint16_t>(p + 6, func_id);
    }

    void finalize() noexcept {
        const std::size_t payload = out->size() - 4;
        // On 32-bit ARM this is already uint32_t-sized
        append<uint32_t>(out->data(), static_cast<std::uint32_t>(payload));
    }

    // containers
    template<class C>
    void push_container(const C& c) {
        using V = typename C::value_type;
        const std::uint32_t nbytes = static_cast<std::uint32_t>(c.size() * sizeof(V));
        append<uint32_t>(append_raw(4), nbytes);
        if (nbytes) {
            std::memcpy(append_raw(nbytes), c.data(), nbytes);
        }
    }

    template<class A> // std::array
    void push_array(const A& a) {
        using V = typename A::value_type;
        constexpr std::size_t nbytes = std::tuple_size<A>::value * sizeof(V);
        if constexpr (nbytes) {
            std::memcpy(append_raw(nbytes), a.data(), nbytes);
        }
    }

    // c-string like (no std::string temp)
    template<std::size_t N>
    void push_cstr(const char (&arr)[N]) {
        const std::string_view sv(arr, N - 1);
        append<uint32_t>(append_raw(4), sv.size());
        if (!sv.empty()) {
            std::memcpy(append_raw(sv.size()), sv.data(), sv.size());
        }
    }
    void push_cstr(const char* s) {
        const auto len = std::char_traits<char>::length(s);
        append<uint32_t>(append_raw(4), len);
        if (len) {
            std::memcpy(append_raw(len), s, len);
        }
    }

    // tuple expansion
    template<class Tup>
    void push_tuple(Tup&& tup) {
        std::apply([&](auto&&... elems) {
            (push_one(std::forward<decltype(elems)>(elems)), ...);
         }, std::forward<Tup>(tup));
    }

    // dispatcher
    template<class T>
    void push_one(T&& v) {
        using U = std::remove_cvref_t<T>;

        if constexpr (ScalarLike<U>) {
            push_scalar(std::forward<T>(v));
        } else if constexpr (is_std_tuple_v<U>) {
            push_tuple(std::forward<T>(v));
        } else if constexpr (StdArray<U>) {
            push_array(v);
        } else if constexpr (std::is_pointer_v<U> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
            push_cstr(v);
        } else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_cv_t<std::remove_extent_t<U>>, char>) {
            push_cstr(v);
        } else {
            push_container(v);
        }
    }

    template<class... Args>
    void push(Args&&... args) { (push_one(std::forward<Args>(args)), ...); }
};

} // namespace koheron

#endif // __SERIALIZER_DESERIALIZER_HPP__