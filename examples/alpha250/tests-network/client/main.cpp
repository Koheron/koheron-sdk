#include <assert.h>
#include <math.h>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <array>

#include "server/client/koheron-client.hpp"

class Driver {
  public:
    Driver(KoheronClient& client_) : client(client_) {}

    void set_test_vector(uint32_t size) {
        client.call<op::AdcDacDma::set_test_vector>(size);
    }

    void get_test_vector() {
        client.call<op::AdcDacDma::get_test_vector>();
        client.recv<op::AdcDacDma::get_test_vector>(vec);
    }

    std::vector<uint32_t> vec;

  private:
    KoheronClient& client;
};

int main(int argc, char *argv[])
{
    const char* host = "192.168.1.98";
    if (argc > 1) host = argv[1];
    std::printf("Try connecting to host: %s\n", host);

    KoheronClient client(host, 36000);
    client.connect();
    Driver driver(client);
    std::printf("Connected...\n");

    driver.set_test_vector(16 * 1004232);

    using clock = std::chrono::steady_clock;
    using secd  = std::chrono::duration<double>;

    // Warm-up
    driver.get_test_vector();

    // Rolling average over last 5 rates
    std::array<double, 5> rbuf{};   // MiB/s
    std::size_t ridx = 0;
    std::size_t rcount = 0;

    for (uint64_t iter = 1; iter < 21; ++iter) {
        const auto t0 = clock::now();
        driver.get_test_vector();
        const auto t1 = clock::now();

        const double dt   = std::chrono::duration_cast<secd>(t1 - t0).count();
        const std::size_t bytes = driver.vec.size() * sizeof(uint32_t);
        const double mib  = bytes / (1024.0 * 1024.0);
        const double rate = (dt > 0.0) ? (mib / dt) : 0.0;

        // Update rolling buffer
        rbuf[ridx] = rate;
        ridx = (ridx + 1) % rbuf.size();
        if (rcount < rbuf.size()) {
            ++rcount;
        }

        double avg5 = 0.0;
        for (std::size_t i = 0; i < rcount; ++i) {
            avg5 += rbuf[i];
        }
        
        if (rcount) {
            avg5 /= static_cast<double>(rcount);
        }

        std::printf(
            "iter=%6llu size=%10zu elems (%8.2f MiB)  took=%7.3f ms"
            "  rate=%7.2f MiB/s  avg5=%7.2f MiB/s\n",
            static_cast<unsigned long long>(iter),
            driver.vec.size(), mib, dt * 1e3, rate, avg5
        );
        std::fflush(stdout);
    }

    return 0;
}