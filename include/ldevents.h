/*!
 * @file ldevents.h
 * @brief Internal API Interface for Evaluation
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "ldvariations.h"

/* event construction */
struct LDJSON *LDi_newBaseEvent(struct LDClient *const client,
    const struct LDUser *const user, const char *const kind);

struct LDJSON *LDi_newFeatureRequestEvent(struct LDClient *const client,
    const char *const key, const struct LDUser *const user,
    const unsigned int *const variation, const struct LDJSON *const value,
    const struct LDJSON *const defaultValue, const char *const prereqOf,
    const struct LDJSON *const flag, const struct LDDetails *const reason);

struct LDJSON *LDi_newCustomEvent(struct LDClient *const client,
    const struct LDUser *const user, const char *const key,
    struct LDJSON *const data);

struct LDJSON *newIdentifyEvent(struct LDClient *const client,
    const struct LDUser *const user);

/* event recording */
void LDi_addEvent(struct LDClient *const client,
    struct LDJSON *const event);

bool LDi_summarizeEvent(struct LDClient *const client,
    const struct LDJSON *const event, const bool unknown);

char *LDi_makeSummaryKey(const struct LDJSON *const event);

struct LDJSON *LDi_prepareSummaryEvent(struct LDClient *const client);

size_t LDi_onHeader(const char *const buffer, const size_t size,
    const size_t itemcount, void *const context);

bool LDi_parseRFC822(const char *const date, struct tm *tm);
