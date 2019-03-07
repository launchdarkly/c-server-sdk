/*!
 * @file ldevents.h
 * @brief Internal API Interface for Evaluation
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

/* event construction */
struct LDJSON *newBaseEvent(const struct LDUser *const user,
    const char *const kind);

struct LDJSON *newFeatureRequestEvent(const char *const key,
    const struct LDUser *const user, const unsigned int *const variation,
    const struct LDJSON *const value, const struct LDJSON *const defaultValue,
    const char *const prereqOf, const struct LDJSON *const flag,
    const struct LDJSON *const reason);

struct LDJSON *newCustomEvent(const struct LDUser *const user,
    const char *const key, struct LDJSON *const data);

struct LDJSON *newIdentifyEvent(const struct LDUser *const user);

/* event recording */
bool addEvent(struct LDClient *const client, const struct LDJSON *const event);

bool summarizeEvent(struct LDClient *const client,
    const struct LDJSON *const event, const bool unknown);

char *makeSummaryKey(const struct LDJSON *const event);
