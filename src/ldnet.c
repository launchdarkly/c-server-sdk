#include "ldinternal.h"
#include "ldnetwork.h"

#include <stdio.h>
#include <unistd.h>

#include <curl/curl.h>

bool
prepareShared(const struct LDConfig *const config, const char *const url,
    CURL **const o_curl, struct curl_slist **const o_headers)
{
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;

    LD_ASSERT(config); LD_ASSERT(url); LD_ASSERT(o_curl); LD_ASSERT(o_headers);

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

    *o_curl = curl; *o_headers = headers;

    return true;

  error:
    curl_easy_cleanup(curl);

    curl_slist_free_all(headers);

    return false;
}

bool
LDi_networkinit(struct LDClient *const client)
{
    LD_ASSERT(client);

    client->multihandle = curl_multi_init();

    return client->multihandle != NULL;
}

bool
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

    struct NetworkInterface *interfaces[1];

    LD_ASSERT(client);

    interfaces[0] = constructPolling(client); LD_ASSERT(interfaces[0]);

    while (true) {
        struct CURLMsg *info = NULL; int running_handles = 0; int active_events = 0;

        LD_ASSERT(LDi_rdlock(&client->lock));
        if (client->shuttingdown) {
            LD_ASSERT(LDi_rdunlock(&client->lock));

            break;
        }
        LD_ASSERT(LDi_rdunlock(&client->lock));

        curl_multi_perform(client->multihandle, &running_handles);

        for (unsigned int i = 0; i < 1; i++) {
            CURL *const handle = interfaces[i]->poll(client, interfaces[i]->context);

            if (handle) {
                LD_ASSERT(curl_easy_setopt(handle, CURLOPT_PRIVATE, interfaces[i]) == CURLE_OK);

                LD_ASSERT(curl_multi_add_handle(client->multihandle, handle) == CURLM_OK);
            }
        }

        do {
            int inqueue = 0;

            info = curl_multi_info_read(client->multihandle, &inqueue);

            if (info && (info->msg == CURLMSG_DONE)) {
                long responsecode;
                CURL *easy = info->easy_handle;
                struct NetworkInterface *interface = NULL;

                LD_ASSERT(curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &responsecode) == CURLE_OK);

                LDi_log(LD_LOG_INFO, "message done code %d %d", info->data.result, responsecode);

                // LDi_log(LD_LOG_INFO, "message data %s", polling.data.memory);

                // LD_ASSERT(updateStore(client->config->store, polling.data.memory));

                LD_ASSERT(curl_easy_getinfo(easy, CURLINFO_PRIVATE, &interface) == CURLE_OK);
                LD_ASSERT(interface); LD_ASSERT(interface->done); LD_ASSERT(interface->context);
                interface->done(client, interface->context);

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

    interfaces[0]->destroy(interfaces[0]->context);
    free(interfaces[0]);

    return THREAD_RETURN_DEFAULT;
}
