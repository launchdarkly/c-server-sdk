#include <string.h>
#include <stdio.h>

#include "ldnetwork.h"
#include "ldinternal.h"

struct StreamContext {
    char *memory;
    size_t size;
    bool active;
    struct curl_slist *headers;
};

void
onSSE()
{

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
            /* streamdata->callback(streamdata->client, mem->memory + eaten); */
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

    context->memory  = NULL;
    context->size    = 0;
    context->active  = false;
    context->headers = NULL;

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
