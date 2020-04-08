#include <string.h>
#include <stdlib.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "events.h"
#include "network.h"
#include "client.h"
#include "config.h"
#include "utility.h"
#include "user.h"
#include "store.h"
#include "concurrency.h"

struct LDClient *
LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli)
{
    struct LDClient *client;

    LD_ASSERT(config);

    if (!(client = (struct LDClient *)LDAlloc(sizeof(struct LDClient)))) {
        return NULL;
    }

    memset(client, 0, sizeof(struct LDClient));

    if (!(client->store = LDStoreNew(config))) {
        LDFree(client);

        return NULL;
    }

    /* construction of store takes ownership of backend */
    config->storeBackend   = NULL;

    client->shouldFlush    = false;
    client->shuttingdown   = false;
    client->config         = config;
    client->summaryStart   = 0;
    client->lastServerTime = 0;

    LDi_getMonotonicMilliseconds(&client->lastUserKeyFlush);
    LDi_rwlock_init(&client->lock);

    if (!(client->events = LDNewArray())) {
        goto error;
    }

    if (!(client->summaryCounters = LDNewObject())) {
        goto error;
    }

    if (!(client->userKeys = LDLRUInit(config->userKeysCapacity))) {
        goto error;
    }

    LDi_thread_create(&client->thread, LDi_networkthread, client);

    LD_LOG(LD_LOG_INFO, "waiting to initialize");
    if (maxwaitmilli){
        unsigned long start, diff, now;

        LDi_getMonotonicMilliseconds(&start);
        do {
            LDi_rwlock_rdlock(&client->lock);
            if (client->initialized) {
                LDi_rwlock_rdunlock(&client->lock);
                break;
            }
            LDi_rwlock_rdunlock(&client->lock);

            LDi_sleepMilliseconds(5);

            LDi_getMonotonicMilliseconds(&now);
        } while ((diff = now - start) < maxwaitmilli);
    }
    LD_LOG(LD_LOG_INFO, "initialized");

    return client;

  error:
    LDStoreDestroy(client->store);

    LDJSONFree(client->events);
    LDJSONFree(client->summaryCounters);
    LDLRUFree(client->userKeys);

    LDFree(client);

    return NULL;
}

void
LDClientClose(struct LDClient *const client)
{
    if (client) {
        /* signal shutdown to background */
        LDi_rwlock_wrlock(&client->lock);
        client->shuttingdown = true;
        LDi_rwlock_wrunlock(&client->lock);

        /* wait until background exits */
        LDi_thread_join(&client->thread);

        /* cleanup resources */
        LDi_rwlock_destroy(&client->lock);
        LDJSONFree(client->events);
        LDJSONFree(client->summaryCounters);
        LDLRUFree(client->userKeys);

        LDStoreDestroy(client->store);

        LDConfigFree(client->config);

        LDFree(client);

        LD_LOG(LD_LOG_INFO, "trace client cleanup");
    }
}

bool
LDClientIsInitialized(struct LDClient *const client)
{
    LD_ASSERT(client);

    return LDStoreInitialized(client->store);
}

bool
LDClientTrack(struct LDClient *const client, const char *const key,
    const struct LDUser *const user, struct LDJSON *const data)
{
    struct LDJSON *event, *indexEvent;

    LD_ASSERT(client);
    LD_ASSERT(LDUserValidate(user));
    LD_ASSERT(key);

    if (!LDi_maybeMakeIndexEvent(client, user, &indexEvent)) {
        LD_LOG(LD_LOG_ERROR, "failed to construct index event");

        return false;
    }

    if (!(event = LDi_newCustomEvent(client, user, key, data))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct custom event");

        LDJSONFree(indexEvent);

        return false;
    }

    LDi_addEvent(client, event);

    if (indexEvent) {
        LDi_addEvent(client, indexEvent);
    }

    return true;
}

bool
LDClientTrackMetric(struct LDClient *const client, const char *const key,
    const struct LDUser *const user, struct LDJSON *const data,
    const double metric)
{
    struct LDJSON *event;

    LD_ASSERT(client);
    LD_ASSERT(user);
    LD_ASSERT(key);

    if (!(event = LDi_newCustomMetricEvent(client, user, key, data, metric))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct custom event");

        return false;
    }

    LDi_addEvent(client, event);

    return true;
}

bool
LDClientIdentify(struct LDClient *const client, const struct LDUser *const user)
{
    struct LDJSON *event;

    LD_ASSERT(client);
    LD_ASSERT(LDUserValidate(user));

    if (!(event = newIdentifyEvent(client, user))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct identify event");

        return false;
    }

    LDi_addEvent(client, event);

    return true;
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
    LDi_rwlock_wrlock(&client->lock);
    client->shouldFlush = true;
    LDi_rwlock_wrunlock(&client->lock);
}
