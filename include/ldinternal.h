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
    #define THREAD_RETURN DWORD
    #define THREAD_RETURN_DEFAULT 0
    #define ld_thread_t HANDLE

    #define ld_rwlock_t SRWLOCK
    #define ld_mutex_t CRITICAL_SECTION
#else
    #define THREAD_RETURN void *
    #define THREAD_RETURN_DEFAULT NULL
    #define ld_thread_t pthread_t

    #define ld_rwlock_t pthread_rwlock_t
    #define ld_mutex_t pthread_mutex_t
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

bool LDi_mtxinit(ld_mutex_t *const mutex);
bool LDi_mtxdestroy(ld_mutex_t *const mutex);
bool LDi_mtxlock(ld_mutex_t *const mutex);
bool LDi_mtxunlock(ld_mutex_t *const mutex);

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

struct LDJSON *LDi_valueOfAttribute(const struct LDUser *const user,
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
    unsigned long lastServerTime;
};

/* **** LDUtility **** */

bool LDi_sleepMilliseconds(const unsigned long milliseconds);
bool LDi_getMonotonicMilliseconds(unsigned long *const resultMilliseconds);
bool LDi_getUnixMilliseconds(unsigned long *const resultMilliseconds);

#ifdef _WIN32
    #define LD_RAND_MAX UINT_MAX
#else
    #define LD_RAND_MAX RAND_MAX
#endif

/* do not use in cryptographic / security focused contexts */
bool LDi_random(unsigned int *const result);

bool LDSetString(char **const target, const char *const value);

double LDi_normalize(const double n, const double nmin, const double nmax,
    const double omin, const double omax);

bool LDi_notNull(const struct LDJSON *const json);
bool LDi_isDeleted(const struct LDJSON *const feature);
bool LDi_textInArray(const struct LDJSON *const array, const char *const text);

#define ASSERT_FMT \
    "LD_ASSERT failed: expected condition '%s' aborting\n"

#define LD_ASSERT(condition) \
    if (!(condition)) { \
        LD_LOG(LD_LOG_FATAL, "LD_ASSERT failed: " #condition " aborting"); \
        abort(); \
    }
