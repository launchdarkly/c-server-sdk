#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>

#include "assertion.h"
#include "utility.h"

#include "store.h"
#ifdef TEST_REDIS
#include "../../stores/redis/src/redis.h"
#include "test-utils/flags.h"
#endif
}

/*
 * This file contains tests that are used for both the redis store and the memory store.
 */

#ifdef TEST_REDIS

static void
flushDB() {
    redisContext *connection;
    redisReply *reply;

    LD_ASSERT(connection = redisConnect("127.0.0.1", 6379));
    LD_ASSERT(!connection->err);

    LD_ASSERT(reply = static_cast<redisReply *>(redisCommand(connection, "FLUSHDB")));
    freeReplyObject(reply);
    redisFree(connection);
}

static struct LDStore *
prepareEmptyRedisStore() {
    struct LDStore *store;
    struct LDStoreInterface *interface;
    struct LDRedisConfig *redisConfig;
    struct LDConfig *config;

    flushDB();

    LD_ASSERT(config = LDConfigNew(""));
    LD_ASSERT(redisConfig = LDRedisConfigNew())
    LD_ASSERT(interface = LDStoreInterfaceRedisNew(redisConfig));
    LDConfigSetFeatureStoreBackend(config, interface);
    LD_ASSERT(store = LDStoreNew(config));
    config->storeBackend = NULL;
    LD_ASSERT(!LDStoreInitialized(store));
    LDConfigFree(config);

    return store;
}

#endif

static struct LDStore *
prepareEmptyMemoryStore() {
    struct LDStore *store;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew(""));
    LD_ASSERT(store = LDStoreNew(config));
    LD_ASSERT(!LDStoreInitialized(store));

    LDConfigFree(config);

    return store;
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class CommonStoreFixture
        : public CommonFixture, public testing::WithParamInterface<std::pair<std::string, struct LDStore *(*)()>> {
protected:
    struct LDStore *store = nullptr;

    void SetUp() override {
        CommonFixture::SetUp();
        store = GetParam().second();
    }

    void TearDown() override {
        LDStoreDestroy(store);
        CommonFixture::TearDown();
    }
};

#ifdef TEST_REDIS

INSTANTIATE_TEST_SUITE_P(
        AvailableStoresTest,
        CommonStoreFixture,
        ::testing::Values(
                std::pair<std::string, struct LDStore *(*)()>("MemoryStore", prepareEmptyMemoryStore),
                std::pair<std::string, struct LDStore *(*)()>("RedisStore", prepareEmptyRedisStore)
        ));
#else
INSTANTIATE_TEST_SUITE_P(
        AvailableStoresTest,
        CommonStoreFixture,
        ::testing::Values(
                std::pair<std::string, struct LDStore *(*)()> ("MemoryStore", prepareEmptyMemoryStore)
        ));
#endif

TEST_P(CommonStoreFixture, IsInitialized) {
    ASSERT_TRUE(store);
}

static struct LDJSON *
makeVersioned(const char *const key, const unsigned int version) {
    struct LDJSON *feature, *tmp;

    LD_ASSERT(feature = LDNewObject());

    LD_ASSERT(tmp = LDNewText(key));
    LD_ASSERT(LDObjectSetKey(feature, "key", tmp));

    LD_ASSERT(tmp = LDNewNumber(version));
    LD_ASSERT(LDObjectSetKey(feature, "version", tmp));

    LD_ASSERT(tmp = LDNewBool(LDBooleanFalse));
    LD_ASSERT(LDObjectSetKey(feature, "deleted", tmp));

    return feature;
}

TEST_P(CommonStoreFixture, InitializeEmpty) {
    ASSERT_FALSE(LDStoreInitialized(store));
    ASSERT_TRUE(LDStoreInitEmpty(store));
    ASSERT_TRUE(LDStoreInitialized(store));
}

TEST_P(CommonStoreFixture, GetALl) {
    struct LDJSON *all, *category;
    struct LDJSONRC *rawFlags;

    ASSERT_FALSE(LDStoreInitialized(store));

    ASSERT_TRUE(all = LDNewObject());
    ASSERT_TRUE(category = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(all, "features", category));
    ASSERT_TRUE(LDObjectSetKey(category, "a", makeVersioned("a", 32)));
    ASSERT_TRUE(LDObjectSetKey(category, "b", makeVersioned("b", 51)));

    ASSERT_TRUE(LDStoreInit(store, all));

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &rawFlags));
    LDJSONRCRelease(rawFlags);

    ASSERT_TRUE(LDStoreInitialized(store));
}

TEST_P(CommonStoreFixture, UpsertUpdatesAll) {
    struct LDJSONRC *result;
    struct LDJSON *flag1, *flag1Dupe, *flag2, *flag2Dupe, *all;

    ASSERT_TRUE(all = LDNewObject());

    ASSERT_TRUE(flag1 = makeVersioned("a", 52));
    ASSERT_TRUE(flag1Dupe = LDJSONDuplicate(flag1));
    ASSERT_TRUE(LDObjectSetKey(all, "a", flag1Dupe));
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, flag1));

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &result));
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(result), all));
    LDJSONRCRelease(result);

    ASSERT_TRUE(flag2 = makeVersioned("b", 30));
    ASSERT_TRUE(flag2Dupe = LDJSONDuplicate(flag2));
    ASSERT_TRUE(LDObjectSetKey(all, "b", flag2Dupe));
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, flag2));

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &result));
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(result), all));
    LDJSONRCRelease(result);

    LDObjectDeleteKey(all, "a");

    LDStoreRemove(store, LD_FLAG, "a", 60);
    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &result));
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(result), all));
    LDJSONRCRelease(result);

    LDJSONFree(all);
}

TEST_P(CommonStoreFixture, DeletedOnly) {
    struct LDJSONRC *lookup;

    ASSERT_FALSE(LDStoreInitialized(store));
    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(LDStoreRemove(store, LD_FLAG, "abc", 123));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_P(CommonStoreFixture, BasicExists) {
    struct LDJSON *feature, *featureCopy;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    ASSERT_TRUE(featureCopy = LDJSONDuplicate(feature));

    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_TRUE(lookup);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(lookup), featureCopy));

    LDJSONFree(featureCopy);
    LDJSONRCRelease(lookup);
}

TEST_P(CommonStoreFixture, BasicDoesNotExist) {
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_P(CommonStoreFixture, UpsertNewer) {
    struct LDJSON *feature, *featureCopy;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, feature));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 5));
    ASSERT_TRUE(featureCopy = LDJSONDuplicate(feature));
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    ASSERT_TRUE(lookup);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(lookup), featureCopy));

    LDJSONFree(featureCopy);
    LDJSONRCRelease(lookup);
}

TEST_P(CommonStoreFixture, UpsertOlder) {
    struct LDJSON *feature1, *feature2, *feature1Copy;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature1 = makeVersioned("my-heap-key", 5));
    ASSERT_TRUE(feature1Copy = LDJSONDuplicate(feature1));
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, feature1));

    ASSERT_TRUE(feature2 = makeVersioned("my-heap-key", 3));
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, feature2));

    ASSERT_TRUE(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    ASSERT_TRUE(lookup);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(lookup), feature1Copy));

    LDJSONFree(feature1Copy);
    LDJSONRCRelease(lookup);
}

TEST_P(CommonStoreFixture, UpsertDelete) {
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, feature));

    ASSERT_TRUE(LDStoreRemove(store, LD_SEGMENT, "my-heap-key", 5));

    ASSERT_TRUE(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    ASSERT_TRUE(!lookup);
}

TEST_P(CommonStoreFixture, ConflictDifferentNamespace) {
    struct LDJSON *feature1, *feature2, *feature1Copy, *feature2Copy;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature1 = makeVersioned("my-heap-key", 3));
    ASSERT_TRUE(feature1Copy = LDJSONDuplicate(feature1));
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, feature1));

    ASSERT_TRUE(feature2 = makeVersioned("my-heap-key", 3));
    ASSERT_TRUE(feature2Copy = LDJSONDuplicate(feature2));
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, feature2));

    ASSERT_TRUE(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    ASSERT_TRUE(lookup);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(lookup), feature1Copy));
    LDJSONRCRelease(lookup);

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_TRUE(lookup);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(lookup), feature2Copy));
    LDJSONRCRelease(lookup);

    LDJSONFree(feature1Copy);
    LDJSONFree(feature2Copy);
}

TEST_P(CommonStoreFixture, UpsertFeatureNotAnObject) {
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = LDNewNumber(52));

    ASSERT_FALSE(LDStoreUpsert(store, LD_FLAG, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_P(CommonStoreFixture, UpsertFeatureMissingVersion) {
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "version");

    ASSERT_FALSE(LDStoreUpsert(store, LD_FLAG, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_P(CommonStoreFixture, UpsertFeatureVersionNotNumber) {
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "version");
    LDObjectSetKey(feature, "version", LDNewText("abc"));

    ASSERT_FALSE(LDStoreUpsert(store, LD_FLAG, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_P(CommonStoreFixture, UpsertFeatureMissingKey) {
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "key");

    ASSERT_FALSE(LDStoreUpsert(store, LD_FLAG, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_P(CommonStoreFixture, UpsertFeatureKeyNotText) {
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "key");
    LDObjectSetKey(feature, "key", LDNewNumber(52));

    ASSERT_FALSE(LDStoreUpsert(store, LD_FLAG, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_P(CommonStoreFixture, UpsertFeatureDeletedNotBool) {
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    ASSERT_TRUE(LDStoreInitEmpty(store));

    ASSERT_TRUE(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "deleted");
    LDObjectSetKey(feature, "deleted", LDNewNumber(52));

    ASSERT_FALSE(LDStoreUpsert(store, LD_FLAG, feature));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    ASSERT_FALSE(lookup);
}

// Any Redis specific tests should be here.
#ifdef TEST_REDIS

static struct LDStore *concurrentStore;
static struct LDJSON * concurrentFlagCopy;

static void
hook()
{
    struct LDJSON *concurrentFlag;
    LD_ASSERT(
            concurrentFlag =
                    makeMinimalFlag("abc", 70, LDBooleanTrue, LDBooleanFalse));
    LD_ASSERT(concurrentFlagCopy = LDJSONDuplicate(concurrentFlag));
    LDStoreUpsert(concurrentStore, LD_FLAG, concurrentFlag);
}

TEST_P(CommonStoreFixture, WriteConflict) {
    struct LDJSON *              flag;
    struct LDJSONRC *            lookup;
    struct LDStoreInterface *    interface;
    struct LDRedisConfig *       redisConfig;
    struct LDStoreCollectionItem collectionItem;
    struct LDConfig *            config;
    char *                       serialized;

    flushDB();

    ASSERT_TRUE(config = LDConfigNew(""));
    ASSERT_TRUE(redisConfig = LDRedisConfigNew());
    ASSERT_TRUE(interface = LDStoreInterfaceRedisNew(redisConfig));
    LDConfigSetFeatureStoreBackend(config, interface);
    ASSERT_TRUE(concurrentStore = LDStoreNew(config));
    config->storeBackend = NULL;
    ASSERT_TRUE(!LDStoreInitialized(concurrentStore));

    ASSERT_TRUE(flag = makeMinimalFlag("abc", 50, LDBooleanTrue, LDBooleanFalse));

    ASSERT_TRUE(LDStoreInitEmpty(concurrentStore));
    LDStoreUpsert(concurrentStore, LD_FLAG, flag);

    ASSERT_TRUE(flag = makeMinimalFlag("abc", 60, LDBooleanTrue, LDBooleanFalse));
    ASSERT_TRUE(serialized = LDJSONSerialize(flag));
    ASSERT_TRUE(collectionItem.buffer = (void *)serialized);
    ASSERT_TRUE(collectionItem.bufferSize = strlen(serialized));
    collectionItem.version = 60;
    LDJSONFree(flag);
    storeUpsertInternal(
            interface->context, "features", &collectionItem, "abc", hook);
    LDFree(serialized);

    ASSERT_TRUE(LDStoreGet(concurrentStore, LD_FLAG, "abc", &lookup));
    ASSERT_TRUE(lookup);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(lookup), concurrentFlagCopy));

    LDJSONFree(concurrentFlagCopy);
    LDJSONRCRelease(lookup);

    LDStoreDestroy(concurrentStore);
    LDConfigFree(config);
}

#endif
