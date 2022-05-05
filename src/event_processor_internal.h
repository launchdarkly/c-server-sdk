#pragma once

/* exposed for testing */

#include "concurrency.h"
#include "lru.h"

#include "event_processor.h"

LDBoolean
LDi_summarizeEvent(
    struct LDEventProcessor *context,
    const struct LDJSON *event,
    LDBoolean unknown
);

void
LDi_addEvent(struct LDEventProcessor *context, struct LDJSON *event);

struct LDJSON *
LDi_newBaseEvent(const char *kind, struct LDTimestamp timestamp);

LDBoolean
LDi_addUserInfoToEvent(
    struct LDJSON *event,
    const struct LDUser *user,
    LDBoolean inlineUsersInEvents,
    LDBoolean allAttributesPrivate,
    const struct LDJSON *privateAttributeNames
);

char *
LDi_makeSummaryKey(const struct LDJSON *event);

struct LDJSON *
LDi_newIdentifyEvent(
    const struct LDUser *user,
    struct LDTimestamp timestamp,
    LDBoolean allAttributesPrivate,
    const struct LDJSON *privateAttributeNames
);

struct LDJSON *
LDi_newCustomEvent(
        const struct LDUser *user,
        const char* key,
        struct LDJSON* data,
        double metric,
        LDBoolean hasMetric,
        LDBoolean inlineUsersInEvents,
        LDBoolean allAttributesPrivate,
        const struct LDJSON *privateAttributeNames,
        struct LDTimestamp timestamp
);

struct LDJSON *
LDi_newAliasEvent(
    const struct LDUser *currentUser,
    const struct LDUser *previousUser,
    struct LDTimestamp  timestamp
);

void
LDi_possiblyQueueEvent(
    struct LDEventProcessor *context,
    struct LDJSON *event,
    struct LDTimestamp until,
    LDBoolean detailedEvaluation
);

LDBoolean
LDi_maybeMakeIndexEvent(
    struct LDEventProcessor *context,
    const struct LDUser *user,
    struct LDTimestamp timestamp,
    struct LDJSON **result
);

struct LDJSON *
LDi_objectToArray(const struct LDJSON *object);

struct LDJSON *
LDi_prepareSummaryEvent(struct LDEventProcessor *context, double now);
