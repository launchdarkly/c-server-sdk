/*!
 * @file ldevaluate.h
 * @brief Internal API Interface for Evaluation
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"
#include "ldstore.h"

typedef enum {
    EVAL_MEM,
    EVAL_SCHEMA,
    EVAL_STORE,
    EVAL_MATCH,
    EVAL_MISS
} EvalStatus;

bool isEvalError(const EvalStatus status);

struct LDJSON *addReason(struct LDJSON **const result,
    const char *const reason, struct LDJSON *const events);

struct LDJSON *addErrorReason(struct LDJSON **const result,
    const char *const kind);

EvalStatus evaluate(const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    struct LDJSON **const result);

EvalStatus checkPrerequisites(const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    const char **const failedKey, struct LDJSON **const events);

EvalStatus ruleMatchesUser(const struct LDJSON *const rule,
    const struct LDUser *const user, struct LDStore *const store);

EvalStatus clauseMatchesUser(const struct LDJSON *const clause,
    const struct LDUser *const user, struct LDStore *const store);

EvalStatus segmentMatchesUser(const struct LDJSON *const segment,
    const struct LDUser *const user);

EvalStatus segmentRuleMatchUser(const struct LDJSON *const segmentRule,
    const char *const segmentKey, const struct LDUser *const user,
    const char *const salt);

EvalStatus clauseMatchesUserNoSegments(const struct LDJSON *const clause,
    const struct LDUser *const user);

bool bucketUser(const struct LDUser *const user,
    const char *const segmentKey, const char *const attribute,
    const char *const salt, float *const bucket);

char *bucketableStringValue(const struct LDJSON *const node);
