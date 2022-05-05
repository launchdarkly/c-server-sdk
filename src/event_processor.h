#pragma once

#include <launchdarkly/boolean.h>
#include <launchdarkly/json.h>
#include <launchdarkly/variations.h>

#include "config.h"
#include "user.h"
#include "time_utils.h"

/* EventProcessor is an opaque handle to the component that
 * stores analytic events generated by the SDK.
 *
 * Events are pulled from the EventProcessor by a dedicated network thread;
 * the EventProcessor itself performs no network activity.
 */
struct LDEventProcessor;


/* Allocate and configure a new LDEventProcessor. */
struct LDEventProcessor *
LDEventProcessor_Create(const struct LDConfig *config);

/* Free memory associated with an LDEventProcessor. */
void
LDEventProcessor_Destroy(struct LDEventProcessor *processor);


/* The following methods are invoked by user-facing LDClient methods. */

LDBoolean
LDEventProcessor_Identify(
    struct LDEventProcessor *processor,
    const struct LDUser *user
);


LDBoolean
LDEventProcessor_Track(
        struct LDEventProcessor *processor,
        const struct LDUser *user,
        const char *eventKey,
        struct LDJSON *data /* ownership transferred if the call succeeds */
);


LDBoolean
LDEventProcessor_TrackMetric(
        struct LDEventProcessor *processor,
        const struct LDUser *user,
        const char *eventKey,
        struct LDJSON *data, /* ownership transferred if the call succeeds */
        double metric
);

LDBoolean
LDEventProcessor_Alias(
    struct LDEventProcessor *processor,
    const struct LDUser *currentUser,
    const struct LDUser *previousUser
);


/* The following methods are used by evaluation code. */

struct EvaluationResult {
    /* required */
    const struct LDUser *user;
    /* optional */
    struct LDJSON *subEvents;
    /* required */
    const char *flagKey;
    /* required */
    const struct LDJSON *actualValue;
    /* required */
    const struct LDJSON *fallbackValue;
    /* optional */
    const struct LDJSON *flag;
    /* required */
    const struct LDDetails *details;
    /* required */
    LDBoolean detailedEvaluation;
};


LDBoolean
LDEventProcessor_ProcessEvaluation(struct LDEventProcessor *processor, struct EvaluationResult *result);


struct LDJSON *
LDi_newFeatureEvent(
        const char *key,
        const struct LDUser *user,
        const unsigned int *variation,
        const struct LDJSON *value,
        const struct LDJSON *defaultValue,
        const char *prereqOf,
        const struct LDJSON *flag,
        const struct LDDetails *details,
        struct LDTimestamp timestamp,
        LDBoolean inlineUsersInEvents,
        LDBoolean allAttributesPrivate,
        const struct LDJSON *privateAttributeNames
);

/* The following methods are used by the analytics networking thread. */

void
LDEventProcessor_SetLastServerTime(
    struct LDEventProcessor *processor,
    time_t unixTime
);

LDBoolean
LDEventProcessor_CreateEventPayloadAndResetState(
    struct LDEventProcessor *processor,
    struct LDJSON **result
);


/* Used by tests. */

struct LDJSON *
LDEventProcessor_GetEvents(struct LDEventProcessor *processor);

double
LDEventProcessor_GetLastServerTime(struct LDEventProcessor *processor);
