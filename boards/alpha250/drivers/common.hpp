/// Common commands for all Koheron Alpha bitstreams
///
/// (c) Koheron

#ifndef __ALPHA_DRIVERS_COMMON_HPP__
#define __ALPHA_DRIVERS_COMMON_HPP__

#include <cstdint>
#include <string>
#include <memory>

class LedsController;
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
    std::unique_ptr<LedsController> leds;
    PrecisionAdc& precisionadc;
};

#endif // __ALPHA_DRIVERS_COMMON_HPP__
