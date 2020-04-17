
/*!
 * @file ldstreaming.h
 * @brief Internal API Interface for Networking. Header primarily for testing.
 */


#pragma once

#include <curl/curl.h>

#include "network.h"
#include "store.h"
#include "sse.h"

struct StreamContext {
    struct LDSSEParser parser;
    bool active;
    struct curl_slist *headers;
    struct LDClient *client;
    /* used for tracking retry */
    unsigned int attempts;
    /* point in future to backoff until */
    unsigned long waitUntil;
    /* record when stream started for backoff purposes */
    unsigned long startedOn;
    /* if stream should never retry */
    bool permanentFailure;
};

bool LDi_parsePath(const char *path, enum FeatureKind *const kind,
    const char **const key);

size_t LDi_streamWriteCallback(const void *const contents, size_t size,
    size_t nmemb, void *rawcontext);

struct StreamContext *LDi_constructStreamContext(struct LDClient *const client);

void resetMemory(struct StreamContext *const context);
