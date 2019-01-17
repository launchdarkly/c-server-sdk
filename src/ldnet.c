#include "ldinternal.h"

#include <curl/curl.h>

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
        LD_ASSERT(LDi_rdlock(&client->lock));
        if (client->shuttingdown) {
            LD_ASSERT(LDi_rdunlock(&client->lock));

            break;
        }
        LD_ASSERT(LDi_rdunlock(&client->lock));
    }

    LD_ASSERT(curl_multi_cleanup(client->multihandle) == CURLM_OK);

    return THREAD_RETURN_DEFAULT;
}
