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

struct FeatureFlag; struct Prerequisite; struct Target; struct WeightedVariation;

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
    /* struct Rule rules [] */
    /* struct VariationOrRollout fallthrough */
    int offVariation; /* optional */
    bool hasOffVariation; /* indicates offVariation */
    /* variations */
    uint64_t debugEventsUntilDate; /* optional */
    bool hasDebugEventsUntilDate; /* indicates debugEventsUntilDate */
    bool clientSide;
    UT_hash_handle hh;
};

cJSON *featureFlagToJSON(const struct FeatureFlag *const featureFlag);
struct FeatureFlag *featureFlagFromJSON(const char *const key, const cJSON *const json);
void featureFlagFree(struct FeatureFlag *const featureFlag);
void featureFlagFreeCollection(struct FeatureFlag *featureFlags);

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
