#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "uthash.h"

#include "ldapi.h"

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
    /* privateAttributeNames */
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
    /* LDNode *custom; */
    /* LDNode *privateAttributeNames; */
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

/* **** LDUtility **** */

#define LD_ASSERT(condition) \
    if (!(condition)) { \
        printf(0, "LD_ASSERT failed: expected condition '%s' in function '%s' aborting\n", #condition, __func__); \
        abort(); \
    } \
