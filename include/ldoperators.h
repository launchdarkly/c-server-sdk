/*!
 * @file ldoperators.h
 * @brief Internal API Interface for Operators
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"

typedef bool (*OpFn)(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue);

OpFn lookupOperation(const char *const operation);
