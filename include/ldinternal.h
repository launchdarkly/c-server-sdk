/*!
 * @file ldinternal.h
 * @brief Internal Miscellaneous Implementation Details
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <curl/curl.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#include "ldapi.h"
#include "ldstore.h"
#include "ldjson.h"

/* **** LDPlatformSpecific **** */

#ifdef _WIN32
    #define THREAD_RETURN DWORD WINAPI
    #define THREAD_RETURN_DEFAULT 0
    #define ld_thread_t HANDLE

    #define LD_RWLOCK_INIT SRWLOCK_INIT
    #define ld_rwlock_t SRWLOCK
#else
    #define THREAD_RETURN void *
    #define THREAD_RETURN_DEFAULT NULL
    #define ld_thread_t pthread_t

    #define LD_RWLOCK_INIT PTHREAD_RWLOCK_INITIALIZER
    #define ld_rwlock_t pthread_rwlock_t
#endif

bool LDi_jointhread(ld_thread_t thread);
bool LDi_createthread(ld_thread_t *const thread,
    THREAD_RETURN (*const routine)(void *), void *const argument);

bool LDi_rwlockinit(ld_rwlock_t *const lock);
bool LDi_rwlockdestroy(ld_rwlock_t *const lock);
bool LDi_rdlock(ld_rwlock_t *const lock);
bool LDi_wrlock(ld_rwlock_t *const lock);
bool LDi_rdunlock(ld_rwlock_t *const lock);
bool LDi_wrunlock(ld_rwlock_t *const lock);

/* **** LDConfig **** */

struct LDConfig {
    char *key;
    char *baseURI;
    char *streamURI;
    char *eventsURI;
    bool stream;
    bool sendEvents;
    unsigned int eventsCapacity;
    unsigned int timeout;
    unsigned int flushInterval;
    unsigned int pollInterval;
    bool offline;
    bool useLDD;
    bool allAttributesPrivate;
    struct LDJSON *privateAttributeNames; /* Array of Text */
    unsigned int userKeysCapacity;
    unsigned int userKeysFlushInterval;
    bool defaultStore;
    struct LDStore *store;
};

/* **** LDUser **** */

struct LDUser {
    char *key;
    bool anonymous;
    char *secondary;
    char *ip;
    char *firstName;
    char *lastName;
    char *email;
    char *name;
    char *avatar;
    char *country;
    struct LDJSON *privateAttributeNames; /* Array of Text */
    struct LDJSON *custom; /* Object, may be NULL */
};

struct LDJSON *valueOfAttribute(const struct LDUser *const user,
    const char *const attribute);

struct LDJSON *LDUserToJSON(struct LDClient *const client,
    const struct LDUser *const lduser, const bool redact);

/* **** LDClient **** */

struct LDClient {
    bool initialized;
    bool shuttingdown;
    struct LDConfig *config;
    ld_thread_t thread;
    ld_rwlock_t lock;
    struct LDJSON *events; /* Array of Objects */
    struct LDJSON *summaryCounters; /* Object */
    unsigned long summaryStart;
    bool shouldFlush;
};

/* **** LDNetwork **** */

THREAD_RETURN LDi_networkthread(void *const clientref);

/* **** LDUtility **** */

bool sleepMilliseconds(const unsigned int milliseconds);
bool getMonotonicMilliseconds(unsigned long *const resultMilliseconds);
bool getUnixMilliseconds(unsigned long *const resultMilliseconds);

bool LDSetString(char **const target, const char *const value);

bool notNull(const struct LDJSON *const json);

#define ASSERT_FMT \
    "LD_ASSERT failed: expected condition '%s' aborting\n"

#define LD_ASSERT(condition) \
    if (!(condition)) { \
        LD_LOG(LD_LOG_FATAL, "LD_ASSERT failed: " #condition " aborting"); \
        abort(); \
    }
