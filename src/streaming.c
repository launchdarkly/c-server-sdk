#include <math.h>
#include <stdio.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "logging.h"
#include "network.h"
#include "streaming.h"
#include "user.h"
#include "utility.h"

bool
LDi_parsePath(
    const char *path, enum FeatureKind *const kind, const char **const key)
{
    size_t      segmentlen, flagslen;
    const char *segments, *flags;

    LD_ASSERT(path);
    LD_ASSERT(kind);
    LD_ASSERT(key);

    *key       = NULL;
    segments   = "/segments/";
    flags      = "/flags/";
    segmentlen = strlen(segments);
    flagslen   = strlen(flags);

    if (strncmp(path, segments, segmentlen) == 0) {
        *key  = path + segmentlen;
        *kind = LD_SEGMENT;
    } else if (strncmp(path, flags, flagslen) == 0) {
        *key  = path + flagslen;
        *kind = LD_FLAG;
    } else {
        return false;
    }

    return true;
}

static bool
getEventPath(
    const struct LDJSON *const event,
    enum FeatureKind *const    kind,
    const char **const         key)
{
    struct LDJSON *tmp;

    LD_ASSERT(event);
    LD_ASSERT(LDJSONGetType(event) == LDObject);
    LD_ASSERT(kind);
    LD_ASSERT(key);

    if (!(tmp = LDObjectLookup(event, "path"))) {
        LD_LOG(LD_LOG_ERROR, "event does not have a path");

        return false;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "event path is not a string");

        return false;
    }

    if (!LDi_parsePath(LDGetText(tmp), kind, key)) {
        LD_LOG(LD_LOG_ERROR, "event path is not recognized");

        return false;
    }

    return true;
}

bool
validatePutBody(const struct LDJSON *const put)
{
    struct LDJSON *tmp;

    LD_ASSERT(put);

    if (LDJSONGetType(put) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "put is not an object");

        return false;
    }

    if (!(tmp = LDObjectLookup(put, "flags"))) {
        LD_LOG(LD_LOG_ERROR, "put.flags does not exist");

        return false;
    }

    if (LDJSONGetType(tmp) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "put.flags is not an object");

        return false;
    }

    if (!(tmp = LDObjectLookup(put, "segments"))) {
        LD_LOG(LD_LOG_ERROR, "put.segments does not exist");

        return false;
    }

    if (LDJSONGetType(tmp) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "put.segments is not an object");

        return false;
    }

    return true;
}

/* consumes input even on failure */
static LDBoolean
onPut(struct LDClient *const client, const char *const eventBuffer)
{
    struct LDJSON *data, *features, *put;
    bool           success;

    LD_ASSERT(client);
    LD_ASSERT(eventBuffer);

    data     = NULL;
    features = NULL;
    success  = LDBooleanFalse;
    put      = NULL;

    if (!(put = LDJSONDeserialize(eventBuffer))) {
        LD_LOG(LD_LOG_ERROR, "sse delete failed to decode event body");

        goto cleanup;
    }

    if (LDJSONGetType(put) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "sse delete body should be object, discarding");

        goto cleanup;
    }

    if (!(data = LDObjectDetachKey(put, "data"))) {
        LD_LOG(LD_LOG_ERROR, "put.data does not exist");

        goto cleanup;
    }

    if (!validatePutBody(data)) {
        LD_LOG(LD_LOG_ERROR, "put.data failed validation");

        goto cleanup;
    }

    features = LDObjectDetachKey(data, "flags");
    LD_ASSERT(features);

    if (!LDObjectSetKey(data, "features", features)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto cleanup;
    }
    features = NULL;

    if (!(success = LDStoreInit(client->store, data))) {
        LD_LOG(LD_LOG_ERROR, "LDStoreInit error");

        data = NULL;

        goto cleanup;
    }
    data = NULL;

    success = LDBooleanTrue;

cleanup:
    LDJSONFree(put);
    LDJSONFree(data);
    LDJSONFree(features);

    return success;
}

/* consumes input even on failure */
static LDBoolean
onPatch(struct LDClient *const client, const char *const eventBuffer)
{
    struct LDJSON *  tmp, *data;
    const char *     key;
    bool             success;
    enum FeatureKind kind;

    LD_ASSERT(client);
    LD_ASSERT(eventBuffer);

    tmp     = NULL;
    key     = NULL;
    success = LDBooleanFalse;
    data    = NULL;

    if (!(data = LDJSONDeserialize(eventBuffer))) {
        LD_LOG(LD_LOG_ERROR, "sse patch failed to decode event body");

        goto cleanup;
    }

    if (LDJSONGetType(data) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "sse patch body should be object, discarding");

        goto cleanup;
    }

    if (!getEventPath(data, &kind, &key)) {
        LD_LOG(LD_LOG_ERROR, "patch failed to get path");

        goto cleanup;
    }

    if (!(tmp = LDObjectDetachKey(data, "data"))) {
        LD_LOG(LD_LOG_ERROR, "patch.data does not exist");

        goto cleanup;
    }

    if (!LDStoreUpsert(client->store, kind, tmp)) {
        LD_LOG(LD_LOG_ERROR, "store error");

        /* tmp is consumed even on failure */

        goto cleanup;
    }

    success = LDBooleanTrue;

cleanup:
    LDJSONFree(data);

    return success;
}

/* consumes input even on failure */
static LDBoolean
onDelete(struct LDClient *const client, const char *const eventBuffer)
{
    struct LDJSON *  tmp, *data;
    const char *     key;
    LDBoolean        success;
    enum FeatureKind kind;

    LD_ASSERT(client);
    LD_ASSERT(eventBuffer);

    tmp     = NULL;
    key     = NULL;
    success = LDBooleanFalse;
    data    = NULL;

    if (!(data = LDJSONDeserialize(eventBuffer))) {
        LD_LOG(LD_LOG_ERROR, "sse delete failed to decode event body");

        goto cleanup;
    }

    if (LDJSONGetType(data) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "sse delete body should be object, discarding");

        goto cleanup;
    }

    if (!getEventPath(data, &kind, &key)) {
        LD_LOG(LD_LOG_ERROR, "patch failed to get path");

        goto cleanup;
    }

    if (!(tmp = LDObjectLookup(data, "version"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDNumber) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (!LDStoreRemove(client->store, kind, key, LDGetNumber(tmp))) {
        LD_LOG(LD_LOG_ERROR, "store error");

        goto cleanup;
    }

    success = LDBooleanTrue;

cleanup:
    LDJSONFree(data);

    return success;
}

static LDBoolean
LDi_onEvent(
    const char *const eventName,
    const char *const eventBuffer,
    void *const       rawContext)
{
    LDBoolean             status;
    struct StreamContext *context;

    LD_ASSERT(eventName);
    LD_ASSERT(eventBuffer);
    LD_ASSERT(rawContext);

    status  = LDBooleanTrue;
    context = (struct StreamContext *)rawContext;

    if (strcmp(eventName, "put") == 0) {
        status = onPut(context->client, eventBuffer);
    } else if (strcmp(eventName, "patch") == 0) {
        status = onPatch(context->client, eventBuffer);
    } else if (strcmp(eventName, "delete") == 0) {
        status = onDelete(context->client, eventBuffer);
    } else {
        LD_LOG_1(LD_LOG_ERROR, "sse unknown event name: %s", eventName);
    }

    return status;
}

size_t
LDi_streamWriteCallback(
    const void *const contents,
    size_t            size,
    size_t            nmemb,
    void *const       rawContext)
{
    size_t                realSize;
    struct StreamContext *context;

    LD_ASSERT(rawContext);

    realSize = size * nmemb;
    context  = (struct StreamContext *)rawContext;

    LDi_getMonotonicMilliseconds(&context->lastReadTimeMilliseconds);

    if (LDSSEParserProcess(&context->parser, contents, realSize)) {
        return realSize;
    }

    return 0;
}

void
resetMemory(struct StreamContext *const context)
{
    LD_ASSERT(context);

    LDSSEParserDestroy(&context->parser);

    curl_slist_free_all(context->headers);
    context->headers = NULL;
}

static void
done(
    struct LDClient *const client,
    void *const            rawcontext,
    const int              responseCode)
{
    struct StreamContext *context;
    bool                  success;

    LD_ASSERT(client);
    LD_ASSERT(rawcontext);

    context = (struct StreamContext *)rawcontext;

    context->active = false;

    if (responseCode == 200) {
        success = true;
    } else {
        success = false;
        /* Most 4xx unrecoverable */
        if (responseCode >= 400 && responseCode < 500) {
            if (responseCode != 400 && responseCode != 408 &&
                responseCode != 429) {
                context->permanentFailure = true;
            }
        }
    }

    if (success) {
        double now;

        LDi_getMonotonicMilliseconds(&now);

        /* if closed within 60 seconds of start count as a failure */
        if (now >= context->startedOn + (60 * 1000)) {
            context->attempts = 0;
        } else {
            context->attempts++;
        }
    } else {
        context->attempts++;
    }

    resetMemory(context);
}

static void
destroy(void *const rawcontext)
{
    struct StreamContext *context;

    LD_ASSERT(rawcontext);

    context = (struct StreamContext *)rawcontext;

    LD_LOG(LD_LOG_INFO, "streaming destroyed");

    resetMemory(context);

    LDFree(context);
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *                curl;
    char                  url[4096];
    struct StreamContext *context;
    struct curl_slist *   headersTmp;

    LD_ASSERT(client);
    LD_ASSERT(rawcontext);

    context = (struct StreamContext *)rawcontext;
    curl    = NULL;

    if (!client->config->stream || context->permanentFailure) {
        return NULL;
    }

    if (context->active) {
        double now;

        LDi_getMonotonicMilliseconds(&now);

        if ((context->lastReadTimeMilliseconds + (300 * 1000)) <= now) {
            LD_LOG(LD_LOG_WARNING, "stream read timout killing stream");

            if (!LDi_removeAndFreeHandle(
                    context->multi, context->networkInterface->current))
            {
                return NULL;
            }

            context->networkInterface->current = NULL;

            done(client, rawcontext, 0);

            return NULL;
        } else {
            return NULL;
        }
    }

    /* logic for checking backoff wait */
    if (context->attempts) {
        double now;

        LDi_getMonotonicMilliseconds(&now);

        if (context->waitUntil) {
            if (now >= context->waitUntil) {
                /* done waiting */
                context->waitUntil = 0;

                context->startedOn = now;
            } else {
                /* continue waiting */
                return NULL;
            }
        } else {
            double       backoff;
            unsigned int rng;

            /* explicit one second wait for first try */
            if (context->attempts == 1) {
                context->waitUntil = now + 1000;
                /* skip because we are waiting */
                return NULL;
            }

            /* random value for jitter */
            if (!LDi_random(&rng)) {
                LD_LOG(
                    LD_LOG_ERROR, "failed to get rng for jitter calculation");

                return NULL;
            }

            /* calculate time to wait */
            backoff = 1000 * pow(2, context->attempts) / 2;

            /* cap (min not built in) */
            if (backoff > 30 * 1000) {
                backoff = 30 * 1000;
            }

            /* jitter */
            backoff /= 2;

            backoff = backoff + LDi_normalize(rng, 0, LD_RAND_MAX, 0, backoff);

            context->waitUntil = now + backoff;
            /* skip because we are waiting */
            return NULL;
        }
    }

    if (snprintf(url, sizeof(url), "%s/all", client->config->streamURI) < 0) {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return NULL;
    }

    {
        char msg[256];

        LD_ASSERT(
            snprintf(
                msg, sizeof(msg), "connection to streaming url: %s", url) >= 0);

        LD_LOG(LD_LOG_INFO, msg);
    }

    if (!LDi_prepareShared(client->config, url, &curl, &context->headers)) {
        goto error;
    }

    if (!(headersTmp =
              curl_slist_append(context->headers, "Accept: text/event-stream")))
    {
        goto error;
    }
    context->headers = headersTmp;

    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, context) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEDATA failed");

        goto error;
    }

    if (curl_easy_setopt(
            curl, CURLOPT_WRITEFUNCTION, LDi_streamWriteCallback) != CURLE_OK)
    {
        LD_LOG(
            LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEFUNCTION failed");

        goto error;
    }

    context->active = true;
    LDi_getMonotonicMilliseconds(&context->lastReadTimeMilliseconds);

    return curl;

error:
    curl_slist_free_all(context->headers);
    curl_easy_cleanup(curl);
    return NULL;
}

struct StreamContext *
LDi_constructStreamContext(
    struct LDClient *const         client,
    CURLM *const                   multi,
    struct NetworkInterface *const networkInterface)
{
    struct StreamContext *context;

    LD_ASSERT(client);

    if (!(context =
              (struct StreamContext *)LDAlloc(sizeof(struct StreamContext)))) {
        return NULL;
    }

    LDSSEParserInitialize(&context->parser, LDi_onEvent, context);

    context->active                   = false;
    context->headers                  = NULL;
    context->client                   = client;
    context->attempts                 = 0;
    context->waitUntil                = 0;
    context->startedOn                = 0;
    context->permanentFailure         = false;
    context->multi                    = multi;
    context->networkInterface         = networkInterface;
    context->lastReadTimeMilliseconds = 0;

    return context;
}

struct NetworkInterface *
LDi_constructStreaming(struct LDClient *const client, CURLM *const multi)
{
    struct NetworkInterface *netInterface;
    struct StreamContext *   context;

    LD_ASSERT(client);

    netInterface = NULL;
    context      = NULL;

    if (!(netInterface = (struct NetworkInterface *)LDAlloc(
              sizeof(struct NetworkInterface))))
    {
        goto error;
    }

    if (!(context = LDi_constructStreamContext(client, multi, netInterface))) {
        goto error;
    }

    netInterface->done    = done;
    netInterface->poll    = poll;
    netInterface->context = context;
    netInterface->destroy = destroy;
    netInterface->current = NULL;

    return netInterface;

error:
    resetMemory(context);
    LDFree(netInterface);
    return NULL;
}
