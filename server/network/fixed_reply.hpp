#ifndef KOHERON_FIXED_REPLY_HPP
#define KOHERON_FIXED_REPLY_HPP

#include "server/network/serializer_deserializer.hpp"

namespace net::detail {

constexpr std::size_t max_fixed_reply_bytes = 512;

// Zero denotes an unsupported type. Containers retain their existing payload
// paths; only small scalar replies and nested scalar tuples use stack storage.
template<class T>
consteval std::size_t fixed_reply_size() {
    using U = strip_units_t<std::remove_cvref_t<T>>;
    if constexpr (is_std_tuple_v<U>) {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            if constexpr (((fixed_reply_size<std::tuple_element_t<I, U>>() != 0) && ...))
                return (std::size_t{0} + ... + fixed_reply_size<std::tuple_element_t<I, U>>());
            else return std::size_t{0};
        }(std::make_index_sequence<std::tuple_size_v<U>>{});
    } else if constexpr (is_std_complex_v<U>) {
        return 2 * fixed_reply_size<typename U::value_type>();
    } else if constexpr (std::is_integral_v<U>) {
        return sizeof(U);
    } else if constexpr (std::is_floating_point_v<U>) {
        return sizeof(U) == 4 ? 4 : 8;
    } else {
        return 0;
    }
}

template<class T>
void append_fixed_reply(unsigned char*& output, const T& value) {
    using U = strip_units_t<std::remove_cvref_t<T>>;
    if constexpr (is_std_tuple_v<U>) {
        std::apply([&](const auto&... fields) { (append_fixed_reply(output, fields), ...); }, value);
    } else if constexpr (is_std_complex_v<U>) {
        append_fixed_reply(output, value.real());
        append_fixed_reply(output, value.imag());
    } else {
        // CommandBuilder serializes scalar quantities by their representation.
        append(output, scicpp::units::value(value));
        output += fixed_reply_size<T>();
    }
}

template<class... T>
auto fixed_reply(uint16_t driver, uint16_t operation, const T&... value) {
    constexpr auto size = 8 + (fixed_reply_size<T>() + ... + 0);
    std::array<unsigned char, size> bytes{};
    append<uint32_t>(bytes.data(), 0);
    append<uint16_t>(bytes.data() + 4, driver);
    append<uint16_t>(bytes.data() + 6, operation);
    auto* output = bytes.data() + 8;
    (append_fixed_reply(output, value), ...);
    return bytes;
}

} // namespace net::detail
#endif
