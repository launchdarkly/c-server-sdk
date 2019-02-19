/*!
 * @file ldclient.h
 * @brief Public API Interface for Client operations
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

struct LDClient *LDClientInit(struct LDConfig *const config,
    const unsigned int maxwaitmilli);

void LDClientClose(struct LDClient *const client);
