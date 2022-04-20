#pragma once

#include <launchdarkly/json.h>
#include <launchdarkly/store.h>
#include "data_source.h"

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
    struct LDDataSource     *dataSource;
};

/* Trims a single trailing slash, if present, from the end of the given string.
 * Returns a newly allocated string. */
char *
LDi_trimTrailingSlash(const char *s);

/* Sets target to the trimmed version of s (via LDi_trimTrailingSlash and LDSetString). */
LDBoolean
LDi_setTrimmedString(char **const target, const char *const s);
