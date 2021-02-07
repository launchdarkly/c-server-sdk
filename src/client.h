#pragma once

#include <stdbool.h>

#include <launchdarkly/json.h>

#include "concurrency.h"
#include "event_processor.h"
#include "lru.h"

struct LDClient
{
    bool                   shuttingdown;
    struct LDConfig *      config;
    ld_thread_t            thread;
    ld_rwlock_t            lock;
    bool                   shouldFlush;
    struct LDStore *       store;
    struct EventProcessor *eventProcessor;
};
