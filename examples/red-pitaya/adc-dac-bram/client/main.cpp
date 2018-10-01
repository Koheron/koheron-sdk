#include "driver.hpp"
#include <assert.h>
#include <math.h>
#include <algorithm>

#define KOHERON_SERVER_PORT 36000

int main(int argc, char *argv[])
{
    const char* host = "192.168.1.100";
    if (argc > 1) {
        host = argv[1];
    }
    printf("Try connecting to host: %s \n", host);

    auto client = KoheronClient(host, KOHERON_SERVER_PORT);
    client.connect();
    auto driver = Driver(client);
    printf("Connected...\n");

    for (uint32_t i = 0; i<256; i++) {
        driver.set_led(i);
    }

    std::array<float, N_PTS> dac1;
    std::array<float, N_PTS> dac2;

    dac1.fill(-0.99999);
    dac2.fill(0.5);

    driver.set_dac(dac1, dac2);

    while (true) {
        driver.get_adc();
        double sum = 0;
        std::for_each(driver.adc1.begin(), driver.adc1.end(), [&sum](double v) {sum += v;});
        printf("Average of ADC1 = %f \n", sum / N_PTS);
    };

    return 0;
}