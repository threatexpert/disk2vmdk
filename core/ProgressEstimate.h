#pragma once

#include <stdint.h>

// Speeds are measured by the caller; pass zero when the current sample has
// no progress. A false result means unknown/finishing, not zero seconds left.
inline bool EstimateRemainingSeconds(bool sparseOutput,
    uint64_t position, uint64_t diskSize, uint64_t dataCopied, uint64_t dataSize,
    double dataSpeed, double positionSpeed, uint64_t& seconds)
{
    seconds = 0;
    if (position >= diskSize)
        return false; // Output finalization is not measured by copy throughput.

    // Sparse images skip free ranges; DD must actually write those zero bytes.
    uint64_t remaining = sparseOutput
        ? ((dataSize > dataCopied) ? dataSize - dataCopied : 0)
        : diskSize - position;
    double speed = sparseOutput ? dataSpeed : positionSpeed;
    if (remaining == 0 || !(speed > 0.0))
        return false;

    double estimate = (double)remaining / speed;
    if (!(estimate < (double)UINT64_MAX))
        return false;
    seconds = (uint64_t)estimate;
    if ((double)seconds < estimate)
        ++seconds;
    if (seconds == 0)
        seconds = 1; // Never report completion while copy work remains.
    return true;
}
