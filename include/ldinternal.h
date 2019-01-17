#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "uthash.h"
#include "cJSON.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#include "ldapi.h"

/* **** Forward Declarations **** */

struct LDHashSet;

/* **** LDConfig **** */

struct LDConfig {
    char *key;
    char *baseURI;
    char *streamURI;
    char *eventsURI;
    bool stream;
    bool sendEvents;
    unsigned int timeout;
    unsigned int flushInterval;
    unsigned int pollInterval;
    bool offline;
    bool useLDD;
    bool allAttributesPrivate;
    struct LDHashSet *privateAttributeNames;
    unsigned int userKeysCapacity;
    unsigned int userKeysFlushInterval;
    /* featurestore */
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

/* **** LDClient **** */

struct LDClient {
    struct LDConfig *config;
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

/* **** LDPlatformSpecific **** */

#ifdef _WIN32
    #define THREAD_RETURN DWORD WINAPI
    #define THREAD_RETURN_DEFAULT 0

    #define ld_thread_t HANDLE
    void LDi_createthread(HANDLE *thread, LPTHREAD_START_ROUTINE fn, void *arg);
#else
    #define THREAD_RETURN void *
    #define THREAD_RETURN_DEFAULT NULL

    #define ld_thread_t pthread_t
#endif

bool LDi_createthread(ld_thread_t *const thread, THREAD_RETURN (*const routine)(void *), void *const argument);

/* **** LDUtility **** */

#define LD_ASSERT(condition) \
    if (!(condition)) { \
        printf(0, "LD_ASSERT failed: expected condition '%s' in function '%s' aborting\n", #condition, __func__); \
        abort(); \
    } \

bool LDHashSetAddKey(struct LDHashSet **const set, const char *const key);
void LDHashSetFree(struct LDHashSet *const set);
struct LDHashSet *LDHashSetLookup(const struct LDHashSet *const set, const char *const key);

struct LDHashSet {
    char *key;
    UT_hash_handle hh;
};
