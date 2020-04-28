#pragma once

#include <stdbool.h>

#include <launchdarkly/api.h>

struct LDJSON *makeMinimalFlag(const char *const key,
    const unsigned int version, const bool on, const bool trackEvents);

void setFallthrough(struct LDJSON *const flag, const unsigned int variation);

void addVariation(struct LDJSON *const flag, struct LDJSON *const variation);
