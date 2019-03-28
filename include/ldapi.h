/*!
 * @file ldapi.h
 * @brief Public API. Include this for every public operation.
 */

#pragma once

#ifdef __cplusplus

extern "C" {
#endif

#define LD_SDK_VERSION "1.0.0-beta.1"

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"
#include "ldconfig.h"
#include "lduser.h"
#include "ldlogging.h"
#include "ldclient.h"
#include "ldvariations.h"
#include "ldmemory.h"

#ifdef __cplusplus
}
#endif
