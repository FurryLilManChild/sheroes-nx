/*
 * sheroes-nx: the Windows side of the host thread hooks that win32_compat.c
 * provides on Linux and the Switch (that file is the POSIX implementation and
 * is not built on Windows, where the real Win32 API is used). The kernel
 * bridge, OHCI and APU call these on every platform.
 *
 * xbox_guest_pin: as on Linux, opt-in with RECOMP_GUEST_ONE_CORE=1 (2: guest
 * threads only), the guest threads share the last core so only one runs at a
 * time, like the Xbox's single CPU (see win32_compat.c for why). The others
 * are Switch-only and do nothing here.
 */
#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

int xbox_guest_core(void)
{
    static int core = -2;
    if (core == -2) {
        const char *e = getenv("RECOMP_GUEST_ONE_CORE");
        core = -1;
        if (e && (*e == '1' || *e == '2')) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            if (si.dwNumberOfProcessors > 1 && si.dwNumberOfProcessors <= 64)
                core = (int)si.dwNumberOfProcessors - 1;
            if (core >= 0)
                fprintf(stderr, "  [THREAD] guest code runs on core %d only\n", core);
        }
    }
    return core;
}

void xbox_guest_pin(int interrupt)
{
    int core = xbox_guest_core();
    const char *e = getenv("RECOMP_GUEST_ONE_CORE");
    if (core < 0)
        return;
    if (interrupt && e && *e == '2')
        return;
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << core);
    if (interrupt)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

void xbox_nx_raise_host_thread(void) { }
void xbox_nx_spread_thread(void) { }
void xbox_nx_track_thread(void *entry) { (void)entry; }
void xbox_nx_retag_thread(void *entry) { (void)entry; }
#endif
