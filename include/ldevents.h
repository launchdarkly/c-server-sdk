/*!
 * @file ldevents.h
 * @brief Internal API Interface for Evaluation
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

struct LDJSON *newBaseEvent(const struct LDUser *const user);

struct LDJSON *newFeatureRequestEvent(const char *const key,
    const struct LDUser *const user, const unsigned int *const variation,
    const struct LDJSON *const value, const struct LDJSON *const defaultValue,
    const char *const prereqOf, const struct LDJSON *const flag,
    const struct LDJSON *const reason);

struct LDJSON *newCustomEvent(const struct LDUser *const user,
    const char *const key, struct LDJSON *const data);

struct LDJSON *newIdentifyEvent(const struct LDUser *const user);
