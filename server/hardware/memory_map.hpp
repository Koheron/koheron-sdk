/// Memory map drivers
///
/// (c) Koheron

#ifndef __SERVER_CONTEXT_MEMORY_MAP_HPP__
#define __SERVER_CONTEXT_MEMORY_MAP_HPP__

#include "server/hardware/memory_catalog.hpp"
#include "server/drivers/uio.hpp"

#include <cstdio>
#include <cstdint>
#include <tuple>
#include <memory>
#include <span>
#include <cstring>
#include <atomic>
#include <string_view>
#include <optional>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace hw {

template<MemID id>
class Memory
{
  public:
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
        if constexpr (is_uio) {
            if (uio_) {
                uio_->unmap(); // Uio<> dtor closes the fd
            }
        } else {
            if (is_opened) {
                const std::size_t pg = page_size();
                const std::size_t delta = phys_addr & (pg - 1);
                const std::size_t len = align_up(size + delta, pg);
                ::munmap(mapped_base, len);

                if (dev_fd >= 0) {
                    ::close(dev_fd);
                }
            }
        }
    }

    int open() {
        rt::print_fmt("Memory[{}]: Opening {}\n", name, device);

        if constexpr (is_uio) {
            uio_.emplace();

            if (uio_->open() < 0) {
                return open_via_devmem_fallback();
            }

            void* p = uio_->mmap();

            if (!p) {
                rt::print_fmt<ERROR>("Memory[{}]: UIO mmap failed\n", name);
                return -1;
            }

            mapped_base = p;
            base_address = reinterpret_cast<uintptr_t>(p);
            is_opened = true;
            dev_fd = uio_->fd();
            return dev_fd;
        } else {
            const auto fd = ::open(device.data(), O_RDWR | O_SYNC);

            if (fd == -1) {
                rt::print_fmt<ERROR>("Memory[{}]: Can't open {}\n", name, device);
                if constexpr (!is_devmem) {
                    return open_via_devmem_fallback();
                } else {
                    return -1;
                }
            }

            return map_memory<is_devmem>(fd);
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
        *(volatile uintptr_t *) addr = (*((volatile uintptr_t *) addr) & ~mask) | (value & mask);
    }

    void write_reg_mask(uint32_t offset, uint32_t mask, uint32_t value) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = (*((volatile uintptr_t *) addr) & ~mask) | (value & mask);
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

        return { get_ptr<const T, offset>(block_idx), N };
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
        return ((*((volatile uintptr_t *) addr) >> index ) & 1U );
    }

    template<uint32_t offset, uint32_t index>
    void set_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1U << index);
    }

    // Set a bit (offset and index defined at run-time)
    void set_bit_reg(uint32_t offset, uint32_t index) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) | (1U << index);
    }

    // Clear a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void clear_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1U << index);
    }

    // Clear a bit (offset and index defined at run-time)
    void clear_bit_reg(uint32_t offset, uint32_t index) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) & ~(1U << index);
    }

    // Toggle a bit (offset and index defined at compile-time)
    template<uint32_t offset, uint32_t index>
    void toggle_bit() {
        static_assert(offset < mem::get_range(id), "Invalid offset");
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1U << index);
    }

    // Toggle a bit (offset and index defined at run-time)
    void toggle_bit_reg(uint32_t offset, uint32_t index) {
        static_assert(mem::is_writable(id), "Not writable");

        uintptr_t addr = base_address + offset;
        *(volatile uintptr_t *) addr = *((volatile uintptr_t *) addr) ^ (1U << index);
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

    std::optional<Uio<id>> uio_; // only used when device == "/dev/uio"

    template<bool is_devmem>
    int map_memory(int fd) {
        const size_t pg = page_size();
        off_t off = 0;
        size_t delta = 0;

        if constexpr (is_devmem) {
            off = static_cast<off_t>(phys_addr & ~(pg - 1));
            delta = phys_addr & (pg - 1);
        }

        const std::size_t len = align_up(size + delta, pg);
        mapped_base = ::mmap(nullptr, len, protection, MAP_SHARED, fd, off);

        if (mapped_base == MAP_FAILED) {
            is_opened = false;
            mapped_base = nullptr;
            return -1;
        }

        is_opened = true;
        base_address = reinterpret_cast<uintptr_t>(mapped_base) + delta;
        dev_fd = fd;
        return fd;
    }

    int open_via_devmem_fallback() {
        rt::print_fmt("Memory[{}]: Fallback to /dev/mem\n", name);
        const int fdm = ::open("/dev/mem", O_RDWR | O_SYNC);

        if (fdm < 0) {
            return -1;
        }

        return map_memory<true>(fdm);
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
