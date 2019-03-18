/*!
 * @file ldnetwork.h
 * @brief Internal API Interface for Networking
 */

#pragma once

#include <curl/curl.h>

#include "ldinternal.h"

struct NetworkInterface {
    /* get next handle */
    CURL *(*poll)(struct LDClient *const client, void *context);
    /* called when handle is ready */
    void (*done)(struct LDClient *const client, void *context, bool success);
    /* final action destroy */
    void (*destroy)(void *context);
    /* stores any private implementation data */
    void *context;
    /* active handle */
    CURL *current;
    /* used for tracking retry */
    unsigned int attempts;
};

bool LDi_prepareShared(const struct LDConfig *const config,
    const char *const url, CURL **const o_curl,
    struct curl_slist **const o_headers);

struct NetworkInterface *LDi_constructPolling(struct LDClient *const client);
struct NetworkInterface *LDi_constructStreaming(struct LDClient *const client);
struct NetworkInterface *LDi_constructAnalytics(struct LDClient *const client);

THREAD_RETURN LDi_networkthread(void *const clientref);
