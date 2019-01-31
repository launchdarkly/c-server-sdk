#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "uthash.h"
#include "cJSON.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#include "ldapi.h"
#include "ldstore.h"

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
    struct LDHashSet *privateAttributeNames;
    unsigned int userKeysCapacity;
    unsigned int userKeysFlushInterval;
    bool defaultStore;
    struct LDFeatureStore *store;
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
    struct LDHashSet *privateAttributeNames;
    struct LDNode *custom;
};

struct LDNode *valueOfAttribute(const struct LDUser *const user, const char *const attribute);

/* **** LDClient **** */

struct LDClient {
    /* LDClientInit */
    bool shuttingdown;
    struct LDConfig *config;
    ld_thread_t thread;
    ld_rwlock_t lock;
    /* LDNetInit */
    CURLM *multihandle;
};

/* **** LDNode **** */

struct LDNode {
    LDNodeType type;
    union {
        char *key;
        unsigned int index;
    } location;
    union {
        bool boolean;
        char *text;
        double number;
        struct LDNode* object;
        struct LDNode* array;
    } value;
    UT_hash_handle hh;
};

cJSON *LDNodeToJSON(const struct LDNode *const node);
struct LDNode *LDNodeFromJSON(const cJSON *const json);

/* **** LDNetwork **** */

bool LDi_networkinit(struct LDClient *const client);

THREAD_RETURN LDi_networkthread(void *const clientref);

/* **** LDLogging **** */

void LDi_log(const LDLogLevel level, const char *const format, ...);

/* **** LDUtility **** */

#define LD_ASSERT(condition) \
    if (!(condition)) { \
        LDi_log(0, "LD_ASSERT failed: expected condition '%s' in function '%s' aborting\n", #condition, __func__); \
        abort(); \
    } \

bool LDHashSetAddKey(struct LDHashSet **const set, const char *const key);
void LDHashSetFree(struct LDHashSet *const set);
struct LDHashSet *LDHashSetLookup(const struct LDHashSet *const set, const char *const key);

struct LDHashSet {
    char *key;
    UT_hash_handle hh;
};
