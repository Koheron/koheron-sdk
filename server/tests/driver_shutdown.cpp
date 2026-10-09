#include "driver_shutdown_instrument.hpp"

int main() {
    services::provide<Bus>();
    auto manager = services::provide<rt::DriverManager>();
    manager->shutdown(); // No lazily allocated drivers yet.
    for (unsigned round = 0; round < 20; ++round) {
        destroyed.clear();
        worker_reads = 0;
        manager->get<Acquisition>();
        while (worker_reads == 0) { std::this_thread::yield(); }
        manager->shutdown();
        assert((destroyed == std::vector<int>{2, 1}));
        const auto reads = worker_reads.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(worker_reads == reads);
        manager->shutdown(); // Repeated shutdown must not destroy twice.
        assert(destroyed.size() == 2);
    }
    services::remove<Bus>();
    assert((destroyed == std::vector<int>{2, 1, 0}));
    services::remove<rt::DriverManager>();
}
