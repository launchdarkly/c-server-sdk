/*!
 * @file client.h
 * @brief Public API Interface for Client operations
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <launchdarkly/config.h>
#include <launchdarkly/user.h>
#include <launchdarkly/json.h>
#include <launchdarkly/export.h>

LD_EXPORT(struct LDClient *) LDClientInit(struct LDConfig *const config,
    const unsigned int maxwaitmilli);

LD_EXPORT(void) LDClientClose(struct LDClient *const client);

LD_EXPORT(bool) LDClientIsInitialized(struct LDClient *const client);

LD_EXPORT(bool) LDClientTrack(struct LDClient *const client,
    const char *const key, const struct LDUser *const user,
    struct LDJSON *const data);

LD_EXPORT(bool) LDClientIdentify(struct LDClient *const client,
    const struct LDUser *const user);

LD_EXPORT(bool) LDClientIsOffline(struct LDClient *const client);

LD_EXPORT(void) LDClientFlush(struct LDClient *const client);
