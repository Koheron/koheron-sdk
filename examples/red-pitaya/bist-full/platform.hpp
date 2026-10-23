#pragma once
#include "ddr.hpp"
#include <barrier>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <numeric>
#include <sys/statvfs.h>
#if defined(__arm__)
#include <arm_neon.h>
#endif

namespace bist_full
{
inline uint32_t crc32(const unsigned char *data, size_t n)
{
    uint32_t crc = ~0U;
    for (size_t i = 0; i < n; ++i)
    {
        crc ^= data[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1)));
    }
    return ~crc;
}
inline int pin(unsigned index)
{
    cpu_set_t cpu;
    CPU_ZERO(&cpu);
    CPU_SET(index, &cpu);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu), &cpu);
}
#if defined(__arm__)
__attribute__((target("fpu=neon"), noinline))
#endif
inline uint64_t neon_errors(uint32_t key)
{
    alignas(16) std::array<uint32_t, 4096> a{}, b{}, result{};
    for (unsigned i = 0; i < a.size(); ++i)
    {
        a[i] = bist::pattern(i, key);
        b[i] = bist::pattern(i, ~key);
    }
#if defined(__arm__)
    for (unsigned i = 0; i < a.size(); i += 4)
        vst1q_u32(result.data() + i, vmlaq_u32(vdupq_n_u32(0x12345678), vld1q_u32(a.data() + i),
                                               vld1q_u32(b.data() + i)));
#else
    for (unsigned i = 0; i < a.size(); ++i)
        result[i] = a[i] * b[i] + 0x12345678U;
#endif
    uint64_t errors = 0;
    for (unsigned i = 0; i < a.size(); ++i)
        errors += result[i] != uint32_t((uint64_t(a[i]) * b[i] + 0x12345678U) & 0xffffffffU);
    return errors;
}
inline uint64_t arithmetic_errors(uint32_t key)
{
    uint64_t errors = 0;
    // Volatile inputs keep these known-answer operations in the executed code.
    volatile uint64_t numerator = 0xfedcba9876543210ULL, denominator = 0x12345678;
    const uint64_t q = numerator / denominator, r = numerator % denominator;
    errors += q != 0xe00000077ULL || r != 0x48;
    volatile double x = 1.25, y = -3.5, z = 2.0;
    errors += std::abs(std::fma(x, y, 4.5) - .125) > 1e-15;
    errors += std::abs(std::sqrt(z) * std::sqrt(z) - 2.0) > 1e-14;
    errors += crc32(reinterpret_cast<const unsigned char *>("123456789"), 9) != 0xcbf43926U;
    errors += neon_errors(key);
    return errors;
}
inline void processors(uint32_t seed, std::atomic<bool> &cancel, bist::Clock::time_point deadline,
                       const Emit &emit)
{
    std::array<uint64_t, 2> errors{};
    std::array<int, 2> affinity{};
    std::vector<uint32_t> memory(8 * 1024 * 1024); // 32 MiB of Linux-owned, cacheable RAM.
    std::barrier rendezvous(2);
    auto work = [&](unsigned cpu)
    {
        affinity[cpu] = pin(cpu);
        errors[cpu] = arithmetic_errors(seed ^ cpu);
        const size_t half = memory.size() / 2;
        for (unsigned phase = 0; phase < 2; ++phase)
        {
            for (size_t i = cpu * half; i < (cpu + 1) * half; ++i)
                memory[i] = bist::pattern(i, seed) ^ (phase ? ~0U : 0U);
            rendezvous.arrive_and_wait();
            // Read the other core's cacheable writes after an acquire/release barrier.
            for (size_t i = (1 - cpu) * half; i < (2 - cpu) * half; ++i)
                errors[cpu] += memory[i] != (bist::pattern(i, seed) ^ (phase ? ~0U : 0U));
            rendezvous.arrive_and_wait();
        }
    };
    const auto begin = bist::Clock::now();
    std::thread a(work, 0), b(work, 1);
    a.join();
    b.join();
    for (unsigned cpu = 0; cpu < 2; ++cpu)
        emit("cpu", "core_" + std::to_string(cpu), !errors[cpu] && !affinity[cpu],
             double(errors[cpu]), "errors",
             "pinned ARM integer division, VFP/FMA, CRC known answer, NEON/scalar agreement and "
             "peer-cache reads",
             "\"affinity_error\":" + std::to_string(affinity[cpu]));
    emit("cpu", "cacheable_32m", !errors[0] && !errors[1],
         16 * double(memory.size()) / bist::seconds(begin) / 1e6, "MB/s checked",
         "two address/inverse passes; each CPU verifies the other CPU's half of 32 MiB cacheable "
         "RAM",
         "");
    struct alignas(64) Mailbox
    {
        std::array<uint32_t, 16> words{};
    } box;
    std::atomic<unsigned> published{0}, consumed{0};
    uint64_t bad = 0;
    constexpr unsigned count = 4096;
    const auto end = std::min(deadline, bist::Clock::now() + std::chrono::seconds(1));
    std::array<int, 2> handoff_affinity{};
    std::thread producer(
        [&]
        {
            handoff_affinity[0] = pin(0);
            for (unsigned seq = 1; seq <= count && !cancel && bist::Clock::now() < end; ++seq)
            {
                while (consumed.load(std::memory_order_acquire) != seq - 1 && !cancel &&
                       bist::Clock::now() < end)
                {
                }
                if (cancel || bist::Clock::now() >= end)
                    break;
                for (unsigned i = 0; i < box.words.size(); ++i)
                    box.words[i] = bist::pattern(i, seed ^ seq);
                published.store(seq, std::memory_order_release);
            }
        });
    std::thread consumer(
        [&]
        {
            handoff_affinity[1] = pin(1);
            for (unsigned seq = 1; seq <= count && !cancel && bist::Clock::now() < end; ++seq)
            {
                while (published.load(std::memory_order_acquire) != seq && !cancel &&
                       bist::Clock::now() < end)
                {
                }
                if (cancel || bist::Clock::now() >= end)
                    break;
                for (unsigned i = 0; i < box.words.size(); ++i)
                    bad += box.words[i] != bist::pattern(i, seed ^ seq);
                consumed.store(seq, std::memory_order_release);
            }
        });
    producer.join();
    consumer.join();
    emit("cpu", "cache_line_handoff",
         !bad && consumed == count && !handoff_affinity[0] && !handoff_affinity[1], consumed,
         "verified messages",
         "4096 release/acquire cache-line handoffs between pinned CPUs, every word checked", "");
}
inline std::string factory_value(const std::string &env, const std::string &key)
{
    for (size_t start = 0; start < env.size();)
    {
        const auto end = env.find('\0', start);
        if (end == std::string::npos || end == start)
            break;
        const auto text = env.substr(start, end - start);
        if (text.starts_with(key + '='))
            return text.substr(key.size() + 1);
        start = end + 1;
    }
    return "";
}
inline void board_identity(const Emit &emit)
{
    const int fd = ::open("/sys/bus/nvmem/devices/0-00500/nvmem", O_RDONLY | O_CLOEXEC);
    std::string bytes(1024, '\0');
    const auto n = fd < 0 ? -1 : ::pread(fd, bytes.data(), bytes.size(), 0x1804);
    if (fd >= 0)
        ::close(fd);
    auto mac = factory_value(bytes, "ethaddr");
    std::transform(mac.begin(), mac.end(), mac.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    const auto serial = factory_value(bytes, "serial"), revision = factory_value(bytes, "hw_rev");
    emit("identity", "factory_eeprom",
         n == 1024 && mac.size() == 17 && !serial.empty() && !revision.empty(),
         double(std::max<ssize_t>(0, n)), "bytes read",
         "read-only factory EEPROM environment; no EEPROM writes",
         "\"factory_mac\":" + bist::quote(mac) + ",\"serial\":" + bist::quote(serial) +
             ",\"hardware_revision\":" + bist::quote(revision));
    std::string net, live;
    std::error_code error;
    for (const auto &entry : std::filesystem::directory_iterator("/sys/class/net", error))
    {
        if (!std::filesystem::exists(entry.path() / "device"))
            continue;
        net = entry.path();
        live = bist::read_file(net + "/address");
        if (!live.empty() && live.back() == '\n')
            live.pop_back();
        break;
    }
    emit("identity", "ethernet_factory_mac", !mac.empty() && live == mac, live == mac, "match",
         "live physical Ethernet address must equal the factory EEPROM address",
         "\"live_mac\":" + bist::quote(live) + ",\"factory_mac\":" + bist::quote(mac));
    const auto speed = bist::read_file(net + "/speed"), duplex = bist::read_file(net + "/duplex");
    emit("identity", "ethernet_link",
         bist::read_file(net + "/carrier").starts_with('1') && speed.starts_with("1000") &&
             duplex.starts_with("full"),
         0, "errors", "fixture requires carrier at 1000 Mb/s full duplex",
         "\"speed\":" + bist::quote(speed) + ",\"duplex\":" + bist::quote(duplex));
}
inline void storage(const std::string &directory, uint32_t seed, const Emit &emit)
{
    constexpr size_t n = 4 * 1024 * 1024;
    struct statvfs fs{};
    const bool space =
        !::statvfs(directory.c_str(), &fs) && uint64_t(fs.f_bavail) * fs.f_frsize > 2 * n;
    if (!space)
    {
        emit("storage", "atomic_file", false, 0, "bytes",
             "insufficient space for owned 4 MiB test file", "");
        return;
    }
    const std::string next = directory + "/media.next", path = directory + "/media.dat";
    const int fd = ::open(next.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0)
    {
        emit("storage", "atomic_file", false, errno, "errno", "create owned test file", "");
        return;
    }
    std::vector<unsigned char> original(n);
    for (size_t i = 0; i < n; ++i)
        original[i] = uint8_t(bist::pattern(i / 4, seed) >> (8 * (i % 4)));
    const auto begin = bist::Clock::now();
    bool ok = ::pwrite(fd, original.data(), n, 0) == ssize_t(n) && !::fsync(fd);
    ok &= !::rename(next.c_str(), path.c_str());
    const int dir = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    ok &= dir >= 0 && !::fsync(dir);
    if (dir >= 0)
        ::close(dir);
    const double write_seconds = bist::seconds(begin);
    ::close(fd);
    // O_DIRECT forces a fresh media read rather than validating the page-cache copy.
    const int direct = ::open(path.c_str(), O_RDONLY | O_DIRECT | O_CLOEXEC);
    void *aligned = nullptr;
    const bool allocated = !::posix_memalign(&aligned, 4096, 65536);
    uint64_t errors = 0, read_bytes = 0;
    const auto read_begin = bist::Clock::now();
    if (direct >= 0 && allocated)
        for (size_t offset = 0; offset < n; offset += 65536)
        {
            if (::pread(direct, aligned, 65536, offset) != 65536)
            {
                ++errors;
                break;
            }
            errors += std::memcmp(aligned, original.data() + offset, 65536) != 0;
            read_bytes += 65536;
        }
    else
        ++errors;
    const double read_seconds = bist::seconds(read_begin);
    if (direct >= 0)
        ::close(direct);
    ::free(aligned);
    emit("storage", "atomic_file", ok && !errors && read_bytes == n, double(read_bytes),
         "verified bytes",
         "4 MiB write, file fsync, atomic rename, directory fsync, reopen and O_DIRECT media "
         "readback",
         "\"write_MB_s\":" + bist::number(n / write_seconds / 1e6) +
             ",\"read_MB_s\":" + bist::number(n / read_seconds / 1e6));
    // Verify that the file integrity detector rejects an actual changed byte and truncation.
    std::vector<unsigned char> readback(n);
    const auto checksum = crc32(original.data(), n);
    const int corrupt = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
    const unsigned char changed = original[4095] ^ 1;
    bool detected = corrupt >= 0;
    if (corrupt >= 0)
    {
        detected &= ::pwrite(corrupt, &changed, 1, 4095) == 1;
        detected &= ::pread(corrupt, readback.data(), n, 0) == ssize_t(n) &&
                    crc32(readback.data(), n) != checksum;
        detected &= !::ftruncate(corrupt, 4095) && ::pread(corrupt, readback.data(), n, 0) == 4095;
        ::close(corrupt);
    }
    emit("storage", "detect_media_corruption", detected, detected, "bool",
         "CRC rejects an actual flipped file byte; exact-length read rejects truncation", "");
    const bool removed = !::unlink(path.c_str()) && !std::filesystem::exists(path);
    ::unlink(next.c_str());
    emit("storage", "cleanup", removed, removed, "bool",
         "owned media files removed; existing files untouched", "");
}
} // namespace bist_full
