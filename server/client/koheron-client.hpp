/// (c) Koheron

#ifndef __KOHERON_CLIENT_HPP__
#define __KOHERON_CLIENT_HPP__

#include <vector>
#include <array>
#include <tuple>
#include <type_traits>
#include <string>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <system_error>

#ifdef _WIN32
  /* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0501  /* Windows XP. */
  #endif
  #include <winsock2.h>
#else
extern "C" {
  #include <sys/socket.h>   // socket definitions
  #include <sys/types.h>    // socket types
  #include <arpa/inet.h>    // inet (3) functions
  #include <netinet/tcp.h>
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

class KoheronClient
{
  public:
    KoheronClient(const char *host_, int port_)
    : host(host_)
    , port(port_)
    , rcv_buffer(0)
    , send_buffer(0)
    {
        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sin_family = AF_INET;
        serveraddr.sin_addr.s_addr = inet_addr(host);
        serveraddr.sin_port = htons(port);
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

    void connect() {
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

        last_class_id = class_id;
        last_func_id = func_id;
        dynamic_serializer.build_command<class_id, func_id>(send_buffer, std::forward<Args>(args)...);
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

#endif // __KOHERON_CLIENT_HPP__
