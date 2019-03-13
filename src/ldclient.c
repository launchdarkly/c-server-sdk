#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"
#include "ldstore.h"
#include "ldclient.h"
#include "ldevents.h"

struct LDClient *
LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli)
{
    struct LDClient *client;

    LD_ASSERT(config);

    if (!(client = LDAlloc(sizeof(struct LDClient)))) {
        return NULL;
    }

    memset(client, 0, sizeof(struct LDClient));

    if (config->store) {
        config->defaultStore = false;
    } else {
        if (!(config->store = makeInMemoryStore())) {
            LDFree(client);

            return NULL;
        }

        config->defaultStore = true;
    }

    client->shouldFlush  = false;
    client->shuttingdown = false;
    client->config       = config;
    client->summaryStart = 0;

    if (!LDi_rwlockinit(&client->lock)) {
        goto error;
    }

    if (!(client->events = LDNewArray())) {
        goto error;
    }

    if (!(client->summaryCounters = LDNewObject())) {
        goto error;
    }

    if (!LDi_createthread(&client->thread, LDi_networkthread, client)) {
        goto error;
    }

    LD_LOG(LD_LOG_INFO, "waiting to initialize");
    if (maxwaitmilli){
        unsigned long start, diff, now;

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
    LD_LOG(LD_LOG_INFO, "initialized");

    return client;

  error:
    if (config->defaultStore && config->store) {
        LDStoreDestroy(config->store);
    }

    LDJSONFree(client->events);
    LDJSONFree(client->summaryCounters);

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
        LDJSONFree(client->events);
        LDJSONFree(client->summaryCounters);

        if (client->config->defaultStore) {
            LDStoreDestroy(client->config->store);
        }

        LDConfigFree(client->config);

        LDFree(client);
    }
}

bool
LDClientIsInitialized(struct LDClient *const client)
{
    LD_ASSERT(client);

    return LDStoreInitialized(client->config->store);
}


bool
LDClientTrack(struct LDClient *const client, const struct LDUser *const user,
    const char *const key, struct LDJSON *const data)
{
    struct LDJSON *event;

    LD_ASSERT(client);
    LD_ASSERT(user);
    LD_ASSERT(key);

    if (!(event = newCustomEvent(client, user, key, data))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct custom event");

        return false;
    }

    return addEvent(client, event);
}

bool
LDClientIdentify(struct LDClient *const client, const struct LDUser *const user)
{
    struct LDJSON *event;

    LD_ASSERT(client);
    LD_ASSERT(user);

    if (!(event = newIdentifyEvent(client, user))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct identify event");

        return false;
    }

    return addEvent(client, event);
}

bool
LDClientIsOffline(struct LDClient *const client)
{
    LD_ASSERT(client);

    return client->config->offline;
}

void
LDClientFlush(struct LDClient *const client)
{
    LD_ASSERT(client);
    LD_ASSERT(LDi_wrlock(&client->lock));
    client->shouldFlush = true;
    LD_ASSERT(LDi_wrunlock(&client->lock));
}
