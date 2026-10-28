/* sheroes-nx: the few POSIX calls nv2a_vk.c makes, for a Windows build of the
 * Vulkan renderer (the same renderer as the Switch, so a PC test exercises the
 * Switch's code). Only what nv2a_vk.c uses: a monotonic clock and one thread
 * with a chosen stack size, joined at once (glslang's compile). */
#ifndef NV2A_VK_WIN32_H
#define NV2A_VK_WIN32_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <process.h>
#include <time.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

static int nv2a_win_clock_gettime(int clk, struct timespec *ts)
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
#define clock_gettime nv2a_win_clock_gettime

typedef struct { HANDLE h; } nv2a_pthread_t;
typedef struct { size_t stack; } nv2a_pthread_attr_t;
typedef struct { void *(*fn)(void *); void *arg; } nv2a_pthread_start;

static unsigned __stdcall nv2a_pthread_tramp(void *p)
{
    nv2a_pthread_start s = *(nv2a_pthread_start *)p;
    free(p);
    s.fn(s.arg);
    return 0;
}

static int nv2a_pthread_attr_init(nv2a_pthread_attr_t *a) { a->stack = 0; return 0; }
static int nv2a_pthread_attr_setstacksize(nv2a_pthread_attr_t *a, size_t n) { a->stack = n; return 0; }
static int nv2a_pthread_attr_destroy(nv2a_pthread_attr_t *a) { (void)a; return 0; }

static int nv2a_pthread_create(nv2a_pthread_t *t, const nv2a_pthread_attr_t *a,
                               void *(*fn)(void *), void *arg)
{
    nv2a_pthread_start *s = (nv2a_pthread_start *)malloc(sizeof *s);
    if (!s)
        return -1;
    s->fn = fn;
    s->arg = arg;
    t->h = (HANDLE)_beginthreadex(NULL, a ? (unsigned)a->stack : 0, nv2a_pthread_tramp, s,
                                  STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!t->h) {
        free(s);
        return -1;
    }
    return 0;
}

static int nv2a_pthread_join(nv2a_pthread_t t, void **ret)
{
    (void)ret;
    WaitForSingleObject(t.h, INFINITE);
    CloseHandle(t.h);
    return 0;
}

#define pthread_t              nv2a_pthread_t
#define pthread_attr_t         nv2a_pthread_attr_t
#define pthread_attr_init      nv2a_pthread_attr_init
#define pthread_attr_setstacksize nv2a_pthread_attr_setstacksize
#define pthread_attr_destroy   nv2a_pthread_attr_destroy
#define pthread_create         nv2a_pthread_create
#define pthread_join           nv2a_pthread_join

#endif
