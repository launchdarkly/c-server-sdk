/*!
 * @file ldoperators.h
 * @brief Internal API Interface for Operators
 */

#pragma once

#include <stddef.h>

#include <launchdarkly/json.h>

#include "timestamp.h"

typedef LDBoolean (*OpFn)(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue);

OpFn
LDi_lookupOperation(const char *const operation);

LDBoolean
LDi_parseTime(const struct LDJSON *const json, timestamp_t *result);
