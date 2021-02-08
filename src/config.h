#pragma once

#include <launchdarkly/json.h>
#include <launchdarkly/store.h>

struct LDConfig
{
    char *                   key;
    char *                   baseURI;
    char *                   streamURI;
    char *                   eventsURI;
    LDBoolean                stream;
    LDBoolean                sendEvents;
    unsigned int             eventsCapacity;
    unsigned int             timeout;
    unsigned int             flushInterval;
    unsigned int             pollInterval;
    LDBoolean                offline;
    LDBoolean                useLDD;
    LDBoolean                allAttributesPrivate;
    struct LDJSON *          privateAttributeNames; /* Array of Text */
    LDBoolean                inlineUsersInEvents;
    unsigned int             userKeysCapacity;
    unsigned int             userKeysFlushInterval;
    struct LDStoreInterface *storeBackend;
    unsigned int             storeCacheMilliseconds;
    char *                   wrapperName;
    char *                   wrapperVersion;
};
