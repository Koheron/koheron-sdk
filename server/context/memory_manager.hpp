/// Driver memory manager
///
/// (c) Koheron

#ifndef __DRIVERS_LIB_MEMORY_MANAGER_HPP__
#define __DRIVERS_LIB_MEMORY_MANAGER_HPP__

#include "server/runtime/syslog.hpp"
#include "server/context/memory_map.hpp"

#include <vector>
#include <tuple>
#include <array>
#include <cstdint>
#include <fcntl.h>
#include <unistd.h>

template<size_t N, class = std::make_index_sequence<N>> class MemoryManagerImpl;

template<size_t N, MemID... ids>
class MemoryManagerImpl<N, std::index_sequence<ids...>>
{
  public:
    MemoryManagerImpl()
    : fd(-1)
    , failed_maps()
    , bram_fds{}
    , mem_maps()
    {
      bram_fds.fill(-1);
    }

    ~MemoryManagerImpl() {
      for (int& bfd : bram_fds) if (bfd >= 0) ::close(bfd);
      if (fd >= 0) ::close(fd);
    }

    // Same semantics as original: returns fd on success, -1 if any region failed.
    int open();

    template<MemID id>
    Memory<id>& get() { return std::get<id>(mem_maps); }

  private:
    // Build "/dev/bram_wc0xXXXXXXXX" (low 32b are sufficient on Zynq-7000)
    static void make_bram_path(uintptr_t base, char (&out)[64]) {
      static constexpr char prefix[] = "/dev/bram_wc0x";
      static constexpr char hex[] =
          "0123456789abcdef";
      // copy prefix
      size_t i = 0;
      for (; i < sizeof(prefix) - 1 && i < sizeof(out) - 1; ++i) out[i] = prefix[i];
      // append 8 hex digits
      uint32_t v = static_cast<uint32_t>(base);
      for (int nib = 7; nib >= 0 && i < sizeof(out) - 1; --nib)
        out[i++] = hex[(v >> (nib*4)) & 0xF];
      out[i] = '\0';
    }

    template<MemID id> void open_memory_map();

    template<MemID cnt>
    std::enable_if_t<cnt == 0, void>
    open_maps() {}

    template<MemID cnt>
    std::enable_if_t<(cnt > 0), void>
    open_maps() {
      open_memory_map<cnt-1>();
      open_maps<cnt-1>();
    }

  private:
    int fd;                                    // shared /dev/mem (as in original)
    std::vector<MemID> failed_maps;            // as in original
    std::array<int, N> bram_fds;               // per-region BRAM node fds
    std::tuple<Memory<ids>...> mem_maps;       // as in original
};

template<size_t N, MemID... ids>
int MemoryManagerImpl<N, std::index_sequence<ids...>>::open()
{
  // Original behavior: open /dev/mem once (may be -1; we can still use BRAM nodes).
  fd = ::open("/dev/mem", O_RDWR | O_SYNC);
  open_maps<N>();
  if (!failed_maps.empty()) return -1;
  return fd;
}

template<size_t N, MemID... ids>
template<MemID id>
void MemoryManagerImpl<N, std::index_sequence<ids...>>::open_memory_map()
{
  constexpr uintptr_t base = mem::get_base_addr(id);
  constexpr uint32_t  size = mem::get_total_size(id);

  // 1) Try BRAM node first
  char devpath[64];
  make_bram_path(base, devpath);
  int bfd = ::open(devpath, O_RDWR | O_SYNC);

  if (bfd >= 0) {
    bram_fds[id] = bfd;
    auto& m = get<id>();
    m.open_devnode(bfd);
    if (m.is_open()) {
      koheron::print_fmt<INFO>("MemoryManager: id={} base=0x{:08x} size={} via={}\n",
                               id, static_cast<uint32_t>(base), size, devpath);
      return; // exactly one line for this id
    } else {
      // mmap failed on BRAM node
      koheron::print_fmt<ERROR>("MemoryManager: id={} base=0x{:08x} size={} failed\n",
                                id, static_cast<uint32_t>(base), size);
      failed_maps.push_back(id);
      return;
    }
  }

  // 2) Fallback to /dev/mem (original path)
  auto& m = get<id>();
  if (fd >= 0) {
    m.open(fd);
    if (m.is_open()) {
      koheron::print_fmt<INFO>("MemoryManager: id={} base=0x{:08x} size={} via=/dev/mem\n",
                               id, static_cast<uint32_t>(base), size);
      return; // exactly one line for this id
    }
  }

  // 3) Failure (no BRAM node and /dev/mem mapping failed)
  koheron::print_fmt<ERROR>("MemoryManager: id={} base=0x{:08x} size={} failed\n",
                            id, static_cast<uint32_t>(base), size);
  failed_maps.push_back(id);
}

using MemoryManager = MemoryManagerImpl<mem::count>;

#endif // __DRIVERS_LIB_MEMORY_MANAGER_HPP__