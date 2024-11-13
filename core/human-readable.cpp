#include "pch.h"
#include <inttypes.h>
#include "human-readable.h"
#include <string>


void calcseconds(uint64_t sec, char* buf, int bufsize)
{
    uint32_t m;
    uint32_t day = (uint32_t)(sec / (3600 * 24));
    m = sec % (3600 * 24);
    uint32_t hour = m / 3600;
    m = m % 3600;
    uint32_t minute = m / 60;
    m = m % 60;
    uint32_t second = m;
    if (day)
        snprintf(buf, bufsize, "%d %s,%.2d:%.2d:%.2d", day, day > 1 ? "days" : "day", hour, minute, second);
    else
        snprintf(buf, bufsize, "%.2d:%.2d:%.2d", hour, minute, second);
}

char* calculateSize1024(uint64_t size, char* buf, int bufsize)
{
    static const char* sizes[] = { "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
    static const uint64_t  exbibytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

    uint64_t  multiplier = exbibytes;
    int i;
    for (i = 0; i < _countof(sizes); i++, multiplier /= 1024)
    {
        if (size < multiplier)
            continue;
        if (size % multiplier == 0)
            snprintf(buf, bufsize, "%" PRIu64 " %s", size / multiplier, sizes[i]);
        else
            snprintf(buf, bufsize, "%.1f %s", (float)size / multiplier, sizes[i]);
        return buf;
    }
    snprintf(buf, bufsize, "0");
    return buf;
}

char* calculateSize1000(uint64_t size, char* buf, int bufsize)
{
    static const char* sizes[] = { "EB", "PB", "TB", "GB", "MB", "KB", "B" };
    static const uint64_t  exbibytes = 1000ULL * 1000ULL * 1000ULL * 1000ULL * 1000ULL * 1000ULL;

    uint64_t  multiplier = exbibytes;
    int i;
    for (i = 0; i < _countof(sizes); i++, multiplier /= 1000)
    {
        if (size < multiplier)
            continue;
        if (size % multiplier == 0)
            snprintf(buf, bufsize, "%" PRIu64 " %s", size / multiplier, sizes[i]);
        else
            snprintf(buf, bufsize, "%.1f %s", (float)size / multiplier, sizes[i]);
        return buf;
    }
    snprintf(buf, bufsize, "0");
    return buf;
}

