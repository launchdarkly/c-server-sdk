#include <string.h>

#include <launchdarkly/api.h>
#include <launchdarkly/store/redis.h>

#include "assertion.h"
#include "src/redis.h"
#include "utility.h"

#include "test-utils/flags.h"
#include "test-utils/store.h"

static void
flushDB()
{
    redisContext *connection;
    redisReply *  reply;

    LD_ASSERT(connection = redisConnect("127.0.0.1", 6379));
    LD_ASSERT(!connection->err);

    LD_ASSERT(reply = redisCommand(connection, "FLUSHDB"));
    freeReplyObject(reply);
    redisFree(connection);
}

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore *         store;
    struct LDStoreInterface *interface;
    struct LDRedisConfig *   redisConfig;
    struct LDConfig *        config;

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

static void
testWriteConflict()
{
    struct LDJSON *              flag;
    struct LDJSONRC *            lookup;
    struct LDStoreInterface *    interface;
    struct LDRedisConfig *       redisConfig;
    struct LDStoreCollectionItem collectionItem;
    struct LDConfig *            config;
    char *                       serialized;

    flushDB();

    LD_ASSERT(config = LDConfigNew(""));
    LD_ASSERT(redisConfig = LDRedisConfigNew())
    LD_ASSERT(interface = LDStoreInterfaceRedisNew(redisConfig));
    LDConfigSetFeatureStoreBackend(config, interface);
    LD_ASSERT(concurrentStore = LDStoreNew(config));
    config->storeBackend = NULL;
    LD_ASSERT(!LDStoreInitialized(concurrentStore));

    LD_ASSERT(flag = makeMinimalFlag("abc", 50, LDBooleanTrue, LDBooleanFalse));

    LD_ASSERT(LDStoreInitEmpty(concurrentStore));
    LDStoreUpsert(concurrentStore, LD_FLAG, flag);

    LD_ASSERT(flag = makeMinimalFlag("abc", 60, LDBooleanTrue, LDBooleanFalse));
    LD_ASSERT(serialized = LDJSONSerialize(flag));
    LD_ASSERT(collectionItem.buffer = (void *)serialized);
    LD_ASSERT(collectionItem.bufferSize = strlen(serialized));
    collectionItem.version = 60;
    LDJSONFree(flag);
    storeUpsertInternal(
        interface->context, "features", &collectionItem, "abc", hook);
    LDFree(serialized);

    LD_ASSERT(LDStoreGet(concurrentStore, LD_FLAG, "abc", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), concurrentFlagCopy));

    LDJSONFree(concurrentFlagCopy);
    LDJSONRCDecrement(lookup);

    LDStoreDestroy(concurrentStore);
    LDConfigFree(config);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    runSharedStoreTests(prepareEmptyStore);
    testWriteConflict();

    return 0;
}
