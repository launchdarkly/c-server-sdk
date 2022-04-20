#pragma once

#include <launchdarkly/json.h>

#include "concurrency.h"
#include "event_processor.h"
#include "lru.h"

struct LDClient
{
    LDBoolean              shuttingdown;
    struct LDConfig *      config;
    ld_thread_t            thread;
    ld_rwlock_t            lock;
    LDBoolean              shouldFlush;
    struct LDStore *       store;
    struct LDEventProcessor *eventProcessor;
};
