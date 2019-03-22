#include <string.h>
#include <stdio.h>

#include "ldnetwork.h"
#include "ldinternal.h"
#include "ldstore.h"
#include "ldjson.h"
#include "ldstreaming.h"

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
onPut(struct LDClient *const client, struct LDJSON *const data)
{
    struct LDJSON *tmp;
    bool success;

    LD_ASSERT(client);
    LD_ASSERT(data);

    success = false;

    if (!(tmp = LDObjectDetachKey(data, "data"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        LDJSONFree(tmp);

        goto cleanup;
    }

    if (!(success = LDStoreInit(client->config->store, tmp))) {
        LD_LOG(LD_LOG_ERROR, "store error");

        goto cleanup;
     }

     success = true;

  cleanup:
    LDJSONFree(data);

    return success;
}

static bool
onPatch(struct LDClient *const client, struct LDJSON *const data)
{
    struct LDJSON *tmp;
    const char *key;
    bool success;
    enum FeatureKind kind;

    LD_ASSERT(client);
    LD_ASSERT(data);

    tmp     = NULL;
    key     = NULL;
    success = false;

    if (!(tmp = LDObjectLookup(data, "path"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (!LDi_parsePath(LDGetText(tmp), &kind, &key)) {
        LD_LOG(LD_LOG_ERROR, "failed to parse path");

        goto cleanup;
    }

    if (!(tmp = LDObjectDetachKey(data, "data"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        LDJSONFree(tmp);

        goto cleanup;
    }

    if (!LDStoreUpsert(client->config->store, kind, tmp)) {
        LD_LOG(LD_LOG_ERROR, "store error");

        goto cleanup;
    }

    success = true;

  cleanup:
    LDJSONFree(data);

    return success;
}

static bool
onDelete(struct LDClient *const client, struct LDJSON *const data)
{
    struct LDJSON *tmp;
    const char *key;
    bool success;
    enum FeatureKind kind;

    LD_ASSERT(client);
    LD_ASSERT(data);

    tmp     = NULL;
    key     = NULL;
    success = false;

    if (!(tmp = LDObjectLookup(data, "path"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (!LDi_parsePath(LDGetText(tmp), &kind, &key)) {
        LD_LOG(LD_LOG_ERROR, "failed to parse path");

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

    if (!LDStoreDelete(client->config->store, kind, key, LDGetNumber(tmp))) {
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
            if (strcmp(context->eventName, "put") == 0) {
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

        context->dataBuffer = LDRealloc(
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

static size_t
writeCallback(void *contents, size_t size, size_t nmemb, void *rawcontext)
{
    char *nl;
    size_t realsize;
    struct StreamContext *mem;

    LD_ASSERT(rawcontext);

    nl       = NULL;
    realsize = size * nmemb;
    mem      = rawcontext;

    if (!(mem->memory = LDRealloc(mem->memory, mem->size + realsize + 1))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);

    mem->size += realsize;
    mem->memory[mem->size] = 0;

    nl = memchr(mem->memory, '\n', mem->size);
    if (nl) {
        size_t eaten = 0;
        while (nl) {
            *nl = 0;
            LDi_onSSE(mem, mem->memory + eaten);
            eaten = nl - mem->memory + 1;
            nl = memchr(mem->memory + eaten, '\n', mem->size - eaten);
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
    struct StreamContext *const context = rawcontext;

    LD_ASSERT(client);
    LD_ASSERT(context);

    context->active = false;

    (void)success;

    resetMemory(context);
}

static void
destroy(void *const rawcontext)
{
    struct StreamContext *const context = rawcontext;

    LD_ASSERT(context);

    LD_LOG(LD_LOG_INFO, "streaming destroyed");

    resetMemory(context);

    LDFree(context);
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *curl;
    char url[4096];

    struct StreamContext *const context = rawcontext;

    LD_ASSERT(client);
    LD_ASSERT(context);

    curl = NULL;

    if (context->active || !client->config->stream) {
        return NULL;
    }

    if (snprintf(url, sizeof(url), "%s/all",
        client->config->streamURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return false;
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

    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback)
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
    struct NetworkInterface *interface;
    struct StreamContext *context;

    LD_ASSERT(client);

    interface = NULL;
    context   = NULL;

    if (!(interface = LDAlloc(sizeof(struct NetworkInterface)))) {
        goto error;
    }

    if (!(context = LDAlloc(sizeof(struct StreamContext)))) {
        goto error;
    }

    context->memory       = NULL;
    context->size         = 0;
    context->active       = false;
    context->headers      = NULL;
    context->eventName[0] = 0;
    context->dataBuffer   = NULL;
    context->client       = client;

    interface->done      = done;
    interface->poll      = poll;
    interface->context   = context;
    interface->destroy   = destroy;
    interface->current   = NULL;
    interface->attempts  = 0;
    interface->waitUntil = 0;

    return interface;

  error:
    LDFree(context);
    LDFree(interface);
    return NULL;
}
