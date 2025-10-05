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

    // Same semantics as original: returns /dev/mem fd on success, -1 if any region failed.
    int open();

    template<MemID id>
    Memory<id>& get() { return std::get<id>(mem_maps); }

  private:
    // Build "/dev/ramwc0xXXXXXXXX" (low 32b of base in hex)
    static void make_ram_path(uintptr_t base, char (&out)[64]) {
      static constexpr char prefix[] = "/dev/ramwc0x";
      static constexpr char hex[]    = "0123456789abcdef";
      size_t i = 0;
      for (; i < sizeof(prefix) - 1 && i < sizeof(out) - 1; ++i) out[i] = prefix[i];
      uint32_t v = static_cast<uint32_t>(base);
      for (int nib = 7; nib >= 0 && i < sizeof(out) - 1; --nib)
        out[i++] = hex[(v >> (nib*4)) & 0xF];
      out[i] = '\0';
    }

    // Build "/dev/bram_wc0xXXXXXXXX"
    static void make_bram_path(uintptr_t base, char (&out)[64]) {
      static constexpr char prefix[] = "/dev/bram_wc0x";
      static constexpr char hex[]    = "0123456789abcdef";
      size_t i = 0;
      for (; i < sizeof(prefix) - 1 && i < sizeof(out) - 1; ++i) out[i] = prefix[i];
      uint32_t v = static_cast<uint32_t>(base);
      for (int nib = 7; nib >= 0 && i < sizeof(out) - 1; --nib)
        out[i++] = hex[(v >> (nib*4)) & 0xF];
      out[i] = '\0';
    }

    // Try /dev/ramwc0xXXXXXXXX first; fallback to legacy /dev/ramwc0
    template<MemID id>
    bool open_ram_devnode_() {
      char devpath[64];
      make_ram_path(mem::get_base_addr(id), devpath);

      const char* used = devpath;
      int rfd = ::open(devpath, O_RDWR | O_SYNC);
      if (rfd < 0) {
        // Optional legacy fallback (single device)
        rfd = ::open("/dev/ramwc0", O_RDWR | O_SYNC);
        if (rfd < 0) return false;
        used = "/dev/ramwc0";
      }

      bram_fds[id] = rfd;                 // reuse slot for per-id devnode
      auto& m = get<id>();
      m.open_devnode(rfd);                // mmap through device node
      m.set_dev_fd(rfd);                  // remember fd for WC cache ops (if any)

      if (m.is_open()) {
        koheron::print_fmt<INFO>("MemoryManager: id={} size={} via={}\n",
                                 id, mem::get_total_size(id), used);
        return true;
      }

      ::close(rfd);
      bram_fds[id] = -1;
      koheron::print_fmt<ERROR>("MemoryManager: id={} size={} mmap({}) failed\n",
                                id, mem::get_total_size(id), used);
      return false;
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
    int fd;                                    // shared /dev/mem
    std::vector<MemID> failed_maps;            // list of ids that failed to map
    std::array<int, N> bram_fds;               // per-region devnode fds (RAMWC/BRAMWC)
    std::tuple<Memory<ids>...> mem_maps;       // Memory<> instances
};

template<size_t N, MemID... ids>
int MemoryManagerImpl<N, std::index_sequence<ids...>>::open()
{
  // /dev/mem is optional if all regions map via devnodes
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

  // 0) RAM first: /dev/ramwc0xXXXXXXXX (fallback: /dev/ramwc0)
  if (open_ram_devnode_<id>()) return;

  // 1) BRAM WC: /dev/bram_wc0xXXXXXXXX
  char devpath[64];
  make_bram_path(base, devpath);
  int bfd = ::open(devpath, O_RDWR | O_SYNC);

  if (bfd >= 0) {
    bram_fds[id] = bfd;
    auto& m = get<id>();
    m.open_devnode(bfd);
    m.set_dev_fd(bfd);

    if (m.is_open()) {
      koheron::print_fmt<INFO>("MemoryManager: id={} base=0x{:08x} size={} via={}\n",
                               id, static_cast<uint32_t>(base), size, devpath);
      return;
    } else {
      koheron::print_fmt<ERROR>("MemoryManager: id={} base=0x{:08x} size={} mmap({}) failed\n",
                                id, static_cast<uint32_t>(base), size, devpath);
      ::close(bfd);
      bram_fds[id] = -1;
      // fall through to /dev/mem
    }
  }

  // 2) Fallback: /dev/mem
  auto& m = get<id>();
  if (fd >= 0) {
    m.open(fd);
    if (m.is_open()) {
      koheron::print_fmt<INFO>("MemoryManager: id={} base=0x{:08x} size={} via=/dev/mem\n",
                               id, static_cast<uint32_t>(base), size);
      return;
    }
  }

  // 3) Total failure
  koheron::print_fmt<ERROR>("MemoryManager: id={} base=0x{:08x} size={} failed\n",
                            id, static_cast<uint32_t>(base), size);
  failed_maps.push_back(id);
}

using MemoryManager = MemoryManagerImpl<mem::count>;

#endif // __DRIVERS_LIB_MEMORY_MANAGER_HPP__
