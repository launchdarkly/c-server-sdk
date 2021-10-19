#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>
#include <launchdarkly/boolean.h>

#include "config.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class ConfigFixture : public CommonFixture {
};

/* we set strings twice to ensure they are freed */
TEST_F(ConfigFixture, DefaultAndReplace) {
    struct LDConfig *config;
    struct LDJSON *attributes, *tmp;

    ASSERT_TRUE(config = LDConfigNew("a"));
    ASSERT_STREQ(config->key, "a");

    ASSERT_STREQ(config->baseURI, "https://app.launchdarkly.com");
    ASSERT_TRUE(LDConfigSetBaseURI(config, "https://test1.com"));
    ASSERT_STREQ(config->baseURI, "https://test1.com");
    ASSERT_TRUE(LDConfigSetBaseURI(config, "https://test2.com"));
    ASSERT_STREQ(config->baseURI, "https://test2.com");

    ASSERT_STREQ(
            config->streamURI, "https://stream.launchdarkly.com");
    ASSERT_TRUE(LDConfigSetStreamURI(config, "https://test3.com"));
    ASSERT_STREQ(config->streamURI, "https://test3.com");
    ASSERT_TRUE(LDConfigSetStreamURI(config, "https://test4.com"));
    ASSERT_STREQ(config->streamURI, "https://test4.com");

    ASSERT_STREQ(
            config->eventsURI, "https://events.launchdarkly.com");
    ASSERT_TRUE(LDConfigSetEventsURI(config, "https://test5.com"));
    ASSERT_STREQ(config->eventsURI, "https://test5.com");
    ASSERT_TRUE(LDConfigSetEventsURI(config, "https://test6.com"));
    ASSERT_STREQ(config->eventsURI, "https://test6.com");

    ASSERT_TRUE(config->stream);
    LDConfigSetStream(config, LDBooleanFalse);
    ASSERT_FALSE(config->stream);

    ASSERT_TRUE(config->sendEvents);
    LDConfigSetSendEvents(config, LDBooleanFalse);
    ASSERT_FALSE(config->sendEvents);

    ASSERT_EQ(config->eventsCapacity, 10000);
    LDConfigSetEventsCapacity(config, 50);
    ASSERT_EQ(config->eventsCapacity, 50);

    ASSERT_EQ(config->timeout, 5000);
    LDConfigSetTimeout(config, 10);
    ASSERT_EQ(config->timeout, 10);

    ASSERT_EQ(config->flushInterval, 5000);
    LDConfigSetFlushInterval(config, 1111);
    ASSERT_EQ(config->flushInterval, 1111);

    ASSERT_EQ(config->pollInterval, 30000);
    LDConfigSetPollInterval(config, 20000);
    ASSERT_EQ(config->pollInterval, 20000);

    ASSERT_FALSE(config->offline);
    LDConfigSetOffline(config, LDBooleanTrue);
    ASSERT_TRUE(config->offline);

    ASSERT_FALSE(config->useLDD);
    LDConfigSetUseLDD(config, LDBooleanTrue);
    ASSERT_TRUE(config->useLDD);

    ASSERT_FALSE(config->allAttributesPrivate);
    LDConfigSetAllAttributesPrivate(config, LDBooleanTrue);
    ASSERT_TRUE(config->allAttributesPrivate);

    ASSERT_FALSE(config->inlineUsersInEvents);
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    ASSERT_TRUE(config->inlineUsersInEvents);

    ASSERT_EQ(config->userKeysCapacity, 1000);
    LDConfigSetUserKeysCapacity(config, 12);
    ASSERT_EQ(config->userKeysCapacity, 12);

    ASSERT_EQ(config->userKeysFlushInterval, 300000);
    LDConfigSetUserKeysFlushInterval(config, 2000);
    ASSERT_EQ(config->userKeysFlushInterval, 2000);

    ASSERT_TRUE(attributes = LDNewArray());
    ASSERT_TRUE(LDJSONCompare(attributes, config->privateAttributeNames));
    ASSERT_TRUE(LDConfigAddPrivateAttribute(config, "name"));
    ASSERT_TRUE(tmp = LDNewText("name"));
    ASSERT_TRUE(LDArrayPush(attributes, tmp));
    ASSERT_TRUE(LDJSONCompare(attributes, config->privateAttributeNames));

    ASSERT_EQ(config->storeBackend, nullptr);
    LDConfigSetFeatureStoreBackend(config, NULL);

    ASSERT_EQ(config->storeCacheMilliseconds, 30000);
    LDConfigSetFeatureStoreBackendCacheTTL(config, 100);
    ASSERT_EQ(config->storeCacheMilliseconds, 100);

    ASSERT_EQ(config->wrapperName, nullptr);
    ASSERT_EQ(config->wrapperVersion, nullptr);
    ASSERT_TRUE(LDConfigSetWrapperInfo(config, "a", "b"));
    ASSERT_STREQ(config->wrapperName, "a");
    ASSERT_STREQ(config->wrapperVersion, "b");
    ASSERT_TRUE(LDConfigSetWrapperInfo(config, "c", "d"));
    ASSERT_STREQ(config->wrapperName, "c");
    ASSERT_STREQ(config->wrapperVersion, "d");
    ASSERT_TRUE(LDConfigSetWrapperInfo(config, "e", NULL));
    ASSERT_STREQ(config->wrapperName, "e");
    ASSERT_EQ(config->wrapperVersion, nullptr);

    LDJSONFree(attributes);
    LDConfigFree(config);
}
