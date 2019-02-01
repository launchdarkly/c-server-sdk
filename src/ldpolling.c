#include <string.h>
#include <stdio.h>

#include "ldnetwork.h"
#include "ldinternal.h"

struct MemoryStruct {
    char *memory;
    size_t size;
};

size_t
WriteMemoryCallback(void *const contents, const size_t size, const size_t nmemb, void *const userp)
{
    const size_t realsize = size * nmemb;
    struct MemoryStruct *const mem = (struct MemoryStruct *)userp;

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
done(void *const rawcontext)
{
    struct MemoryStruct *context = rawcontext;

    LD_ASSERT(context);

    LDi_log(LD_LOG_INFO, "done!");

    free(context->memory);

    free(context);
}

CURL *
constructPolling(const struct LDClient *const client)
{
    CURL *curl = NULL;
    char url[4096];
    struct curl_slist *headers = NULL;
    struct NetworkInterface *interface = NULL;
    struct MemoryStruct *context = NULL;

    LD_ASSERT(client);

    if (!(interface = malloc(sizeof(struct NetworkInterface)))) {
        goto error;
    }

    if (!(context = malloc(sizeof(struct MemoryStruct)))) {
        goto error;
    }

    interface->done    = done;
    interface->context = context;

    if (!prepareShared2(client->config, url, interface, &curl, &headers)) {
        goto error;
    }

    if (snprintf(url, sizeof(url), "%s/sdk/latest-all", client->config->baseURI) < 0) {
        LDi_log(LD_LOG_CRITICAL, "snprintf usereport failed");

        return false;
    }

    LDi_log(LD_LOG_INFO, "connecting to url: %s", url);

    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEFUNCTION failed");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEDATA failed");

        goto error;
    }

    return curl;

  error:
    free(context);

    if (curl) {
        curl_easy_cleanup(curl);
    }

    return NULL;
}
