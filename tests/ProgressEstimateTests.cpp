#include "../core/ProgressEstimate.h"
#include <assert.h>
#include <limits>
#include <stdio.h>

int main()
{
    const uint64_t MiB = 1024 * 1024;
    const uint64_t GiB = 1024 * MiB;
    const double oneMiBPerSecond = (double)MiB;
    uint64_t seconds = 123;

    // Same 100 GiB disk, 10 GiB used: sparse ETA excludes the 90 GiB hole.
    assert(EstimateRemainingSeconds(true, GiB, 100 * GiB, GiB, 10 * GiB,
        100 * MiB, 100 * MiB, seconds) && seconds == 93);
    assert(EstimateRemainingSeconds(false, GiB, 100 * GiB, GiB, 10 * GiB,
        100 * MiB, 100 * MiB, seconds) && seconds == 1014);

    // DD still has zero bytes to write after the last used cluster.
    assert(EstimateRemainingSeconds(false, 10 * GiB, 100 * GiB, 10 * GiB, 10 * GiB,
        0, 100 * MiB, seconds) && seconds == 922);
    assert(!EstimateRemainingSeconds(true, 10 * GiB, 100 * GiB, 10 * GiB, 10 * GiB,
        100 * MiB, 100 * MiB, seconds) && seconds == 0);

    // Empty source, all-free sparse source, and finalization have no copy ETA.
    assert(!EstimateRemainingSeconds(true, 0, 0, 0, 0, 0, 0, seconds));
    assert(!EstimateRemainingSeconds(true, 0, GiB, 0, 0, 0, oneMiBPerSecond, seconds));
    assert(!EstimateRemainingSeconds(false, GiB, GiB, GiB, GiB, oneMiBPerSecond, oneMiBPerSecond, seconds));

    // No sample / stalled sample must not count a previous estimate down.
    seconds = 100;
    assert(!EstimateRemainingSeconds(true, 0, GiB, 0, GiB, 0, oneMiBPerSecond, seconds) && seconds == 0);
    assert(!EstimateRemainingSeconds(false, 0, GiB, 0, GiB, oneMiBPerSecond, 0, seconds));

    // Low speeds are valid, subsecond work rounds up, counters never underflow.
    assert(EstimateRemainingSeconds(true, 0, 4096, 0, 1024, 512, 512, seconds) && seconds == 2);
    assert(EstimateRemainingSeconds(true, 0, 4096, 1023, 1024, oneMiBPerSecond, oneMiBPerSecond, seconds) && seconds == 1);
    assert(!EstimateRemainingSeconds(true, 0, 4096, 1025, 1024, oneMiBPerSecond, oneMiBPerSecond, seconds));
    assert(!EstimateRemainingSeconds(false, 4097, 4096, 0, 0, oneMiBPerSecond, oneMiBPerSecond, seconds));
    assert(!EstimateRemainingSeconds(true, 0, 4096, 0, 1024,
        std::numeric_limits<double>::quiet_NaN(), oneMiBPerSecond, seconds));
    assert(!EstimateRemainingSeconds(true, 0, UINT64_MAX, 0, UINT64_MAX, 0.1, 0.1, seconds));

    puts("ProgressEstimate: 15 cases passed");
    return 0;
}
