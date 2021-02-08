#include <stdbool.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "config.h"

/* we set strings twice to ensure they are freed */
static void
testDefaultAndReplace()
{
    struct LDConfig *config;
    struct LDJSON *  attributes, *tmp;

    LD_ASSERT(config = LDConfigNew("a"));
    LD_ASSERT(strcmp(config->key, "a") == 0);

    LD_ASSERT(strcmp(config->baseURI, "https://app.launchdarkly.com") == 0);
    LD_ASSERT(LDConfigSetBaseURI(config, "https://test1.com"));
    LD_ASSERT(strcmp(config->baseURI, "https://test1.com") == 0);
    LD_ASSERT(LDConfigSetBaseURI(config, "https://test2.com"));
    LD_ASSERT(strcmp(config->baseURI, "https://test2.com") == 0);

    LD_ASSERT(
        strcmp(config->streamURI, "https://stream.launchdarkly.com") == 0);
    LD_ASSERT(LDConfigSetStreamURI(config, "https://test3.com"));
    LD_ASSERT(strcmp(config->streamURI, "https://test3.com") == 0);
    LD_ASSERT(LDConfigSetStreamURI(config, "https://test4.com"));
    LD_ASSERT(strcmp(config->streamURI, "https://test4.com") == 0);

    LD_ASSERT(
        strcmp(config->eventsURI, "https://events.launchdarkly.com") == 0);
    LD_ASSERT(LDConfigSetEventsURI(config, "https://test5.com"));
    LD_ASSERT(strcmp(config->eventsURI, "https://test5.com") == 0);
    LD_ASSERT(LDConfigSetEventsURI(config, "https://test6.com"));
    LD_ASSERT(strcmp(config->eventsURI, "https://test6.com") == 0);

    LD_ASSERT(config->stream == true);
    LDConfigSetStream(config, false);
    LD_ASSERT(config->stream == false);

    LD_ASSERT(config->sendEvents == true);
    LDConfigSetSendEvents(config, false);
    LD_ASSERT(config->sendEvents == false);

    LD_ASSERT(config->eventsCapacity == 10000);
    LDConfigSetEventsCapacity(config, 50);
    LD_ASSERT(config->eventsCapacity == 50);

    LD_ASSERT(config->timeout == 5000);
    LDConfigSetTimeout(config, 10);
    LD_ASSERT(config->timeout == 10);

    LD_ASSERT(config->flushInterval == 5000);
    LDConfigSetFlushInterval(config, 1111);
    LD_ASSERT(config->flushInterval == 1111);

    LD_ASSERT(config->pollInterval == 30000);
    LDConfigSetPollInterval(config, 20000);
    LD_ASSERT(config->pollInterval == 20000);

    LD_ASSERT(config->offline == false);
    LDConfigSetOffline(config, true);
    LD_ASSERT(config->offline == true);

    LD_ASSERT(config->useLDD == false);
    LDConfigSetUseLDD(config, true);
    LD_ASSERT(config->useLDD == true);

    LD_ASSERT(config->allAttributesPrivate == false);
    LDConfigSetAllAttributesPrivate(config, true);
    LD_ASSERT(config->allAttributesPrivate == true);

    LD_ASSERT(config->inlineUsersInEvents == false);
    LDConfigInlineUsersInEvents(config, true);
    LD_ASSERT(config->inlineUsersInEvents == true);

    LD_ASSERT(config->userKeysCapacity == 1000);
    LDConfigSetUserKeysCapacity(config, 12);
    LD_ASSERT(config->userKeysCapacity == 12);

    LD_ASSERT(config->userKeysFlushInterval == 300000);
    LDConfigSetUserKeysFlushInterval(config, 2000);
    LD_ASSERT(config->userKeysFlushInterval == 2000);

    LD_ASSERT(attributes = LDNewArray());
    LD_ASSERT(LDJSONCompare(attributes, config->privateAttributeNames));
    LD_ASSERT(LDConfigAddPrivateAttribute(config, "name"));
    LD_ASSERT(tmp = LDNewText("name"));
    LD_ASSERT(LDArrayPush(attributes, tmp));
    LD_ASSERT(LDJSONCompare(attributes, config->privateAttributeNames));

    LD_ASSERT(config->storeBackend == NULL);
    LDConfigSetFeatureStoreBackend(config, NULL);

    LD_ASSERT(config->storeCacheMilliseconds == 30000);
    LDConfigSetFeatureStoreBackendCacheTTL(config, 100);
    LD_ASSERT(config->storeCacheMilliseconds == 100);

    LD_ASSERT(config->wrapperName == NULL);
    LD_ASSERT(config->wrapperVersion == NULL);
    LD_ASSERT(LDConfigSetWrapperInfo(config, "a", "b"));
    LD_ASSERT(strcmp(config->wrapperName, "a") == 0);
    LD_ASSERT(strcmp(config->wrapperVersion, "b") == 0);
    LD_ASSERT(LDConfigSetWrapperInfo(config, "c", "d"));
    LD_ASSERT(strcmp(config->wrapperName, "c") == 0);
    LD_ASSERT(strcmp(config->wrapperVersion, "d") == 0);
    LD_ASSERT(LDConfigSetWrapperInfo(config, "e", NULL));
    LD_ASSERT(strcmp(config->wrapperName, "e") == 0);
    LD_ASSERT(config->wrapperVersion == NULL);

    LDJSONFree(attributes);
    LDConfigFree(config);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testDefaultAndReplace();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
