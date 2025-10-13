/// Common commands for all Koheron Alpha bitstreams
///
/// (c) Koheron

#ifndef __ALPHA15_DRIVERS_COMMON_HPP__
#define __ALPHA15_DRIVERS_COMMON_HPP__

#include <cstdint>
#include <thread>
#include <atomic>
#include <string>

class GpioExpander;

class Common
{
  public:
    Common();
    ~Common();

    void set_led(uint32_t value);
    void init();
    void ip_on_leds();
    void adp5071_sync(bool enable, bool state_out);
    std::string get_instrument_config();

  private:
    void start_blink();
    void stop_blink();

    GpioExpander& gpio;
    std::thread blinker;
    std::atomic<bool> blinker_should_stop{true}; // true = not running yet
};

#endif // __ALPHA15_DRIVERS_COMMON_HPP__
