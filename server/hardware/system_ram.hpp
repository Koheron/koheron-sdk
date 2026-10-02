#ifndef __KOHERON_SYSTEM_RAM_HPP__
#define __KOHERON_SYSTEM_RAM_HPP__

#include <charconv>
#include <cstdint>
#include <istream>
#include <limits>
#include <string>
#include <string_view>

namespace hw {

// Fixed physical DMA windows must be excluded from Linux RAM at boot. A
// reusable CMA pool is still System RAM unless a driver owns an allocation.
inline bool outside_system_ram(std::istream& iomem, uint64_t base, uint64_t size) {
    if (!size || size - 1 > std::numeric_limits<uint64_t>::max() - base) return false;
    const uint64_t last = base + size - 1;
    bool readable_ram = false;
    std::string line;
    while (std::getline(iomem, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string_view name{line.data() + colon + 1, line.size() - colon - 1};
        const auto label_start = name.find_first_not_of(" \t");
        if (label_start == std::string_view::npos) continue;
        name.remove_prefix(label_start);
        name = name.substr(0, name.find_last_not_of(" \t\r") + 1);
        if (name != "System RAM") continue;
        const auto first = line.find_first_not_of(" \t");
        const auto dash = line.find('-', first);
        if (first == std::string::npos || dash == std::string::npos || dash >= colon) return false;
        uint64_t low{}, high{};
        auto a = std::from_chars(line.data() + first, line.data() + dash, low, 16);
        const auto end = line.find_last_not_of(" \t", colon - 1) + 1;
        auto b = std::from_chars(line.data() + dash + 1, line.data() + end, high, 16);
        if (a.ec != std::errc{} || a.ptr != line.data() + dash ||
            b.ec != std::errc{} || b.ptr != line.data() + end || high < low) return false;
        // Unprivileged /proc/iomem can hide every physical address as zero.
        if (low == 0 && high == 0) continue;
        readable_ram = true;
        if (base <= high && low <= last) return false;
    }
    return readable_ram && !iomem.bad();
}

} // namespace hw
#endif
