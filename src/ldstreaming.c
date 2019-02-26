#include <string.h>
#include <stdio.h>

#include "ldnetwork.h"
#include "ldinternal.h"
#include "ldstore.h"
#include "ldjson.h"

struct StreamContext {
    char *memory;
    size_t size;
    bool active;
    struct curl_slist *headers;
    char eventName[256];
    char *dataBuffer;
    struct LDClient *client;
};

bool
parsePath(const char *path, char **const kind, char **const key)
{
    const char *const segments = "/segments/";
    size_t segmentlen;

    const char *const flags = "/flags/";
    size_t flagslen;

    LD_ASSERT(path);
    LD_ASSERT(kind);
    LD_ASSERT(key);

    segmentlen = strlen(segments);
    flagslen = strlen(flags);

    if (strncmp(path, segments, segmentlen) == 0) {
        if (!(*key = strdup(path + segmentlen))) {
            return false;
        }

        if (!(*kind = strndup(path + 1, segmentlen - 2))) {
            free(*key);

            return false;
        }
    } else if(strncmp(path, flags, flagslen) == 0) {
        if (!(*key = strdup(path + flagslen))) {
            return false;
        }

        if (!(*kind = strndup(path + 1, flagslen - 2))) {
            free(*key);

            return false;
        }
    } else {
        LD_LOG(LD_LOG_ERROR, "failed to parse prefix %s", path);

        return false;
    }

    return true;
}

static bool
onPut(struct LDClient *const client, struct LDJSON *const data)
{
    bool status;

    LD_ASSERT(client);
    LD_ASSERT(data);

    status = LDStoreInit(client->config->store, data);

    LDJSONFree(data);

    return status;
}

static bool
onPatch(struct LDClient *const client, struct LDJSON *const data)
{
    struct LDJSON *tmp;
    char *kind = NULL;
    char *key = NULL;
    bool success = false;

    LD_ASSERT(client);
    LD_ASSERT(data);

    if (!(tmp = LDObjectLookup(data, "path"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (!parsePath(LDGetText(tmp), &kind, &key)) {
        LD_LOG(LD_LOG_ERROR, "failed to parse path");

        goto cleanup;
    }

    if (!(tmp = LDObjectLookup(data, "data"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (!(tmp = LDJSONDuplicate(tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto cleanup;
    }

    if (!LDStoreUpsert(client->config->store, kind, tmp)) {
        LD_LOG(LD_LOG_ERROR, "store error");

        goto cleanup;
    }

    success = true;

  cleanup:
    free(kind);
    free(key);
    LDJSONFree(data);

    return success;
}

static bool
onDelete(struct LDClient *const client, struct LDJSON *const data)
{
    struct LDJSON *tmp;
    char *kind = NULL;
    char *key = NULL;
    bool success = false;

    LD_ASSERT(client);
    LD_ASSERT(data);

    if (!(tmp = LDObjectLookup(data, "path"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        goto cleanup;
    }

    if (!parsePath(LDGetText(tmp), &kind, &key)) {
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
    free(kind);
    free(key);
    LDJSONFree(data);

    return success;
}

static bool
onSSE(struct StreamContext *const context, const char *line)
{
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
                LD_LOG(LD_LOG_WARNING,
                    "streamcallback unknown event name: %s",
                    context->eventName);
            }
        }

        free(context->dataBuffer);
        context->dataBuffer = NULL;
        context->eventName[0] = 0;

        if (!status) {
            return false;
        }
    } else if (strncmp(line, "data:", 5) == 0) {
        bool nempty;
        size_t linesize;
        size_t currentsize = 0;

        line += 5;
        line += line[0] == ' ';

        nempty = context->dataBuffer != NULL;
        linesize = strlen(line);

        if (nempty) {
            currentsize = strlen(context->dataBuffer);
        }

        context->dataBuffer = realloc(
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
writeCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    char *nl;
    size_t realsize = size * nmemb;
    struct StreamContext *mem = userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        /* out of memory! */
        LDi_log(LD_LOG_CRITICAL, "not enough memory (realloc returned NULL)");
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
            onSSE(mem, mem->memory + eaten);
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

    free(context->memory);
    context->memory = NULL;

    curl_slist_free_all(context->headers);
    context->headers = NULL;

    context->size = 0;
}

static void
done(struct LDClient *const client, void *const rawcontext)
{
    struct StreamContext *const context = rawcontext;

    LD_ASSERT(client);
    LD_ASSERT(context);

    context->active = false;

    resetMemory(context);
}

static void
destroy(void *const rawcontext)
{
    struct StreamContext *const context = rawcontext;

    LD_ASSERT(context);

    LD_LOG(LD_LOG_INFO, "streaming destroyed");

    resetMemory(context);

    free(context);
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *curl = NULL;
    char url[4096];

    struct StreamContext *const context = rawcontext;

    LD_ASSERT(client);
    LD_ASSERT(context);

    if (context->active || !client->config->stream) {
        return NULL;
    }

    if (snprintf(url, sizeof(url), "%s/all",
        client->config->streamURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return false;
    }

    LD_LOG(LD_LOG_INFO, "connecting to url: %s", url);

    if (!prepareShared(client->config, url, &curl, &context->headers)) {
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
constructStreaming(struct LDClient *const client)
{
    struct NetworkInterface *interface = NULL;
    struct StreamContext *context = NULL;

    LD_ASSERT(client);

    if (!(interface = malloc(sizeof(struct NetworkInterface)))) {
        goto error;
    }

    if (!(context = malloc(sizeof(struct StreamContext)))) {
        goto error;
    }

    context->memory       = NULL;
    context->size         = 0;
    context->active       = false;
    context->headers      = NULL;
    context->eventName[0] = 0;
    context->dataBuffer   = NULL;
    context->client       = client;

    interface->done    = done;
    interface->poll    = poll;
    interface->context = context;
    interface->destroy = destroy;
    interface->current = NULL;

    return interface;

  error:
    free(context);
    free(interface);
    return NULL;
}
