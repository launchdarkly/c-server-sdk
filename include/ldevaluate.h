/*!
 * @file ldevaluate.h
 * @brief Internal API Interface for Evaluation
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"

bool evaluate(const struct LDJSON *const flag, const struct LDUser *const user,
    struct LDJSON **const result);

bool checkPrerequisites(const struct LDJSON *const flag,
    const struct LDUser *const user, bool *const matches);

bool ruleMatchesUser(const struct LDJSON *const rule,
    const struct LDUser *const user, bool *const matches);

bool clauseMatchesUser(const struct LDJSON *const clause,
    const struct LDUser *const user, bool *const matches);

bool segmentMatchesUser(const struct LDJSON *const segment,
    const struct LDUser *const user, bool *const matches);

bool segmentRuleMatchUser(const struct LDJSON *const segmentRule,
    const char *const segmentKey, const struct LDUser *const user,
    const char *const salt, bool *const matches);

bool clauseMatchesUserNoSegments(const struct LDJSON *const clause,
    const struct LDUser *const user, bool *const matches);

bool bucketUser(const struct LDUser *const user,
    const char *const segmentKey, const char *const attribute,
    const char *const salt, float *const bucket);

char *bucketableStringValue(const struct LDJSON *const node);
