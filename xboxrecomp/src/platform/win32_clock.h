/* sheroes-nx: clock_gettime(CLOCK_MONOTONIC) for Windows builds, where the
 * Visual C++ runtime has no clock_gettime. Include after <time.h>; nothing
 * changes elsewhere. */
#ifndef XBOX_WIN32_CLOCK_H
#define XBOX_WIN32_CLOCK_H
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <time.h>
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
static inline int xbox_win32_clock_gettime(int clk, struct timespec *ts)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER c;
    (void)clk;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&c);
    ts->tv_sec = (time_t)(c.QuadPart / freq.QuadPart);
    ts->tv_nsec = (long)((c.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
    return 0;
}
#define clock_gettime xbox_win32_clock_gettime
#endif
#endif
