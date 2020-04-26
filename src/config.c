#include <string.h>
#include <stdlib.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "config.h"
#include "utility.h"

struct LDConfig *
LDConfigNew(const char *const key)
{
    struct LDConfig *config;

    LD_ASSERT_API(key);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (key == NULL) {
            return NULL;
        }
    #endif

    if (!(config = (struct LDConfig *)LDAlloc(sizeof(struct LDConfig)))) {
        return NULL;
    }

    memset(config, 0, sizeof(struct LDConfig));

    if (!(config->key = LDStrDup(key))) {
        goto error;
    }

    if (!(config->baseURI = LDStrDup("https://app.launchdarkly.com"))) {
        goto error;
    }

    if (!(config->streamURI = LDStrDup("https://stream.launchdarkly.com"))) {
        goto error;
    }

    if (!(config->eventsURI = LDStrDup("https://events.launchdarkly.com"))) {
        goto error;
    }

    if (!(config->privateAttributeNames = LDNewArray())) {
        goto error;
    }

    config->stream                 = true;
    config->sendEvents             = true;
    config->eventsCapacity         = 10000;
    config->timeout                = 5000;
    config->flushInterval          = 5000;
    config->pollInterval           = 30000;
    config->offline                = false;
    config->useLDD                 = false;
    config->allAttributesPrivate   = false;
    config->inlineUsersInEvents    = false;
    config->userKeysCapacity       = 1000;
    config->userKeysFlushInterval  = 300000;
    config->storeBackend           = NULL;
    config->storeCacheMilliseconds = 30 * 1000;

    return config;

  error:
    LDConfigFree(config);

    return NULL;
}

void
LDConfigFree(struct LDConfig *const config)
{
    if (config) {
        if (config->storeBackend) {
            if (config->storeBackend->destructor) {
                config->storeBackend->destructor(config->storeBackend->context);
            }

            LDFree(config->storeBackend);
        }

        LDFree(     config->key                   );
        LDFree(     config->baseURI               );
        LDFree(     config->streamURI             );
        LDFree(     config->eventsURI             );
        LDJSONFree( config->privateAttributeNames );
        LDFree(     config                        );
    }
}

bool
LDConfigSetBaseURI(struct LDConfig *const config, const char *const baseURI)
{
    LD_ASSERT_API(config);
    LD_ASSERT_API(baseURI);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL || baseURI == NULL) {
            return false;
        }
    #endif

    return LDSetString(&config->baseURI, baseURI);
}

bool
LDConfigSetStreamURI(struct LDConfig *const config, const char *const streamURI)
{
    LD_ASSERT_API(config);
    LD_ASSERT_API(streamURI);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL || streamURI == NULL) {
            return false;
        }
    #endif

    return LDSetString(&config->streamURI, streamURI);
}

bool
LDConfigSetEventsURI(struct LDConfig *const config, const char *const eventsURI)
{
    LD_ASSERT_API(config);
    LD_ASSERT_API(eventsURI);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL || eventsURI == NULL) {
            return false;
        }
    #endif

    return LDSetString(&config->eventsURI, eventsURI);
}

bool
LDConfigSetStream(struct LDConfig *const config, const bool stream)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->stream = stream;

    return true;
}

bool
LDConfigSetSendEvents(struct LDConfig *const config, const bool sendEvents)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->sendEvents = sendEvents;

    return true;
}

bool
LDConfigSetEventsCapacity(struct LDConfig *const config,
    const unsigned int eventsCapacity)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->eventsCapacity = eventsCapacity;

    return true;
}

bool
LDConfigSetTimeout(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->timeout = milliseconds;

    return true;
}

bool
LDConfigSetFlushInterval(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->flushInterval = milliseconds;

    return true;
}

bool
LDConfigSetPollInterval(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->pollInterval = milliseconds;

    return true;
}

bool
LDConfigSetOffline(struct LDConfig *const config, const bool offline)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->offline = offline;

    return true;
}

bool
LDConfigSetUseLDD(struct LDConfig *const config, const bool useLDD)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->useLDD = useLDD;

    return true;
}

bool
LDConfigSetAllAttributesPrivate(struct LDConfig *const config,
    const bool allAttributesPrivate)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->allAttributesPrivate = allAttributesPrivate;

    return true;
}

bool
LDConfigInlineUsersInEvents(struct LDConfig *const config,
    const bool inlineUsersInEvents)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->inlineUsersInEvents = inlineUsersInEvents;

    return true;
}

bool
LDConfigSetUserKeysCapacity(struct LDConfig *const config,
    const unsigned int userKeysCapacity)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->userKeysCapacity = userKeysCapacity;

    return true;
}

bool
LDConfigSetUserKeysFlushInterval(struct LDConfig *const config,
    const unsigned int userKeysFlushInterval)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->userKeysFlushInterval = userKeysFlushInterval;

    return true;
}

bool
LDConfigAddPrivateAttribute(struct LDConfig *const config,
    const char *const attribute)
{
    struct LDJSON *temp;

    LD_ASSERT_API(config);
    LD_ASSERT_API(attribute);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL || attribute == NULL) {
            return false;
        }
    #endif

    if ((temp = LDNewText(attribute))) {
        return LDArrayPush(config->privateAttributeNames, temp);
    } else {
        return false;
    }
}

bool
LDConfigSetFeatureStoreBackend(struct LDConfig *const config,
    struct LDStoreInterface *const backend)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->storeBackend = backend;

    return true;
}

bool
LDConfigSetFeatureStoreBackendCacheTTL(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    #ifdef LAUNCHDARKLY_DEFENSIVE
        if (config == NULL) {
            return false;
        }
    #endif

    config->storeCacheMilliseconds = milliseconds;

    return true;
}
