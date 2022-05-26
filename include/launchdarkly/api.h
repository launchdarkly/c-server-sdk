/*!
 * @file api.h
 * @brief Public API. Include this for every public operation.
 */

#pragma once

/** @brief The current SDK version string. This value adheres to semantic
 * versioning and is included in the HTTP user agent sent to LaunchDarkly.
 */
#define LD_SDK_VERSION "2.6.2"

#ifdef __cplusplus
extern "C" {
#endif

#include <launchdarkly/boolean.h>
#include <launchdarkly/client.h>
#include <launchdarkly/config.h>
#include <launchdarkly/json.h>
#include <launchdarkly/logging.h>
#include <launchdarkly/memory.h>
#include <launchdarkly/store.h>
#include <launchdarkly/user.h>
#include <launchdarkly/variations.h>

/** @brief Returns the SDK version specified by `LD_SDK_VERSION`.
* @return SDK version string.
*/
LD_EXPORT(const char*) LDVersion(void);

#ifdef __cplusplus
}
#endif

