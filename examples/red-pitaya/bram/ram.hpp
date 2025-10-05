/// RAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_RAM_HPP__
#define __DRIVERS_RAM_HPP__

#include <context.hpp>
#include <array>
#include <chrono>

constexpr uint32_t ram_size = mem::bram0_range/sizeof(uint32_t);

class Ram
{
  public:
    Ram(Context& ctx_)
    : ctx(ctx_)
    , ram(ctx.mm.get<mem::ram>())
    {
    }

    void test() {
        // 1) Initialize
        std::array<uint32_t, ram_size> data;
        for (uint32_t i = 0; i < data.size(); ++i) data[i] = 0x12340000u + i;

        auto fence = [](){ asm volatile("" ::: "memory"); };

        // Warmup (touch paths & TLB)
        ram.write_array<uint32_t, ram_size,0,false>(data);
        ram.write_array<uint32_t, ram_size,0,false>(data);
        (void)ram.read_array<uint32_t, ram_size>();

        constexpr int R = 1000;
        const double bytes_total = double(ram_size) * sizeof(uint32_t) * R;

        // RAM WRITE
        fence();
        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < R; ++i)
            ram.write_array<uint32_t, ram_size,0,false>(data);
        fence();
        auto t1 = std::chrono::steady_clock::now();
        double s = std::chrono::duration<double>(t1 - t0).count();
        ctx.logf<INFO>("RAM WRITE= {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

        // RAM MEMCPY WRITE
        fence();
        t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < R; ++i)
            ram.write_array<uint32_t, ram_size,0,true>(data);
        fence();
        t1 = std::chrono::steady_clock::now();
        s = std::chrono::duration<double>(t1 - t0).count();
        ctx.logf<INFO>("RAM WRITE (MEMCPY) = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

        // RAM READ
        uint64_t sum0 = 0;
        fence();
        t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < R; ++i) {
            auto tmp = ram.read_array<uint32_t, ram_size>();
            for (auto v : tmp) sum0 += v;
        }
        fence();
        t1 = std::chrono::steady_clock::now();
        s = std::chrono::duration<double>(t1 - t0).count();
        ctx.logf<INFO>("RAM READ = {} s ({:.1f} MB/s) [sum={}]\n",
                        s, bytes_total/s/1e6, (unsigned long long)sum0);

    }

 private:
    Context& ctx;
    Memory<mem::ram>& ram;

}; // class Ram

#endif // __DRIVERS_RAM_HPP__
