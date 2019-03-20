/*!
 * @file ldvariations.h
 * @brief Public API Interface for evaluation variations
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lduser.h"
#include "ldjson.h"

enum LDEvalReason {
    LD_UNKNOWN = 0,
    LD_ERROR,
    LD_OFF,
    LD_PREREQUISITE_FAILED,
    LD_TARGET_MATCH,
    LD_RULE_MATCH,
    LD_FALLTHROUGH

};

enum LDEvalErrorKind {
    LD_NULL_CLIENT,
    LD_CLIENT_NOT_READY,
    LD_NULL_KEY,
    LD_STORE_ERROR,
    LD_FLAG_NOT_FOUND,
    LD_USER_NOT_SPECIFIED,
    LD_MALFORMED_FLAG,
    LD_WRONG_TYPE
};

struct LDDetailsRule {
    unsigned int ruleIndex;
    char *id;
    bool hasId;
};

struct LDDetails {
    unsigned int variationIndex;
    bool hasVariation;
    enum LDEvalReason kind;
    union {
        enum LDEvalErrorKind errorKind;
        char *prequisiteKey;
        struct LDDetailsRule rule;
    } extra;
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
