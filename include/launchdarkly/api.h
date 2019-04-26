/*!
 * @file api.h
 * @brief Public API. Include this for every public operation.
 */

#pragma once

#define LD_SDK_VERSION "1.0.0-beta.2"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

#include <launchdarkly/json.h>
#include <launchdarkly/config.h>
#include <launchdarkly/user.h>
#include <launchdarkly/client.h>
#include <launchdarkly/memory.h>
#include <launchdarkly/logging.h>
#include <launchdarkly/variations.h>

#ifdef __cplusplus
}
#endif
