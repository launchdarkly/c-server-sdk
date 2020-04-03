#include <string.h>

#include <launchdarkly/api.h>

#include "network.h"
#include "misc.h"

#ifdef _WIN32
    #include <windows.h>
    #define _CRT_RAND_S
#else
    #include <time.h>
    #include <unistd.h>
#endif

int
LDi_strncasecmp(const char *const s1, const char *const s2, const size_t n)
{
    #ifdef _WIN32
        return _strnicmp(s1, s2, n);
    #else
        return strncasecmp(s1, s2, n);
    #endif
}

bool
LDi_random(unsigned int *const result)
{
    #ifdef _WIN32
        LD_ASSERT(result);
        errno_t status;
        status = rand_s(result);
        LD_ASSERT(status == 0);
        return status == 0;
    #else
        static unsigned int state;
        static bool init = false;
        static ld_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

        LD_ASSERT(result);

        LDi_mutex_lock(&lock);

        if (!init) {
            state = time(NULL);
            init  = true;
        }

        *result = rand_r(&state);

        LDi_mutex_unlock(&lock);

        return true;
    #endif
}

bool
LDi_sleepMilliseconds(const unsigned long milliseconds)
{
    #ifdef _WIN32
        Sleep(milliseconds);
        return true;
    #else
        int status;

        if ((status = usleep(1000 * milliseconds)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg), "usleep failed with: %s",
                strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);

            LD_ASSERT(false);

            return false;
        }

        return true;
    #endif
}

#ifndef _WIN32
    static unsigned long
    LDi_timeSpecToMilliseconds(const struct timespec ts)
    {
        return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
    }

    bool
    LDi_clockGetTime(struct timespec *const ts, ld_clock_t clockid)
    {
        #ifdef __APPLE__
            kern_return_t status;
            clock_serv_t clock_serve;
            mach_timespec_t mach_timespec;

            status = host_get_clock_service(mach_host_self(), clockid,
                &clock_serve);

            if (status != KERN_SUCCESS) {
                return false;
            }

            status = clock_get_time(clock_serve, &mach_timespec);
            mach_port_deallocate(mach_task_self(), clock_serve);

            if (status != KERN_SUCCESS) {
                return false;
            }

            ts->tv_sec  = mach_timespec.tv_sec;
            ts->tv_nsec = mach_timespec.tv_nsec;
        #else
            if (clock_gettime(clockid, ts) != 0) {
                return false;
            }
        #endif

        return true;
    }
#endif

bool
LDi_getMonotonicMilliseconds(unsigned long *const resultMilliseconds)
{
    #ifdef _WIN32
        *resultMilliseconds = GetTickCount64();
        return true;
    #else
        struct timespec ts;
        if (LDi_clockGetTime(&ts, LD_CLOCK_MONOTONIC)) {
            *resultMilliseconds = LDi_timeSpecToMilliseconds(ts);
            return true;
        } else {
            LD_ASSERT(false);
            return false;
        }
    #endif
}

bool
LDi_getUnixMilliseconds(unsigned long *const resultMilliseconds)
{
    #ifdef _WIN32
        *resultMilliseconds = (double)time(NULL) * 1000.0;
        return true;
    #else
        struct timespec ts;
        if (LDi_clockGetTime(&ts, LD_CLOCK_REALTIME)) {
            *resultMilliseconds = LDi_timeSpecToMilliseconds(ts);
            return true;
        } else {
            LD_ASSERT(false);
            return false;
        }
    #endif
}
