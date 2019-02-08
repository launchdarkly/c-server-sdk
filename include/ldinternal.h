#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#include "ldapi.h"
#include "ldstore.h"
#include "ldjson.h"

/* **** Forward Declarations **** */

struct LDHashSet;

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
bool LDi_createthread(ld_thread_t *const thread, THREAD_RETURN (*const routine)(void *), void *const argument);

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
    struct LDJSON *privateAttributeNames; /* Array of Text */
    struct LDJSON *custom; /* Object, may be NULL */
};

struct LDJSON *valueOfAttribute(const struct LDUser *const user, const char *const attribute);
struct LDJSON *LDUserToJSON(struct LDClient *const client, struct LDUser *const lduser, const bool redact);

/* **** LDClient **** */

struct LDClient {
    /* LDClientInit */
    bool initialized;
    bool shuttingdown;
    struct LDConfig *config;
    ld_thread_t thread;
    ld_rwlock_t lock;
    /* LDNetInit */
    CURLM *multihandle;
};

/* **** LDNetwork **** */

bool LDi_networkinit(struct LDClient *const client);

THREAD_RETURN LDi_networkthread(void *const clientref);

/* **** LDLogging **** */

void LDi_log(const LDLogLevel level, const char *const format, ...);

/* **** LDUtility **** */

bool sleepMilliseconds(const unsigned int milliseconds);
bool getMonotonicMilliseconds(unsigned int *const resultMilliseconds);

#define LD_ASSERT(condition) \
    if (!(condition)) { \
        LDi_log(0, "LD_ASSERT failed: expected condition '%s' in function '%s' aborting\n", #condition, __func__); \
        abort(); \
    }
