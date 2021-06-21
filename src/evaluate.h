/*!
 * @file ldevaluate.h
 * @brief Internal API Interface for Evaluation
 */

#pragma once

#include <stddef.h>

#include <launchdarkly/json.h>
#include <launchdarkly/variations.h>

#include "store.h"

typedef enum
{
    EVAL_MEM,
    EVAL_SCHEMA,
    EVAL_STORE,
    EVAL_MATCH,
    EVAL_MISS
} EvalStatus;

LDBoolean
LDi_isEvalError(const EvalStatus status);

EvalStatus
LDi_evaluate(
    struct LDClient *const     client,
    const struct LDJSON *const flag,
    const struct LDUser *const user,
    struct LDStore *const      store,
    struct LDDetails *const    details,
    struct LDJSON **const      o_events,
    struct LDJSON **const      o_value,
    const LDBoolean            recordReason);

EvalStatus
LDi_checkPrerequisites(
    struct LDClient *const     client,
    const struct LDJSON *const flag,
    const struct LDUser *const user,
    struct LDStore *const      store,
    const char **const         failedKey,
    struct LDJSON **const      events,
    const LDBoolean            recordReason);

EvalStatus
LDi_ruleMatchesUser(
    const struct LDJSON *const rule,
    const struct LDUser *const user,
    struct LDStore *const      store);

EvalStatus
LDi_clauseMatchesUser(
    const struct LDJSON *const clause,
    const struct LDUser *const user,
    struct LDStore *const      store);

EvalStatus
LDi_segmentMatchesUser(
    const struct LDJSON *const segment, const struct LDUser *const user);

EvalStatus
LDi_segmentRuleMatchUser(
    const struct LDJSON *const segmentRule,
    const char *const          segmentKey,
    const struct LDUser *const user,
    const char *const          salt);

EvalStatus
LDi_clauseMatchesUserNoSegments(
    const struct LDJSON *const clause, const struct LDUser *const user);

LDBoolean
LDi_bucketUser(
    const struct LDUser *const user,
    const char *const          segmentKey,
    const char *const          attribute,
    const char *const          salt,
    const int *const           seed,
    float *const               bucket);

LDBoolean
LDi_variationIndexForUser(
    const struct LDJSON *const  varOrRoll,
    const struct LDUser *const  user,
    const char *const           key,
    const char *const           salt,
    LDBoolean *const            inExperiment,
    const struct LDJSON **const index);

LDBoolean
LDi_getIndexForVariationOrRollout(
    const struct LDJSON *const  flag,
    const struct LDJSON *const  varOrRoll,
    const struct LDUser *const  user,
    LDBoolean *const            inExperiment,
    const struct LDJSON **const result);
