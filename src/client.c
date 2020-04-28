#include <string.h>
#include <stdlib.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "event_processor.h"
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

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientInit NULL config");

            return NULL;
        }
    #endif

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

    if (!(client->eventProcessor = LDi_newEventProcessor(config))) {
        LDStoreDestroy(client->store);
        LDFree(client);

        return NULL;
    }

    LDi_rwlock_init(&client->lock);

    LDi_thread_create(&client->thread, LDi_networkthread, client);

    LD_LOG(LD_LOG_INFO, "waiting to initialize");
    if (maxwaitmilli){
        double start, diff, now;

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
}

bool
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
        LDi_freeEventProcessor(client->eventProcessor);

        LDStoreDestroy(client->store);

        LDConfigFree(client->config);

        LDFree(client);

        LD_LOG(LD_LOG_INFO, "trace client cleanup");
    }

    return true;
}

bool
LDClientIsInitialized(struct LDClient *const client)
{
    LD_ASSERT(client);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (client == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientIsInitialized NULL client");

            return false;
        }
    #endif

    return LDStoreInitialized(client->store);
}

bool
LDClientTrack(struct LDClient *const client, const char *const key,
    const struct LDUser *const user, struct LDJSON *const data)
{
    LD_ASSERT(client);
    LD_ASSERT(key);
    LD_ASSERT(user);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (client == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientTrack NULL client");

            return false;
        }

        if (key == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientTrack NULL key");

            return false;
        }

        if (user == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientTrack NULL user");

            return false;
        }
    #endif

    return LDi_track(client->eventProcessor, user, key, data, 0, false);
}

bool
LDClientTrackMetric(struct LDClient *const client, const char *const key,
    const struct LDUser *const user, struct LDJSON *const data,
    const double metric)
{
    LD_ASSERT(client);
    LD_ASSERT(key);
    LD_ASSERT(user);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (client == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientTrackMetric NULL client");

            return false;
        }

        if (key == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientTrackMetric NULL key");

            return false;
        }

        if (user == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientTrackMetric NULL user");

            return false;
        }
    #endif

    return LDi_track(client->eventProcessor, user, key, data, metric, true);
}

bool
LDClientIdentify(struct LDClient *const client, const struct LDUser *const user)
{
    LD_ASSERT(client);
    LD_ASSERT(user);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (client == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientIdentify NULL client");

            return false;
        }

        if (user == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientIdentify NULL user");

            return false;
        }
    #endif

    return LDi_identify(client->eventProcessor, user);
}

bool
LDClientIsOffline(struct LDClient *const client)
{
    LD_ASSERT(client);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (client) {
            LD_LOG(LD_LOG_WARNING, "LDClientIsOffline NULL client");

            return false;
        }
    #endif

    return client->config->offline;
}

bool
LDClientFlush(struct LDClient *const client)
{
    LD_ASSERT(client);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (client == NULL) {
            LD_LOG(LD_LOG_WARNING, "LDClientFlush NULL client");

            return false;
        }
    #endif

    LDi_rwlock_wrlock(&client->lock);
    client->shouldFlush = true;
    LDi_rwlock_wrunlock(&client->lock);

    return true;
}
