// (c) Koheron
// metadata.hpp - PMR-backed, thread-safe key/value store

#ifndef __SERVER_UTILITIES_METADATA_HPP__
#define __SERVER_UTILITIES_METADATA_HPP__

#include <array>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <memory_resource>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace ut {

template <std::size_t SeedBytes = 1024>
class Metadata {
  public:
    using string_t = std::pmr::string;
    using key_t    = std::pmr::string;
    using value_t  = std::variant<int64_t, double, bool, string_t>; // Stored types
    using map_t    = std::pmr::unordered_map<key_t, value_t>;

    explicit Metadata(std::pmr::memory_resource* upstream = std::pmr::get_default_resource())
    : pool_(seed_.data(), seed_.size(), upstream)
    , map_{ &pool_ } {}

    // ---------------- setters ----------------
    void set(std::string_view key, std::string_view value) {
        std::scoped_lock lk(mtx_);
        map_[key_t{key, &pool_}] = value_t{ string_t{value, &pool_} };
    }

    void set(std::string_view key, const string_t& value) {
        std::scoped_lock lk(mtx_);
        map_[key_t{key, &pool_}] = value_t{ string_t{value, &pool_} };
    }

    void set(std::string_view key, bool v) {
        std::scoped_lock lk(mtx_);
        map_[key_t{key, &pool_}] = v;
    }

    template <std::floating_point T>
    void set(std::string_view key, T v) {
        std::scoped_lock lk(mtx_);
        map_[key_t{key, &pool_}] = static_cast<double>(v);
    }

    template <std::integral T>
    requires (!std::same_as<T, bool>)
    void set(std::string_view key, T v) {
        std::scoped_lock lk(mtx_);
        map_[key_t{key, &pool_}] = static_cast<std::int64_t>(v);
    }

    void set(std::string_view key, value_t v) {
        std::scoped_lock lk(mtx_);
        if (auto s = std::get_if<string_t>(&v)) {
            v = string_t{*s, &pool_}; // rebind into our arena
        }
        map_[key_t{key, &pool_}] = std::move(v);
    }

    template <typename T>
    static constexpr bool is_direct_alt =
        std::same_as<T, std::int64_t> ||
        std::same_as<T, double>       ||
        std::same_as<T, bool>         ||
        std::same_as<T, string_t>; // pmr::string

    template <typename T>
    std::optional<T> get(std::string_view key) const {
        std::scoped_lock lk(mtx_);
        auto it = map_.find(key_t{key, &pool_});
        if (it == map_.end())
            return std::nullopt;

        // 1) Only try direct retrieval if T is *exactly* one of the alternatives.
        if constexpr (is_direct_alt<T>) {
            if (auto p = std::get_if<T>(&it->second))
                return *p;
        }

        // 2) Integral widening/narrowing via int64_t storage
        if constexpr (std::integral<T> && (!std::same_as<T, bool>)) {
            if (auto q = std::get_if<std::int64_t>(&it->second))
                return static_cast<T>(*q);
        }

        // 3) Floating-point from double (or long double if you chose that)
        if constexpr (std::floating_point<T>) {
            if (auto q = std::get_if<double>(&it->second))
                return static_cast<T>(*q);
        }

        // 4) Conversions from pmr::string
        if constexpr (std::same_as<T, std::string>) {
            if (auto q = std::get_if<string_t>(&it->second))
                return std::string{q->data(), q->size()};
        }
        if constexpr (std::same_as<T, std::string_view>) {
            if (auto q = std::get_if<string_t>(&it->second))
                return std::string_view{*q}; // valid while metadata store lives
        }

        return std::nullopt;
    }

    bool has(std::string_view key) const {
        std::scoped_lock lk(mtx_);
        return map_.find(key_t{key, &pool_}) != map_.end();
    }

    void erase(std::string_view key) {
        std::scoped_lock lk(mtx_);
        map_.erase(key_t{key, &pool_});
        // Note: monotonic pool doesn't reclaim; call clear(true) to release.
    }

    void clear(bool release_arena = false) {
        std::scoped_lock lk(mtx_);
        map_.clear();
        if (release_arena) {
            pool_.release(); // invalidates all strings from this store
        }
    }

    const map_t& all() const { return map_; }
    std::pmr::memory_resource* resource() noexcept { return &pool_; }

  private:
    mutable std::mutex mtx_;
    std::array<std::byte, SeedBytes> seed_{};
    mutable std::pmr::monotonic_buffer_resource pool_;
    map_t map_;
};

} // namespace ut

#endif // __SERVER_UTILITIES_METADATA_HPP__
