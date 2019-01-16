#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "uthash.h"
#include "cJSON.h"

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
