#include <string.h>

#include <launchdarkly/store/redis.h>

#include "utlist.h"
#include "misc.h"
#include "redis.h"

static const char *const defaultHost   = "127.0.0.1";
static const char *const defaultPrefix = "launchdarkly";
static const char *const initedKey     = "$inited";

struct LDRedisConfig {
    char *host;
    uint16_t port;
    unsigned int poolSize;
    char *prefix;
};

static const char *
LDRedisConfigGetPrefix(const struct LDRedisConfig *const config)
{
    LD_ASSERT(config);

    if (config->prefix) {
        return config->prefix;
    } else {
        return defaultPrefix;
    }
}

static const char *
LDRedisConfigGetHost(const struct LDRedisConfig *const config)
{
    LD_ASSERT(config);

    if (config->host) {
        return config->host;
    } else {
        return defaultHost;
    }
}

struct LDRedisConfig *
LDRedisConfigNew()
{
    struct LDRedisConfig *config;

    if (!(config =
        (struct LDRedisConfig *)LDAlloc(sizeof(struct LDRedisConfig))))
    {
        return NULL;
    }

    config->host     = NULL;
    config->port     = 6379;
    config->poolSize = 10;
    config->prefix   = NULL;

    return config;
}

bool
LDRedisConfigSetHost(struct LDRedisConfig *const config, const char *const host)
{
    char *hostCopy;

    LD_ASSERT(config);
    LD_ASSERT(host);

    if (!(hostCopy = LDStrDup(host))) {
        return false;
    }

    LDFree(config->host);

    config->host = hostCopy;

    return true;
}

bool
LDRedisConfigSetPort(struct LDRedisConfig *const config, const uint16_t port)
{
    LD_ASSERT(config);

    config->port = port;

    return true;
}

bool
LDRedisConfigSetPrefix(struct LDRedisConfig *const config,
    const char *const prefix)
{
    char *prefixCopy;

    LD_ASSERT(config);
    LD_ASSERT(prefix);

    if (!(prefixCopy = LDStrDup(prefix))) {
        return false;
    }

    LDFree(config->prefix);

    config->prefix = prefixCopy;

    return true;
}

bool
LDRedisConfigSetPoolSize(struct LDRedisConfig *const config,
    const unsigned int poolSize)
{
    LD_ASSERT(config);

    config->poolSize = poolSize;

    return true;
}

void
LDRedisConfigFree(struct LDRedisConfig *const config)
{
    if (config) {
        LDFree(config->host);
        LDFree(config->prefix);

        LDFree(config);
    }
}

struct Context {
    struct Connection *connections;
    unsigned int count;
    ld_mutex_t lock;
    struct LDRedisConfig *config;
    ld_cond_t condition;
};

struct Connection {
    redisContext *connection;
    struct Connection *next;
};

static struct Connection *
borrowConnection(struct Context *const context)
{
    struct Connection *connection;

    LD_ASSERT(context);

    connection = NULL;

    LD_ASSERT(LDi_mtxlock(&context->lock));
    while (!connection) {
        if (context->connections) {
            LD_LOG(LD_LOG_TRACE, "using existing redis connection");

            connection = context->connections;

            LL_DELETE(context->connections, context->connections);

            LD_ASSERT(LDi_mtxunlock(&context->lock));
        } else {
            if (context->count < context->config->poolSize) {
                LD_LOG(LD_LOG_TRACE, "opening new redis connection");

                context->count++; /* pre increment before attempt */

                LD_ASSERT(LDi_mtxunlock(&context->lock));

                LD_ASSERT(connection = LDAlloc(sizeof(struct Connection)));
                connection->next = NULL;

                LD_ASSERT(connection->connection =
                    redisConnect(LDRedisConfigGetHost(context->config),
                    context->config->port));

                LD_ASSERT(!connection->connection->err);
            } else {
                LD_LOG(LD_LOG_TRACE, "waiting on free connection");

                LDi_condwait(&context->condition, &context->lock, 1000 * 10);
            }
        }
    }

    return connection;
}

static void
returnConnection(struct Context *const context,
    struct Connection *const connection)
{
    LD_ASSERT(context);
    LD_ASSERT(connection);

    LD_ASSERT(LDi_mtxlock(&context->lock));

    if (connection->connection->err == 0) {
        LD_LOG(LD_LOG_TRACE, "returning redis connection");

        LL_PREPEND(context->connections, connection);
    } else {
        LD_LOG(LD_LOG_TRACE, "deleting failed redis context");

        redisFree(connection->connection);

        context->count--;

        LDFree(connection);
    }

    LD_ASSERT(LDi_mtxunlock(&context->lock));

    LDi_condsignal(&context->condition);
}

static const char *
getFeatureKey(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if (!(tmp = LDObjectLookup(feature, "key"))) {
        LD_LOG(LD_LOG_ERROR, "feature missing key");

        return NULL;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "feature key is not a string");

        return NULL;
    }

    return LDGetText(tmp);
}

static unsigned int
getFeatureVersion(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if (!(tmp = LDObjectLookup(feature, "version"))) {
        LD_LOG(LD_LOG_ERROR, "feature missing version");

        return 0;
    }

    if (LDJSONGetType(tmp) != LDNumber) {
        LD_LOG(LD_LOG_ERROR, "feature version is not a number");

        return 0;
    }

    return LDGetNumber(tmp);
}

static bool
isFeatureDeleted(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if (!(tmp = LDObjectLookup(feature, "deleted"))) {
        return false;
    }

    if (LDJSONGetType(tmp) != LDBool) {
        LD_LOG(LD_LOG_ERROR, "feature deletion status is not boolean");

        return false;
    }

    return LDGetBool(tmp);
}

static bool
redisCheckReply(redisReply *const reply, const int expectedStatus)
{
    if (!reply) {
        LD_LOG(LD_LOG_ERROR, "redisReply == NULL");

        return false;
    }

    if (reply->type == REDIS_REPLY_ERROR) {
        LD_LOG(LD_LOG_ERROR, "REDIS_REPLY_ERROR");

        return false;
    }

    return reply->type == expectedStatus;
}

static bool
redisCheckStatus(redisReply *const reply, const char *const expectedStatus)
{
    if (!redisCheckReply(reply, REDIS_REPLY_STATUS)) {
        return false;
    }

    if (strcmp(reply->str, expectedStatus) != 0) {
        LD_LOG(LD_LOG_ERROR, "Redis unexpected status");

        return false;
    }

    return true;
}

static void
resetReply(redisReply **const reply)
{
    freeReplyObject(*reply);

    *reply = NULL;
}

static bool
storeInit(void *const contextRaw, struct LDJSON *const sets)
{
    struct Context *context;
    redisReply *reply;
    struct LDJSON *set, *setItem;
    struct Connection *connection;
    bool success;

    LD_LOG(LD_LOG_TRACE, "redis storeInit");

    LD_ASSERT(contextRaw);
    LD_ASSERT(sets);

    context = (struct Context *)contextRaw;
    reply   = NULL;
    set     = NULL;
    setItem = NULL;
    success = false;

    connection = borrowConnection(context);

    reply = redisCommand(connection->connection, "MULTI");

    if (!redisCheckStatus(reply, "OK")) {
        goto cleanup;
    }

    resetReply(&reply);

    for (set = LDGetIter(sets); set; set = LDIterNext(set)) {
        reply = redisCommand(connection->connection, "DEL %s:%s",
            LDRedisConfigGetPrefix(context->config), LDIterKey(set));

        if (!redisCheckStatus(reply, "QUEUED")) {
            goto cleanup;
        }

        resetReply(&reply);

        for (setItem = LDGetIter(set); setItem; setItem = LDIterNext(setItem)) {
            char *serialized;

            if (!(serialized = LDJSONSerialize(setItem))) {
                goto cleanup;
            }

            reply = redisCommand(connection->connection, "HSET %s:%s %s %s",
                LDRedisConfigGetPrefix(context->config), LDIterKey(set),
                LDIterKey(setItem), serialized);

            LDFree(serialized);

            if (!redisCheckStatus(reply, "QUEUED")) {
                goto cleanup;
            }

            resetReply(&reply);
        }
    }

    reply = redisCommand(connection->connection, "SET %s:%s %s",
        LDRedisConfigGetPrefix(context->config), initedKey, "");

    if (!redisCheckStatus(reply, "QUEUED")) {
        goto cleanup;
    }

    resetReply(&reply);

    reply = redisCommand(connection->connection, "EXEC");

    if (!redisCheckReply(reply, REDIS_REPLY_ARRAY)) {
        goto cleanup;
    }

    success = true;

  cleanup:
    freeReplyObject(reply);

    LDJSONFree(sets);

    returnConnection(context, connection);

    return success;
}

static bool
storeGetInternal(void *const contextRaw, const char *const kind,
    const char *const key, struct LDJSONRC **const result,
    struct Connection *const connection)
{
    struct Context *context;
    redisReply *reply;
    struct LDJSON *json;
    bool success;

    LD_LOG(LD_LOG_TRACE, "redis storeGetInternal");

    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(key);
    LD_ASSERT(result);
    LD_ASSERT(connection);

    context = (struct Context *)contextRaw;
    reply   = NULL;
    json    = NULL;
    success = false;
    *result = NULL;

    reply = redisCommand(connection->connection, "HGET %s:%s %s",
        LDRedisConfigGetPrefix(context->config), kind, key);

    if (reply->type == REDIS_REPLY_NIL) {
        success = true;

        goto cleanup;
    }

    if (!redisCheckReply(reply, REDIS_REPLY_STRING)) {
        goto cleanup;
    }

    if (!(json = LDJSONDeserialize(reply->str))) {
        goto cleanup;
    }

    resetReply(&reply);

    if (isFeatureDeleted(json)) {
        success = true;

        LDJSONFree(json);

        goto cleanup;
    }

    if (!(*result = LDJSONRCNew(json))) {
        LDJSONFree(json);

        goto cleanup;
    }

    success = true;

  cleanup:
    freeReplyObject(reply);

    return success;
}

static bool
storeGet(void *const contextRaw, const char *const kind,
    const char *const key, struct LDJSONRC **const result)
{
    struct Context *context;
    struct Connection *connection;
    bool success;

    LD_LOG(LD_LOG_TRACE, "redis storeGet");

    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(key);
    LD_ASSERT(result);

    context = (struct Context *)contextRaw;

    connection = borrowConnection(context);

    success = storeGetInternal(contextRaw, kind, key, result, connection);

    returnConnection(context, connection);

    return success;
}

static bool
storeAll(void *const contextRaw, const char *const kind,
    struct LDJSONRC ***const result)
{
    struct Context *context;
    redisReply *reply;
    struct Connection *connection;
    bool success;
    unsigned int i;
    struct LDJSONRC **collection, **collectionIter;

    LD_LOG(LD_LOG_TRACE, "redis storeAll");

    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(result);

    context    = (struct Context *)contextRaw;
    reply      = NULL;
    success    = false;
    *result    = NULL;
    collection = NULL;

    connection = borrowConnection(context);

    reply = redisCommand(connection->connection, "HGETALL %s:%s",
        LDRedisConfigGetPrefix(context->config), kind);

    if (reply->type == REDIS_REPLY_NIL) {
        success = true;

        goto cleanup;
    }

    if (!redisCheckReply(reply, REDIS_REPLY_ARRAY)) {
        goto cleanup;
    }

    if (!(collection = (struct LDJSONRC **)LDAlloc(sizeof(struct LDJSONRC *) *
        ((reply->elements) + 1))))
    {
        LD_LOG(LD_LOG_ERROR, "LDAlloc failed");

        return false;
    }

    collectionIter = collection;

    for (i = 0; i < reply->elements; i++) {
        struct LDJSON *json;

        /* skip name field */
        i++;

        if (!redisCheckReply(reply->element[i], REDIS_REPLY_STRING)) {
            LD_LOG(LD_LOG_ERROR, "not a string");

            goto cleanup;
        }

        if (!(json = LDJSONDeserialize(reply->element[i]->str))) {
            LD_LOG(LD_LOG_ERROR, "deserialization failed");

            goto cleanup;
        }

        if (!isFeatureDeleted(json)) {
            struct LDJSONRC *jsonRC;

            if (!(jsonRC = LDJSONRCNew(json))) {
                LD_LOG(LD_LOG_ERROR, "json RC failed");

                goto cleanup;
            }

            *collectionIter = jsonRC;

            collectionIter++;
        }
    }

    /* null terminate the list */
    *collectionIter = NULL;

    *result = collection;

    success = true;

  cleanup:
    freeReplyObject(reply);

    returnConnection(context, connection);

    return success;
}

bool
storeUpsertInternal(void *const contextRaw, const char *const kind,
    struct LDJSON *const feature, void (*const hook)())
{
    struct Context *context;
    redisReply *reply;
    struct LDJSONRC *existing;
    struct Connection *connection;
    char *serialized;
    bool success;

    LD_LOG(LD_LOG_TRACE, "redis storeUpsertInternal");

    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(feature);

    context    = (struct Context *)contextRaw;
    reply      = NULL;
    serialized = NULL;
    existing   = NULL;
    success    = false;

    connection = borrowConnection(context);

    while (true) {
        reply = redisCommand(connection->connection, "WATCH %s:%s",
            LDRedisConfigGetPrefix(context->config), getFeatureKey(feature));

        if (!redisCheckStatus(reply, "OK")) {
            goto cleanup;
        }

        if (hook) {
            hook();
        }

        resetReply(&reply);

        if (!storeGetInternal(contextRaw, kind, getFeatureKey(feature),
            &existing, connection))
        {
            goto cleanup;
        }

        if (existing && (getFeatureVersion(LDJSONRCGet(existing)) >=
            getFeatureVersion(feature)))
        {
            success = true;

            goto cleanup;
        }

        if (!(serialized = LDJSONSerialize(feature))) {
            goto cleanup;
        }

        reply = redisCommand(connection->connection, "MULTI");

        if (!redisCheckStatus(reply, "OK")) {
            LD_LOG(LD_LOG_ERROR, "Redis MULTI failed");

            goto cleanup;
        }

        resetReply(&reply);

        reply = redisCommand(connection->connection, "HSET %s:%s %s %s",
            LDRedisConfigGetPrefix(context->config), kind,
            getFeatureKey(feature), serialized);

        if (!redisCheckStatus(reply, "QUEUED")) {
            LD_LOG(LD_LOG_ERROR, "Redis expected OK");

            goto cleanup;
        }

        resetReply(&reply);

        reply = redisCommand(connection->connection, "EXEC");

        if (reply) {
            if (reply->type == REDIS_REPLY_ARRAY) {
                break;
            } else if (reply->type == REDIS_REPLY_NIL) {
                LD_LOG(LD_LOG_WARNING, "Redis race detected retrying");

                freeReplyObject(reply);
            } else {
                LD_LOG(LD_LOG_ERROR, "Redis EXEC incorrect type");

                goto cleanup;
            }
        } else {
            LD_LOG(LD_LOG_ERROR, "Redis reply is NULL");

            goto cleanup;
        }
    }

    success = true;

  cleanup:
    LDFree(serialized);
    LDJSONFree(feature);
    LDJSONRCDecrement(existing);

    freeReplyObject(reply);

    returnConnection(context, connection);

    return success;
}

static bool
storeUpsert(void *const contextRaw, const char *const kind,
    struct LDJSON *const feature)
{
    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(feature);

    return storeUpsertInternal(contextRaw, kind, feature, NULL);
}

static bool
storeInitialized(void *const contextRaw)
{
    struct Context *context;
    redisReply *reply;
    struct Connection *connection;
    bool initialized;

    LD_LOG(LD_LOG_TRACE, "redis storeInitialized");

    LD_ASSERT(contextRaw);

    context = (struct Context *)contextRaw;
    reply   = NULL;

    connection = borrowConnection(context);

    reply = redisCommand(connection->connection, "EXISTS %s:%s",
        LDRedisConfigGetPrefix(context->config), initedKey);

    initialized = !redisCheckReply(reply, REDIS_REPLY_INTEGER)
        || reply->integer;

    freeReplyObject(reply);

    returnConnection(context, connection);

    return initialized;
}

static void
storeDestructor(void *const contextRaw)
{
    struct Context *context;

    LD_LOG(LD_LOG_TRACE, "redis storeDestructor");

    context = (struct Context *)contextRaw;

    if (context) {
        while (context->connections) {
            struct Connection *tmp;

            LD_ASSERT(tmp = context->connections);

            redisFree(tmp->connection);

            LL_DELETE(context->connections, tmp);

            LDFree(tmp);
        }

        LD_ASSERT(LDi_mtxdestroy(&context->lock));
        LDi_conddestroy(&context->condition);

        LDRedisConfigFree(context->config);

        LDFree(context);
    }
}

struct LDStore *
LDMakeRedisStore(struct LDRedisConfig *const config)
{
    struct LDStore *handle;
    struct Context *context;

    LD_ASSERT(config);

    handle  = NULL;
    context = NULL;

    if (!(context = (struct Context *)LDAlloc(sizeof(struct Context)))) {
        goto error;
    }

    if (!(handle = (struct LDStore *)LDAlloc(sizeof(struct LDStore)))) {
        goto error;
    }

    context->count       = 0;
    context->connections = NULL;
    context->config      = config;

    LD_ASSERT(LDi_mtxinit(&context->lock));
    LDi_condinit(&context->condition);

    handle->context     = context;
    handle->init        = storeInit;
    handle->get         = storeGet;
    handle->all         = storeAll;
    handle->upsert      = storeUpsert;
    handle->initialized = storeInitialized;
    handle->destructor  = storeDestructor;

    return handle;

  error:
    LDFree(handle);
    LDFree(context);

    return NULL;
}
