/*!
 * @file ldvariations.h
 * @brief Public API Interface for evaluation variations
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lduser.h"
#include "ldjson.h"

struct LDDetails {
    int variationIndex;
    bool hasVariation;
    struct LDJSON *reason;
};

void LDDetailsInit(struct LDDetails *const details);

void LDDetailsClear(struct LDDetails *const details);

bool LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback,
    struct LDJSON **const details);

int LDIntVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const int fallback,
    struct LDJSON **const details);

double LDDoubleVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key, const double fallback,
    struct LDJSON **const details);

char *LDStringVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key,
    const char* const fallback, struct LDJSON **const details);

struct LDJSON *LDJSONVariation(struct LDClient *const client,
    struct LDUser *const user, const char *const key,
    const struct LDJSON *const fallback, struct LDJSON **const details);

struct LDJSON *LDAllFlags(struct LDClient *const client,
    struct LDUser *const user);
