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
    struct LDRedisConfig* config;

    flushDB();

    LD_ASSERT(config = LDRedisConfigNew())

    LD_ASSERT(store = LDMakeRedisStore(config));
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

    LD_ASSERT(concurrentStore = prepareEmptyStore());
    LD_ASSERT(flag = makeMinimalFlag("abc", 50, true, false));

    LD_ASSERT(LDStoreInitEmpty(concurrentStore));
    LDStoreUpsert(concurrentStore, LD_FLAG, flag);

    LD_ASSERT(flag = makeMinimalFlag("abc", 60, true, false));
    storeUpsertInternal(concurrentStore->context, "features", flag, hook);

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
