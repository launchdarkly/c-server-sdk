#include <string.h>
#include <stdlib.h>

#include <launchdarkly/api.h>

#include "config.h"
#include "misc.h"

struct LDConfig *
LDConfigNew(const char *const key)
{
    struct LDConfig *config;

    LD_ASSERT(key);

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

    config->stream                = true;
    config->sendEvents            = true;
    config->eventsCapacity        = 10000;
    config->timeout               = 5;
    config->flushInterval         = 5;
    config->pollInterval          = 30;
    config->offline               = false;
    config->useLDD                = false;
    config->allAttributesPrivate  = false;
    config->inlineUsersInEvents   = false;
    config->userKeysCapacity      = 1000;
    config->userKeysFlushInterval = 300;
    config->store                 = NULL;

    return config;

  error:
    LDConfigFree(config);

    return NULL;
}

void
LDConfigFree(struct LDConfig *const config)
{
    if (config) {
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
    LD_ASSERT(config);
    LD_ASSERT(baseURI);

    return LDSetString(&config->baseURI, baseURI);
}

bool
LDConfigSetStreamURI(struct LDConfig *const config, const char *const streamURI)
{
    LD_ASSERT(config);
    LD_ASSERT(streamURI);

    return LDSetString(&config->streamURI, streamURI);
}

bool
LDConfigSetEventsURI(struct LDConfig *const config, const char *const eventsURI)
{
    LD_ASSERT(config);
    LD_ASSERT(eventsURI);

    return LDSetString(&config->eventsURI, eventsURI);
}

void
LDConfigSetStream(struct LDConfig *const config, const bool stream)
{
    LD_ASSERT(config);

    config->stream = stream;
}

void
LDConfigSetSendEvents(struct LDConfig *const config, const bool sendEvents)
{
    LD_ASSERT(config);

    config->sendEvents = sendEvents;
}

void
LDConfigSetEventsCapacity(struct LDConfig *const config,
    const unsigned int eventsCapacity)
{
    LD_ASSERT(config);

    config->eventsCapacity = eventsCapacity;
}

void
LDConfigSetTimeout(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT(config);

    config->timeout = milliseconds;
}

void
LDConfigSetFlushInterval(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT(config);

    config->flushInterval = milliseconds;
}

void
LDConfigSetPollInterval(struct LDConfig *const config,
    const unsigned int milliseconds)
{
    LD_ASSERT(config);

    config->pollInterval = milliseconds;
}

void
LDConfigSetOffline(struct LDConfig *const config, const bool offline)
{
    LD_ASSERT(config);

    config->offline = offline;
}

void
LDConfigSetUseLDD(struct LDConfig *const config, const bool useLDD)
{
    LD_ASSERT(config);

    config->useLDD = useLDD;
}

void
LDConfigSetAllAttributesPrivate(struct LDConfig *const config,
    const bool allAttributesPrivate)
{
    LD_ASSERT(config);

    config->allAttributesPrivate = allAttributesPrivate;
}

void
LDConfigInlineUsersInEvents(struct LDConfig *const config,
    const bool inlineUsersInEvents)
{
    LD_ASSERT(config);

    config->inlineUsersInEvents = inlineUsersInEvents;
}

void
LDConfigSetUserKeysCapacity(struct LDConfig *const config,
    const unsigned int userKeysCapacity)
{
    LD_ASSERT(config);

    config->userKeysCapacity = userKeysCapacity;
}

void
LDConfigSetUserKeysFlushInterval(struct LDConfig *const config,
    const unsigned int userKeysFlushInterval)
{
    LD_ASSERT(config);

    config->userKeysFlushInterval = userKeysFlushInterval;
}

bool
LDConfigAddPrivateAttribute(struct LDConfig *const config,
    const char *const attribute)
{
    struct LDJSON *temp;

    LD_ASSERT(config);
    LD_ASSERT(attribute);

    if ((temp = LDNewText(attribute))) {
        return LDArrayPush(config->privateAttributeNames, temp);
    } else {
        return false;
    }
}

void
LDConfigSetFeatureStore(struct LDConfig *const config,
    struct LDStore *const store)
{
    LD_ASSERT(config);

    config->store = store;
}
