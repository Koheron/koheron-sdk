/// Memory map drivers
///
/// (c) Koheron

#ifndef __SERVER_CONTEXT_MEMORY_MAP_HPP__
#define __SERVER_CONTEXT_MEMORY_MAP_HPP__

#include "server/hardware/memory_catalog.hpp"
#include "server/hardware/system_ram.hpp"
#include "server/drivers/uio.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <tuple>
#include <memory>
#include <span>
#include <cstring>
#include <atomic>
#include <string_view>
#include <optional>
#include <fstream>
#include <expected>
#include <system_error>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace hw {

enum class MemoryOperation { reservation, open, map };

struct MemoryFailure {
    std::string_view device;
    MemoryOperation operation;
    std::error_code code;
};

struct MemoryOpenError {
    MemID id;
    MemoryFailure failure;
    std::optional<MemoryFailure> fallback_from;
};

inline std::string_view memory_operation_name(MemoryOperation operation) {
    switch (operation) {
        case MemoryOperation::reservation: return "check DMA reservation";
        case MemoryOperation::open: return "open";
        case MemoryOperation::map: return "mmap";
        default: return "unknown operation";
    }
}

inline void report_memory_open_error(const MemoryOpenError& error) {
    const auto& failure = error.failure;
    logf<ERROR>("Memory[{}] (id={}): {} {} failed: {}\n", mem::get_name(error.id), error.id,
                memory_operation_name(failure.operation), failure.device, failure.code.message());
    if (error.fallback_from) {
        const auto& original = *error.fallback_from;
        logf<ERROR>("Memory[{}]: fallback followed {} {} failure: {}\n", mem::get_name(error.id),
                    memory_operation_name(original.operation), original.device, original.code.message());
    }
    if (failure.operation == MemoryOperation::reservation) {
        logf<ERROR>("Memory[{}]: Cannot verify fixed DMA window exclusion from System RAM. "
                    "Check /proc/iomem and the exclusive no-map boot reservation.\n", mem::get_name(error.id));
    }
}

inline int memory_error_errno(const MemoryOpenError& error) {
    const auto& code = error.failure.code;
    return code.category() == std::generic_category() || code.category() == std::system_category()
        ? code.value() : EIO;
}

template<MemID id>
class Memory
{
  public:
    using OpenResult = std::expected<int, MemoryOpenError>;

    static_assert(id < mem::count, "Invalid ID");

    static constexpr uintptr_t phys_addr = mem::get_base_addr(id);
    static constexpr uint32_t n_blocks = mem::get_n_blocks(id);
    static constexpr uint32_t block_size = mem::get_range(id);
    static constexpr uint32_t size = mem::get_total_size(id);
    static constexpr int protection = mem::get_protection(id);
    static constexpr auto name = mem::get_name(id);
    static constexpr auto device = mem::get_device_driver(id);
    static constexpr bool is_devmem = (device == "/dev/mem");
    static constexpr bool is_uio = (device == "/dev/uio");

    Memory()
    : mapped_base(nullptr)
    , base_address(0)
    , is_opened(false)
    , dev_fd(-1)
    {}

    ~Memory() {
        // Uio owns its mapping and descriptor. Fallback mappings are owned here.
        if (!uio_) {
            if (is_opened) {
                ::munmap(mapped_base, mapped_length);
            }
            if (dev_fd >= 0) {
                ::close(dev_fd);
            }
        }
    }

    int open() {
        const auto result = open_checked();
        if (!result) {
            report_memory_open_error(result.error());
            errno = memory_error_errno(result.error());
            return -1;
        }
        return *result;
    }

    [[nodiscard]] OpenResult open_checked() {
        // Keep existing pointers and mappings valid when initialization is retried.
        if (is_opened) { return dev_fd; }
        logf("Memory[{}]: Opening {}\n", name, device);

        if constexpr (device.starts_with("/dev/mem_wc")) {
            std::ifstream iomem{"/proc/iomem"};
            if (!iomem) {
                return open_error(MemoryOperation::reservation, "/proc/iomem",
                                  std::make_error_code(std::io_errc::stream));
            }
            if (!outside_system_ram(iomem, phys_addr, size)) {
                return open_error(MemoryOperation::reservation, device, iomem.bad()
                    ? std::make_error_code(std::io_errc::stream)
                    : std::make_error_code(std::errc::operation_not_permitted));
            }
        }

        if constexpr (is_uio) {
            uio_.emplace();

            if (const auto result = uio_->open_checked(); !result) {
                const MemoryFailure original{device, MemoryOperation::open, result.error()};
                uio_.reset();
                return open_via_devmem_fallback(original);
            }

            const auto result = uio_->mmap_checked();
            if (!result) {
                const auto error = open_error(MemoryOperation::map, device, result.error());
                uio_.reset();
                return error;
            }

            mapped_base = *result;
            base_address = reinterpret_cast<uintptr_t>(*result);
            is_opened = true;
            dev_fd = uio_->fd();
            return dev_fd;
        } else {
            const auto fd = ::open(device.data(), O_RDWR | O_SYNC | O_CLOEXEC);

            if (fd == -1) {
                const std::error_code error{errno, std::generic_category()};
                if constexpr (!is_devmem) {
                    return open_via_devmem_fallback({device, MemoryOperation::open, error});
                } else {
                    return open_error(MemoryOperation::open, device, error);
                }
            }

            return map_memory<is_devmem>(fd, device);
        }
    }

    constexpr int get_protection() const noexcept {return protection;}
    int is_open() const noexcept {return is_opened;}
    uintptr_t base_addr() const noexcept {return base_address;}
    constexpr uint32_t mapped_size() const noexcept {return size;}
    constexpr uintptr_t phys_address() const noexcept {return phys_addr;}

    auto get_params() {
        return std::make_tuple(
            base_address,
            is_opened,
            phys_addr,
            size,
            protection
        );
    }

    ////////////////////////////////////////
    // Write functions
    ////////////////////////////////////////

    // Write a register (offset defined at compile-time)
    template<uint32_t offset, typename T = uint32_t>
    void write(T value) {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        *(volatile T *) (base_address + offset) = value;
    }

    // Write a register (offset defined at run-time)
    template<typename T = uint32_t>
    void write_reg(uint32_t offset, T value) {
        static_assert(mem::is_writable(id), "Not writable");
        *(volatile T *) (base_address + offset) = value;
    }

    template<typename T = uint32_t, uint32_t offset = 0>
    void set_ptr(const T *data_ptr, uint32_t buff_size, uint32_t block_idx = 0) {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + block_size * block_idx + offset;
        for (uint32_t i=0; i < buff_size; i++)
            *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    }

    template<typename T = uint32_t>
    void set_reg_ptr(uint32_t offset, const T *data_ptr, uint32_t buff_size) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        for (uint32_t i=0; i < buff_size; i++)
            *(volatile T *) (addr + sizeof(T) * i) = data_ptr[i];
    }

    // Write a std::array (offset defined at compile-time)
    template<typename T, size_t N, uint32_t offset = 0, bool use_memcpy = false>
    void write_array(const std::array<T, N>& arr, uint32_t block_idx = 0) {
        static_assert(offset + sizeof(T) * (N - 1) < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        if constexpr (use_memcpy) {
            const std::size_t off = block_size * block_idx + offset;
            constexpr std::size_t len = sizeof(T) * N;
            std::memcpy(reinterpret_cast<void*>(base_address + off), arr.data(), len);
            wc_flush_(off, len); // Ensure the fabric sees the data before you poke any control register
        } else {
            set_ptr<T, offset>(arr.data(), N, block_idx);
        }
    }

    // Write a std::array (offset defined at run-time)
    template<typename T, size_t N>
    void write_reg_array(uint32_t offset, const std::array<T, N>& arr) {
        static_assert(mem::is_writable(id), "Not writable");
        set_reg_ptr<T>(offset, arr.data(), N);
    }

    template<uint32_t offset, uint32_t mask, typename T = uint32_t>
    void write_mask(uint32_t value) {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = (*((volatile uint32_t *) addr) & ~mask) | (value & mask);
    }

    void write_reg_mask(uint32_t offset, uint32_t mask, uint32_t value) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = (*((volatile uint32_t *) addr) & ~mask) | (value & mask);
    }

    ////////////////////////////////////////
    // Read functions
    ////////////////////////////////////////

    // Read a register (offset defined at compile-time)
    template<uint32_t offset, typename T = uint32_t>
    T read() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_readable(id), "Not readable");

        return *(volatile T *) (base_address + offset);
    }

    // Read a register (offset defined at run-time)
    template<typename T = uint32_t>
    T read_reg(uint32_t offset) {
        static_assert(mem::is_readable(id), "Not readable");
        return *(volatile T *) (base_address + offset);
    }

    template<typename T = uint32_t, uint32_t offset = 0>
    T* get_ptr(uint32_t block_idx = 0) {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_readable(id), "Not readable");
        static_assert(offset % alignof(T) == 0, "Offset is not properly aligned for this type");

        return reinterpret_cast<T*>(base_address + block_size * block_idx + offset);
    }

    template<typename T = uint32_t>
    T* get_reg_ptr(uint32_t offset = 0) {
        static_assert(mem::is_readable(id), "Not readable");
        return reinterpret_cast<T*>(base_address + offset);
    }

    // Read a std::array (offset defined at compile-time)
    template<typename T, size_t N, uint32_t offset = 0>
    std::array<T, N>& read_array(uint32_t block_idx = 0) {
        static_assert(offset + sizeof(T) * (N - 1) < (mem::get_range(id) * mem::get_n_blocks(id)), "Invalid offset");
        static_assert(mem::is_readable(id), "Not readable");

        auto p = get_ptr<std::array<T, N>, offset>(block_idx);
        return *p;
    }

    template<typename T, std::size_t N, uint32_t offset = 0>
    std::span<const T, N> read_span(uint32_t block_idx = 0) {
        static_assert(offset + sizeof(T) * (N - 1) < (mem::get_range(id) * mem::get_n_blocks(id)), "Invalid offset");
        static_assert(mem::is_readable(id), "Not readable");

        const T* ptr = get_ptr<const T, offset>(block_idx);
        return std::span<const T, N>(ptr, N);
    }

    template<typename T, uint32_t offset = 0>
    std::span<const T> read_span(std::size_t n, uint32_t block_idx = 0) {
        static_assert(mem::is_readable(id), "Not readable");

        const std::size_t total_bytes = mem::get_range(id) * mem::get_n_blocks(id);
        const std::size_t start = std::size_t(block_size) * block_idx + offset;

        const std::size_t max_elems =
            (total_bytes > start)
                ? (total_bytes - start) / sizeof(T)
                : 0;

        const std::size_t clamped_n = std::min(n, max_elems);

        const T* ptr = get_ptr<const T, offset>(block_idx);
        return std::span<const T>(ptr, clamped_n);
    }

    // Read a std::array (offset defined at run-time)
    template<typename T, size_t N>
    std::array<T, N>& read_reg_array(uint32_t offset) {
        static_assert(mem::is_readable(id), "Not readable");

        auto p = get_reg_ptr<std::array<T, N>>(offset);
        return *p;
    }

    ////////////////////////////////////////
    // Bit manipulation
    ////////////////////////////////////////

    // Set a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    bool get_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_readable(id), "Not readable");

        uintptr_t addr = base_address + offset;
        return ((*((volatile uint32_t *) addr) >> index ) & 1U );
    }

    template<uint32_t offset, uint32_t index>
    void set_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = *((volatile uint32_t *) addr) | (1U << index);
    }

    // Set a bit (offset and index defined at run-time)
    void set_bit_reg(uint32_t offset, uint32_t index) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = *((volatile uint32_t *) addr) | (1U << index);
    }

    // Clear a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void clear_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = *((volatile uint32_t *) addr) & ~(1U << index);
    }

    // Clear a bit (offset and index defined at run-time)
    void clear_bit_reg(uint32_t offset, uint32_t index) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = *((volatile uint32_t *) addr) & ~(1U << index);
    }

    // Toggle a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void toggle_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = *((volatile uint32_t *) addr) ^ (1U << index);
    }

    // Toggle a bit (offset and index defined at run-time)
    void toggle_bit_reg(uint32_t offset, uint32_t index) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uint32_t *) addr = *((volatile uint32_t *) addr) ^ (1U << index);
    }

    // Write a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void write_bit(bool value) {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        value ? set_bit<offset, index>() : clear_bit<offset, index>();
    }

    // Write a bit (offset and index defined at run-time)
    void write_bit_reg(uint32_t offset, uint32_t index, bool value) {
        static_assert(mem::is_writable(id), "Not writable");

        value ? set_bit_reg(offset, index) : clear_bit_reg(offset, index);
    }

    // Read a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    bool read_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_readable(id), "Not readable");

        return *((volatile uint32_t *) (base_address + offset)) & (1U << index);
    }

    // Read a bit (offset and index defined at run-time)
    bool read_bit_reg(uint32_t offset, uint32_t index) {
        static_assert(mem::is_readable(id), "Not readable");
        return *((volatile uint32_t *) (base_address + offset)) & (1U << index);
    }

  private:
    void *mapped_base;       ///< Map base address
    uintptr_t base_address;  ///< Virtual memory base address of the driver
    bool is_opened;
    int dev_fd = -1;
    std::size_t mapped_length = 0;

    std::optional<Uio<id>> uio_; // only used when device == "/dev/uio"

    static std::unexpected<MemoryOpenError> open_error(MemoryOperation operation,
                                                      std::string_view backend, std::error_code code) {
        return std::unexpected(MemoryOpenError{id, {backend, operation, code}, std::nullopt});
    }

    template<bool is_devmem>
    OpenResult map_memory(int fd, std::string_view backend) {
        const size_t pg = page_size();
        off_t off = 0;
        size_t delta = 0;

        if constexpr (is_devmem) {
            off = static_cast<off_t>(phys_addr & ~(pg - 1));
            delta = phys_addr & (pg - 1);
        }

        const std::size_t len = align_up(size + delta, pg);
        void* p = ::mmap(nullptr, len, protection, MAP_SHARED, fd, off);

        if (p == MAP_FAILED) {
            const std::error_code error{errno, std::generic_category()};
            ::close(fd);
            return open_error(MemoryOperation::map, backend, error);
        }

        mapped_base = p;
        mapped_length = len;
        is_opened = true;
        base_address = reinterpret_cast<uintptr_t>(mapped_base) + delta;
        dev_fd = fd;
        return fd;
    }

    OpenResult open_via_devmem_fallback(const MemoryFailure& original) {
        logf("Memory[{}]: Fallback to /dev/mem after {} {} failed: {}\n", name,
             memory_operation_name(original.operation), original.device, original.code.message());
        const int fdm = ::open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);

        if (fdm < 0) {
            return std::unexpected(MemoryOpenError{id,
                {"/dev/mem", MemoryOperation::open, {errno, std::generic_category()}}, original});
        }

        auto result = map_memory<true>(fdm, "/dev/mem");
        if (!result) { result.error().fallback_from = original; }
        return result;
    }

    void wc_flush_(std::size_t off, std::size_t len) const {
        // Drain WC writes toward the fabric before you trigger hardware.
        if (len == 0) {
            std::atomic_thread_fence(std::memory_order_seq_cst);
            return;
        }
        const auto* p = reinterpret_cast<const std::uint8_t*>(base_address);
        if (len >= 4) {
            const std::size_t touch = (off + len - 4) & ~std::size_t(3);
            volatile std::uint32_t sink =
                *reinterpret_cast<const volatile std::uint32_t*>(p + touch);
            (void)sink;
        } else {
            volatile std::uint8_t sink =
                *reinterpret_cast<const volatile std::uint8_t*>(p + off + len - 1);
            (void)sink;
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }

};

} // namespace hw

#endif // __SERVER_CONTEXT_MEMORY_MAP_HPP__
