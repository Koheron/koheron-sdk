/// TEST CLOCK driver
///
/// (c) Koheron

#ifndef __DRIVERS_TEST_CLOCK_HPP__
#define __DRIVERS_TEST_CLOCK_HPP__

#include <context.hpp>

class TestClock
{
  public:
    TestClock(Context& ctx_)
    : ctx(ctx_)
    , ps_sts(ctx.mm.get<mem::ps_status>())
    , sts(ctx.mm.get<mem::status>())
    {
    }

    uint64_t get_counter_fclk0() {
        return ps_sts.read<reg::counter_fclk00,uint64_t>();
    }

    uint64_t get_counter_adc_clk() {
        return ps_sts.read<reg::counter_adc_clk0,uint64_t>();
    }

 private:
    Context& ctx;
    Memory<mem::ps_status>& ps_sts;
    Memory<mem::status>& sts;

}; // class TestClock

#endif // __DRIVERS_TEST_CLOCK_HPP__
