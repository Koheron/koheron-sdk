#include "../pacing.hpp"
#include <cassert>

int main()
{
    using namespace bist;
    using namespace std::chrono_literals;
    const Clock::time_point epoch{};
    Schedule schedule(epoch, 2ms, 5);
    assert(schedule.release() == epoch);
    schedule.skip_expired(epoch + 1900us);
    assert(schedule.missed == 0);
    schedule.advance();
    schedule.skip_expired(epoch + 7500us);
    assert(schedule.missed == 2); // Slots at 2 and 4 ms are gone, 6 ms is still usable.
    assert(schedule.release() == epoch + 6ms);
    schedule.advance();
    schedule.skip_expired(epoch + 20ms);
    assert(schedule.done() && schedule.missed == 3);

    Schedule boundary(epoch, 2ms, 5);
    boundary.skip_expired(epoch + 2ms);
    assert(boundary.missed == 1 && boundary.release() == epoch + 2ms);
    boundary.skip_expired(epoch + 10ms);
    assert(boundary.done() && boundary.missed == 5);

    std::atomic<bool> cancelled{true};
    PacedResult result;
    paced(result, Clock::now(), 600, 2, cancelled, [] { assert(false); return true; });
    assert(result.requested == 1200 && result.started == 0 && result.missed == 1200);
    assert(!result.valid());
    result.completed = 1176;
    assert(result.valid()); // Exactly the declared 98% minimum.
    result.completed = 1175;
    assert(!result.valid());
    result.completed = 1200;
    result.errors = 1;
    assert(!result.valid());
    result.errors = 0;
    result.affinity_error = 22;
    assert(!result.valid());
}
