#pragma once

#include <launchdarkly/json.h>
#include <launchdarkly/variations.h>

#include "config.h"
#include "user.h"

struct EventProcessor;

struct EventProcessor *
LDi_newEventProcessor(
    const struct LDConfig *const config
);

void
LDi_freeEventProcessor(
    struct EventProcessor *const context
);

bool
LDi_identify(
    struct EventProcessor *const context,
    const struct LDUser *const   user
);

bool
LDi_track(
    struct EventProcessor *const context,
    const struct LDUser *const   user,
    const char *const            key,
    struct LDJSON *const         data,
    const double                 metric,
    const bool                   hasMetric
);

bool
LDi_processEvaluation(
    /* required */
    struct EventProcessor *const  context,
    /* required */
    const struct LDUser *const    user,
    /* optional */
    struct LDJSON *const          subEvents,
    /* required */
    const char *const             flagKey,
    /* required */
    const struct LDJSON *const    actualValue,
    /* required */
    const struct LDJSON *const    fallbackValue,
    /* optional */
    const struct LDJSON *const    flag,
    /* required */
    const struct LDDetails *const details,
    /* required */
    const bool                    detailedEvaluation
);

bool
LDi_bundleEventPayload(
    struct EventProcessor *const context,
    struct LDJSON **const        result
);

struct LDJSON *
LDi_newFeatureRequestEvent(
    const struct EventProcessor *const  context,
    const char *const                   key,
    const struct LDUser *const          user,
    const unsigned int *const           variation,
    const struct LDJSON *const          value,
    const struct LDJSON *const          defaultValue,
    const char *const                   prereqOf,
    const struct LDJSON *const          flag,
    const struct LDDetails *const       details,
    const unsigned long                 now
);

void
LDi_setServerTime(
    struct EventProcessor *const context,
    const unsigned long long     serverTime
);
