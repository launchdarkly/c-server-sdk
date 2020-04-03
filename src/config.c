#include <string.h>
#include <stdlib.h>

#include <launchdarkly/api.h>

#include "config.h"
#include "misc.h"

struct LDConfig *
LDConfigNew(const char *const key)
{
    struct LDConfig *config;

    LD_ASSERT_API(key);

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

    return LDSetString(&config->baseURI, baseURI);
}

bool
LDConfigSetStreamURI(struct LDConfig *const config, const char *const streamURI)
{
    LD_ASSERT_API(config);
    LD_ASSERT_API(streamURI);

    return LDSetString(&config->streamURI, streamURI);
}

bool
LDConfigSetEventsURI(struct LDConfig *const config, const char *const eventsURI)
{
    LD_ASSERT_API(config);
    LD_ASSERT_API(eventsURI);

    return LDSetString(&config->eventsURI, eventsURI);
}

void
LDConfigSetStream(struct LDConfig *const config, const bool stream)
{
    LD_ASSERT_API(config);

    config->stream = stream;
}

void
LDConfigSetSendEvents(struct LDConfig *const config, const bool sendEvents)
{
    LD_ASSERT_API(config);

    config->sendEvents = sendEvents;
}

void
LDConfigSetEventsCapacity(struct LDConfig *const config,
    const unsigned int eventsCapacity)
{
    LD_ASSERT_API(config);

    config->eventsCapacity = eventsCapacity;
}

void
LDConfigSetTimeout(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    config->timeout = milliseconds;
}

void
LDConfigSetFlushInterval(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    config->flushInterval = milliseconds;
}

void
LDConfigSetPollInterval(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    config->pollInterval = milliseconds;
}

void
LDConfigSetOffline(struct LDConfig *const config, const bool offline)
{
    LD_ASSERT_API(config);

    config->offline = offline;
}

void
LDConfigSetUseLDD(struct LDConfig *const config, const bool useLDD)
{
    LD_ASSERT_API(config);

    config->useLDD = useLDD;
}

void
LDConfigSetAllAttributesPrivate(struct LDConfig *const config,
    const bool allAttributesPrivate)
{
    LD_ASSERT_API(config);

    config->allAttributesPrivate = allAttributesPrivate;
}

void
LDConfigInlineUsersInEvents(struct LDConfig *const config,
    const bool inlineUsersInEvents)
{
    LD_ASSERT_API(config);

    config->inlineUsersInEvents = inlineUsersInEvents;
}

void
LDConfigSetUserKeysCapacity(struct LDConfig *const config,
    const unsigned int userKeysCapacity)
{
    LD_ASSERT_API(config);

    config->userKeysCapacity = userKeysCapacity;
}

void
LDConfigSetUserKeysFlushInterval(struct LDConfig *const config,
    const unsigned int userKeysFlushInterval)
{
    LD_ASSERT_API(config);

    config->userKeysFlushInterval = userKeysFlushInterval;
}

bool
LDConfigAddPrivateAttribute(struct LDConfig *const config,
    const char *const attribute)
{
    struct LDJSON *temp;

    LD_ASSERT_API(config);
    LD_ASSERT_API(attribute);

    if ((temp = LDNewText(attribute))) {
        return LDArrayPush(config->privateAttributeNames, temp);
    } else {
        return false;
    }
}

void
LDConfigSetFeatureStoreBackend(struct LDConfig *const config,
    struct LDStoreInterface *const backend)
{
    LD_ASSERT_API(config);

    config->storeBackend = backend;
}

void
LDConfigSetFeatureStoreBackendCacheTTL(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT_API(config);

    config->storeCacheMilliseconds = milliseconds;
}
