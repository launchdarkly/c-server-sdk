#pragma once

#include <curl/curl.h>

#include "ldinternal.h"

struct NetworkInterface {
    /* get next handle */
    CURL *(*poll)(const struct LDClient *const client, void *context);
    /* called when handle is ready */
    void (*done)(const struct LDClient *const client, void *context);
    /* final action destroy */
    void (*destroy)(void *context);
    /* stores any private implementation data */
    void *context;
};

bool prepareShared(const struct LDConfig *const config, const char *const url,
    CURL **const o_curl, struct curl_slist **const o_headers);

struct NetworkInterface *constructPolling(const struct LDClient *const client);
