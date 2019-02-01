#include "ldinternal.h"
#include "ldnetwork.h"

#include <stdio.h>
#include <unistd.h>

#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

struct LDNetworkContext {
    CURL *curl;
    struct curl_slist *headers;
    struct MemoryStruct data;
};

static size_t
WriteMemoryCallback(void *const contents, const size_t size, const size_t nmemb, void *const userp)
{
    const size_t realsize = size * nmemb;
    struct MemoryStruct *const mem = (struct MemoryStruct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        LDi_log(LD_LOG_CRITICAL, "not enough memory (realloc returned NULL)");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static bool
prepareShared(const struct LDConfig *const config, struct LDNetworkContext *const context, const char *const url)
{
    CURL *curl = NULL; struct curl_slist *headers = NULL; char headerauth[256];

    LD_ASSERT(config); LD_ASSERT(context); LD_ASSERT(url);

    if (!(curl = curl_easy_init())) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_init returned NULL"); goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_URL failed on: %s", url); goto error;
    }

    if (snprintf(headerauth, sizeof(headerauth), "Authorization: %s", config->key) < 0) {
        LDi_log(LD_LOG_CRITICAL, "snprintf during Authorization header creation failed"); goto error;
    }

    LDi_log(LD_LOG_INFO, "using authentication: %s", headerauth);

    if (!(headers = curl_slist_append(headers, headerauth))) {
        LDi_log(LD_LOG_CRITICAL, "curl_slist_append failed for headerauth"); goto error;
    }

    if (!(headers = curl_slist_append(headers, "User-Agent: CServerClient/0.1"))) {
        LDi_log(LD_LOG_CRITICAL, "curl_slist_append failed for headeragent"); goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEFUNCTION failed"); goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context->data) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_WRITEDATA failed"); goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_HTTPHEADER failed"); goto error;
    }

    memset(context, 0, sizeof(struct LDNetworkContext));

    context->curl = curl; context->headers = headers;

    return true;

  error:
    if (curl) {
        curl_easy_cleanup(curl);
    }

    return false;
}

bool
prepareShared2(const struct LDConfig *const config, const char *const url, const struct NetworkInterface *const interface,
    CURL **const o_curl, struct curl_slist **const o_headers)
{
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;

    LD_ASSERT(config); LD_ASSERT(url); LD_ASSERT(interface); LD_ASSERT(o_curl); LD_ASSERT(o_headers);

    if (!(curl = curl_easy_init())) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_init returned NULL");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_URL failed on: %s", url);

        goto error;
    }

    {
        char headerauth[256];

        if (snprintf(headerauth, sizeof(headerauth), "Authorization: %s", config->key) < 0) {
            LDi_log(LD_LOG_CRITICAL, "snprintf during Authorization header creation failed");

            goto error;
        }

        LDi_log(LD_LOG_INFO, "using authentication: %s", headerauth);

        if (!(headers = curl_slist_append(headers, headerauth))) {
            LDi_log(LD_LOG_CRITICAL, "curl_slist_append failed for headerauth");

            goto error;
        }
    }

    if (!(headers = curl_slist_append(headers, "User-Agent: CServerClient/0.1"))) {
        LDi_log(LD_LOG_CRITICAL, "curl_slist_append failed for headeragent");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_HTTPHEADER failed");

        goto error;
    }

    if (curl_easy_setopt(curl,CURLOPT_PRIVATE, interface) != CURLE_OK) {
        LDi_log(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_PRIVATE failed"); goto error;
    }

    *o_curl = curl; *o_headers = headers;

    return true;

  error:
    if (curl) {
        curl_easy_cleanup(curl);
    }

    if (headers) {
        curl_slist_free_all(headers);
    }

    return false;
}

bool
LDi_prepareFetch(const struct LDClient *const client, struct LDNetworkContext *const context)
{
    char url[4096];

    LD_ASSERT(client); LD_ASSERT(context);

    if (snprintf(url, sizeof(url), "%s/sdk/latest-all", client->config->baseURI) < 0) {
        LDi_log(LD_LOG_CRITICAL, "snprintf usereport failed");

        return false;
    }

    LDi_log(LD_LOG_INFO, "connecting to url: %s", url);

    if (!prepareShared(client->config, context, url)) {
        return false;
    }

    return true;
}

bool
LDi_networkinit(struct LDClient *const client)
{
    LD_ASSERT(client);

    client->multihandle = curl_multi_init();

    return client->multihandle != NULL;
}

static bool
updateStore(struct LDFeatureStore *const store, const char *const rawupdate)
{
    struct LDVersionedSet *sets = NULL, *flagset = NULL, *segmentset = NULL;

    cJSON *decoded = NULL; struct LDVersionedData *versioned = NULL; const char *namespace = NULL;

    LD_ASSERT(store); LD_ASSERT(rawupdate);

    if (!(segmentset = malloc(sizeof(struct LDVersionedSet)))) {
        LDi_log(LD_LOG_ERROR, "segmentset alloc failed");

        return false;
    }

    memset(segmentset, 0, sizeof(struct LDVersionedSet));

    segmentset->kind = getSegmentKind();

    if (!(flagset = malloc(sizeof(struct LDVersionedSet)))) {
        LDi_log(LD_LOG_ERROR, "flagset alloc failed");

        return false;
    }

    memset(flagset, 0, sizeof(struct LDVersionedSet));

    flagset->kind = getFlagKind();

    if (!(decoded = cJSON_Parse(rawupdate))) {
        LDi_log(LD_LOG_ERROR, "JSON parsing failed");

        return false;
    }

    {
        cJSON *flags = NULL; cJSON *iter = NULL;

        if (!(flags = cJSON_GetObjectItemCaseSensitive(decoded, "flags"))) {
            LDi_log(LD_LOG_ERROR, "key flags does not exist");

            return false;
        }

        cJSON_ArrayForEach(iter, flags) {
            struct FeatureFlag *const flag = featureFlagFromJSON(iter->string, iter);

            if (!flag) {
                LDi_log(LD_LOG_ERROR, "failed to marshall feature flag");

                return false;
            }

            if (!(versioned = flagToVersioned(flag))) {
                LDi_log(LD_LOG_ERROR, "failed make version interface for flag");

                return false;
            }

            HASH_ADD_KEYPTR(hh, flagset->elements, flag->key, strlen(flag->key), versioned);
        }
    }

    {
        cJSON *segments = NULL; cJSON *iter = NULL;

        if (!(segments = cJSON_GetObjectItemCaseSensitive(decoded, "segments"))) {
            LDi_log(LD_LOG_ERROR, "key segments does not exist");

            return false;
        }

        cJSON_ArrayForEach(iter, segments) {
            struct Segment *const segment = segmentFromJSON(iter);

            if (!segment) {
                LDi_log(LD_LOG_ERROR, "failed to marshall segment");

                return false;
            }

            if (!(versioned = segmentToVersioned(segment))) {
                LDi_log(LD_LOG_ERROR, "failed make version interface for segment");

                return false;
            }

            HASH_ADD_KEYPTR(hh, segmentset->elements, segment->key, strlen(segment->key), versioned);
        }
    }

    cJSON_Delete(decoded);

    namespace = flagset->kind.getNamespace();
    HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), flagset);

    namespace = segmentset->kind.getNamespace();
    HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), segmentset);

    return store->init(store->context, sets);
}

THREAD_RETURN
LDi_networkthread(void* const clientref)
{
    struct LDClient *const client = clientref;

    struct LDNetworkContext polling;

    LD_ASSERT(client);

    LD_ASSERT(LDi_prepareFetch(client, &polling));

    LD_ASSERT(curl_multi_add_handle(client->multihandle, polling.curl) == CURLM_OK);

    while (true) {
        struct CURLMsg *info = NULL; int running_handles = 0; int active_events = 0;

        LD_ASSERT(LDi_rdlock(&client->lock));
        if (client->shuttingdown) {
            LD_ASSERT(LDi_rdunlock(&client->lock));

            break;
        }
        LD_ASSERT(LDi_rdunlock(&client->lock));

        curl_multi_perform(client->multihandle, &running_handles);

        do {
            int inqueue = 0;

            info = curl_multi_info_read(client->multihandle, &inqueue);

            if (info && (info->msg == CURLMSG_DONE)) {
                long responsecode;
                CURL *easy = info->easy_handle;

                LD_ASSERT(curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responsecode) == CURLE_OK);

                LDi_log(LD_LOG_INFO, "message done code %d %d", info->data.result, responsecode);

                LDi_log(LD_LOG_INFO, "message data %s", polling.data.memory);

                LD_ASSERT(updateStore(client->config->store, polling.data.memory));

                curl_multi_remove_handle(client->multihandle, easy);
                curl_easy_cleanup(easy);
            }
        } while (info);

        LD_ASSERT(curl_multi_wait(client->multihandle, NULL, 0, 5, &active_events) == CURLM_OK);

        if (!active_events) {
            /* if curl is not doing anything wait so we don't burn CPU */
            usleep(1000 * 10);
        }
    }

    LD_ASSERT(curl_multi_cleanup(client->multihandle) == CURLM_OK);

    free(polling.data.memory);
    curl_slist_free_all(polling.headers);

    return THREAD_RETURN_DEFAULT;
}
