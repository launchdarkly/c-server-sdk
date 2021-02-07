/*!
 * @file ldnetwork.h
 * @brief Internal API Interface for Networking
 */

#pragma once

#include <curl/curl.h>

#include <launchdarkly/json.h>

#include "client.h"
#include "concurrency.h"

struct NetworkInterface
{
    /* get next handle */
    CURL *(*poll)(struct LDClient *const client, void *context);
    /* called when handle is ready */
    void (*done)(
        struct LDClient *const client, void *context, int responseCode);
    /* final action destroy */
    void (*destroy)(void *context);
    /* stores any private implementation data */
    void *context;
    /* active handle */
    CURL *current;
};

bool
LDi_prepareShared(
    const struct LDConfig *const config,
    const char *const            url,
    CURL **const                 o_curl,
    struct curl_slist **const    o_headers);

struct NetworkInterface *
LDi_constructPolling(struct LDClient *const client);
struct NetworkInterface *
LDi_constructStreaming(struct LDClient *const client, CURLM *const multi);
struct NetworkInterface *
LDi_constructAnalytics(struct LDClient *const client);

THREAD_RETURN
LDi_networkthread(void *const clientref);

bool
validatePutBody(const struct LDJSON *const put);

bool
LDi_addHandle(
    CURLM *const                   multi,
    struct NetworkInterface *const networkInterface,
    CURL *const                    handle);

bool
LDi_removeAndFreeHandle(CURLM *const multi, CURL *const handle);
