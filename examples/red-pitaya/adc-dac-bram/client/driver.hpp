/// (c) Koheron

#ifndef __DRIVER_HPP__
#define __DRIVER_HPP__

#include <koheron-client.hpp>
#include <operations.hpp>

static constexpr uint32_t N_PTS = 16384;

static uint32_t float_to_i14(float dac) {
    return static_cast<uint32_t>(8192 * dac) % 16384;
}

class Driver
{
  public:
    Driver(KoheronClient& client_)
    : client(client_)
    {}

    void set_led(uint32_t value) {
        client.call<op::Common::set_led>(value);
    }

    void set_dac(std::array<float, N_PTS>& dac1, std::array<float, N_PTS>& dac2)
    {
        uint32_t dac_data_1, dac_data_2;

        for (unsigned int i=0; i<N_PTS; i++) {
            dac_data_1 = float_to_i14(dac1[i]);
            dac_data_2 = float_to_i14(dac2[i]);
            __dac_buffer[i] = dac_data_1 + 65536 * dac_data_2;
        }
        client.call<op::AdcDacBram::set_dac_data>(__dac_buffer);
    }

    void get_adc() {
        client.call<op::AdcDacBram::get_adc>();
        auto buffer = client.recv<std::array<uint32_t, N_PTS>>();

        for (uint32_t i = 0; i < N_PTS; i++) {
            adc1[i] = double(((int32_t(buffer[i] & 0x3FFF) - 8192) & 0x3FFF) - 8192) / 8192.0;
            adc2[i] = double(((int32_t(buffer[i] >> 16) - 8192) & 0x3FFF) - 8192) / 8192.0;
        }
    }

    std::array<double, N_PTS> adc1;
    std::array<double, N_PTS> adc2;

  private:
    KoheronClient& client;
    std::array<uint32_t, N_PTS> __dac_buffer;

};

#endif // __DRIVER_HPP__