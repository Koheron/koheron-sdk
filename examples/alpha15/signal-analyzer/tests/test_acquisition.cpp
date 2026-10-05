#include "examples/alpha15/signal-analyzer/moving_averager.hpp"
#include "examples/alpha15/signal-analyzer/spectrum_snapshot.hpp"

#include <cassert>
#include <cmath>
#include <vector>

int main() {
    MovingAverager<16> averager;
    SpectrumSnapshot publication(2);
    for (int i = 0; i < 16; ++i) { averager.append(std::vector<double>{100, 200}); }
    assert(averager.full());
    publication.publish(averager.average());
    const auto old = publication.snapshot();
    assert(std::get<1>(old) == 1);
    assert(std::get<2>(old)[0] == 100);

    averager.clear();
    const auto generation = publication.restart();
    assert(generation != std::get<0>(old));
    assert(std::get<1>(publication.snapshot()) == 0);
    assert(std::isnan(std::get<2>(publication.snapshot())[0]));
    for (int i = 0; i < 15; ++i) {
        averager.append(std::vector<double>{1, 2});
        assert(!averager.full());
    }
    averager.append(std::vector<double>{1, 2});
    assert(averager.full());
    publication.publish(averager.average());
    auto [epoch, sequence, values] = publication.snapshot();
    assert(epoch == generation && sequence == 1);
    assert(values[0] == 1 && values[1] == 2);
    values[0] = 99;
    assert(std::get<2>(publication.snapshot())[0] == 1);
    publication.publish(std::vector<double>{3, 4});
    assert(std::get<1>(publication.snapshot()) == 2);
    // A second reset while refilling cannot retain a partial previous average.
    averager.clear(); publication.restart();
    averager.append(std::vector<double>{9, 9});
    averager.clear(); publication.restart();
    for (int i = 0; i < 16; ++i) { averager.append(std::vector<double>{5, 6}); }
    publication.publish(averager.average());
    assert(std::get<2>(publication.snapshot())[0] == 5);
}
