#pragma once

/* exposed for testing */

#include "concurrency.h"
#include "lru.h"

#include "event_processor.h"

struct EventProcessor {
    ld_mutex_t lock;
    struct LDJSON *events; /* Array of Objects */
    struct LDJSON *summaryCounters; /* Object */
    unsigned long summaryStart;
    struct LDLRU *userKeys;
    unsigned long lastUserKeyFlush;
    unsigned long long lastServerTime;
    const struct LDConfig *config;
};

bool
LDi_summarizeEvent(
    struct EventProcessor *const context,
    const struct LDJSON *const   event,
    const bool                   unknown
);

void
LDi_addEvent(
    struct EventProcessor *const context,
    struct LDJSON *const         event
);

struct LDJSON *
LDi_newBaseEvent(
    const char *const   kind,
    const unsigned long now
);

bool
LDi_addUserInfoToEvent(
    const struct EventProcessor *const context,
    struct LDJSON *const               event,
    const struct LDUser *const         user
);

char *LDi_makeSummaryKey(const struct LDJSON *const event);

struct LDJSON *
LDi_newIdentifyEvent(
    const struct EventProcessor *const context,
    const struct LDUser *const         user,
    const unsigned long                now
);

struct LDJSON *
LDi_newCustomEvent(
    const struct EventProcessor *const context,
    const struct LDUser *const         user,
    const char *const                  key,
    struct LDJSON *const               data,
    const double                       metric,
    const bool                         hasMetric,
    const unsigned long                now
);

void
LDi_possiblyQueueEvent(
    struct EventProcessor *const context,
    struct LDJSON *const         event,
    const unsigned long          now
);

bool
LDi_maybeMakeIndexEvent(
    struct EventProcessor *const context,
    const struct LDUser *const   user,
    const unsigned long          now,
    struct LDJSON **const        result
);

struct LDJSON *LDi_objectToArray(const struct LDJSON *const object);

struct LDJSON *
LDi_prepareSummaryEvent(
    struct EventProcessor *const context,
    const unsigned long          now
);
