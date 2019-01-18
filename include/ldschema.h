#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <curl/curl.h>

#include "uthash.h"
#include "cJSON.h"

/* **** FeatureFlag **** */

struct FeatureFlag {
    char *key;
    unsigned int version;
    bool on;
    bool trackEvents;
    bool deleted;
    /* struct Prerequisite Prerequisites [] */
    char *salt;
    char *sel;
    /* struct Target targets [] */
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
struct FeatureFlag* featureFlagFromJSON(const char *const key, const cJSON *const json);
void featureFlagFree(struct FeatureFlag *const featureFlag);
