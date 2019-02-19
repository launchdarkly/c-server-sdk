#include "ldinternal.h"
#include "ldnetwork.h"

#include <stdio.h>
#include <unistd.h>

#include <curl/curl.h>

const char *const agentheader = "User-Agent: CServerClient/0.1";

bool
prepareShared(const struct LDConfig *const config, const char *const url,
    CURL **const o_curl, struct curl_slist **const o_headers)
{
    CURL *curl = NULL;
    struct curl_slist *headers = NULL;

    LD_ASSERT(config);
    LD_ASSERT(url);
    LD_ASSERT(o_curl);
    LD_ASSERT(o_headers);

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

        LD_LOG(LD_LOG_INFO, "using authentication: %s", headerauth);

        if (!(headers = curl_slist_append(headers, headerauth))) {
            LD_LOG(LD_LOG_CRITICAL, "curl_slist_append failed for headerauth");

            goto error;
        }
    }

    if (!(headers = curl_slist_append(headers, agentheader))) {
        LD_LOG(LD_LOG_CRITICAL, "curl_slist_append failed for headeragent");

        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
        LD_LOG(LD_LOG_CRITICAL, "curl_easy_setopt CURLOPT_HTTPHEADER failed");

        goto error;
    }

    *o_curl = curl;
    *o_headers = headers;

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

THREAD_RETURN
LDi_networkthread(void* const clientref)
{
    struct LDClient *const client = clientref;

    struct NetworkInterface *interfaces[1];

    const size_t interfacecount =
        sizeof(interfaces) / sizeof(struct NetworkInterface *);

    LD_ASSERT(client);

    if (!(interfaces[0] = constructPolling(client))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct polling");

        return THREAD_RETURN_DEFAULT;
    }

    while (true) {
        struct CURLMsg *info = NULL;
        int running_handles = 0;
        int active_events = 0;

        LD_ASSERT(LDi_rdlock(&client->lock));
        if (client->shuttingdown) {
            LD_ASSERT(LDi_rdunlock(&client->lock));

            break;
        }
        LD_ASSERT(LDi_rdunlock(&client->lock));

        curl_multi_perform(client->multihandle, &running_handles);

        for (unsigned int i = 0; i < interfacecount; i++) {
            CURL *const handle =
                interfaces[i]->poll(client, interfaces[i]->context);

            if (handle) {
                LD_ASSERT(curl_easy_setopt(
                    handle, CURLOPT_PRIVATE, interfaces[i]) == CURLE_OK);

                LD_ASSERT(curl_multi_add_handle(
                    client->multihandle, handle) == CURLM_OK);
            }
        }

        do {
            int inqueue = 0;

            info = curl_multi_info_read(client->multihandle, &inqueue);

            if (info && (info->msg == CURLMSG_DONE)) {
                long responsecode;
                CURL *easy = info->easy_handle;
                struct NetworkInterface *interface = NULL;

                LD_ASSERT(curl_easy_getinfo(
                    easy, CURLINFO_RESPONSE_CODE, &responsecode) == CURLE_OK);

                LD_LOG(LD_LOG_INFO, "message done code %d %d",
                    info->data.result, responsecode);

                LD_ASSERT(curl_easy_getinfo(
                    easy, CURLINFO_PRIVATE, &interface) == CURLE_OK);

                LD_ASSERT(interface);
                LD_ASSERT(interface->done);
                LD_ASSERT(interface->context);

                interface->done(client, interface->context);

                curl_multi_remove_handle(client->multihandle, easy);
                curl_easy_cleanup(easy);
            }
        } while (info);

        LD_ASSERT(curl_multi_wait(
            client->multihandle, NULL, 0, 5, &active_events) == CURLM_OK);

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
