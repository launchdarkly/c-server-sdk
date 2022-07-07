
/*!
 * @file ldstreaming.h
 * @brief Internal API Interface for Networking. Header primarily for testing.
 */

#pragma once

#include <curl/curl.h>

#include "network.h"
#include "sse.h"
#include "store.h"

struct StreamContext
{
    struct LDSSEParser       parser;
    LDBoolean                active;
    struct curl_slist *      headers;
    struct LDClient *        client;
    struct NetworkInterface *networkInterface;
    CURLM *                  multi;
    /* used for tracking retry */
    unsigned int attempts;
    /* point in future to backoff until */
    unsigned long waitUntil;
    /* record when stream started for backoff purposes */
    unsigned long startedOn;
    /* if stream should never retry */
    LDBoolean permanentFailure;
    /* used to track stream read timeouts */
    double lastReadTimeMilliseconds;
};

/* Used as a return value for LDi_parsePath.  */
enum ParsePathStatus {
    /* The path was parsed successfully for a key and resource type. */
    PARSE_PATH_SUCCESS,
    /* The path was malformed. */
    PARSE_PATH_FAILURE,
    /* The path was parsed successfully, but should be ignored.  */
    PARSE_PATH_IGNORE
};

/**
 * Parses a patch path, extracting out the type and key of the resource being patched.
 * @param path Input path
 * @param kind Out parameter containing the kind, if recognized.
 * @param key Out parameter containing the key, if recognized.
 * @return Parse status.
 */
enum ParsePathStatus
LDi_parsePath(
    const char *path, enum FeatureKind *o_kind, const char **o_key);

size_t
LDi_streamWriteCallback(
    const void *const contents, size_t size, size_t nmemb, void *rawcontext);

struct StreamContext *
LDi_constructStreamContext(
    struct LDClient *const         client,
    CURL *const                    multi,
    struct NetworkInterface *const networkInterface);

void
resetMemory(struct StreamContext *const context);
