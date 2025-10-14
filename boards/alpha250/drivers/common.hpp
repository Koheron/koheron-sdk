/// Common commands for all Koheron Alpha bitstreams
///
/// (c) Koheron

#ifndef __ALPHA_DRIVERS_COMMON_HPP__
#define __ALPHA_DRIVERS_COMMON_HPP__

#include <thread>
#include <atomic>

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
    void start_blink();
    void stop_blink();
  private:
    GpioExpander& gpio;
    PrecisionAdc& precisionadc;

    std::thread blinker;
    std::atomic<bool> blinker_should_stop{true}; // true = not running yet
};

#endif // __ALPHA_DRIVERS_COMMON_HPP__
