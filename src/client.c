#include <stdlib.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "concurrency.h"
#include "config.h"
#include "event_processor.h"
#include "network.h"
#include "store.h"
#include "user.h"
#include "utility.h"

struct LDClient *
LDClientInit(struct LDConfig *const config, const unsigned int maxwaitmilli)
{
    struct LDClient *client;

    LD_ASSERT_API(config);

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
    config->storeBackend = NULL;

    client->shouldFlush  = false;
    client->shuttingdown = false;
    client->config       = config;

    if (!(client->eventProcessor = LDi_newEventProcessor(config))) {
        LDStoreDestroy(client->store);
        LDFree(client);

        return NULL;
    }

    LDi_rwlock_init(&client->lock);

    LDi_thread_create(&client->thread, LDi_networkthread, client);

    LD_LOG(LD_LOG_INFO, "waiting to initialize");
    if (maxwaitmilli) {
        double start, diff, now;

        LDi_getMonotonicMilliseconds(&start);
        do {
            if (LDStoreInitialized(client->store)) {
                break;
            }

            LDi_sleepMilliseconds(5);

            LDi_getMonotonicMilliseconds(&now);
        } while ((diff = now - start) < maxwaitmilli);
    }
    LD_LOG(LD_LOG_INFO, "initialized");

    return client;
}

LDBoolean
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

LDBoolean
LDClientIsInitialized(struct LDClient *const client)
{
    LD_ASSERT_API(client);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (client == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDClientIsInitialized NULL client");

        return false;
    }
#endif

    return LDStoreInitialized(client->store);
}

LDBoolean
LDClientTrack(
    struct LDClient *const     client,
    const char *const          key,
    const struct LDUser *const user,
    struct LDJSON *const       data)
{
    LD_ASSERT_API(client);
    LD_ASSERT_API(key);
    LD_ASSERT_API(user);

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

LDBoolean
LDClientTrackMetric(
    struct LDClient *const     client,
    const char *const          key,
    const struct LDUser *const user,
    struct LDJSON *const       data,
    const double               metric)
{
    LD_ASSERT_API(client);
    LD_ASSERT_API(key);
    LD_ASSERT_API(user);

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

LDBoolean
LDClientAlias(
    struct LDClient *const     client,
    const struct LDUser *const currentUser,
    const struct LDUser *const previousUser)
{
    LD_ASSERT_API(client);
    LD_ASSERT_API(currentUser);
    LD_ASSERT_API(previousUser);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (client == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDClientAlias NULL client");

        return 0;
    }

    if (currentUser == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDClientAlias NULL currentUser");

        return 0;
    }

    if (previousUser == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDClientAlias NULL previousUser");

        return 0;
    }
#endif

    return LDi_alias(client->eventProcessor, currentUser, previousUser);
}

LDBoolean
LDClientIdentify(struct LDClient *const client, const struct LDUser *const user)
{
    LD_ASSERT_API(client);
    LD_ASSERT_API(user);

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

LDBoolean
LDClientIsOffline(struct LDClient *const client)
{
    LD_ASSERT_API(client);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (client == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDClientIsOffline NULL client");

        return false;
    }
#endif

    return client->config->offline;
}

LDBoolean
LDClientFlush(struct LDClient *const client)
{
    LD_ASSERT_API(client);

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
