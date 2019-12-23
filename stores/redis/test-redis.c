#include <launchdarkly/store/redis.h>

#include "misc.h"
#include "src/redis.h"

#include "util-store.h"
#include "util-flags.h"

static void
flushDB()
{
    redisContext *connection;
    redisReply *reply;

    LD_ASSERT(connection = redisConnect("127.0.0.1", 6379));
    LD_ASSERT(!connection->err);

    LD_ASSERT(reply = redisCommand(connection, "FLUSHDB"));
    freeReplyObject(reply);
    redisFree(connection);
}

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore* store;
    struct LDStoreInterface *interface;
    struct LDRedisConfig* config;

    flushDB();

    LD_ASSERT(config = LDRedisConfigNew())
    LD_ASSERT(interface = LDStoreInterfaceRedisNew(config));
    LD_ASSERT(store = LDStoreNew(interface));
    LD_ASSERT(!LDStoreInitialized(store));

    return store;
}

static struct LDStore *concurrentStore;
static struct LDJSON *concurrentFlagCopy;

static void
hook()
{
    struct LDJSON *concurrentFlag;
    LD_ASSERT(concurrentFlag = makeMinimalFlag("abc", 70, true, false));
    LD_ASSERT(concurrentFlagCopy = LDJSONDuplicate(concurrentFlag));
    LDStoreUpsert(concurrentStore, LD_FLAG, concurrentFlag);
}

static void
testWriteConflict()
{
    struct LDJSON *flag;
    struct LDJSONRC *lookup;
    struct LDStoreInterface *interface;
    struct LDRedisConfig *config;
    struct LDStoreCollectionItem collectionItem;
    char *serialized;

    flushDB();

    LD_ASSERT(config = LDRedisConfigNew())
    LD_ASSERT(interface = LDStoreInterfaceRedisNew(config));
    LD_ASSERT(concurrentStore = LDStoreNew(interface));
    LD_ASSERT(!LDStoreInitialized(concurrentStore));

    LD_ASSERT(flag = makeMinimalFlag("abc", 50, true, false));

    LD_ASSERT(LDStoreInitEmpty(concurrentStore));
    LDStoreUpsert(concurrentStore, LD_FLAG, flag);

    LD_ASSERT(flag = makeMinimalFlag("abc", 60, true, false));
    LD_ASSERT(serialized = LDJSONSerialize(flag));
    LD_ASSERT(collectionItem.buffer = (void *)serialized);
    LD_ASSERT(collectionItem.bufferSize = strlen(serialized));
    collectionItem.version = 60;
    LDJSONFree(flag);
    storeUpsertInternal(interface->context, "features", &collectionItem, "abc",
        hook);
    LDFree(serialized);

    LD_ASSERT(LDStoreGet(concurrentStore, LD_FLAG, "abc", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), concurrentFlagCopy));

    LDJSONFree(concurrentFlagCopy);
    LDJSONRCDecrement(lookup);

    LDStoreDestroy(concurrentStore);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    runSharedStoreTests(prepareEmptyStore);
    testWriteConflict();

    return 0;
}
