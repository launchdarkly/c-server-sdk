/*!
 * @file ldclient.h
 * @brief Public API Interface for Client operations
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"
#include "lduser.h"

struct LDClient *LDClientInit(struct LDConfig *const config,
    const unsigned int maxwaitmilli);

void LDClientClose(struct LDClient *const client);

bool LDClientIsInitialized(struct LDClient *const client);

bool LDClientTrack(struct LDClient *const client,
    const struct LDUser *const user, struct LDJSON *const data);

bool LDClientIdentify(struct LDClient *const client,
    const struct LDUser *const user);

bool LDClientIsOffline(struct LDClient *const client);

void LDClientFlush(struct LDClient *const client);
