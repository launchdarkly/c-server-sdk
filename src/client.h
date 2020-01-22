#pragma once

#include <launchdarkly/api.h>

#include "misc.h"
#include "lru.h"

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
    unsigned long long lastServerTime;
    struct LDLRU *userKeys;
    unsigned long lastUserKeyFlush;
    struct LDStore *store;
};
