/*!
 * @file ldoperators.h
 * @brief Internal API Interface for Operators
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "timestamp.h"
#include "ldjson.h"

typedef bool (*OpFn)(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue);

OpFn lookupOperation(const char *const operation);

bool parseTime(const struct LDJSON *const json, timestamp_t *result);
