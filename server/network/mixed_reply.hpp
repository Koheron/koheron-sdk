#ifndef KOHERON_MIXED_REPLY_HPP
#define KOHERON_MIXED_REPLY_HPP

#include "server/network/serializer_deserializer.hpp"

namespace net {

// Small fields stay packed together; only sizeable raw containers are borrowed.
constexpr std::size_t mixed_reply_min_bytes = 4096;
constexpr std::size_t max_reply_parts = 32;

template<class T>
consteval std::size_t mixed_reply_containers() {
    using U = std::remove_cvref_t<T>;
    if constexpr (is_std_tuple_v<U>) {
        return []<std::size_t... I>(std::index_sequence<I...>) {
            return (std::size_t{0} + ... + mixed_reply_containers<std::tuple_element_t<I, U>>());
        }(std::make_index_sequence<std::tuple_size_v<U>>{});
    } else if constexpr (is_std_array_v<U>) {
        return std::is_trivially_copyable_v<typename U::value_type> &&
               sizeof(typename U::value_type) * std::tuple_size_v<U> >= mixed_reply_min_bytes;
    } else if constexpr (is_std_vector_v<U> || is_std_span_v<U>) {
        return std::is_trivially_copyable_v<typename U::value_type>;
    } else {
        return 0;
    }
}

template<std::size_t Capacity>
class MixedReply {
  public:
    explicit MixedReply(CommandBuilder& builder_) : builder(builder_) {}

    template<class T>
    void push(T&& value) {
        using U = std::remove_cvref_t<T>;
        if constexpr (is_std_tuple_v<U>) {
            std::apply([&](auto&&... fields) {
                (push(std::forward<decltype(fields)>(fields)), ...);
            }, std::forward<T>(value));
        } else if constexpr (mixed_reply_containers<U>() > 0) {
            const auto bytes = std::as_bytes(std::span{value.data(), value.size()});
            if (bytes.size() >= mixed_reply_min_bytes) {
                // Within a mixed reply, dynamic containers have a 32-bit length,
                // matching CommandBuilder (the standalone fast path is separate).
                if constexpr (!is_std_array_v<U>) {
                    builder.push_scalar(static_cast<uint32_t>(bytes.size()));
                }
                flush_metadata();
                parts[count++].bytes = bytes;
                borrowed = true;
                return;
            }
            builder.push_one(std::forward<T>(value));
        } else {
            builder.push_one(std::forward<T>(value));
        }
    }

    bool has_borrowed_payload() const { return borrowed; }

    auto finish() {
        flush_metadata();
        // Resolve offsets after all metadata is packed: vector growth can move it.
        const auto metadata = std::as_bytes(std::span{*builder.out});
        for (std::size_t i = 0; i < count; ++i) {
            views[i] = parts[i].length != 0
                ? metadata.subspan(parts[i].offset, parts[i].length) : parts[i].bytes;
        }
        return std::span<const std::span<const std::byte>>{views.data(), count};
    }

  private:
    struct Part {
        std::span<const std::byte> bytes{};
        std::size_t offset = 0, length = 0;
    };
    CommandBuilder& builder;
    std::array<Part, Capacity> parts{};
    std::array<std::span<const std::byte>, Capacity> views{};
    std::size_t count = 0, metadata_start = 0;
    bool borrowed = false;

    void flush_metadata() {
        const auto end = builder.out->size();
        if (end != metadata_start) {
            parts[count++] = {{}, metadata_start, end - metadata_start};
            metadata_start = end;
        }
    }
};

} // namespace net
#endif
