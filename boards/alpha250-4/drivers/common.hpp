/// Common commands for all Koheron ALPHA250-4 bitstreams
///
/// (c) Koheron

#ifndef __ALPHA250_4_DRIVERS_COMMON_HPP__
#define __ALPHA250_4_DRIVERS_COMMON_HPP__

#include <atomic>
#include <thread>
#include <cstdint>
#include <string>

class GpioExpander;
class PrecisionAdc;

class Common
{
  public:
    Common();
    ~Common();

    void set_led(uint32_t value);
    void init();
    std::string get_instrument_config();
    void ip_on_leds();

  private:
    GpioExpander& gpio;
    PrecisionAdc& precisionadc;

    std::thread blinker;
    std::atomic<bool> blinker_should_stop{true}; // true = not running yet

    void start_blink();
    void stop_blink();
};

#endif // __ALPHA250_4_DRIVERS_COMMON_HPP__
