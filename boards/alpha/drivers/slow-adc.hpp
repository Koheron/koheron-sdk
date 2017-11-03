#ifndef __SLOW_ADC_HPP__
#define __SLOW_ADC_HPP__

#include <context.hpp>

class SlowAdc
{
  public:
    SlowAdc(Context& ctx_)
    : ctx(ctx_)
    , spi(ctx.spi.get("spidev1.0"))
    {
        if (! spi.is_ok()) {
            return;
        }

        spi.set_mode(SPI_MODE_3);
        spi.set_speed(10000000);

        set_channel(AIN0);
    }

    uint32_t read(uint32_t address, uint32_t len) {
        uint8_t cmd[] = {uint8_t((0 << 7) + (1 << 6) + (address & 0x3F))};
        uint8_t data[4];
        spi.transfer(cmd, data, len + 1);
        switch (len) {
            case 1: return data[1];
            case 2: return (data[1] << 8) + data[2];
            case 3: return (data[1] << 16) + (data[2] << 8) + data[3];
            default: return 0;
        }
    }

    void write(uint32_t address, uint32_t value, uint32_t len) {
        uint8_t cmd[4];
        uint8_t data[4];
        cmd[0] = uint8_t((0 << 7) + (0 << 6) + (address & 0x3F));
        switch (len) {
            case 1: {
                cmd[1] = value & 0xFF;
                break;
            };
            case 2: {
                cmd[1] = (value >> 8) & 0xFF;
                cmd[2] = value & 0xFF;
                break;
            };
            case 3: {
                cmd[1] = (value >> 16) & 0xFF;
                cmd[2] = (value >> 8) & 0xFF;
                cmd[3] = value & 0xFF;
                break;
            };
            default: break;
        }

        spi.transfer(cmd, data, len + 1);
    }

    uint32_t get_device_id() {
        return read(0x05, 1);
    }

    uint32_t get_data() {
        return read(0x02, 3);
    }

    float get_data_volts() {
        constexpr float range = 1.0; // V
        constexpr int32_t two_pow_23 = 8388608;
        return (get_data() * range - two_pow_23) / two_pow_23;
    }

    // Set the channel to be acquired (a single channel is read)
    void set_channel(uint32_t chan_num) {
        if (chan_num == last_channel_set) {
            return;
        }

        for (uint32_t i=0; i < inputs_num; i++) {
            if (i == chan_num) {
                write(0x09 + i, (1 << 15) + (2 * i << 5) + (2 * i + 1), 2);
            } else {
                write(0x09 + i, (0 << 15) + (2 * i << 5) + (2 * i + 1), 2);
            }
        }

        last_channel_set = chan_num;
    }

  private:
    Context& ctx;
    SpiDev& spi;

    uint32_t last_channel_set = inputs_num;

    enum inputs {
        AIN0,
        AIN1,
        AIN2,
        AIN3,
        DAC_OFFSET2,
        DAC_OFFSET1,
        ADC_OFFSET2,
        ADC_OFFSET1,
        inputs_num
    };
};

#endif // __SLOW_ADC_HPP__