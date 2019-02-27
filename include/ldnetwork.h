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
    void (*done)(struct LDClient *const client, void *context);
    /* final action destroy */
    void (*destroy)(void *context);
    /* stores any private implementation data */
    void *context;
    /* active handle */
    CURL *current;
};

bool prepareShared(const struct LDConfig *const config, const char *const url,
    CURL **const o_curl, struct curl_slist **const o_headers);

struct NetworkInterface *constructPolling(struct LDClient *const client);
struct NetworkInterface *constructStreaming(struct LDClient *const client);
struct NetworkInterface *constructAnalytics(struct LDClient *const client);
