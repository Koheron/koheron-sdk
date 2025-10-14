// ini configuration files

#ifndef __SERVER_CONTEXT_CONFIG_INI_HPP__
#define __SERVER_CONTEXT_CONFIG_INI_HPP__

#include <string>
#include <string_view>
#include <map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cctype>
#include <vector>
#include <charconv>
#include <system_error>

namespace cfg {

struct ini {
    // data[section][key] = value
    std::map<std::string, std::map<std::string, std::string>> data;
};

inline std::string trim(std::string_view v) {
    auto l = v.begin(), r = v.end();
    while (l != r && std::isspace(static_cast<unsigned char>(*l))) ++l;
    while (r != l && std::isspace(static_cast<unsigned char>(*(r-1)))) --r;
    return std::string(l, r);
}

inline int load_ini(const std::filesystem::path& p, ini& out) {
    std::ifstream f(p);

    if (!f) {
        return -1;
    }

    std::string line, sect;

    while (std::getline(f, line)) {
        // strip comments (# or ;)
        auto hash = line.find('#');

        if (hash != std::string::npos) {
            line.resize(hash);
        }

        auto semi = line.find(';');
        
        if (semi != std::string::npos) {
            line.resize(semi);
        }

        auto s = trim(line);
        if (s.empty()) {
            continue;
        }

        if (s.front() == '[' && s.back() == ']') {
            sect = trim(std::string_view{s}.substr(1, s.size()-2));
            continue;
        }
    
        auto eq = s.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        
        auto k = trim(std::string_view{s}.substr(0, eq));
        auto v = trim(std::string_view{s}.substr(eq+1));
        out.data[sect][k] = v;
    }

    return 0;
}

template<class T>
inline bool parse_to(std::string_view s, T& out);

// integral (not bool)
template<class T>
inline bool parse_to(std::string_view s, T& out)
requires(std::is_integral_v<T> && !std::is_same_v<T,bool>)
{
    s = trim(s);
    auto* b = s.data();
    auto* e = b + s.size();
    auto [p, ec] = std::from_chars(b, e, out, 10);
    return ec == std::errc{} && p == e;
}

// floating
template<class T>
inline bool parse_to(std::string_view s, T& out)
requires(std::is_floating_point_v<T>)
{
    s = trim(s);
    auto* b = s.data();
    auto* e = b + s.size();
    auto [p, ec] = std::from_chars(b, e, out, std::chars_format::general);
    return ec == std::errc{} && p == e;
}

// bool: accept 0/1 via from_chars, or strings like "true/on/yes"
template<>
inline bool parse_to<bool>(std::string_view s, bool& out)
{
    s = trim(s);
    int tmp{};
    if (parse_to<int>(s, tmp)) {
        if (tmp==0||tmp==1){
            out = (tmp!=0);
            return true;
        }
    }

    std::string t;
    t.reserve(s.size());
    for (char c : s) {
        t.push_back(std::tolower(static_cast<unsigned char>(c)));
    }

    if (t=="true" || t=="on" || t=="yes"){
        out = true; 
        return true;
    }

    if (t=="false" || t=="off" || t=="no"){
        out = false;
        return true;
    }

    return false;
}

// string passthrough
template<>
inline bool parse_to<std::string>(std::string_view s, std::string& out) {
    out = trim(s);
    return true;
}

// comma list helper
template<class T>
T get(const ini& cfg,
      const std::string& sect,
      const std::string& key,
      T def) {
    if (auto sit = cfg.data.find(sect); sit != cfg.data.end()) {
        if (auto kit = sit->second.find(key); kit != sit->second.end()) {
            T v{};
            if (parse_to<T>(kit->second, v)) {
                return v;
            }
        }
    }

    return def;
}

// --- setters / serialization ---

// Generic numeric -> string via to_chars
template<class T>
inline std::string to_string_num(T v)
requires(std::is_integral_v<T> && !std::is_same_v<T,bool>) {
    char buf[64];
    auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf), v, 10);
    return (ec == std::errc{}) ? std::string(buf, ptr) : std::string{};
}

template<class T>
inline std::string to_string_num(T v)
requires(std::is_floating_point_v<T>) {
    // Use general format with enough precision to round-trip
    char buf[128];
    auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf),
                                   v, std::chars_format::general,
                                   std::numeric_limits<T>::max_digits10);
    return (ec == std::errc{}) ? std::string(buf, ptr) : std::string{};
}

// Base "set raw" (stores as-is; you can choose to trim if you want)
inline void set(ini& cfg,
                const std::string& sect,
                const std::string& key,
                std::string_view value) {
    cfg.data[sect][key] = std::string(value);
}

// Overloads for numbers
template<class T>
inline void set(ini& cfg,
                const std::string& sect,
                const std::string& key,
                T value)
requires(std::is_integral_v<T> && !std::is_same_v<T,bool>) {
    cfg.data[sect][key] = to_string_num(value);
}

template<class T>
inline void set(ini& cfg,
                const std::string& sect,
                const std::string& key,
                T value)
requires(std::is_floating_point_v<T>) {
    cfg.data[sect][key] = to_string_num(value);
}

// Bool: human-friendly
inline void set(ini& cfg,
                const std::string& sect,
                const std::string& key,
                bool value) {
    cfg.data[sect][key] = value ? "true" : "false";
}

// String passthrough
inline void set(ini& cfg,
                const std::string& sect,
                const std::string& key,
                const std::string& value) {
    cfg.data[sect][key] = value;
}

// Small utilities (handy)
inline bool has(const ini& cfg,
                const std::string& sect,
                const std::string& key) {
    if (auto sit = cfg.data.find(sect); sit != cfg.data.end())
        return sit->second.find(key) != sit->second.end();
    return false;
}

inline void erase(ini& cfg,
                  const std::string& sect,
                  const std::string& key) {
    if (auto sit = cfg.data.find(sect); sit != cfg.data.end())
        sit->second.erase(key);
}

// atomic save: write tmp + rename
inline bool save_ini(const std::filesystem::path& p, const ini& in) {
    namespace fs = std::filesystem;

    fs::create_directories(p.parent_path());
    auto tmp = p; tmp += ".tmp";
    {
        std::ofstream f(tmp, std::ios::trunc);
        if (!f) {
            return false;
        }

        for (auto& [sec, kv] : in.data) {
            if (!sec.empty()) {
                f << '[' << sec << "]\n";
            }

            for (auto& [k, v] : kv) {
                f << k << " = " << v << "\n";
            }

            f << "\n";
        }

        f.flush();

        if (!f) {
            return false;
        }
    }

    fs::rename(tmp, p);
    return true;
}

} // namespace cfg

#endif // __SERVER_CONTEXT_CONFIG_INI_HPP__
