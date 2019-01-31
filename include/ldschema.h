#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "uthash.h"
#include "cJSON.h"

#include "ldinternal.h"

/* **** Forward Declarations **** */

/* Segment describes a group of users */
struct Segment;
/* FeatureFlag describes an individual feature flag */
struct FeatureFlag;
/* Prerequisite describes a requirement that another feature flag return a specific variation */
struct Prerequisite;
/* Target describes a set of users who will receive a specific variation */
struct Target;
/* WeightedVariation describes a fraction of users who will receive a specific variation */
struct WeightedVariation;
/* Rollout describes how users will be bucketed into variations during a percentage rollout */
struct Rollout;
/* VariationOrRollout contains either the fixed variation or percent rollout to serve */
struct VariationOrRollout;
/* Clause describes an individual cluuse within a targeting rule */
struct Clause;
/* Rule expresses a set of AND-ed matching conditions for a user, along with either a fixed variation or a set of rollout percentages */
struct Rule;
/* SegmentRule describes a set of clauses */
struct SegmentRule;

/* **** Prerequisite **** */

struct Prerequisite {
    unsigned int hhindex;
    char *key;
    int variation;
    UT_hash_handle hh;
};

cJSON *prerequisiteToJSON(const struct FeatureFlag *const featureFlag);
struct Prerequisite *prerequisiteFromJSON(const cJSON *const json);
void prerequisiteFree(struct Prerequisite *const prerequisite);
void prerequisiteFreeCollection(struct Prerequisite *prerequisites);

/* **** FeatureFlag **** */

struct FeatureFlag {
    char *key;
    unsigned int version;
    bool on;
    bool trackEvents;
    bool deleted;
    struct Prerequisite *prerequisites;
    char *salt;
    char *sel;
    struct Target *targets;
    struct Rule *rules;
    struct VariationOrRollout *fallthrough;
    int offVariation; /* optional */
    bool hasOffVariation; /* indicates offVariation */
    struct LDNode *variations; /* LDNodeGetType(values) == LDNodeArray */
    uint64_t debugEventsUntilDate; /* optional */
    bool hasDebugEventsUntilDate; /* indicates debugEventsUntilDate */
    bool clientSide;
    UT_hash_handle hh;
};

cJSON *featureFlagToJSON(const struct FeatureFlag *const featureFlag);
struct FeatureFlag *featureFlagFromJSON(const char *const key, const cJSON *const json);
void featureFlagFree(struct FeatureFlag *const featureFlag);
void featureFlagFreeCollection(struct FeatureFlag *featureFlags);

struct FeatureFlag *featureFlagMakeDeleted(const char *const key, const unsigned int version);

/* **** Target **** */

struct Target {
    unsigned int hhindex;
    struct LDHashSet *values;
    int variation;
    UT_hash_handle hh;
};

cJSON *targetToJSON(const struct Target *const target);
struct Target *targetFromJSON(const cJSON *const json);
void targetFree(struct Target *const target);
void targetFreeCollection(struct Target *targets);

/* **** WeightedVariation **** */

struct WeightedVariation {
    unsigned int hhindex;
    int variation;
    int weight;
    UT_hash_handle hh;
};

cJSON *weightedVariationToJSON(const struct WeightedVariation *const weightedVariation);
struct WeightedVariation *weightedVariationFromJSON(const cJSON *const json);
void weightedVariationFree(struct WeightedVariation *const weightedVariation);
void weightedVariationFreeCollection(struct WeightedVariation *weightedVariations);

/* **** Rollout **** */

struct Rollout {
    char *bucketBy; /* bucketBy may == NULL */
    struct WeightedVariation *variations;
};

cJSON *rolloutToJSON(const struct Rollout *const rollout);
struct Rollout *rolloutFromJSON(const cJSON *const json);
void rolloutFree(struct Rollout *const rollout);

/* **** VariationOrRollout **** */

struct VariationOrRollout {
    bool isVariation; /* true == varation, false == rollout */
    union {
        int variation;
        struct Rollout *rollout;
    } value;
};

cJSON *variationOrRolloutToJSON(const struct VariationOrRollout *const variationOrRollout);
struct VariationOrRollout *variationOrRolloutFromJSON(const cJSON *const json);
void variationOrRolloutFree(struct VariationOrRollout *const variationOrRollout);

/* **** Operator **** */

enum Operator {
    OperatorIn,
    OperatorEndsWith,
    OperatorStartsWith,
    OperatorMatches,
    OperatorContains,
    OperatorLessThan,
    OperatorLessThanOrEqual,
    OperatorGreaterThan,
    OperatorGreaterThanOrEqual,
    OperatorBefore,
    OperatorAfter,
    OperatorSegmentMatch,
    OperatorSemVerEqual,
    OperatorSemVerLessThan,
    OperatorSemVerGreaterThan
};

bool operatorFromString(const char *const text, enum Operator *const operator);

/* **** Clause **** */

struct Clause {
    unsigned int hhindex;
    char *attribute;
    enum Operator op;
    struct LDNode *values; /* LDNodeGetType(values) == LDNodeArray */
    bool negate;
    UT_hash_handle hh;
};

cJSON *clauseToJSON(const struct Clause *const clause);
struct Clause *clauseFromJSON(const cJSON *const json);
void clauseFree(struct Clause *const clause);
void clauseFreeCollection(struct Clause *clauses);

/* **** Rule **** */

struct Rule {
    unsigned int hhindex;
    char *id;
    struct VariationOrRollout *plan; /* supposed to be inline */
    struct Clause *clauses;
    UT_hash_handle hh;
};

cJSON *ruleToJSON(const struct Rule *const rule);
struct Rule *ruleFromJSON(const cJSON *const json);
void ruleFree(struct Rule *const rule);
void ruleFreeCollection(struct Rule *rules);

/* **** SegmentRule **** */

struct SegmentRule {
    unsigned int hhindex;
    char *id;
    struct Clause *clauses;
    int weight; /* optional */
    bool hasWeight; /* indicates weight */
    char *bucketBy; /* optional */
    UT_hash_handle hh;
};

cJSON *segmentRuleToJSON(const struct SegmentRule *const segmentRule);
struct SegmentRule *segmentRuleFromJSON(const cJSON *const json);
void segmentRuleFree(struct SegmentRule *const segmentRule);
void segmentRuleFreeCollection(struct SegmentRule *segmentRules);

/* **** Segment **** */

struct Segment {
    unsigned int hhindex;
    char *key;
    struct LDHashSet *included;
    struct LDHashSet *excluded;
    char *salt;
    struct SegmentRule *rules;
    int version;
    bool deleted;
    UT_hash_handle hh;
};

cJSON *segmentToJSON(const struct Segment *const segment);
struct Segment *segmentFromJSON(const cJSON *const json);
void segmentFree(struct Segment *const segment);
void segmentFreeCollection(struct Segment *segments);
struct Segment *segmentMakeDeleted(const char *const key, const unsigned int version);
