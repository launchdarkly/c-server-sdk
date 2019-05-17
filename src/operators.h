/*!
 * @file ldoperators.h
 * @brief Internal API Interface for Operators
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <launchdarkly/json.h>

#include "timestamp.h"

typedef bool (*OpFn)(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue);

OpFn LDi_lookupOperation(const char *const operation);

bool LDi_parseTime(const struct LDJSON *const json, timestamp_t *result);
