#include <string.h>
#include <stdio.h>

#include "ldnetwork.h"
#include "ldinternal.h"

static bool
updateStore(struct LDStore *const store, const char *const rawupdate)
{
    struct LDJSON *update = NULL;

    LD_ASSERT(store); LD_ASSERT(rawupdate);

    if (!(update = LDJSONDeserialize(rawupdate))) {
        return false;
    }

    LDi_log(LD_LOG_INFO, "running store init");
    return LDStoreInit(store, update);
}

struct PollContext {
    char *memory;
    size_t size;
    struct curl_slist *headers;
    int count;
    bool active;
    unsigned int lastpoll;
};

size_t
WriteMemoryCallback(void *const contents, const size_t size, const size_t nmemb, void *const userp)
{
    const size_t realsize = size * nmemb;
    struct PollContext *const mem = (struct PollContext *)userp;

    LD_ASSERT(mem);

    if (!(mem->memory = realloc(mem->memory, mem->size + realsize + 1))) {
        LDi_log(LD_LOG_CRITICAL, "not enough memory (realloc returned NULL)");

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

    free(context->memory); context->memory = NULL;

    curl_slist_free_all(context->headers); context->headers = NULL;

    context->size = 0;
}

static void
done(struct LDClient *const client, void *const rawcontext)
{
    struct PollContext *const context = rawcontext;

    LD_ASSERT(client); LD_ASSERT(context);

    LDi_log(LD_LOG_INFO, "done!");

    LDi_log(LD_LOG_INFO, "message data %s", context->memory);

    LD_ASSERT(updateStore(client->config->store, context->memory));

    context->count++;
    context->active = false;

    LD_ASSERT(LDi_wrlock(&client->lock));
    client->initialized = true;
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LD_ASSERT(getMonotonicMilliseconds(&context->lastpoll));

    resetMemory(context);
}

static void
destroy(void *const rawcontext)
{
    struct PollContext *const context = rawcontext;

    LD_ASSERT(context);

    LDi_log(LD_LOG_INFO, "destroy!");

    resetMemory(context);

    free(context);
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *curl = NULL; char url[4096];

    struct PollContext *const context = rawcontext;

    LD_ASSERT(context);

    if (context->active || context->count > 2) {
        return NULL;
    }

    {
        unsigned int now;

        LD_ASSERT(getMonotonicMilliseconds(&now));
        LD_ASSERT(now >= context->lastpoll);

        if (now - context->lastpoll < client->config->pollInterval * 1000) {
            return NULL;
        }
    }

    LDi_log(LD_LOG_INFO, "poll!");

    if (snprintf(url, sizeof(url), "%s/sdk/latest-all", client->config->baseURI) < 0) {
        LDi_log(LD_LOG_CRITICAL, "snprintf usereport failed");

        return false;
    }

    LDi_log(LD_LOG_INFO, "connecting to url: %s", url);

    if (!prepareShared(client->config, url, &curl, &context->headers)) {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEFUNCTION failed");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, context) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEDATA failed");

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
constructPolling(struct LDClient *const client)
{
    struct NetworkInterface *interface = NULL;
    struct PollContext *context = NULL;

    LD_ASSERT(client);

    if (!(interface = malloc(sizeof(struct NetworkInterface)))) {
        goto error;
    }

    if (!(context = malloc(sizeof(struct PollContext)))) {
        goto error;
    }

    context->memory   = NULL;
    context->size     = 0;
    context->count    = 0;
    context->headers  = NULL;
    context->active   = false;
    context->lastpoll = 0;

    interface->done    = done;
    interface->poll    = poll;
    interface->context = context;
    interface->destroy = destroy;
    interface->context = context;

    return interface;

  error:
    free(context);

    free(interface);

    return NULL;
}
