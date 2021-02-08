#include <stdio.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "network.h"
#include "store.h"
#include "user.h"
#include "utility.h"

static LDBoolean
updateStore(struct LDStore *const store, const char *const rawupdate)
{
    struct LDJSON *update, *features;

    LD_ASSERT(store);
    LD_ASSERT(rawupdate);

    update = NULL;

    if (!(update = LDJSONDeserialize(rawupdate))) {
        LD_LOG(LD_LOG_ERROR, "failed to deserialize put");

        return LDBooleanFalse;
    }

    if (!validatePutBody(update)) {
        LD_LOG(LD_LOG_ERROR, "failed to validate put");

        goto error;
    }

    features = LDObjectDetachKey(update, "flags");
    LD_ASSERT(features);

    if (!LDObjectSetKey(update, "features", features)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(features);

        goto error;
    }

    LD_LOG(LD_LOG_INFO, "running store init");
    return LDStoreInit(store, update);

error:
    LDJSONFree(update);

    return LDBooleanFalse;
}

struct PollContext
{
    char *             memory;
    size_t             size;
    struct curl_slist *headers;
    LDBoolean          active;
    double             lastpoll;
};

static size_t
writeCallback(
    void *const  contents,
    const size_t size,
    const size_t nmemb,
    void *const  rawmem)
{
    size_t              realsize;
    struct PollContext *mem;

    LD_ASSERT(rawmem);

    realsize = size * nmemb;
    mem      = (struct PollContext *)rawmem;

    if (!(mem->memory =
              (char *)LDRealloc(mem->memory, mem->size + realsize + 1))) {
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static void
resetMemory(struct PollContext *const context)
{
    LD_ASSERT(context);

    LDFree(context->memory);
    context->memory = NULL;

    curl_slist_free_all(context->headers);
    context->headers = NULL;

    context->size = 0;
}

static void
done(
    struct LDClient *const client,
    void *const            rawcontext,
    const int              responseCode)
{
    struct PollContext *context;
    const LDBoolean     success = responseCode == 200;

    LD_ASSERT(client);
    LD_ASSERT(rawcontext);

    context         = (struct PollContext *)rawcontext;
    context->active = LDBooleanFalse;

    if (success) {
        if (!updateStore(client->store, context->memory)) {
            LD_LOG(LD_LOG_ERROR, "polling failed to update store");
        }

        LDi_getMonotonicMilliseconds(&context->lastpoll);
    }

    resetMemory(context);
}

static void
destroy(void *const rawcontext)
{
    struct PollContext *context;

    LD_ASSERT(rawcontext);

    context = (struct PollContext *)rawcontext;

    resetMemory(context);

    LDFree(context);
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *              curl;
    char                url[4096];
    struct PollContext *context;

    LD_ASSERT(rawcontext);

    curl    = NULL;
    context = (struct PollContext *)rawcontext;

    if (context->active || client->config->stream) {
        return NULL;
    }

    {
        double now;

        LDi_getMonotonicMilliseconds(&now);
        LD_ASSERT(now >= context->lastpoll);

        if (now - context->lastpoll < client->config->pollInterval) {
            return NULL;
        }
    }

    if (snprintf(
            url, sizeof(url), "%s/sdk/latest-all", client->config->baseURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return NULL;
    }

    LD_LOG_1(LD_LOG_INFO, "connection to polling url: %s", url);

    if (!LDi_prepareShared(client->config, url, &curl, &context->headers)) {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback) !=
        CURLE_OK) {
        LD_LOG(
            LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEFUNCTION failed");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, context) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEDATA failed");

        goto error;
    }

    context->active = LDBooleanTrue;

    return curl;

error:
    curl_slist_free_all(context->headers);

    curl_easy_cleanup(curl);

    return NULL;
}

struct NetworkInterface *
LDi_constructPolling(struct LDClient *const client)
{
    struct NetworkInterface *netInterface;
    struct PollContext *     context;

    LD_ASSERT(client);

    netInterface = NULL;
    context      = NULL;

    if (!(netInterface = (struct NetworkInterface *)LDAlloc(
              sizeof(struct NetworkInterface))))
    {
        goto error;
    }

    if (!(context = (struct PollContext *)LDAlloc(sizeof(struct PollContext))))
    {
        goto error;
    }

    context->memory   = NULL;
    context->size     = 0;
    context->headers  = NULL;
    context->active   = LDBooleanFalse;
    context->lastpoll = 0;

    netInterface->done    = done;
    netInterface->poll    = poll;
    netInterface->context = context;
    netInterface->destroy = destroy;
    netInterface->context = context;
    netInterface->current = NULL;

    return netInterface;

error:
    LDFree(context);

    LDFree(netInterface);

    return NULL;
}
