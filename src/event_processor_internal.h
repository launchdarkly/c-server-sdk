#pragma once

/* exposed for testing */

#include "concurrency.h"
#include "lru.h"

#include "event_processor.h"

struct EventProcessor
{
    ld_mutex_t             lock;
    struct LDJSON *        events;          /* Array of Objects */
    struct LDJSON *        summaryCounters; /* Object */
    double                 summaryStart;
    struct LDLRU *         userKeys;
    double                 lastUserKeyFlush;
    double                 lastServerTime;
    const struct LDConfig *config;
};

LDBoolean
LDi_summarizeEvent(
    struct EventProcessor *const context,
    const struct LDJSON *const   event,
    const LDBoolean              unknown);

void
LDi_addEvent(struct EventProcessor *const context, struct LDJSON *const event);

struct LDJSON *
LDi_newBaseEvent(const char *const kind, const double now);

LDBoolean
LDi_addUserInfoToEvent(
    const struct EventProcessor *const context,
    struct LDJSON *const               event,
    const struct LDUser *const         user);

char *
LDi_makeSummaryKey(const struct LDJSON *const event);

struct LDJSON *
LDi_newIdentifyEvent(
    const struct EventProcessor *const context,
    const struct LDUser *const         user,
    const double                       now);

struct LDJSON *
LDi_newCustomEvent(
    const struct EventProcessor *const context,
    const struct LDUser *const         user,
    const char *const                  key,
    struct LDJSON *const               data,
    const double                       metric,
    const LDBoolean                    hasMetric,
    const double                       now);

struct LDJSON *
LDi_newAliasEvent(
    const struct LDUser *const currentUser,
    const struct LDUser *const previousUser,
    const double               now);

void
LDi_possiblyQueueEvent(
    struct EventProcessor *const context,
    struct LDJSON *              event,
    const double                 now,
    const LDBoolean              detailedEvaluation);

LDBoolean
LDi_maybeMakeIndexEvent(
    struct EventProcessor *const context,
    const struct LDUser *const   user,
    const double                 now,
    struct LDJSON **const        result);

struct LDJSON *
LDi_objectToArray(const struct LDJSON *const object);

struct LDJSON *
LDi_prepareSummaryEvent(struct EventProcessor *const context, const double now);
