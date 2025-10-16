/// Common commands for all Koheron Alpha bitstreams
///
/// (c) Koheron

#ifndef __ALPHA15_DRIVERS_COMMON_HPP__
#define __ALPHA15_DRIVERS_COMMON_HPP__

#include <cstdint>
#include <thread>
#include <atomic>
#include <string>

class LedsController;

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
    std::unique_ptr<LedsController> leds;
};

#endif // __ALPHA15_DRIVERS_COMMON_HPP__
