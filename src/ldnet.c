#include "ldinternal.h"

#include <stdio.h>

#include <curl/curl.h>

struct LDNetworkContext {
    CURL *curl;
    struct curl_slist *headerlist;
};

bool
prepareContext(struct LDNetworkContext *const context)
{
    LD_ASSERT(context);

    return true;
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

    LD_ASSERT(client);

    while (true) {
        int running_handles = 0;

        LD_ASSERT(LDi_rdlock(&client->lock));
        if (client->shuttingdown) {
            LD_ASSERT(LDi_rdunlock(&client->lock));

            break;
        }
        LD_ASSERT(LDi_rdunlock(&client->lock));

        curl_multi_perform(client->multihandle, &running_handles);

        LD_ASSERT(curl_multi_wait(client->multihandle, NULL, 0, 10, NULL) == CURLM_OK);
    }

    LD_ASSERT(curl_multi_cleanup(client->multihandle) == CURLM_OK);

    return THREAD_RETURN_DEFAULT;
}
