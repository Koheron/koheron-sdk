/// (c) Koheron

#ifndef __KOHERON_CLIENT_HPP__
#define __KOHERON_CLIENT_HPP__

#include <vector>
#include <array>
#include <tuple>
#include <type_traits>
#include <string>
#include <string_view>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <unordered_map>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <system_error>

#if defined(__GNUC__) && !defined(_WIN32)
  #include <typeinfo>
  #include <cxxabi.h>
  #define KOHERON_HAVE_DEMANGLE 1
#endif

#ifdef _WIN32
  /* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501  /* Windows XP. */
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
extern "C" {
  #include <sys/socket.h>   // socket definitions
  #include <sys/types.h>    // socket types
  #include <arpa/inet.h>    // inet (3) functions
  #include <netinet/tcp.h>
  #include <netdb.h>
  #include <unistd.h>
}
#endif

#include <operations.hpp>

#ifdef _WIN32
  using socket_t = unsigned long long;
  using sockaddr_in_t = sockaddr_in;
#else
  using socket_t = int;
  using sockaddr_in_t = struct sockaddr_in;
#endif

namespace serdes {
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

template<typename Tp> Tp extract(const unsigned char *buff);       // Deserialization
template<typename Tp> void append(unsigned char *buff, Tp value);  // Serialization

// uint8_t

template<> constexpr size_t size_of<uint8_t> = 1;

template<>
inline constexpr uint8_t extract<uint8_t>(const unsigned char *buff)
{
    return buff[0];
}

template<>
inline constexpr void append<uint8_t>(unsigned char *buff, uint8_t value)
{
    buff[0] = value;
}

// int8_t

template<> constexpr size_t size_of<int8_t> = 1;

template<>
inline int8_t extract<int8_t>(const unsigned char *buff)
{
    return buff[0];
}

template<>
inline void append<int8_t>(unsigned char *buff, int8_t value)
{
    buff[0] = reinterpret_cast<unsigned char&>(value);
}

// uint16_t

template<> constexpr size_t size_of<uint16_t> = 2;

template<>
inline constexpr uint16_t extract<uint16_t>(const unsigned char *buff)
{
    return buff[1] + (buff[0] << 8);
}

template<>
inline constexpr void append<uint16_t>(unsigned char *buff, uint16_t value)
{
    buff[0] = (value >> 8) & 0xff;
    buff[1] = value & 0xff;
}

// int16_t

template<> constexpr size_t size_of<int16_t> = 2;

template<>
inline int16_t extract<int16_t>(const unsigned char *buff)
{
    const uint16_t tmp = extract<uint16_t>(buff);
    return *reinterpret_cast<const int16_t*>(&tmp);
}

template<>
inline void append<int16_t>(unsigned char *buff, int16_t value)
{
    append<uint16_t>(buff, reinterpret_cast<uint16_t&>(value));
}

// uint32_t

template<> constexpr size_t size_of<uint32_t> = 4;

template<>
inline constexpr uint32_t extract<uint32_t>(const unsigned char *buff)
{
    return buff[3] + (buff[2] << 8) + (buff[1] << 16) + (buff[0] << 24);
}

template<>
inline constexpr void append<uint32_t>(unsigned char *buff, uint32_t value)
{
    buff[0] = (value >> 24) & 0xff;
    buff[1] = (value >> 16) & 0xff;
    buff[2] = (value >>  8) & 0xff;
    buff[3] = value & 0xff;
}

// int32_t

template<> constexpr size_t size_of<int32_t> = 4;

template<>
inline int32_t extract<int32_t>(const unsigned char *buff)
{
    const uint32_t tmp = extract<uint32_t>(buff);
    return *reinterpret_cast<const int32_t*>(&tmp);
}

template<>
inline void append<int32_t>(unsigned char *buff, int32_t value)
{
    append<uint32_t>(buff, reinterpret_cast<uint32_t&>(value));
}

// uint64_t

template<> constexpr size_t size_of<uint64_t> = 8;

template<>
inline constexpr uint64_t extract<uint64_t>(const unsigned char *buff)
{
    const uint32_t u1 = extract<uint32_t>(buff);
    const uint32_t u2 = extract<uint32_t>(buff + size_of<uint32_t>);
    return static_cast<uint64_t>(u2) + (static_cast<uint64_t>(u1) << 32);
}

template<>
inline constexpr void append<uint64_t>(unsigned char *buff, uint64_t value)
{
    append<uint32_t>(buff, (value >> 32));
    append<uint32_t>(buff + size_of<uint32_t>, value);
}

// int64_t

template<> constexpr size_t size_of<int64_t> = 8;

template<>
inline int64_t extract<int64_t>(const unsigned char *buff)
{
    const uint64_t tmp = extract<uint64_t>(buff);
    return *reinterpret_cast<const int64_t*>(&tmp);
}

template<>
inline void append<int64_t>(unsigned char *buff, int64_t value)
{
    append<uint64_t>(buff, reinterpret_cast<uint64_t&>(value));
}

// float

template<> constexpr size_t size_of<float> = size_of<uint32_t>;
static_assert(sizeof(float) == size_of<float>, "Invalid float size");

template<>
inline float extract<float>(const unsigned char *buff)
{
    return pseudo_cast<float, uint32_t>(extract<uint32_t>(buff));
}

template<>
inline void append<float>(unsigned char *buff, float value)
{
    append<uint32_t>(buff, pseudo_cast<uint32_t, float>(value));
}

// double

template<> constexpr size_t size_of<double> = size_of<uint64_t>;
static_assert(sizeof(double) == size_of<double>, "Invalid double size");

template<>
inline double extract<double>(const unsigned char *buff)
{
    return pseudo_cast<double, uint64_t>(extract<uint64_t>(buff));
}

template<>
inline void append<double>(unsigned char *buff, double value)
{
    append<uint64_t>(buff, pseudo_cast<uint64_t, double>(value));
}

// bool

template<> constexpr size_t size_of<bool> = 1;

template<>
inline constexpr bool extract<bool>(const unsigned char *buff)
{
    return buff[0] == 1;
}

template<>
inline constexpr void append<bool>(unsigned char *buff, bool value)
{
    value ? buff[0] = 1 : buff[0] = 0;
}

namespace test {
    // Test constexpr conversions.
    // Only unsigned integers (not requiring reinterpret_cast or memcpy)
    // can be defined as constexpr conversion functions.

    template<typename T>
    constexpr bool test_static_serdes(T value) {
        unsigned char buff[sizeof(T)] = {0};
        append<T>(buff, value);
        return extract<T>(buff) == T(value);
    }

    static_assert(test_static_serdes<bool>(true), "");
    static_assert(test_static_serdes<uint8_t>(255), "");
    static_assert(test_static_serdes<uint16_t>(65535), "");
    static_assert(test_static_serdes<uint32_t>(4294967295), "");
    static_assert(test_static_serdes<uint64_t>(18446744073709551615ULL), "");
}

// ---------------------------
// Type traits
// ---------------------------

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
    std::is_same<char*, typename std::remove_reference<T>::type>::value ||
    std::is_same<const char*, typename std::remove_reference<T>::type>::value
>{};

template<typename T>
static constexpr bool is_c_string_v = is_c_string<T>::value;

static_assert(is_c_string_v<char*>, "");
static_assert(is_c_string_v<const char*>, "");
static_assert(!is_c_string_v<std::string>, "");

// ------------------------
// Deserializer
// ------------------------

namespace detail {
    template<size_t position, typename... Tp>
    inline constexpr std::enable_if_t<0 == sizeof...(Tp), std::tuple<Tp...>>
    deserialize(const unsigned char *buff)
    {
        return std::make_tuple();
    }

    template<size_t position, typename Tp0, typename... Tp>
    inline constexpr std::enable_if_t<0 == sizeof...(Tp), std::tuple<Tp0, Tp...>>
    deserialize(const unsigned char *buff)
    {
        return std::make_tuple(extract<Tp0>(&buff[position]));
    }

    template<size_t position, typename Tp0, typename... Tp>
    inline constexpr std::enable_if_t<0 < sizeof...(Tp), std::tuple<Tp0, Tp...>>
    deserialize(const unsigned char *buff)
    {
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
    required_buffer_size()
    {
        return size_of<Tp0>;
    }

    template<typename Tp0, typename... Tp>
    constexpr std::enable_if_t<0 < sizeof...(Tp), size_t>
    required_buffer_size()
    {
        return size_of<Tp0> + required_buffer_size<Tp...>();
    }
}

template<typename... Tp>
constexpr size_t required_buffer_size()
{
    return detail::required_buffer_size<Tp...>();
}

template<size_t position, typename... Tp>
inline constexpr std::tuple<Tp...> deserialize(const unsigned char *buff)
{
    return detail::deserialize<position, Tp...>(buff);
}

namespace test {
    constexpr bool test_deserialize() {
        unsigned char buff[required_buffer_size<uint8_t, uint32_t>()] = {0};
        append<uint8_t>(buff, 255);
        append<uint32_t>(buff + size_of<uint8_t>, 8987);
        return deserialize<0, uint8_t, uint32_t>(buff) == std::make_tuple(uint8_t(255), uint32_t(8987));
    }

    static_assert(test_deserialize(), "");
}

// ------------------------
// Serializer
// ------------------------

namespace detail {
    template<size_t buff_pos, size_t I, typename... Tp>
    inline constexpr std::enable_if_t<I == sizeof...(Tp), void>
    serialize(const std::tuple<Tp...>& t, unsigned char *buff)
    {}

    template<size_t buff_pos, size_t I, typename... Tp>
    inline constexpr std::enable_if_t<I < sizeof...(Tp), void>
    serialize(const std::tuple<Tp...>& t, unsigned char *buff)
    {
        using type = typename std::tuple_element_t<I, std::tuple<Tp...>>;
        append<type>(&buff[buff_pos], std::get<I>(t));
        serialize<buff_pos + size_of<type>, I + 1, Tp...>(t, &buff[0]);
    }
}

template<typename... Tp>
inline constexpr void
serialize(unsigned char *buff, const std::tuple<Tp...>& t)
{
    detail::serialize<0, 0, Tp...>(t, buff);
}

template<typename... Tp>
inline std::array<unsigned char, required_buffer_size<Tp...>()>
serialize(const std::tuple<Tp...>& t)
{
    std::array<unsigned char, required_buffer_size<Tp...>()> arr;
    detail::serialize<0, 0, Tp...>(t, arr.data());
    return arr;
}

template<typename... Tp>
inline std::array<unsigned char, required_buffer_size<Tp...>()>
serialize(Tp... t)
{
    return serialize<Tp...>(std::make_tuple(t...));
}

namespace test {
    constexpr bool test_serialize() {
        const auto t = std::make_tuple(uint8_t(255), uint32_t(8987));
        unsigned char buff[required_buffer_size<uint8_t, uint32_t>()] = {0};
        serialize(buff, t);
        return deserialize<0, uint8_t, uint32_t>(buff) == t;
    }

    static_assert(test_deserialize(), "");
}

// ---------------------------
// Commands serializer
// ---------------------------

template<size_t SCALAR_PACK_LEN>
class DynamicSerializer {
  private:
    // Scalars

    template<typename T>
    void append(T t) {
        serdes::append<T>(&scal_data[scal_size], t);
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
    inline std::enable_if_t<0 == sizeof...(Tp) && is_scalar_v<Tp0>, void>
    command_serializer(std::vector<unsigned char>& buffer, Tp0&& t, Tp&&... args) {
        append(std::forward<Tp0>(t));
    }

    template <typename Tp0, typename... Tp>
    inline std::enable_if_t<0 < sizeof...(Tp) && is_scalar_v<Tp0>, void>
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
        serdes::append(buffer.data() + buffer.size() - size_of<uint32_t>, n_bytes);

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
        buffer.resize(serdes::required_buffer_size<uint32_t, uint16_t, uint16_t>());
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
        buffer.resize(serdes::required_buffer_size<uint32_t, uint16_t, uint16_t>());
        std::move(header.begin(), header.end(), buffer.begin());
    }

    template<uint16_t class_id, uint16_t func_id, typename... Args>
    void build_command(std::vector<unsigned char>& buffer,
                       std::tuple<Args...> tup_args) {
        call_command_serializer<class_id, func_id>(buffer,
                std::index_sequence_for<Args...>{}, tup_args);
    }

    // Runtime ids (e.g. resolved from the instrument context by name):
    // class_id/func_id are only used as values, so a runtime version is
    // equivalent to the template one.
    template<typename Tp0, typename... Args>
    void build_command_rt(std::vector<unsigned char>& buffer,
                          uint16_t class_id, uint16_t func_id, Tp0&& arg0, Args&&... args) {
        const auto& header = serialize(0U, class_id, func_id);
        buffer.resize(serdes::required_buffer_size<uint32_t, uint16_t, uint16_t>());
        std::move(header.begin(), header.end(), buffer.begin());
        scal_size = 0;
        command_serializer(buffer, std::forward<Tp0>(arg0),
                           std::forward<Args>(args)...);
        dump_scalar_pack(buffer);
    }

    inline void build_command_rt(std::vector<unsigned char>& buffer,
                                 uint16_t class_id, uint16_t func_id) {
        const auto& header = serialize(0U, class_id, func_id);
        buffer.resize(serdes::required_buffer_size<uint32_t, uint16_t, uint16_t>());
        std::move(header.begin(), header.end(), buffer.begin());
    }

  private:
    std::array<unsigned char, SCALAR_PACK_LEN> scal_data;
    uint64_t scal_size = 0;
};

} // namespace serdes

struct socket_error : std::system_error {
    socket_error(const char *err_msg_)
    : system_error(std::error_code())
    , err_msg(err_msg_) {}

    virtual const char* what() const noexcept {
        return err_msg;
    }

 private:
    const char *err_msg;
};

// http://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912s
//
// To avoid initialization on resize.
//
// Allocator adaptor that interposes construct() calls to
// convert value initialization into default initialization.
template <typename T, typename A=std::allocator<T>>
class default_init_allocator : public A {
  typedef std::allocator_traits<A> a_t;
public:
  template <typename U> struct rebind {
    using other =
      default_init_allocator<
        U, typename a_t::template rebind_alloc<U>
      >;
  };

  using A::A;

  template <typename U>
  void construct(U* ptr)
    noexcept(std::is_nothrow_default_constructible<U>::value) {
    ::new(static_cast<void*>(ptr)) U;
  }
  template <typename U, typename...Args>
  void construct(U* ptr, Args&&... args) {
    a_t::construct(static_cast<A&>(*this),
                   ptr, std::forward<Args>(args)...);
  }
};

// ==================================================
// Instrument status / run over HTTP
//
// C++ equivalents of the koheron Python client module functions
// instrument_status(), run_instrument() and connect(). They talk to
// the HTTP API exposed by koheron-server (nginx/uwsgi, port 80):
//
//   GET /api/instruments              -> {"instruments":[...], "live_instrument":"name"}
//   GET /api/instruments/run/<name>   -> installs <name>.zip as the live instrument
//
// They are used to check that the expected instrument (bitstream +
// context server) is loaded before opening the TCP command socket.
// ==================================================

struct instrument_error : std::runtime_error {
    explicit instrument_error(const std::string& msg)
    : std::runtime_error(msg) {}
};

namespace koheron_http {

inline void close_socket(socket_t fd) {
#ifdef _WIN32
    ::closesocket(fd);
#else
    ::close(fd);
#endif
}

constexpr socket_t invalid_socket = static_cast<socket_t>(-1);

inline void set_socket_timeout(socket_t fd, int timeout_ms) {
#ifdef _WIN32
    DWORD tmo = static_cast<DWORD>(timeout_ms);
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tmo), sizeof(tmo));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tmo), sizeof(tmo));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

inline socket_t tcp_connect(const std::string& host, int port, int timeout_ms) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        throw instrument_error("WSAStartup failed");
    }
#endif
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || res == nullptr) {
        throw instrument_error("Cannot resolve koheron-server address " + host);
    }

    socket_t fd = invalid_socket;
    for (struct addrinfo* rp = res; rp != nullptr; rp = rp->ai_next) {
        fd = ::socket(rp->ai_family, rp->ai_socktype, static_cast<int>(rp->ai_protocol));
        if (fd == invalid_socket) continue;
        if (::connect(fd, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) break;
        close_socket(fd);
        fd = invalid_socket;
    }
    ::freeaddrinfo(res);

    if (fd == invalid_socket) {
        throw instrument_error("Cannot connect to " + host + ":" + std::to_string(port));
    }

    set_socket_timeout(fd, timeout_ms);
    return fd;
}

inline void send_all(socket_t fd, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        const int n = ::send(fd, data + sent, len - sent, 0);
        if (n <= 0) {
            throw instrument_error("Cannot send request to koheron-server");
        }
        sent += static_cast<size_t>(n);
    }
}

struct HttpResponse {
    int status_code = 0;
    std::string body;
};

inline bool header_value(const std::string& headers, const std::string& name, std::string& value) {
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t eol = headers.find("\r\n", pos);
        if (eol == std::string::npos) eol = headers.size();
        if (eol > pos + name.size() + 1) {
            bool match = true;
            for (size_t i = 0; i < name.size(); i++) {
                const unsigned char a = static_cast<unsigned char>(headers[pos + i]);
                const unsigned char b = static_cast<unsigned char>(name[i]);
                if (std::tolower(a) != std::tolower(b)) { match = false; break; }
            }
            if (match && headers[pos + name.size()] == ':') {
                size_t v = pos + name.size() + 1;
                while (v < eol && (headers[v] == ' ' || headers[v] == '\t')) v++;
                size_t end = eol;
                while (end > v && (headers[end - 1] == ' ' || headers[end - 1] == '\t')) end--;
                value = headers.substr(v, end - v);
                return true;
            }
        }
        pos = eol + 2;
    }
    return false;
}

inline HttpResponse get(const std::string& host, const std::string& path, int port, int timeout_ms) {
    const socket_t fd = tcp_connect(host, port, timeout_ms);
    try {
        const std::string request = "GET " + path + " HTTP/1.1\r\n"
                                    "Host: " + host + "\r\n"
                                    "User-Agent: koheron-client-cpp\r\n"
                                    "Connection: close\r\n\r\n";
        send_all(fd, request.data(), request.size());

        std::string raw;
        char buf[4096];
        size_t header_end = std::string::npos;
        while (header_end == std::string::npos) {
            const int n = ::recv(fd, buf, sizeof(buf), 0);
            if (n < 0) throw instrument_error("Cannot receive HTTP response from koheron-server");
            if (n == 0) break;
            raw.append(buf, static_cast<size_t>(n));
            header_end = raw.find("\r\n\r\n");
            if (raw.size() > (1u << 20)) {
                throw instrument_error("Malformed HTTP response from koheron-server");
            }
        }
        if (header_end == std::string::npos) {
            throw instrument_error("Incomplete HTTP response from koheron-server");
        }

        HttpResponse resp;
        const size_t line_end = raw.find("\r\n");
        const std::string status_line = raw.substr(0, line_end);
        const size_t sp = status_line.find(' ');
        if (status_line.compare(0, 5, "HTTP/") != 0 || sp == std::string::npos) {
            throw instrument_error("Malformed HTTP status line from koheron-server: " + status_line);
        }
        resp.status_code = static_cast<int>(std::strtol(status_line.c_str() + sp + 1, nullptr, 10));

        const std::string headers = raw.substr(0, header_end);
        resp.body = raw.substr(header_end + 4);

        std::string content_length;
        if (header_value(headers, "Content-Length", content_length)) {
            const size_t expected = static_cast<size_t>(std::strtoull(content_length.c_str(), nullptr, 10));
            while (resp.body.size() < expected) {
                const int n = ::recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) throw instrument_error("Incomplete HTTP body from koheron-server");
                resp.body.append(buf, static_cast<size_t>(n));
            }
        } else {
            for (;;) {
                const int n = ::recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) break; // connection closed (or read timeout): end of body
                resp.body.append(buf, static_cast<size_t>(n));
            }
        }

        close_socket(fd);
        return resp;
    } catch (...) {
        close_socket(fd);
        throw;
    }
}

} // namespace koheron_http

namespace koheron_json {

inline std::string unescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            i++;
            switch (s[i]) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            default: out += '\\'; out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

inline bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Position of the value associated with "key" in a flat JSON object,
// or npos when the key is absent.
inline size_t find_value(const std::string& json, const std::string& key) {
    const std::string quoted = "\"" + key + "\"";
    size_t pos = 0;
    for (;;) {
        const size_t k = json.find(quoted, pos);
        if (k == std::string::npos) return std::string::npos;

        size_t c = k + quoted.size();
        while (c < json.size() && is_ws(json[c])) c++;
        if (c < json.size() && json[c] == ':') {
            // Make sure this is a key (preceded by '{' or ','), not a value.
            size_t p = k;
            while (p > 0 && is_ws(json[p - 1])) p--;
            if (p == 0 || json[p - 1] == '{' || json[p - 1] == ',') {
                c++;
                while (c < json.size() && is_ws(json[c])) c++;
                return c;
            }
        }
        pos = k + quoted.size();
    }
}

inline bool find_string(const std::string& json, const std::string& key, std::string& out) {
    const size_t vpos = find_value(json, key);
    if (vpos == std::string::npos || json[vpos] != '"') return false;

    std::string raw;
    size_t i = vpos + 1;
    while (i < json.size()) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            raw += json[i];
            raw += json[i + 1];
            i += 2;
            continue;
        }
        if (json[i] == '"') {
            out = unescape(raw);
            return true;
        }
        raw += json[i++];
    }
    return false;
}

inline bool find_string_array(const std::string& json, const std::string& key, std::vector<std::string>& out) {
    const size_t vpos = find_value(json, key);
    if (vpos == std::string::npos || json[vpos] != '[') return false;

    size_t i = vpos + 1;
    while (i < json.size() && is_ws(json[i])) i++;
    if (i < json.size() && json[i] == ']') return true; // empty array

    while (i < json.size()) {
        while (i < json.size() && (is_ws(json[i]) || json[i] == ',')) i++;
        if (i >= json.size() || json[i] == ']') break;
        if (json[i] != '"') return false; // non-string element

        std::string raw;
        bool closed = false;
        i++;
        while (i < json.size()) {
            if (json[i] == '\\' && i + 1 < json.size()) {
                raw += json[i];
                raw += json[i + 1];
                i += 2;
                continue;
            }
            if (json[i] == '"') { i++; closed = true; break; }
            raw += json[i++];
        }
        if (!closed) return false;
        out.push_back(unescape(raw));
    }
    return true;
}

} // namespace koheron_json

struct InstrumentStatus {
    std::vector<std::string> instruments; // instruments stored on the board
    std::string live_instrument;          // instrument currently loaded (empty if none)

    bool is_live(const std::string& name) const {
        return !name.empty() && live_instrument == name;
    }

    bool in_store(const std::string& name) const {
        return std::find(instruments.begin(), instruments.end(), name) != instruments.end();
    }
};

// GET /api/instruments
inline InstrumentStatus instrument_status(const std::string& host, int http_port = 80, int timeout_ms = 5000) {
    const auto resp = koheron_http::get(host, "/api/instruments", http_port, timeout_ms);
    if (resp.status_code != 200) {
        throw instrument_error("GET /api/instruments failed with HTTP status " + std::to_string(resp.status_code));
    }

    InstrumentStatus status;
    koheron_json::find_string_array(resp.body, "instruments", status.instruments);
    koheron_json::find_string(resp.body, "live_instrument", status.live_instrument);
    return status;
}

// Equivalent of python koheron.run_instrument(host, name, restart).
// An empty name behaves like Python's None: no-op when an instrument is
// already live. Throws instrument_error when the instrument is not in
// the board store.
inline void run_instrument(const std::string& host, const std::string& name = std::string(),
                           bool restart = false, int http_port = 80) {
    bool instrument_running = false;
    bool instrument_in_store = false;
    std::string target = name;

    const InstrumentStatus status = instrument_status(host, http_port);

    if (target.empty() || status.live_instrument == target) { // Instrument already running
        target = status.live_instrument;
        instrument_running = true;
    }

    if (instrument_running && !restart) return;

    if (!instrument_running) { // Find the instrument in the local store:
        if (status.in_store(target)) {
            instrument_in_store = true;
        } else {
            std::string msg = "Instrument " + target + " not found.\nAvailable instruments:";
            for (const auto& instrument : status.instruments) {
                msg += "\n- " + instrument;
            }
            throw instrument_error(msg);
        }
    }

    if (instrument_in_store || (instrument_running && restart)) {
        const auto resp = koheron_http::get(host, "/api/instruments/run/" + target, http_port, 60000);
        if (resp.status_code != 200) {
            throw instrument_error("GET /api/instruments/run/" + target + " failed with HTTP status "
                                   + std::to_string(resp.status_code));
        }
        if (resp.body.find("Failed") != std::string::npos) {
            throw instrument_error("Failed to install instrument " + target + ": " + resp.body);
        }
    }
}

// Poll the HTTP status until 'name' is the live instrument.
inline bool wait_for_instrument(const std::string& host, const std::string& name,
                                std::chrono::milliseconds timeout = std::chrono::milliseconds(30000),
                                int http_port = 80,
                                std::chrono::milliseconds interval = std::chrono::milliseconds(500)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        try {
            if (instrument_status(host, http_port).is_live(name)) return true;
        } catch (const std::exception&) {
            // Server may still be restarting: keep polling until the deadline.
        }
        if (std::chrono::steady_clock::now() >= deadline) return false;
        std::this_thread::sleep_for(interval);
    }
}

// ==================================================
// Instrument context / op id validation
//
// C++ equivalent of python KoheronClient.check_version() + load_devices() +
// get_ids(): the compile-time op ids of operations.hpp (op::Class::func =
// class_id << 16 | func_id) are checked against the class/function ids
// declared by the context JSON served by koheron-server (device 1, function
// 1), the same source python's load_devices() uses. Detects a client built
// from a different instrument revision than the loaded firmware. The checks
// are KoheronClient members, e.g.
//   client.check_op<op::DataMover::get_fifo_count>("DataMover", "get_fifo_count");
// ==================================================

struct ServerContext;

struct op_check_error : std::runtime_error {
    explicit op_check_error(const std::string& msg) : std::runtime_error(msg) {}
};

// Compile-time op id paired with the names the context declares:
//   { "DataMover", "get_fifo_count", op::DataMover::get_fifo_count }
struct ExpectedOp {
    const char* class_name;
    const char* func_name;
    uint32_t id;
};

namespace op_detail {

inline bool parse_uint(const std::string& s, size_t pos, uint32_t& out) {
    const char* start = s.c_str() + pos;
    char* end = nullptr;
    const unsigned long v = std::strtoul(start, &end, 10);
    if (end == start) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

// Pure parsing of a context JSON string; see KoheronClient::find_op().
inline bool find_op_in_json(const std::string& json, const std::string& class_name,
                            const std::string& func_name, uint16_t& class_id, uint16_t& func_id)
{
    const std::string ckey = "\"class\":\"" + class_name + "\"";
    const size_t cpos = json.find(ckey);
    if (cpos == std::string::npos) return false;

    const size_t next = json.find("\"class\":", cpos + ckey.size());
    const size_t cend = (next == std::string::npos) ? json.size() : next;

    const size_t cid_pos = json.find("\"id\":", cpos + ckey.size());
    if (cid_pos >= cend) return false;
    uint32_t cid = 0;
    if (!parse_uint(json, cid_pos + 5, cid)) return false;

    const std::string fkey = "\"name\":\"" + func_name + "\"";
    const size_t fpos = json.find(fkey, cid_pos);
    if (fpos >= cend) return false;

    const size_t fid_pos = json.find("\"id\":", fpos + fkey.size());
    if (fid_pos >= cend) return false;
    uint32_t fid = 0;
    if (!parse_uint(json, fid_pos + 5, fid)) return false;

    class_id = static_cast<uint16_t>(cid);
    func_id = static_cast<uint16_t>(fid);
    return true;
}

// One op declared by the instrument context, as downloaded from koheron-server.
struct OpEntry {
    uint32_t id;            // (class_id << 16) | func_id
    uint16_t class_id;
    uint16_t func_id;
    std::string class_name;
    std::string func_name;
    std::string ret_type;           // instrument-declared return type
    std::vector<std::string> args;  // instrument-declared arg types
};

// Position after the closing quote of the string value whose opening quote
// is at pos, npos when pos does not point at an opening quote.
inline size_t parse_string(const std::string& json, size_t pos, std::string& out) {
    if (pos >= json.size() || json[pos] != '"') return std::string::npos;
    std::string raw;
    size_t i = pos + 1;
    while (i < json.size()) {
        if (json[i] == '\\' && i + 1 < json.size()) { raw += json[i + 1]; i += 2; continue; }
        if (json[i] == '"') { out = raw; return i + 1; }
        raw += json[i++];
    }
    return std::string::npos;
}

inline size_t skip_ws(const std::string& json, size_t pos) {
    while (pos < json.size() && koheron_json::is_ws(json[pos])) pos++;
    return pos;
}

// Position after the object/array starting at pos ('{' or '['), strings
// tracked so delimiters inside them are ignored.
inline size_t skip_object(const std::string& json, size_t pos)
{
    int depth = 0;
    bool in_str = false;
    bool esc = false;
    for (size_t i = pos; i < json.size(); i++) {
        const char c = json[i];
        if (in_str) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
            continue;
        }
        if (c == '"') in_str = true;
        else if (c == '{' || c == '[') depth++;
        else if (c == '}' || c == ']') {
            depth--;
            if (depth == 0) return i + 1;
        }
    }
    return std::string::npos;
}

inline void replace_all(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t p = 0;
    for (;;) {
        p = s.find(from, p);
        if (p == std::string::npos) return;
        s.replace(p, from.size(), to);
        p += to.size();
    }
}

inline void trim_str(std::string& s) {
    while (!s.empty() && koheron_json::is_ws(s.front())) s.erase(s.begin());
    while (!s.empty() && koheron_json::is_ws(s.back())) s.pop_back();
}

// Split at top-level commas (depth 0 outside strings/nested <>).
inline std::vector<std::string> split_top(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    bool in_str = false;
    for (size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        if (in_str) { cur += c; if (c == '"') in_str = false; continue; }
        if (c == '"') { in_str = true; cur += c; continue; }
        if (c == '<' || c == '(') depth++;
        else if (c == '>' || c == ')') depth--;
        if (c == ',' && depth == 0) { out.push_back(cur); cur.clear(); continue; }
        cur += c;
    }
    out.push_back(cur);
    return out;
}

// Normalize a type name to the instrument's spelling: gcc demangled names
// ("unsigned int", "std::vector<float, std::allocator<float> >", template
// size suffixes) and instrument source spellings ("uint32_t",
// "std::vector<float>") map to the same canonical string. Unknown names are
// returned trimmed; pointers/refs beyond '&' stripping stay verbatim.
inline std::string canonical_type(std::string s)
{
    trim_str(s);
    replace_all(s, "std::__cxx11::", "std::");
    while (!s.empty() && (s.back() == '&' || koheron_json::is_ws(s.back()))) s.pop_back();
    trim_str(s);

    if (s == "bool" || s == "char" || s == "float" || s == "double") return s;
    if (s == "signed char" || s == "signed int" || s == "signed") return "int8_t";
    if (s == "unsigned char") return "uint8_t";
    if (s == "short" || s == "short int") return "int16_t";
    if (s == "unsigned short" || s == "unsigned short int") return "uint16_t";
    if (s == "int") return "int32_t";
    if (s == "unsigned int" || s == "unsigned") return "uint32_t";
    if (s == "long" || s == "long int" || s == "long long" || s == "long long int") return "int64_t";
    if (s == "unsigned long" || s == "unsigned long int" ||
        s == "unsigned long long" || s == "unsigned long long int") return "uint64_t";
    if (s.rfind("std::basic_string<", 0) == 0) return "std::string";

    if (s.rfind("std::vector<", 0) == 0 && !s.empty() && s.back() == '>') {
        const auto parts = split_top(s.substr(12, s.size() - 13));
        if (!parts.empty()) return "std::vector<" + canonical_type(parts[0]) + ">";
    }
    if (s.rfind("std::array<", 0) == 0 && !s.empty() && s.back() == '>') {
        const auto parts = split_top(s.substr(11, s.size() - 12));
        if (parts.size() == 2) {
            std::string n = parts[1];
            trim_str(n);
            while (!n.empty() && (n.back() == 'u' || n.back() == 'l')) n.pop_back();
            return "std::array<" + canonical_type(parts[0]) + ", " + n + ">";
        }
    }
    return s;
}

// Canonical instrument-style type name of a C++ type. Empty when unsupported
// (non-gcc), in which case the comparison is skipped.
template<typename T>
inline std::string type_str()
{
#ifdef KOHERON_HAVE_DEMANGLE
    int status = 0;
    char* n = abi::__cxa_demangle(typeid(T).name(), nullptr, nullptr, &status);
    std::string s = (status == 0 && n != nullptr) ? std::string(n) : std::string(typeid(T).name());
    std::free(n);
    return canonical_type(s);
#else
    return std::string();
#endif
}

// Extract the instrument-declared ret_type and arg type list from the
// function object spanning [start, end).
inline void fill_op_extras(const std::string& json, size_t start, size_t end, OpEntry& e)
{
    const std::string rkey = "\"ret_type\":\"";
    const size_t rp = json.find(rkey, start);
    if (rp < end) parse_string(json, rp + rkey.size() - 1, e.ret_type);

    const std::string akey = "\"args\":[";
    const size_t ap = json.find(akey, start);
    if (ap >= end) return;
    const std::string tkey = "\"type\":\"";
    size_t p = skip_ws(json, ap + akey.size());
    while (p < end && json[p] == '{') {
        const size_t oend = skip_object(json, p);
        if (oend == std::string::npos || oend > end) break;
        std::string t;
        const size_t tp = json.find(tkey, p);
        if (tp < oend) parse_string(json, tp + tkey.size() - 1, t);
        e.args.push_back(t);
        p = skip_ws(json, oend);
        if (p < end && json[p] == ',') p = skip_ws(json, p + 1);
    }
}

// Flatten the instrument context JSON into the array of ops it declares.
// Function objects are {"name":"<f>","id":<M>,...}; argument objects are
// {"name":"<a>","type":"<t>"} so they are skipped by requiring "id" after
// the name (generated key order, name before id).
inline std::vector<OpEntry> parse_ops(const std::string& json)
{
    std::vector<OpEntry> ops;
    const std::string ckey = "\"class\":\"";
    size_t pos = 0;
    for (;;) {
        const size_t cpos = json.find(ckey, pos);
        if (cpos == std::string::npos) break;

        std::string class_name;
        // ckey includes the value's opening quote
        const size_t after_name = parse_string(json, cpos + ckey.size() - 1, class_name);
        if (after_name == std::string::npos) { pos = cpos + ckey.size(); continue; }

        const size_t next_class = json.find(ckey, after_name);
        const size_t cend = (next_class == std::string::npos) ? json.size() : next_class;

        const size_t cid_pos = json.find("\"id\":", after_name);
        if (cid_pos >= cend) break;
        uint32_t cid = 0;
        if (!parse_uint(json, cid_pos + 5, cid)) break;

        // Parse the "functions":[...] array of function objects.
        const size_t farr = json.find("\"functions\":[", cid_pos);
        if (farr >= cend) { pos = cend; continue; }
        size_t p = skip_ws(json, farr + 13);
        while (p < cend && json[p] == '{') {
            const size_t oend = skip_object(json, p);
            if (oend == std::string::npos || oend > cend) break;

            std::string func_name;
            const size_t after_fn = parse_string(json, p + 8, func_name);
            if (after_fn == std::string::npos) break;

            const size_t ip = json.find("\"id\":", after_fn);
            if (ip >= oend) break;
            uint32_t fid = 0;
            if (!parse_uint(json, ip + 5, fid)) break;

            OpEntry e;
            e.id = (cid << 16) | fid;
            e.class_id = static_cast<uint16_t>(cid);
            e.func_id = static_cast<uint16_t>(fid);
            e.class_name = class_name;
            e.func_name = func_name;
            fill_op_extras(json, after_fn, oend, e);
            ops.push_back(e);

            p = skip_ws(json, oend);
            if (p < cend && json[p] == ',') p = skip_ws(json, p + 1);
        }
        pos = cend;
    }
    return ops;
}

// Op lookup table built from the context JSON: the id -> name array checked
// by every KoheronClient::call<...>.
struct OpTable {
    std::vector<OpEntry> ops;
    std::unordered_map<uint32_t, size_t> index; // id -> position in ops

    static OpTable build(const std::string& json) {
        OpTable t;
        t.ops = parse_ops(json);
        for (size_t i = 0; i < t.ops.size(); i++) {
            if (t.index.find(t.ops[i].id) == t.index.end()) {
                t.index[t.ops[i].id] = i;
            }
        }
        return t;
    }

    const OpEntry* find(uint32_t id) const {
        const auto it = index.find(id);
        return (it == index.end()) ? nullptr : &ops[it->second];
    }

    bool has_class(uint16_t class_id) const {
        for (const auto& op : ops) {
            if (op.class_id == class_id) return true;
        }
        return false;
    }

    std::string class_name_of(uint16_t class_id) const {
        for (const auto& op : ops) {
            if (op.class_id == class_id) return op.class_name;
        }
        return std::string();
    }

    // Lookup by "Class::func", or by bare "func" (first class serving it).
    const OpEntry* by_name(const std::string& class_func) const {
        const size_t sep = class_func.find("::");
        for (const auto& op : ops) {
            if (sep == std::string::npos) {
                if (op.func_name == class_func) return &op;
            } else if (op.class_name == class_func.substr(0, sep) &&
                       op.func_name == class_func.substr(sep + 2)) {
                return &op;
            }
        }
        return nullptr;
    }
};

} // namespace op_detail

// ==================================================
// Compile-time op names (C++20 class-type template parameters)
//
// Builds "Class::func" as a literal template argument:
//
//   client.call_by_name<KOHERON_OP(DataMover, get_fifo_count)>(val);
//   auto v = client.recv_by_name<KOHERON_OP(DataMover, get_fifo_count), uint32_t>();
//
// KOHERON_OP takes the driver class and the method-name identifier: the
// __func__ variable cannot reach the preprocessor (#__func__ stringifies to
// "__func__"), so the method name is passed as a macro token and stringified
// together with the class name; adjacent literal concatenation happens at
// compile time. std::string itself is still unusable as a template parameter
// in some standard libraries (no constexpr destructor), hence fixed_string.
// Requires C++20.
// ==================================================
namespace op_detail {

template<size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) {
        for (size_t i = 0; i < N; i++) data[i] = s[i];
    }
    constexpr const char* c_str() const { return data; }
    constexpr size_t size() const { return N - 1; }
    constexpr operator std::string_view() const { return std::string_view(data, N - 1); }
    // Compare template arguments by value, not by address.
    template<size_t M>
    friend constexpr bool operator==(fixed_string a, fixed_string<M> b) {
        return std::string_view(a) == std::string_view(b);
    }
};

} // namespace op_detail

#define KOHERON_OP(Class, Func) #Class "::" #Func

// Runtime "Class::func" name with the method name taken from __func__:
// nothing is typed twice, so the call site cannot get the function name
// wrong (it must equal the instrument function name; a mismatch throws
// op_check_error at call time). Usable only with the runtime const char*
// APIs, never as a template argument: __func__ is a runtime array and
// #__func__ would stringify to the literal "__func__".
//   client.call_by_name(KOHERON_OP_FUNC(DataMover), val);
//   auto v = client.recv_by_name<uint32_t>(KOHERON_OP_FUNC(DataMover));
#define KOHERON_OP_FUNC(Class) (std::string(#Class) + "::" + __func__).c_str()

class KoheronClient
{
  public:
    KoheronClient(const char *host_, int port_)
    : sockfd(koheron_http::invalid_socket)
    , host(host_)
    , port(port_)
    , rcv_buffer(0)
    , send_buffer(0)
    {
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = inet_addr(host);
        serveraddr.sin_port = htons(port);
    }

    /// @brief Constructor checking that a given firmware image is running.
    /// @details connect() validates that 'firmware_' is the live instrument
    /// (equivalent of python koheron.connect(host, name=firmware, restart=...)):
    /// when it is not live it is loaded from the board store; when it is not
    /// in the store an instrument_error is thrown listing the available
    /// instruments. An empty firmware name skips the validation.
    KoheronClient(const char *host_, int port_, const char *firmware_, bool restart_ = false)
    : KoheronClient(host_, port_)
    {
        firmware = (firmware_ != nullptr) ? firmware_ : "";
        firmware_restart = restart_;
    }

    ~KoheronClient() {
        close();
    }

    void close() {
        if (sockfd > 0) {
#ifdef _WIN32
            if (shutdown(sockfd, SD_BOTH) < 0) {
                WSACleanup();
                closesocket(sockfd);
                throw socket_error("Cannot shutdown socket\n");
            }
            closesocket(sockfd);
#else
            if (shutdown(sockfd, SHUT_RDWR) < 0) {
                ::close(sockfd);
                throw socket_error("Cannot shutdown socket\n");
            }
            ::close(sockfd);
#endif
        }
    }

    /// @brief HTTP port of the instrument API used by firmware validation
    /// (default 80, the nginx/uwsgi endpoint of koheron-board)
    void set_http_port(int p) { http_port = p; }

    // ----------------------------------------
    // Instrument context / op id checks
    // ----------------------------------------

    /// Context JSON served by the instrument (device 1, function 1, like
    /// python load_devices()). Fetched on first use and cached.
    const ServerContext& server_context(int timeout_ms = 2000);

    /// Fetch the instrument context again (after loading another instrument).
    void refresh_server_context(int timeout_ms = 2000);

    /// Inject a context already read elsewhere (e.g. before connecting).
    void set_server_context(const ServerContext& ctx);

    /// Equivalent of python KoheronClient.get_ids(device, command): ids the
    /// instrument declares for a named class/func. False when not served.
    bool find_op(const std::string& class_name, const std::string& func_name,
                 uint16_t& class_id, uint16_t& func_id);

    /// Throws op_check_error unless the template op id (from operations.hpp)
    /// matches the id the instrument declares for that class::func.
    template<uint32_t id>
    void check_op(const char* class_name, const char* func_name);

    /// Check a table of ops, one message per mismatch (empty when all match).
    std::vector<std::string> check_ops(const std::vector<ExpectedOp>& ops);

    /// Array of all ops (id + class/func name) declared by the instrument,
    /// built from the context downloaded at connect(). Empty before that.
    const std::vector<op_detail::OpEntry>& served_ops() const;

    /// True when the instrument declares an op with this id.
    bool op_served(uint32_t id) const;

    /// "Class::func" declared by the instrument for this id, "" when absent.
    std::string served_op_name(uint32_t id) const;

    // ----------------------------------------
    // Name-checked call/recv: pass __func__
    //
    // __func__ is a runtime array and string literals are not template
    // arguments before C++20, so the caller's method name is passed as the
    // first function argument while the op id stays the template argument:
    //
    //   client.call_n<op::DataMover::get_fifo_count>(__func__);
    //   auto v = client.recv_n<op::DataMover::get_fifo_count, uint32_t>(__func__);
    //
    // Checked automatically on every call: the instrument declares the id, its
    // class::func matches the passed name ("func" or "Class::func"), the
    // instrument-declared arg count matches the call, and each declared type
    // string matches the C++ argument/return types (against the instrument,
    // catching drift beyond the compile-time checks of operations.hpp).
    // ----------------------------------------

    template<uint32_t id, typename... Args>
    void call_n(const char* func_name, Args&&... args);

    template<uint32_t id, typename... Tp>
    decltype(auto) recv_n(const char* func_name);

    /// id declared by the instrument for "Class::func"; throws op_check_error.
    uint32_t op_id(const char* class_func) const;

    /// Call "Class::func" with the id resolved from the downloaded table.
    /// Without a compile-time id the C++ argument types cannot be checked
    /// against operations.hpp, so only the instrument-declared arg count and
    /// type strings are validated.
    template<typename... Args>
    void call_by_name(const char* class_func, Args&&... args);

    /// recv() with a runtime id: validated against the downloaded table and
    /// the instrument-declared return type (check_ret_types). The
    /// operations.hpp ret_type_t compile-time check only applies to
    /// recv<id, ...>().
    template<typename... Tp>
    decltype(auto) recv_rt(uint32_t id);

    // "Class::func" as a compile-time template argument:
    //   client.call_by_name<KOHERON_OP(DataMover, set_udp_streaming)>(val);
    //   auto v = client.recv_by_name<KOHERON_OP(DataMover, get_fifo_count), uint32_t>();

    template<op_detail::fixed_string Name, typename... Args>
    void call_by_name(Args&&... args);

    template<op_detail::fixed_string Name, typename... Tp>
    decltype(auto) recv_by_name();

    /// Runtime-name variant, e.g. with KOHERON_OP_FUNC (uses __func__).
    template<typename... Tp>
    decltype(auto) recv_by_name(const char* class_func);

    /// @brief Connect to koheron-server, first validating/loading the
    /// expected firmware when a firmware name was given at construction.
    /// @throws instrument_error when the expected firmware is not in the
    /// board store or does not become the live instrument
    void connect() {
        if (firmware.empty()) {
            connect_once();
            return;
        }

        bool will_run = true;
        try {
            will_run = (instrument_status(host, http_port).live_instrument != firmware) || firmware_restart;
        } catch (const std::exception&) {
            // HTTP status unavailable: run_instrument() below raises a meaningful error
        }

        // Throws instrument_error (listing available instruments) when the
        // requested firmware is not in the board store.
        run_instrument(host, firmware, firmware_restart, http_port);

        if (!will_run) {
            connect_once();
            return;
        }

        if (!wait_for_instrument(host, firmware, std::chrono::milliseconds(30000), http_port)) {
            throw instrument_error("Firmware " + firmware + " did not become the live instrument");
        }

        // (re)installing the firmware restarts koheron-server: retry the
        // TCP connection until the context server is back.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(30000);
        for (;;) {
            try {
                connect_once();
                break;
            } catch (const socket_error&) {
                if (std::chrono::steady_clock::now() >= deadline) throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }

    void connect_once() {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
            throw socket_error("WSAStartup failed");
        }

        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if (sockfd == INVALID_SOCKET) {
            WSACleanup();
            throw socket_error("Cannot open TCP socket\n");
        }

        int on = 1;

        if (::connect(sockfd, (SOCKADDR*) &serveraddr, sizeof serveraddr) == SOCKET_ERROR) {
            close();
            sockfd = INVALID_SOCKET;
            throw socket_error("Cannot connect to server\n");
        }
#else
        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if (sockfd < 0) {
            throw socket_error("Cannot open TCP socket\n");
        }

        int on = 1;

        if (::connect(sockfd, (struct sockaddr*) &serveraddr, sizeof serveraddr) < 0) {
            close();
            sockfd = -1;
            throw socket_error("Cannot connect to server\n");
        }
#endif

        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
                       (const char *)&on, sizeof(int)) < 0) {
            close();
            sockfd = -1;
            throw socket_error("Cannot set TCP_NODELAY option\n");
        }

        // Download the instrument op table (python load_devices() equivalent)
        // so that every subsequent call<id> is validated against the server.
        refresh_server_context();
    }

    template<uint32_t id, typename... Args>
    void call(Args&&... args) {
        static_assert(std::is_same<arg_types_t<id>, std::tuple<std::decay_t<Args>...>>::value,
                      "Invalid argument type for call");
        call<(id >> 16), id & 0xFFFF>(std::forward<Args>(args)...);
    }

    template<uint16_t class_id, uint16_t func_id, typename... Args>
    void call(Args&&... args) {
        static_assert(class_id > 0, "class_id 0 is reserved");
        check_served(class_id, func_id);

        last_class_id = class_id;
        last_func_id = func_id;
        dynamic_serializer.build_command<class_id, func_id>(send_buffer, std::forward<Args>(args)...);
        send();
    }

    /// call() with runtime ids, e.g. resolved from the instrument context
    /// (op_id()/call_by_name()). Validated against the downloaded table.
    template<typename... Args>
    void call_rt(uint16_t class_id, uint16_t func_id, Args&&... args) {
        check_served(class_id, func_id);

        last_class_id = class_id;
        last_func_id = func_id;
        dynamic_serializer.build_command_rt(send_buffer, class_id, func_id,
                                            std::forward<Args>(args)...);
        send();
    }

    // API that allocates dynamic containers and gives back ownership to caller
    template<uint32_t id, typename... Tp>
    decltype(auto) recv() {
        using Tup = std::tuple<Tp...>;
        using Tp0 = typename std::tuple_element<0, Tup>::type; // get first type of variadic list
        static_assert((sizeof...(Tp) == 1 && (
                          (std::is_same<ret_type_t<id>, Tp0>::value) ||
                          (serdes::is_c_string_v<ret_type_t<id>>
                           && std::is_same<std::string, Tp0>::value))) ||
                      (sizeof...(Tp) > 1 && std::is_same<ret_type_t<id>, Tup>::value),
                      "Invalid receive type");

        return command_deserializer<Tp...>();
    }

    // API for preallocated dynamic containers (vector, string, ...)
    template<uint32_t id, typename Container>
    void recv(Container& cont) {
        // C strings are received into std::string
        static_assert(std::is_same<ret_type_t<id>, std::decay_t<Container>>::value
                      || (serdes::is_c_string_v<ret_type_t<id>>
                          && std::is_same<std::string, std::decay_t<Container>>::value),
                      "Invalid container for receive");
        command_deserializer(cont);
    }

  private:
    socket_t sockfd;
    sockaddr_in_t serveraddr;

    const char *host;
    int port;

    int http_port = 80;         ///< HTTP API port used for firmware validation

    std::string firmware;       ///< Expected live instrument (empty: no check)
    bool firmware_restart = false; ///< Reload the firmware even when already live

    std::shared_ptr<ServerContext> server_ctx; ///< Cached instrument context
    std::shared_ptr<op_detail::OpTable> op_table; ///< Downloaded id -> name array

    /// Throw op_check_error unless the instrument declares (class_id, func_id)
    /// in the table downloaded at connect(). No-op when no table was fetched.
    void check_served(uint16_t class_id, uint16_t func_id) const;

    /// Throw op_check_error unless the instrument's name for id matches the
    /// caller-supplied method name (__func__, "func" or "Class::func").
    void check_op_name(uint32_t id, const char* func_name) const;

    /// Throw op_check_error unless the instrument-declared arg list of id
    /// matches the C++ argument pack (count and canonical type strings).
    template<typename... Args>
    void check_arg_types(uint32_t id) const;

    /// Throw op_check_error unless the instrument-declared return type of id
    /// matches the C++ receive type pack.
    template<typename... Tp>
    void check_ret_types(uint32_t id) const;

    uint16_t last_class_id = 0;
    uint16_t last_func_id = 0;

    std::vector<unsigned char> rcv_buffer;
    std::vector<unsigned char> send_buffer;

    serdes::DynamicSerializer<1024> dynamic_serializer;

  private:
    static constexpr auto header_size = serdes::required_buffer_size<uint32_t, uint16_t, uint16_t>();

    void send() {
        int err = ::send(sockfd, reinterpret_cast<const char*>(send_buffer.data()), send_buffer.size(), 0);
#ifdef _WIN32
        if (err == SOCKET_ERROR) {
            throw socket_error("Cannot send command to koheron-server\n");
        }
#else
        if (err < 0) {
            throw socket_error("Cannot send command to koheron-server\n");
        }
#endif
    }

    void recv_all(int n_bytes) {
        rcv_buffer.resize(n_bytes);
        int bytes_rcv = 0;
        int bytes_read = 0;

        while (bytes_read < n_bytes) {
            bytes_rcv = ::recv(sockfd, reinterpret_cast<char*>(rcv_buffer.data() + bytes_read), n_bytes - bytes_read, 0);

            if (bytes_rcv == 0)
                // Technically not really an error.
                throw socket_error("Connection closed by koheron-server\n");

            if (bytes_rcv < 0)
                throw socket_error("Cannot receive data\n");

            bytes_read += bytes_rcv;
        }

        assert(bytes_read == n_bytes);
    }

    // ---------------------------
    // Commands deserializer
    // ---------------------------

    // http://stackoverflow.com/questions/777261/avoiding-unused-variables-warnings-when-using-assert-in-a-release-build
    #define _unused(x) ((void)(x))

    void check_returned_header() {
        const auto t = serdes::deserialize<0, uint32_t, uint16_t, uint16_t>(rcv_buffer.data());
        assert(std::get<0>(t) == 0); // RESERVED
        assert(std::get<1>(t) == last_class_id);
        assert(std::get<2>(t) == last_func_id);
        _unused(t);
    }

    template<typename Tp>
    std::enable_if_t<std::is_scalar<Tp>::value, Tp>
    command_deserializer() {
        recv_all(serdes::required_buffer_size<uint32_t, uint16_t, uint16_t, Tp>());
        check_returned_header();
        return std::get<0>(serdes::deserialize<0, Tp>(rcv_buffer.data() + header_size));
    }

    template<typename Tp>
    std::enable_if_t<std::is_same<
                        Tp, std::array<typename Tp::value_type, std::tuple_size<Tp>::value>
                    >::value, const Tp&>
    command_deserializer() {
        using T = typename Tp::value_type;
        constexpr auto N = std::tuple_size<Tp>::value;

        recv_all(header_size + serdes::size_of<T, N>);
        check_returned_header();
        const auto p = reinterpret_cast<const Tp*>(rcv_buffer.data() + header_size);
        assert(p->data() == (const T*)(rcv_buffer.data() + header_size));
        return *p;
    }

    template<typename Tp>
    std::enable_if_t<std::is_same<Tp, std::vector<typename Tp::value_type>>::value, Tp>
    command_deserializer() {
        using T = typename Tp::value_type;

        get_payload_dynamic();
        const auto data = reinterpret_cast<const T*>(rcv_buffer.data());
        const auto length = rcv_buffer.size() / sizeof(T);

        Tp vec(length);
        std::move(data, data + length, vec.begin());
        return vec;
    }

    template<typename Tp>
    std::enable_if_t<std::is_same<Tp, std::string>::value, Tp>
    command_deserializer() {
        get_payload_dynamic();
        return std::string(reinterpret_cast<const char*>(rcv_buffer.data()), rcv_buffer.size());
    }

    template<typename... Tp>
    std::enable_if_t< 1 < sizeof...(Tp), std::tuple<Tp...>>
    command_deserializer() {
        recv_all(serdes::required_buffer_size<uint32_t, uint16_t, uint16_t, Tp...>());
        check_returned_header();
        return serdes::deserialize<0, Tp...>(rcv_buffer.data() + header_size);
    }

    void get_payload_dynamic() {
        recv_all(serdes::required_buffer_size<uint32_t, uint16_t, uint16_t, uint32_t>());
        check_returned_header();
        recv_all(std::get<0>(serdes::deserialize<0, uint32_t>(rcv_buffer.data() + header_size)));
    }

    // Prealocated containers
    template<typename Tp>
    std::enable_if_t<std::is_same<Tp, std::vector<typename Tp::value_type>>::value, void>
    command_deserializer(Tp& vec) {
        get_payload_dynamic();
        const auto data = reinterpret_cast<const typename Tp::value_type*>(rcv_buffer.data());
        const auto length = rcv_buffer.size() / sizeof(typename Tp::value_type);
        vec.resize(length);
        std::move(data, data + length, vec.begin());
    }

    template<typename Tp>
    std::enable_if_t<std::is_same<Tp, std::string>::value, void>
    command_deserializer(Tp& str) {
        get_payload_dynamic();
        str.resize(rcv_buffer.size());
        std::move(rcv_buffer.begin(), rcv_buffer.end(), str.begin());
    }
};

// Quick check that the TCP command server (koheron-server) is accepting
// connections, i.e. the instrument context is loaded.
inline bool context_server_up(const std::string& host, int port = 36000, int timeout_ms = 1000) {
    try {
        const socket_t fd = koheron_http::tcp_connect(host, port, timeout_ms);
        koheron_http::close_socket(fd);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Context served by koheron-server on the TCP command port
// (equivalent of python KoheronClient.check_version + load_devices).
struct ServerContext {
    std::string version;  // Context::get_version()
    std::string context;  // Context::context_json_string()
};

namespace context_detail {

inline void recv_exact(socket_t fd, unsigned char* p, size_t n) {
    size_t got = 0;
    while (got < n) {
        const int r = ::recv(fd, reinterpret_cast<char*>(p + got), n - got, 0);
        if (r == 0) throw socket_error("Connection closed by koheron-server\n");
        if (r < 0) throw socket_error("Cannot receive data\n");
        got += static_cast<size_t>(r);
    }
}

inline void send_command(socket_t fd, uint16_t class_id, uint16_t func_id) {
    unsigned char cmd[8];
    serdes::append<uint32_t>(cmd, 0);
    serdes::append<uint16_t>(cmd + 4, class_id);
    serdes::append<uint16_t>(cmd + 6, func_id);
    koheron_http::send_all(fd, reinterpret_cast<const char*>(cmd), sizeof(cmd));
}

// Dynamic payload reply: reserved(4) class(2) func(2) length(4) + length bytes
inline std::string recv_string(socket_t fd, uint16_t func_id) {
    unsigned char hdr[12];
    recv_exact(fd, hdr, sizeof(hdr));
    const uint32_t reserved = serdes::extract<uint32_t>(hdr);
    const uint16_t class_id = serdes::extract<uint16_t>(hdr + 4);
    const uint16_t func = serdes::extract<uint16_t>(hdr + 6);
    const uint32_t length = serdes::extract<uint32_t>(hdr + 8);

    if (reserved != 0 || class_id != 1 || func != func_id) {
        throw socket_error("Unexpected reply from koheron-server\n");
    }

    std::string s(length, '\0');
    if (length > 0) {
        recv_exact(fd, reinterpret_cast<unsigned char*>(&s[0]), length);
    }
    return s;
}

} // namespace context_detail

// Read the version and the context JSON served by the instrument on the
// TCP command port (device id 1 is the reserved Context device).
inline ServerContext read_server_context(const std::string& host, int port = 36000, int timeout_ms = 2000) {
    const socket_t fd = koheron_http::tcp_connect(host, port, timeout_ms);
    ServerContext ctx;
    try {
        context_detail::send_command(fd, 1, 0); // Context::get_version
        ctx.version = context_detail::recv_string(fd, 0);
        context_detail::send_command(fd, 1, 1); // Context::context_json_string
        ctx.context = context_detail::recv_string(fd, 1);
    } catch (...) {
        koheron_http::close_socket(fd);
        throw;
    }
    koheron_http::close_socket(fd);
    return ctx;
}

inline const ServerContext& KoheronClient::server_context(int timeout_ms) {
    if (server_ctx == nullptr) refresh_server_context(timeout_ms);
    return *server_ctx;
}

inline void KoheronClient::refresh_server_context(int timeout_ms) {
    server_ctx = std::make_shared<ServerContext>(read_server_context(host, port, timeout_ms));
    op_table = std::make_shared<op_detail::OpTable>(op_detail::OpTable::build(server_ctx->context));
}

inline void KoheronClient::set_server_context(const ServerContext& ctx) {
    server_ctx = std::make_shared<ServerContext>(ctx);
    op_table = std::make_shared<op_detail::OpTable>(op_detail::OpTable::build(ctx.context));
}

inline const std::vector<op_detail::OpEntry>& KoheronClient::served_ops() const {
    static const std::vector<op_detail::OpEntry> empty;
    return (op_table != nullptr) ? op_table->ops : empty;
}

inline bool KoheronClient::op_served(uint32_t id) const {
    return op_table != nullptr && op_table->find(id) != nullptr;
}

inline std::string KoheronClient::served_op_name(uint32_t id) const {
    if (op_table == nullptr) return std::string();
    const op_detail::OpEntry* e = op_table->find(id);
    return (e == nullptr) ? std::string() : e->class_name + "::" + e->func_name;
}

inline void KoheronClient::check_served(uint16_t class_id, uint16_t func_id) const {
    if (op_table == nullptr) return; // context not fetched: nothing to check yet
    const uint32_t id = (static_cast<uint32_t>(class_id) << 16) | func_id;
    if (op_table->find(id) != nullptr) return;

    std::string msg;
    if (op_table->has_class(class_id)) {
        msg = "call<>: " + op_table->class_name_of(class_id) + " does not serve func " +
              std::to_string(func_id) + " (id " + std::to_string(id) + ")";
    } else {
        msg = "call<>: class " + std::to_string(class_id) +
              " is not served by the instrument (id " + std::to_string(id) + ")";
    }
    throw op_check_error(msg);
}

inline void KoheronClient::check_op_name(uint32_t id, const char* func_name) const {
    if (op_table == nullptr) return;
    const op_detail::OpEntry* e = op_table->find(id);
    const std::string name = (func_name != nullptr) ? func_name : "";
    if (e == nullptr) {
        std::string msg = "call_n/recv_n: id " + std::to_string(id) +
                          " not served by instrument";
        if (!name.empty()) {
            const op_detail::OpEntry* by_name = op_table->by_name(name);
            if (by_name != nullptr) {
                msg += ": '" + name + "' is class " + std::to_string(by_name->class_id) +
                       "/func " + std::to_string(by_name->func_id);
            }
        }
        throw op_check_error(msg);
    }
    if (name.empty()) return;
    if (e->func_name != name && (e->class_name + "::" + e->func_name) != name) {
        throw op_check_error("name/id mismatch: caller reports '" + name +
                             "' but id " + std::to_string(id) + " is " +
                             e->class_name + "::" + e->func_name);
    }
}

template<typename... Args>
inline void KoheronClient::check_arg_types(uint32_t id) const {
    if (op_table == nullptr) return;
    const op_detail::OpEntry* e = op_table->find(id);
    if (e == nullptr) return; // reported by check_served/check_op_name
    if (sizeof...(Args) != e->args.size()) {
        throw op_check_error(e->class_name + "::" + e->func_name + " takes " +
                             std::to_string(e->args.size()) + " args, call passes " +
                             std::to_string(sizeof...(Args)));
    }
    const std::vector<std::string> given{
        op_detail::type_str<typename std::decay<Args>::type>()...};
    size_t i = 0;
    for (const auto& want : e->args) {
        const std::string got = given[i];
        const std::string w = op_detail::canonical_type(want);
        if (!want.empty() && !got.empty() && got != w) {
            throw op_check_error(e->class_name + "::" + e->func_name + " arg " +
                                 std::to_string(i) + ": instrument declares '" + want +
                                 "' but call passes '" + given[i] + "'");
        }
        i++;
    }
}

template<typename... Tp>
inline void KoheronClient::check_ret_types(uint32_t id) const {
    if (op_table == nullptr) return;
    const op_detail::OpEntry* e = op_table->find(id);
    if (e == nullptr || e->ret_type.empty() || sizeof...(Tp) != 1) return;
    const std::vector<std::string> given{ op_detail::type_str<Tp>()... };
    if (given[0].empty()) return;
    if (given[0] != op_detail::canonical_type(e->ret_type)) {
        throw op_check_error(e->class_name + "::" + e->func_name + " returns '" +
                             e->ret_type + "', recv expects '" + given[0] + "'");
    }
}

template<uint32_t id, typename... Args>
inline void KoheronClient::call_n(const char* func_name, Args&&... args) {
    static_assert(std::is_same<arg_types_t<id>, std::tuple<std::decay_t<Args>...>>::value,
                  "Invalid argument type for call_n");
    check_op_name(id, func_name);
    check_arg_types<Args...>(id);
    call<id>(std::forward<Args>(args)...);
}

template<uint32_t id, typename... Tp>
inline decltype(auto) KoheronClient::recv_n(const char* func_name) {
    check_op_name(id, func_name);
    check_ret_types<Tp...>(id);
    return recv<id, Tp...>();
}

inline uint32_t KoheronClient::op_id(const char* class_func) const {
    if (op_table == nullptr || class_func == nullptr) {
        throw op_check_error(std::string("op_id: no instrument context (asked for '") +
                             (class_func != nullptr ? class_func : "?") + "')");
    }
    const op_detail::OpEntry* e = op_table->by_name(class_func);
    if (e == nullptr) {
        throw op_check_error(std::string("op_id: instrument does not declare '") +
                             class_func + "'");
    }
    return e->id;
}

template<typename... Args>
inline void KoheronClient::call_by_name(const char* class_func, Args&&... args) {
    const uint32_t id = op_id(class_func);
    check_arg_types<Args...>(id);
    call_rt(static_cast<uint16_t>(id >> 16), static_cast<uint16_t>(id & 0xFFFF),
            std::forward<Args>(args)...);
}

template<typename... Tp>
inline decltype(auto) KoheronClient::recv_rt(uint32_t id) {
    check_served(static_cast<uint16_t>(id >> 16), static_cast<uint16_t>(id & 0xFFFF));
    check_ret_types<Tp...>(id);
    return command_deserializer<Tp...>();
}

template<op_detail::fixed_string Name, typename... Args>
inline void KoheronClient::call_by_name(Args&&... args) {
    const uint32_t id = op_id(Name.c_str());
    check_arg_types<Args...>(id);
    call_rt(static_cast<uint16_t>(id >> 16), static_cast<uint16_t>(id & 0xFFFF),
            std::forward<Args>(args)...);
}

template<op_detail::fixed_string Name, typename... Tp>
inline decltype(auto) KoheronClient::recv_by_name() {
    return recv_rt<Tp...>(op_id(Name.c_str()));
}

template<typename... Tp>
inline decltype(auto) KoheronClient::recv_by_name(const char* class_func) {
    return recv_rt<Tp...>(op_id(class_func));
}

inline bool KoheronClient::find_op(const std::string& class_name, const std::string& func_name,
                                   uint16_t& class_id, uint16_t& func_id)
{
    return op_detail::find_op_in_json(server_context().context, class_name, func_name,
                                      class_id, func_id);
}

template<uint32_t id>
inline void KoheronClient::check_op(const char* class_name, const char* func_name) {
    uint16_t server_class_id = 0, server_func_id = 0;
    if (!find_op(class_name, func_name, server_class_id, server_func_id)) {
        throw op_check_error(std::string("Op ") + class_name + "::" + func_name +
                             " not found in the instrument context");
    }
    constexpr uint16_t want_class_id = static_cast<uint16_t>(id >> 16);
    constexpr uint16_t want_func_id = static_cast<uint16_t>(id & 0xFFFF);
    if (server_class_id != want_class_id || server_func_id != want_func_id) {
        throw op_check_error(std::string("Op id mismatch for ") + class_name + "::" + func_name +
                             ": client uses class " + std::to_string(want_class_id) +
                             "/func " + std::to_string(want_func_id) +
                             " but instrument serves class " + std::to_string(server_class_id) +
                             "/func " + std::to_string(server_func_id));
    }
}

inline std::vector<std::string> KoheronClient::check_ops(const std::vector<ExpectedOp>& ops)
{
    std::vector<std::string> errors;
    for (const auto& op : ops) {
        uint16_t server_class_id = 0, server_func_id = 0;
        const std::string name = std::string(op.class_name) + "::" + op.func_name;
        if (!find_op(op.class_name, op.func_name, server_class_id, server_func_id)) {
            errors.push_back(name + " not found in the instrument context");
            continue;
        }
        const uint16_t want_class_id = static_cast<uint16_t>(op.id >> 16);
        const uint16_t want_func_id = static_cast<uint16_t>(op.id & 0xFFFF);
        if (server_class_id != want_class_id || server_func_id != want_func_id) {
            errors.push_back(name + ": client class/func " +
                             std::to_string(want_class_id) + "/" + std::to_string(want_func_id) +
                             " != instrument " + std::to_string(server_class_id) + "/" +
                             std::to_string(server_func_id));
        }
    }
    return errors;
}

// True when the class registered in the instrument context is present.
inline bool context_has_class(const ServerContext& ctx, const std::string& class_name) {
    const std::string q = "\"" + class_name + "\"";
    for (size_t pos = ctx.context.find(q); pos != std::string::npos; pos = ctx.context.find(q, pos + 1)) {
        size_t k = pos;
        while (k > 0 && koheron_json::is_ws(ctx.context[k - 1])) k--;
        if (k == 0 || ctx.context[k - 1] != ':') continue;
        k--;
        while (k > 0 && koheron_json::is_ws(ctx.context[k - 1])) k--;
        if (k >= 6 && ctx.context.compare(k - 6, 6, "class") == 0) return true;
    }
    return false;
}

// Full check that everything is loaded:
//  1. the HTTP API reports 'name' as the live instrument
//  2. the context server answers on the TCP command port (version + context JSON)
//  3. every expected driver class is present in the context
inline bool check_instrument_loaded(const std::string& host, const std::string& name,
                                    const std::vector<std::string>& expected_classes = std::vector<std::string>(),
                                    int http_port = 80, int server_port = 36000) {
    try {
        if (!instrument_status(host, http_port).is_live(name)) return false;
        const ServerContext ctx = read_server_context(host, server_port);
        if (ctx.version.empty() || ctx.context.empty()) return false;
        for (const auto& class_name : expected_classes) {
            if (!context_has_class(ctx, class_name)) return false;
        }
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

// Equivalent of python koheron.connect(): run the instrument if needed,
// wait for it to become live, and return a connected KoheronClient.
inline std::unique_ptr<KoheronClient> connect_instrument(const std::string& host,
                                                         const std::string& name = std::string(),
                                                         bool restart = false,
                                                         int http_port = 80, int port = 36000) {
    // KoheronClient::connect() validates the firmware on the default HTTP
    // port; for a custom one do it here and let the client only open TCP.
    std::string firmware;
    if (!name.empty()) {
        if (http_port == 80) {
            firmware = name;
        } else {
            run_instrument(host, name, restart, http_port);
            wait_for_instrument(host, name, std::chrono::milliseconds(30000), http_port);
        }
    }
    std::unique_ptr<KoheronClient> client(
        new KoheronClient(host.c_str(), port, firmware.c_str(), restart));
    client->connect();
    return client;
}

#endif // __KOHERON_CLIENT_HPP__


