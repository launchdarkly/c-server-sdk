#include <string.h>
#include <stdio.h>

#include <launchdarkly/api.h>

#include "network.h"
#include "streaming.h"
#include "config.h"
#include "client.h"
#include "user.h"
#include "misc.h"

bool
LDi_parsePath(const char *path, enum FeatureKind *const kind,
    const char **const key)
{
    size_t segmentlen, flagslen;
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
        *key = path + segmentlen;
        *kind = LD_SEGMENT;
    } else if(strncmp(path, flags, flagslen) == 0) {
        *key = path + flagslen;
        *kind = LD_FLAG;
    } else {
        return false;
    }

    return true;
}

static bool
getEventPath(const struct LDJSON *const event, enum FeatureKind *const kind,
    const char **const key)
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
static bool
onPut(struct LDClient *const client, struct LDJSON *const put)
{
    struct LDJSON *data, *features;
    bool success;

    LD_ASSERT(client);
    LD_ASSERT(put);
    LD_ASSERT(LDJSONGetType(put) == LDObject);

    data     = NULL;
    features = NULL;
    success  = false;

    if (!(data = LDObjectDetachKey(put, "data"))) {
        LD_LOG(LD_LOG_ERROR, "put.data does not exist");

        goto cleanup;
    }

    if (!validatePutBody(data)) {
        LD_LOG(LD_LOG_ERROR, "put.data failed validation");

        goto cleanup;
    }

    LD_ASSERT(features = LDObjectDetachKey(data, "flags"));

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

     success = true;

  cleanup:
    LDJSONFree(put);
    LDJSONFree(data);
    LDJSONFree(features);

    return success;
}

/* consumes input even on failure */
static bool
onPatch(struct LDClient *const client, struct LDJSON *const data)
{
    struct LDJSON *tmp;
    const char *key;
    bool success;
    enum FeatureKind kind;

    LD_ASSERT(client);
    LD_ASSERT(data);
    LD_ASSERT(LDJSONGetType(data) == LDObject);

    tmp     = NULL;
    key     = NULL;
    success = false;

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

    success = true;

  cleanup:
    LDJSONFree(data);

    return success;
}

/* consumes input even on failure */
static bool
onDelete(struct LDClient *const client, struct LDJSON *const data)
{
    struct LDJSON *tmp;
    const char *key;
    bool success;
    enum FeatureKind kind;

    LD_ASSERT(client);
    LD_ASSERT(data);
    LD_ASSERT(LDJSONGetType(data) == LDObject);

    tmp     = NULL;
    key     = NULL;
    success = false;

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

    success = true;

  cleanup:
    LDJSONFree(data);

    return success;
}

bool
LDi_onSSE(struct StreamContext *const context, const char *line)
{
    LD_ASSERT(context);

    if (!line) {
        /* should not happen from the networking side but is not fatal */
        LD_LOG(LD_LOG_ERROR, "streamcallback got NULL line");
    } else if (line[0] == ':') {
        /* skip comment */
    } else if (line[0] == 0) {
        bool status = true;
        struct LDJSON *json;

        if (context->eventName[0] == 0) {
            LD_LOG(LD_LOG_WARNING,
                "streamcallback got dispatch but type was never set");
        } else if (context->dataBuffer == NULL) {
            LD_LOG(LD_LOG_WARNING,
                "streamcallback got dispatch but data was never set");
        } else if ((json = LDJSONDeserialize(context->dataBuffer))) {
            if (LDJSONGetType(json) != LDObject) {
                LDJSONFree(json);

                LD_LOG(LD_LOG_ERROR, "event should be object, discarding");
            } else if (strcmp(context->eventName, "put") == 0) {
                status = onPut(context->client, json);
            } else if (strcmp(context->eventName, "patch") == 0) {
                status = onPatch(context->client, json);
            } else if (strcmp(context->eventName, "delete") == 0) {
                status = onDelete(context->client, json);
            } else {
                char msg[256];

                LD_ASSERT(snprintf(msg, sizeof(msg),
                    "streamcallback unknown event name: %s",
                    context->eventName) >= 0);

                LD_LOG(LD_LOG_ERROR, msg);

                LDJSONFree(json);
            }
        }

        LDFree(context->dataBuffer);
        context->dataBuffer = NULL;
        context->eventName[0] = 0;

        if (!status) {
            return false;
        }
    } else if (strncmp(line, "data:", 5) == 0) {
        bool nempty;
        size_t linesize, currentsize;

        currentsize = 0;

        line += 5;
        line += line[0] == ' ';

        nempty = context->dataBuffer != NULL;
        linesize = strlen(line);

        if (nempty) {
            currentsize = strlen(context->dataBuffer);
        }

        context->dataBuffer = (char *)LDRealloc(
            context->dataBuffer, linesize + currentsize + nempty + 1);

        if (nempty) {
            context->dataBuffer[currentsize] = '\n';
        }

        memcpy(context->dataBuffer + currentsize + nempty, line, linesize);

        context->dataBuffer[currentsize + nempty + linesize] = 0;
    } else if (strncmp(line, "event:", 6) == 0) {
        line += 6;
        line += line[0] == ' ';

        if (snprintf(context->eventName,
            sizeof(context->eventName), "%s", line) < 0)
        {
            LD_LOG(LD_LOG_CRITICAL,
                "snprintf failed in streamcallback type processing");

            return false;
        }
    }

    return true;
}

size_t
LDi_streamWriteCallback(const void *const contents, size_t size, size_t nmemb,
    void *rawcontext)
{
    char *nl;
    size_t realsize;
    struct StreamContext *mem;

    LD_ASSERT(rawcontext);

    nl       = NULL;
    realsize = size * nmemb;
    mem      = (struct StreamContext *)rawcontext;

    if (!(mem->memory =
        (char *)LDRealloc(mem->memory, mem->size + realsize + 1)))
    {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);

    mem->size += realsize;
    mem->memory[mem->size] = 0;

    nl = (char *)memchr(mem->memory, '\n', mem->size);
    if (nl) {
        size_t eaten = 0;
        while (nl) {
            *nl = 0;
            LDi_onSSE(mem, mem->memory + eaten);
            eaten = nl - mem->memory + 1;
            nl = (char *)memchr(mem->memory + eaten, '\n', mem->size - eaten);
        }
        mem->size -= eaten;
        memmove(mem->memory, mem->memory + eaten, mem->size);
    }
    return realsize;
}

static void
resetMemory(struct StreamContext *const context)
{
    LD_ASSERT(context);

    context->eventName[0] = 0;
    context->dataBuffer = NULL;

    LDFree(context->memory);
    context->memory = NULL;

    curl_slist_free_all(context->headers);
    context->headers = NULL;

    context->size = 0;
}

static void
done(struct LDClient *const client, void *const rawcontext, const bool success)
{
    struct StreamContext *context;

    LD_ASSERT(client);
    LD_ASSERT(rawcontext);

    context = (struct StreamContext *)rawcontext;

    context->active = false;

    (void)success;

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
    CURL *curl;
    char url[4096];
    struct StreamContext *context;

    LD_ASSERT(client);
    LD_ASSERT(rawcontext);

    context = (struct StreamContext *)rawcontext;
    curl    = NULL;

    if (context->active || !client->config->stream) {
        return NULL;
    }

    if (snprintf(url, sizeof(url), "%s/all",
        client->config->streamURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return NULL;
    }

    {
        char msg[256];

        LD_ASSERT(snprintf(msg, sizeof(msg),
            "connection to streaming url: %s", url) >= 0);

        LD_LOG(LD_LOG_INFO, msg);
    }

    if (!LDi_prepareShared(client->config, url, &curl, &context->headers)) {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, context) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEDATA failed");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, LDi_streamWriteCallback)
        != CURLE_OK)
    {
        LD_LOG(LD_LOG_CRITICAL,
            "curl_easy_setopt CURLOPT_WRITEFUNCTION failed");

        goto error;
    }

    context->active = true;

    return curl;

  error:
    curl_slist_free_all(context->headers);
    curl_easy_cleanup(curl);
    return NULL;
}

struct NetworkInterface *
LDi_constructStreaming(struct LDClient *const client)
{
    struct NetworkInterface *netInterface;
    struct StreamContext *context;

    LD_ASSERT(client);

    netInterface = NULL;
    context      = NULL;

    if (!(netInterface =
        (struct NetworkInterface *)LDAlloc(sizeof(struct NetworkInterface))))
    {
        goto error;
    }

    if (!(context =
        (struct StreamContext *)LDAlloc(sizeof(struct StreamContext))))
    {
        goto error;
    }

    context->memory       = NULL;
    context->size         = 0;
    context->active       = false;
    context->headers      = NULL;
    context->eventName[0] = 0;
    context->dataBuffer   = NULL;
    context->client       = client;

    netInterface->done      = done;
    netInterface->poll      = poll;
    netInterface->context   = context;
    netInterface->destroy   = destroy;
    netInterface->current   = NULL;
    netInterface->attempts  = 0;
    netInterface->waitUntil = 0;

    return netInterface;

  error:
    LDFree(context);
    LDFree(netInterface);
    return NULL;
}
