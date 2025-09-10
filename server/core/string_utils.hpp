/// (c) Koheron

#ifndef __STRING_UTILS_HPP__
#define __STRING_UTILS_HPP__

#include <cstring>
#include <string>
#include <array>
#include <syslog.h>

namespace koheron {

// -------------------------------------------------------------------------
// Variadic string formating functions accepting parameter packs
// -------------------------------------------------------------------------

// printf
template<typename... Args>
typename std::enable_if_t< 0 < sizeof...(Args), void >
printf(const char *fmt, Args&&... args) {
    std::printf(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
typename std::enable_if_t< 0 == sizeof...(Args), void >
printf(const char *fmt, Args&&... args) {
    std::printf("%s", fmt);
}

// fprintf
template<typename... Args>
typename std::enable_if_t< 0 < sizeof...(Args), void >
fprintf(FILE *stream, const char *fmt, Args&&... args) {
    std::fprintf(stream, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
typename std::enable_if_t< 0 == sizeof...(Args), void >
fprintf(FILE *stream, const char *fmt, Args&&... args) {
    std::fprintf(stream, "%s", fmt);
}

// snprintf
template<typename... Args>
typename std::enable_if_t< 0 < sizeof...(Args), int >
snprintf(char *s, size_t n, const char *fmt, Args&&... args) {
    return std::snprintf(s, n, fmt, std::forward<Args>(args)...);
}

template<typename... Args>
typename std::enable_if_t< 0 == sizeof...(Args), int >
snprintf(char *s, size_t n, const char *fmt, Args&&... args) {
    return std::snprintf(s, n, "%s", fmt);
}

// syslog
template<int priority, typename... Args>
typename std::enable_if_t< 0 < sizeof...(Args), void >
syslog(const char *fmt, Args&&... args) {
    ::syslog(priority, fmt, std::forward<Args>(args)...);
}

template<int priority, typename... Args>
typename std::enable_if_t< 0 == sizeof...(Args), void >
syslog(const char *fmt, Args&&... args) {
    ::syslog(priority, "%s", fmt);
}

// -------------------------------------------------------------------------
// Compile-time string
// -------------------------------------------------------------------------

// http://stackoverflow.com/questions/15858141/conveniently-declaring-compile-time-strings-in-c
class str_const { // constexpr string
  private:
    const char* const p_;
    const std::size_t sz_;

  public:
    template<std::size_t N>
    explicit constexpr str_const(const char(&a)[N]) : // ctor
        p_(a), sz_(N-1) {}

    constexpr char operator[](std::size_t n) { // []
        return n < sz_ ? p_[n] :
        throw std::out_of_range("");
    }

    constexpr std::size_t size() const {return sz_;} // size()

    constexpr const char* data() const {return p_;}

    std::string to_string() const {return std::string(p_);}
};

template<class T, class... Tail, class Elem = typename std::decay<T>::type>
constexpr std::array<Elem,1+sizeof...(Tail)> make_array(T&& head, Tail&&... values)
{
  return {{ std::forward<T>(head), std::forward<Tail>(values)... }};
}

} // namespace koheron

#endif // __STRING_UTILS_HPP__