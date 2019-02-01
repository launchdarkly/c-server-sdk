#pragma once

#include <curl/curl.h>

#include "ldinternal.h"

struct NetworkInterface {
    /* expected to free context */
    void (*done)(void *context);
    /* stores any private implementation data */
    void *context;
};

bool prepareShared2(const struct LDConfig *const config, const char *const url, const struct NetworkInterface *const interface,
    CURL **const o_curl, struct curl_slist **const o_headers);

CURL *constructPolling(const struct LDClient *const client);
