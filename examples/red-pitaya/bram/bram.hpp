/// BRAM driver
///
/// (c) Koheron

#ifndef __DRIVERS_BRAM_HPP__
#define __DRIVERS_BRAM_HPP__

#include <context.hpp>
#include <array>
#include <chrono>

constexpr uint32_t bram_size = mem::bram0_range/sizeof(uint32_t);

class Bram
{
  public:
    Bram(Context& ctx_)
    : ctx(ctx_)
    , bram0(ctx.mm.get<mem::bram0>())
    , bram1(ctx.mm.get<mem::bram1>())
    {
      test();
    }

  void test() {
      // 1) Initialize
      std::array<uint32_t, bram_size> data;
      for (uint32_t i = 0; i < data.size(); ++i) data[i] = 0x12340000u + i;

      auto fence = [](){ asm volatile("" ::: "memory"); };

      // Warmup (touch paths & TLB)
      bram0.write_array<uint32_t, bram_size,0,false>(data);
      bram1.write_array<uint32_t, bram_size,0,false>(data);
      (void)bram0.read_array<uint32_t, bram_size>();
      (void)bram1.read_array<uint32_t, bram_size>();

      constexpr int R = 1000;
      const double bytes_total = double(bram_size) * sizeof(uint32_t) * R;

      // AXI4-Lite WRITE
      fence();
      auto t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < R; ++i)
          bram0.write_array<uint32_t, bram_size,0,false>(data);
      fence();
      auto t1 = std::chrono::steady_clock::now();
      double s = std::chrono::duration<double>(t1 - t0).count();
      ctx.logf<INFO>("AXI4LITE WRITE= {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

      // AXI4-Lite WRITE
      fence();
      t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < R; ++i)
          bram0.write_array<uint32_t, bram_size,0,true>(data);
      fence();
      t1 = std::chrono::steady_clock::now();
      s = std::chrono::duration<double>(t1 - t0).count();
      ctx.logf<INFO>("AXI4LITE WRITE (MEMCPY) = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

      // AXI4 WRITE
      fence();
      t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < R; ++i)
          bram1.write_array<uint32_t, bram_size,0,false>(data);
      fence();
      t1 = std::chrono::steady_clock::now();
      s = std::chrono::duration<double>(t1 - t0).count();
      ctx.logf<INFO>("AXI4 WRITE = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

      // AXI4 MEMCPY WRITE
      fence();
      t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < R; ++i)
          bram1.write_array<uint32_t, bram_size,0,true>(data);
      fence();
      t1 = std::chrono::steady_clock::now();
      s = std::chrono::duration<double>(t1 - t0).count();
      ctx.logf<INFO>("AXI4 WRITE (MEMCPY) = {} s ({:.1f} MB/s)\n", s, bytes_total/s/1e6);

      // AXI4-Lite READ (use checksum so compiler can't delete it)
      uint64_t sum0 = 0;
      fence();
      t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < R; ++i) {
          auto tmp = bram0.read_array<uint32_t, bram_size>();
          for (auto v : tmp) sum0 += v;
      }
      fence();
      t1 = std::chrono::steady_clock::now();
      s = std::chrono::duration<double>(t1 - t0).count();
      ctx.logf<INFO>("AXI4LITE READ = {} s ({:.1f} MB/s) [sum={}]\n",
                    s, bytes_total/s/1e6, (unsigned long long)sum0);

      // AXI4 READ
      uint64_t sum1 = 0;
      fence();
      t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < R; ++i) {
          auto tmp = bram1.read_array<uint32_t, bram_size>();
          for (auto v : tmp) sum1 += v;
      }
      fence();
      t1 = std::chrono::steady_clock::now();
      s = std::chrono::duration<double>(t1 - t0).count();
      ctx.logf<INFO>("AXI4 READ = {} s ({:.1f} MB/s) [sum={}]\n",
                    s, bytes_total/s/1e6, (unsigned long long)sum1);
  }

    uint32_t get_bram_size() {
        return bram_size;
    }

 private:
    Context& ctx;
    Memory<mem::bram0>& bram0;
    Memory<mem::bram1>& bram1;

}; // class Bram

#endif // __DRIVERS_BRAM_HPP__
