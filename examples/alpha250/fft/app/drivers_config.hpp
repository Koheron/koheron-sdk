#ifndef __DRIVERS_CONFIG_HPP__
#define __DRIVERS_CONFIG_HPP__

#include "server/core/lib/drivers_table.hpp"

namespace drivers {

class Common; // NB: We require a 'Common' driver with an init() method
class Eeprom;
class GpioExpander;
class TemperatureSensor;
class PowerMonitor;
class ClockGenerator;
class Ltc2157;
class Ad9747;
class PrecisionAdc;
class PrecisionDac;
class SpiConfig;
class FFT;

using table = koheron::drivers_table<Common, Eeprom, GpioExpander, TemperatureSensor, PowerMonitor, ClockGenerator, Ltc2157, Ad9747, PrecisionAdc, PrecisionDac, SpiConfig, FFT>;

}// namespace drivers

#endif // __DRIVERS_CONFIG_HPP__