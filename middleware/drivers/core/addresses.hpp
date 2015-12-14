/// Laser development kit addresses
///
/// (c) Koheron

#ifndef __DRIVERS_CORE_ADDRESSES_HPP__
#define __DRIVERS_CORE_ADDRESSES_HPP__

// -- Base addresses
#define CONFIG_ADDR        0x60000000
#define STATUS_ADDR        0x50000000
#define DAC_ADDR           0x40000000
#define ADC1_ADDR          0x42000000
#define ADC2_ADDR          0x44000000
#define GPIO_ADDR          0x41200000
#define XADC_ADDR          0x43C00000
#define SPECTRUM_ADDR      0x42000000
#define DEMOD_ADDR         0x44000000

// -- Config offsets
#define LEDS_OFF           0
#define PWM0_OFF           4
#define PWM1_OFF           8
#define PWM2_OFF           12
#define PWM3_OFF           16
#define ADDR_OFF           20
#define BITSTREAM_ID_OFF   36
// Oscillo
#define AVG1_OFF           24
#define AVG2_OFF           28
// Spectrum
#define SUBSTRACT_MEAN_OFF 24
#define CFG_FFT_OFF        28
#define AVG_OFF_OFF        32

// -- Status offsets
#define N_AVG1_OFF         0
#define N_AVG2_OFF         0 // 4 ??

// -- GPIO offsets
#define CHAN1_VALUE_OFF    0
#define CHAN2_VALUE_OFF    8
#define CHAN1_DIR_OFF      4
#define CHAN2_DIR_OFF      12

// -- XADC offsets
#define SET_CHAN_OFF       0x324
#define AVG_EN_OFF         0x32C
#define READ_OFF           0x240
#define AVG_OFF            0x300

#endif // __DRIVERS_CORE_ADDRESSES_HPP__
