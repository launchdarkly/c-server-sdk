#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"
#include "ldstore.h"

struct LDClient *
LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli)
{
    struct LDClient *client = NULL;

    LD_ASSERT(config);

    if (!(client = malloc(sizeof(struct LDClient)))) {
        return NULL;
    }

    memset(client, 0, sizeof(struct LDClient));

    if (config->store) {
        config->defaultStore = false;
    } else {
        if (!(config->store = makeInMemoryStore())) {
            free(client);

            return NULL;
        }

        config->defaultStore = true;
    }

    client->shuttingdown = false;
    client->config       = config;

    if (!LDi_networkinit(client)) {
        goto error;
    }

    if (!LDi_rwlockinit(&client->lock)) {
        goto error;
    }

    if (!LDi_createthread(&client->thread, LDi_networkthread, client)) {
        goto error;
    }

    LDi_log(LD_LOG_INFO, "waiting to initialize");
    if (maxwaitmilli){
        unsigned int start, diff, now;

        LD_ASSERT(getMonotonicMilliseconds(&start));
        do {
            LD_ASSERT(LDi_rdlock(&client->lock));
            if (client->initialized) {
                LD_ASSERT(LDi_rdunlock(&client->lock));
                break;
            }
            LD_ASSERT(LDi_rdunlock(&client->lock));

            sleepMilliseconds(5);

            LD_ASSERT(getMonotonicMilliseconds(&now));
        } while ((diff = now - start) < maxwaitmilli);
    }
    LDi_log(LD_LOG_INFO, "initialized");

    return client;

  error:
    if (config->defaultStore && config->store) {
        LDStoreDestroy(config->store);
    }

    free(client);

    return NULL;
}

void
LDClientClose(struct LDClient *const client)
{
    if (client) {
        /* signal shutdown to background */
        LD_ASSERT(LDi_wrlock(&client->lock));
        client->shuttingdown = true;
        LD_ASSERT(LDi_wrunlock(&client->lock));

        /* wait until background exits */
        LD_ASSERT(LDi_jointhread(client->thread));

        /* cleanup resources */
        LD_ASSERT(LDi_rwlockdestroy(&client->lock));

        if (client->config->defaultStore) {
            LDStoreDestroy(client->config->store);
        }

        LDConfigFree(client->config);

        free(client);
    }
}
