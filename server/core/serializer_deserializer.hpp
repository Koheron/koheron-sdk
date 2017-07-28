/// Serialize and deserialize binary buffers
///
/// (c) Koheron

#ifndef __SERIALIZER_DESERIALIZER_HPP__
#define __SERIALIZER_DESERIALIZER_HPP__

#include <cstring>
#include <tuple>
#include <array>
#include <vector>
#include <string>
#include <complex>

namespace koheron {

// http://stackoverflow.com/questions/17789928/whats-a-proper-way-of-type-punning-a-float-to-an-int-and-vice-versa
template <typename T, typename U>
inline T pseudo_cast(const U &x)
{
    T to = T(0);
    std::memcpy(&to, &x, (sizeof(T) < sizeof(U)) ? sizeof(T) : sizeof(U));
    return to;
}

// ------------------------
// Definitions
// ------------------------

template<typename Tp, size_t N = 1> constexpr size_t size_of = sizeof(Tp) * N;

template<typename Tp> Tp extract(const char *buff);                // Deserialization
template<typename Tp> void append(unsigned char *buff, Tp value);  // Serialization

// uint8_t

template<>
inline uint8_t extract<uint8_t>(const char *buff)
{
    return static_cast<unsigned char>(buff[0]);
}

template<>
inline void append<uint8_t>(unsigned char *buff, uint8_t value)
{
    buff[0] = value;
}

// int8_t

template<>
inline int8_t extract<int8_t>(const char *buff)
{
    uint8_t tmp = extract<uint8_t>(buff);
    return *reinterpret_cast<int8_t*>(&tmp);
}

template<>
inline void append<int8_t>(unsigned char *buff, int8_t value)
{
    buff[0] = reinterpret_cast<uint8_t&>(value);
}

// uint16_t

template<>
inline uint16_t extract<uint16_t>(const char *buff)
{
    return static_cast<unsigned char>(buff[1])
           + (static_cast<unsigned char>(buff[0]) << 8);
}

template<>
inline void append<uint16_t>(unsigned char *buff, uint16_t value)
{
    buff[0] = (value >> 8) & 0xff;
    buff[1] = value & 0xff;
}

// int16_t

template<>
inline int16_t extract<int16_t>(const char *buff)
{
    uint16_t tmp = extract<uint16_t>(buff);
    return *reinterpret_cast<int16_t*>(&tmp);
}

template<>
inline void append<int16_t>(unsigned char *buff, int16_t value)
{
    append<uint16_t>(buff, reinterpret_cast<uint16_t&>(value));
}

// uint32_t

template<>
inline uint32_t extract<uint32_t>(const char *buff)
{
    auto tmp = static_cast<unsigned char>(buff[3]) + (static_cast<unsigned char>(buff[2]) << 8)
             + (static_cast<unsigned char>(buff[1]) << 16) + (static_cast<unsigned char>(buff[0]) << 24);
    return static_cast<uint32_t>(tmp);
}

template<>
inline void append<uint32_t>(unsigned char *buff, uint32_t value)
{
    buff[0] = (value >> 24) & 0xff;
    buff[1] = (value >> 16) & 0xff;
    buff[2] = (value >>  8) & 0xff;
    buff[3] = value & 0xff;
}

// int32_t

template<>
inline int32_t extract<int32_t>(const char *buff)
{
    uint32_t tmp = extract<uint32_t>(buff);
    return *reinterpret_cast<int32_t*>(&tmp);
}

template<>
inline void append<int32_t>(unsigned char *buff, int32_t value)
{
    append<uint32_t>(buff, reinterpret_cast<uint32_t&>(value));
}

// uint64_t

template<> constexpr size_t size_of<uint64_t> = 8;

template<>
inline uint64_t extract<uint64_t>(const char *buff)
{
    uint32_t u1 = extract<uint32_t>(buff);
    uint32_t u2 = extract<uint32_t>(buff + size_of<uint32_t>);
    return static_cast<uint64_t>(u2) + (static_cast<uint64_t>(u1) << 32);
}

template<>
inline void append<uint64_t>(unsigned char *buff, uint64_t value)
{
    append<uint32_t>(buff, (value >> 32));
    append<uint32_t>(buff + size_of<uint32_t>, value);
}

// int64_t

template<>
inline int64_t extract<int64_t>(const char *buff)
{
    uint64_t tmp = extract<uint64_t>(buff);
    return *reinterpret_cast<int64_t*>(&tmp);
}

template<>
inline void append<int64_t>(unsigned char *buff, int64_t value)
{
    append<uint64_t>(buff, reinterpret_cast<uint64_t&>(value));
}

// float

template<>
inline float extract<float>(const char *buff)
{
    return pseudo_cast<float, uint32_t>(extract<uint32_t>(buff));
}

template<>
inline void append<float>(unsigned char *buff, float value)
{
    append<uint32_t>(buff, pseudo_cast<uint32_t, float>(value));
}

// double

template<>
inline double extract<double>(const char *buff)
{
    return pseudo_cast<double, uint64_t>(extract<uint64_t>(buff));
}

template<>
inline void append<double>(unsigned char *buff, double value)
{
    append<uint64_t>(buff, pseudo_cast<uint64_t, double>(value));
}

// complex

namespace detail {

template<typename T>
inline auto extract_cplx(const char *buff)
{
    T r = extract<T>(buff);
    T i = extract<T>(buff + sizeof(T));
    return std::complex<T>(r, i);
}

template<typename T>
inline void append_cplx(unsigned char *buff, std::complex<T> value)
{
    append<T>(buff, value.real());
    append<T>(buff + sizeof(T), value.imag());
}

}  // namespace detail

template<> constexpr size_t size_of<std::complex<float>> = 2 * sizeof(float);

template<>
inline std::complex<float> extract<std::complex<float>>(const char *buff)
{
    return detail::extract_cplx<float>(buff);
}

template<>
inline void append<std::complex<float>>(unsigned char *buff, std::complex<float> value)
{
    detail::append_cplx<float>(buff, value);
}

template<> constexpr size_t size_of<std::complex<double>> = 2 * sizeof(double);

template<>
inline std::complex<double> extract<std::complex<double>>(const char *buff)
{
    return detail::extract_cplx<double>(buff);
}

template<>
inline void append<std::complex<double>>(unsigned char *buff, std::complex<double> value)
{
    detail::append_cplx<double>(buff, value);
}

// bool

template<>
inline bool extract<bool>(const char *buff)
{
    return static_cast<unsigned char>(buff[0]) == 1;
}

template<>
inline void append<bool>(unsigned char *buff, bool value)
{
    value ? buff[0] = 1 : buff[0] = 0;
}

// ------------------------
// Deserializer
// ------------------------

namespace detail {
    template<size_t position, typename... Tp>
    inline std::enable_if_t<0 == sizeof...(Tp), std::tuple<Tp...>>
    deserialize(const char *buff) {
        return std::make_tuple();
    }

    template<size_t position, typename Tp0, typename... Tp>
    inline std::enable_if_t<0 == sizeof...(Tp), std::tuple<Tp0, Tp...>>
    deserialize(const char *buff) {
        return std::make_tuple(extract<Tp0>(&buff[position]));
    }

    template<size_t position, typename Tp0, typename... Tp>
    inline std::enable_if_t<0 < sizeof...(Tp), std::tuple<Tp0, Tp...>>
    deserialize(const char *buff) {
        return std::tuple_cat(std::make_tuple(extract<Tp0>(&buff[position])),
                              deserialize<position + size_of<Tp0>, Tp...>(buff));
    }

    // Required buffer size
    template<typename... Tp>
    constexpr std::enable_if_t<0 == sizeof...(Tp), size_t>
    required_buffer_size() {
        return 0;
    }

    template<typename Tp0, typename... Tp>
    constexpr std::enable_if_t<0 == sizeof...(Tp), size_t>
    required_buffer_size() {
        return size_of<Tp0>;
    }

    template<typename Tp0, typename... Tp>
    constexpr std::enable_if_t<0 < sizeof...(Tp), size_t>
    required_buffer_size() {
        return size_of<Tp0> + required_buffer_size<Tp...>();
    }
} // namespace detail

template<typename... Tp>
constexpr size_t required_buffer_size() {
    return detail::required_buffer_size<Tp...>();
}

template<size_t position, typename... Tp>
inline std::tuple<Tp...> deserialize(const char *buff) {
    return detail::deserialize<position, Tp...>(buff);
}

// ------------------------
// Serializer
// ------------------------

namespace detail {
    template<size_t buff_pos, size_t I, typename... Tp>
    inline std::enable_if_t<I == sizeof...(Tp), void>
    serialize(const std::tuple<Tp...>&, unsigned char *)
    {}

    template<size_t buff_pos, size_t I, typename... Tp>
    inline std::enable_if_t<I < sizeof...(Tp), void>
    serialize(const std::tuple<Tp...>& t, unsigned char *buff) {
        using type = typename std::tuple_element<I, std::tuple<Tp...>>::type;
        // append<type>(&buff[buff_pos], std::get<I>(t));
        append(&buff[buff_pos], std::get<I>(t));
        serialize<buff_pos + size_of<type>, I + 1, Tp...>(t, &buff[0]);
    }
}

template<typename... Tp>
inline std::array<unsigned char, required_buffer_size<Tp...>()>
serialize(const std::tuple<Tp...>& t) {
    std::array<unsigned char, required_buffer_size<Tp...>()> arr;
    detail::serialize<0, 0, Tp...>(t, arr.data());
    return arr;
}

template<typename... Tp>
inline std::array<unsigned char, required_buffer_size<Tp...>()>
serialize(Tp&&... t) {
    return serialize<Tp...>(std::make_tuple(std::forward<Tp>(t)...));
}

// ---------------------------
// Commands serializer
// ---------------------------

template<size_t SCALAR_PACK_LEN>
class DynamicSerializer {
  private:
    // Dynamic container
    // http://stackoverflow.com/questions/12042824/how-to-write-a-type-trait-is-container-or-is-vector
    template<typename T, typename _ = void>
    struct is_container : std::false_type {};

    template<typename... Ts>
    struct is_container_helper {};

    template<typename T>
    struct is_container<
        T,
        std::conditional_t<
            false,
            is_container_helper<
                typename T::value_type,
                typename T::size_type,
                typename T::allocator_type,
                typename T::iterator,
                typename T::const_iterator,
                decltype(std::declval<T>().size()),
                decltype(std::declval<T>().data()),
                decltype(std::declval<T>().begin()),
                decltype(std::declval<T>().end()),
                decltype(std::declval<T>().cbegin()),
                decltype(std::declval<T>().cend())
                >,
            void
            >
        > : public std::true_type {};

    template<typename T>
    static constexpr bool is_container_v = is_container<T>::value;

    static_assert(is_container_v<std::vector<float>>, "");
    static_assert(is_container_v<std::string>, "");
    static_assert(!is_container_v<float>, "");

    // Scalar
    template<typename T>
    static constexpr bool is_scalar_v = std::is_scalar<std::remove_reference_t<T>>::value &&
                                        !std::is_pointer<std::remove_reference_t<T>>::value;

    static_assert(is_scalar_v<float>, "");
    static_assert(!is_scalar_v<uint32_t*>, "");
    static_assert(!is_scalar_v<std::vector<float>>, "");

    // Complex
    template<typename T>
    struct is_std_complex : std::false_type {};
    template<typename T>
    struct is_std_complex<std::complex<T>> : std::true_type {};

    template<typename T>
    static constexpr bool is_std_complex_v = is_std_complex<std::decay_t<T>>::value;

    static_assert(is_std_complex_v<std::complex<float>>, "");
    static_assert(is_std_complex_v<std::complex<double>&>, "");
    static_assert(!is_std_complex_v<float>, "");

    // Tuple
    template <typename T>
    struct is_std_tuple : std::false_type {};
    template <typename... Args>
    struct is_std_tuple<std::tuple<Args...>> : std::true_type {};

    template<typename T>
    static constexpr bool is_std_tuple_v = is_std_tuple<T>::value;

    static_assert(is_std_tuple_v<std::tuple<uint32_t, float>>, "");
    static_assert(!is_std_tuple_v<uint32_t>, "");

    // Array
    template <typename T>
    struct is_std_array : std::false_type {};
    template <typename V, size_t N>
    struct is_std_array<std::array<V, N>> : std::true_type {};

    template <typename T>
    static constexpr bool is_std_array_v = is_std_array<T>::value;

    static_assert(is_std_array_v<std::array<uint32_t, 10>>, "");
    static_assert(!is_std_array_v<std::vector<uint32_t>>, "");

    // C string
    // http://stackoverflow.com/questions/8097534/type-trait-for-strings
    template <typename T>
    struct is_c_string : public
    std::integral_constant<bool,
        std::is_same<char*, std::decay_t<T>>::value ||
        std::is_same<const char*, std::decay_t<T>>::value
    >{};

    template<typename T>
    static constexpr bool is_c_string_v = is_c_string<T>::value;

    static_assert(is_c_string_v<char*>, "");
    static_assert(is_c_string_v<const char*>, "");
    static_assert(!is_c_string_v<std::string>, "");

  private:
    // Scalars

    template<typename T>
    void append(T t) {
        koheron::append<T>(&scal_data[scal_size], t);
        scal_size += size_of<T>;
    }

    void dump_scalar_pack(std::vector<unsigned char>& buffer) {
        if (scal_size > 0) {
            buffer.reserve(buffer.size() + scal_size);
            buffer.insert(buffer.end(), scal_data.data(), scal_data.data() + scal_size);
            scal_size = 0;
        }
    }

    template<typename Tp0, typename... Tp>
    inline std::enable_if_t<0 == sizeof...(Tp) && (is_scalar_v<Tp0> || is_std_complex_v<Tp0>), void>
    command_serializer(std::vector<unsigned char>&, Tp0&& t, Tp&&...) {
        append(std::forward<Tp0>(t));
    }

    template <typename Tp0, typename... Tp>
    inline std::enable_if_t<0 < sizeof...(Tp) && (is_scalar_v<Tp0> || is_std_complex_v<Tp0>), void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        append(std::forward<Tp0>(t));
        command_serializer(buffer, std::forward<Tp>(args)...);
    }

    // Dynamic containers (vector, string)

    template<typename Container>
    void dump_container_to_buffer(std::vector<unsigned char>& buffer,
                                  const Container& container) {
        static_assert(is_container_v<Container>, "");

        using T = typename Container::value_type;
        const uint32_t n_bytes = container.size() * sizeof(T);
        buffer.resize(buffer.size() + size_of<uint32_t>);
        koheron::append(buffer.data() + buffer.size() - size_of<uint32_t>, n_bytes);

        if (n_bytes > 0) {
            const auto bytes = reinterpret_cast<const unsigned char*>(container.data());
            buffer.insert(buffer.end(), bytes, bytes + n_bytes);
        }
    }

    template<typename Tp0, typename... Tp>
    std::enable_if_t<0 == sizeof...(Tp) && is_container_v<std::remove_reference_t<Tp0>>, void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        dump_scalar_pack(buffer);
        dump_container_to_buffer(buffer, std::forward<Tp0>(t));
    }

    template <typename Tp0, typename... Tp>
    std::enable_if_t<0 < sizeof...(Tp) && is_container_v<std::remove_reference_t<Tp0>>, void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        dump_scalar_pack(buffer);
        dump_container_to_buffer(buffer, std::forward<Tp0>(t));
        command_serializer(buffer, std::forward<Tp>(args)...);
    }

    // std::array

    template<typename Array>
    void dump_array_to_buffer(std::vector<unsigned char>& buffer,
                                  const Array& arr) {
        using T = typename Array::value_type;
        constexpr auto n_bytes = std::tuple_size<Array>::value * sizeof(T);

        if (n_bytes > 0) {
            const auto bytes = reinterpret_cast<const unsigned char*>(arr.data());
            buffer.insert(buffer.end(), bytes, bytes + n_bytes);
        }
    }

    template<typename Tp0, typename... Tp>
    std::enable_if_t<0 == sizeof...(Tp) && is_std_array_v<std::decay_t<Tp0>>, void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        dump_scalar_pack(buffer);
        dump_array_to_buffer(buffer, std::forward<Tp0>(t));
    }

    template <typename Tp0, typename... Tp>
    std::enable_if_t<0 < sizeof...(Tp) && is_std_array_v<std::decay_t<Tp0>>, void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        dump_scalar_pack(buffer);
        dump_array_to_buffer(buffer, std::forward<Tp0>(t));
        command_serializer(buffer, std::forward<Tp>(args)...);
    }

    // C strings

    template<typename Tp0, typename... Tp>
    std::enable_if_t<0 == sizeof...(Tp) && is_c_string_v<Tp0>, void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        dump_scalar_pack(buffer);
        dump_container_to_buffer(buffer, std::string(std::forward<Tp0>(t)));
    }

    template <typename Tp0, typename... Tp>
    std::enable_if_t<0 < sizeof...(Tp) && is_c_string_v<Tp0>, void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        dump_scalar_pack(buffer);
        dump_container_to_buffer(buffer, std::string(std::forward<Tp0>(t)));
        command_serializer(buffer, std::forward<Tp>(args)...);
    }

    // Tuples are unpacked before serialization

    template<uint16_t class_id, uint16_t func_id,
             std::size_t... I, typename... Args>
    void call_command_serializer(std::vector<unsigned char>& buffer,
                                 std::index_sequence<I...>,
                                 std::tuple<Args...> tup_args) {
        build_command<class_id, func_id>(buffer, std::get<I>(tup_args)...);
    }

  public:
    template<uint16_t class_id, uint16_t func_id, typename Tp0, typename... Args>
    std::enable_if_t<0 <= sizeof...(Args) &&
                     !is_std_tuple_v<
                         typename std::remove_reference<Tp0>::type
                     >, void>
    build_command(std::vector<unsigned char>& buffer, Tp0&& arg0, Args&&... args) {
        const auto& header = serialize(0U, class_id, func_id);
        buffer.resize(koheron::required_buffer_size<uint32_t, uint16_t, uint16_t>());
        std::move(header.begin(), header.end(), buffer.begin());
        scal_size = 0;
        command_serializer(buffer, std::forward<Tp0>(arg0),
                           std::forward<Args>(args)...);
        dump_scalar_pack(buffer);
    }

    template<uint16_t class_id, uint16_t func_id, typename... Args>
    std::enable_if_t< 0 == sizeof...(Args), void >
    build_command(std::vector<unsigned char>& buffer, Args&&... args) {
        const auto& header = serialize(0U, class_id, func_id);
        buffer.resize(koheron::required_buffer_size<uint32_t, uint16_t, uint16_t>());
        std::move(header.begin(), header.end(), buffer.begin());
    }

    template<uint16_t class_id, uint16_t func_id, typename... Args>
    void build_command(std::vector<unsigned char>& buffer,
                       std::tuple<Args...> tup_args) {
        call_command_serializer<class_id, func_id>(buffer,
                std::index_sequence_for<Args...>{}, tup_args);
    }

  private:
    std::array<unsigned char, SCALAR_PACK_LEN> scal_data;
    uint64_t scal_size = 0;
};

} // namespace koheron

#endif // __SERIALIZER_DESERIALIZER_HPP__