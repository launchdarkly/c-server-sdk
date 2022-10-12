#pragma once

#include <launchdarkly/api.h>

struct LDJSON *
makeMinimalFlag(
    const char *const  key,
    const unsigned int version,
    const LDBoolean    on,
    const LDBoolean    trackEvents);

void
setFallthrough(struct LDJSON *const flag, const unsigned int variation);

void
addVariation(struct LDJSON *const flag, struct LDJSON *const variation);

void
addPrerequisite(struct LDJSON *const flag, struct LDJSON *const prereq, unsigned int expectedPrereqVariation);

void
addVariations1(struct LDJSON *const flag);

void
addVariations2(struct LDJSON *const flag);

struct LDJSON *
makeFlagToMatchUser(
    const char *const key, struct LDJSON *const variationOrRollout);
