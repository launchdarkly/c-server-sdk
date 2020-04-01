#include <stdio.h>
#include <math.h>
#include <curl/curl.h>

#include <launchdarkly/api.h>

#include "network.h"
#include "config.h"
#include "client.h"
#include "misc.h"

#define LD_USER_AGENT "User-Agent: CServerClient/" LD_SDK_VERSION

bool
LDi_prepareShared(const struct LDConfig *const config, const char *const url,
    CURL **const o_curl, struct curl_slist **const o_headers)
{
    CURL *curl;
    struct curl_slist *headers;

    LD_ASSERT(config);
    LD_ASSERT(url);
    LD_ASSERT(o_curl);
    LD_ASSERT(o_headers);

    curl    = NULL;
    headers = NULL;

    if (!(curl = curl_easy_init())) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_init returned NULL");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_URL failed on");

        goto error;
    }

    {
        char headerauth[256];

        if (snprintf(headerauth, sizeof(headerauth), "Authorization: %s",
            config->key) < 0)
        {
            LD_LOG(LD_LOG_CRITICAL, "snprintf during failed");

            goto error;
        }

        if (!(headers = curl_slist_append(headers, headerauth))) {
            LD_LOG(LD_LOG_CRITICAL, "curl_slist_append failed for headerauth");

            goto error;
        }
    }

    if (!(headers = curl_slist_append(headers, LD_USER_AGENT))) {
        LD_LOG(LD_LOG_CRITICAL, "curl_slist_append failed for headeragent");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_HTTPHEADER failed");

        goto error;
    }

    *o_curl    = curl;
    *o_headers = headers;

    return true;

  error:
    curl_easy_cleanup(curl);

    curl_slist_free_all(headers);

    return false;
}

THREAD_RETURN
LDi_networkthread(void* const clientref)
{
    struct LDClient *const client = (struct LDClient *)clientref;

    /* allocated to max size */
    struct NetworkInterface *interfaces[3];
    /* record how many threads are actually running */
    size_t interfacecount = 0;

    CURLM *multihandle;

    LD_ASSERT(client);

    if (!(multihandle = curl_multi_init())) {
        LD_LOG(LD_LOG_ERROR, "failed to construct multihandle");

        return THREAD_RETURN_DEFAULT;
    }

    if (!client->config->useLDD) {
        if (!(interfaces[interfacecount++] = LDi_constructPolling(client))) {
            LD_LOG(LD_LOG_ERROR, "failed to construct polling");

            return THREAD_RETURN_DEFAULT;
        }

        if (!(interfaces[interfacecount++] = LDi_constructStreaming(client))) {
            LD_LOG(LD_LOG_ERROR, "failed to construct streaming");

            return THREAD_RETURN_DEFAULT;
        }
    }

    if (!(interfaces[interfacecount++] = LDi_constructAnalytics(client))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct analytics");

        return THREAD_RETURN_DEFAULT;
    }

    while (true) {
        struct CURLMsg *info;
        int running_handles, active_events;
        unsigned int i;
        bool offline;

        info            = NULL;
        running_handles = 0;
        active_events   = 0;

        LD_ASSERT(LDi_rwlock_rdlock(&client->lock));
        if (client->shuttingdown) {
            LD_ASSERT(LDi_rwlock_rdunlock(&client->lock));

            break;
        }
        offline = client->config->offline;
        LD_ASSERT(LDi_rwlock_rdunlock(&client->lock));

        curl_multi_perform(multihandle, &running_handles);

        if (!offline) {
            for (i = 0; i < interfacecount; i++) {
                CURL *handle;

                /* not waiting on backoff */
                handle = interfaces[i]->poll(client, interfaces[i]->context);

                if (handle) {
                    interfaces[i]->current = handle;

                    if (curl_easy_setopt(
                        handle, CURLOPT_PRIVATE, interfaces[i]) != CURLE_OK)
                    {
                        LD_LOG(LD_LOG_ERROR, "failed to associate context");

                        goto cleanup;
                    }

                    if (curl_multi_add_handle(
                        multihandle, handle) != CURLM_OK)
                    {
                        LD_LOG(LD_LOG_ERROR, "failed to add handle");

                        goto cleanup;
                    }
                }
            }
        }

        do {
            int inqueue = 0;

            info = curl_multi_info_read(multihandle, &inqueue);

            if (info && (info->msg == CURLMSG_DONE)) {
                long responseCode;
                CURL *easy = info->easy_handle;
                struct NetworkInterface *netInterface = NULL;

                if (curl_easy_getinfo(
                    easy, CURLINFO_RESPONSE_CODE, &responseCode) != CURLE_OK)
                {
                    LD_LOG(LD_LOG_ERROR, "failed to get response code");

                    goto cleanup;
                }

                if (responseCode == 401 || responseCode == 403) {
                    LD_LOG(LD_LOG_ERROR, "LaunchDarkly API Access Denied");

                    goto cleanup;
                }

                {
                    char msg[256];

                    LD_ASSERT(snprintf(msg, sizeof(msg),
                        "message done code %s %ld",
                        curl_easy_strerror(info->data.result),
                        responseCode) >= 0);

                    LD_LOG(LD_LOG_TRACE, msg);
                }

                if (curl_easy_getinfo(
                    easy, CURLINFO_PRIVATE, &netInterface) != CURLE_OK)
                {
                    LD_LOG(LD_LOG_ERROR, "failed to get context");

                    goto cleanup;
                }

                LD_ASSERT(netInterface);
                LD_ASSERT(netInterface->done);
                LD_ASSERT(netInterface->context);

                netInterface->done(client, netInterface->context,
                    info->data.result == CURLE_OK ? responseCode : 0);

                netInterface->current = NULL;

                LD_ASSERT(curl_multi_remove_handle(
                    multihandle, easy) == CURLM_OK);

                curl_easy_cleanup(easy);
            }
        } while (info);

        if (curl_multi_wait(
            multihandle, NULL, 0, 5, &active_events) != CURLM_OK)
        {
            LD_LOG(LD_LOG_ERROR, "failed to wait on handles");

            goto cleanup;
        }

        if (!active_events) {
            /* if curl is not doing anything wait so we don't burn CPU */
            LD_ASSERT(LDi_sleepMilliseconds(10));
        }
    }

  cleanup:
    LD_LOG(LD_LOG_INFO, "cleanup up networking thread");

    {
        unsigned int i;

        for (i = 0; i < interfacecount; i++) {
            struct NetworkInterface *const netInterface = interfaces[i];

            if (netInterface->current) {
                LD_ASSERT(curl_multi_remove_handle(
                    multihandle, netInterface->current) == CURLM_OK);

                curl_easy_cleanup(netInterface->current);
            }

            netInterface->destroy(netInterface->context);
            LDFree(netInterface);
        }
    }

    LD_ASSERT(curl_multi_cleanup(multihandle) == CURLM_OK);

    return THREAD_RETURN_DEFAULT;
}
