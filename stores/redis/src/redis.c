#include <string.h>

#include <launchdarkly/store/redis.h>

#include "utlist.h"
#include "misc.h"
#include "redis.h"
#include "store.h"

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

    LD_ASSERT_API(config);
    LD_ASSERT_API(host);

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
    LD_ASSERT_API(config);

    config->port = port;

    return true;
}

bool
LDRedisConfigSetPrefix(struct LDRedisConfig *const config,
    const char *const prefix)
{
    char *prefixCopy;

    LD_ASSERT_API(config);
    LD_ASSERT_API(prefix);

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
    LD_ASSERT_API(config);

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

    LDi_mutex_lock(&context->lock);
    while (!connection) {
        if (context->connections) {
            LD_LOG(LD_LOG_TRACE, "using existing redis connection");

            connection = context->connections;

            LL_DELETE(context->connections, context->connections);

            LDi_mutex_unlock(&context->lock);
        } else {
            if (context->count < context->config->poolSize) {
                LD_LOG(LD_LOG_TRACE, "opening new redis connection");

                context->count++; /* pre increment before attempt */

                LDi_mutex_unlock(&context->lock);

                if (!(connection = LDAlloc(sizeof(struct Connection)))) {
                    LD_LOG(LD_LOG_ERROR,
                        "failed to allocate connection pool item");

                    goto error;
                }

                connection->next       = NULL;
                connection->connection = NULL;

                if (!(connection->connection =
                    redisConnect(LDRedisConfigGetHost(context->config),
                    context->config->port)))
                {
                    LD_LOG(LD_LOG_ERROR, "failed to create redis connection");

                    goto error;
                }

                if (connection->connection->err) {
                    LD_LOG(LD_LOG_ERROR, "redis connection had error");

                    goto error;
                }
            } else {
                LD_LOG(LD_LOG_TRACE, "waiting on free connection");

                LDi_cond_wait(&context->condition, &context->lock, 1000 * 10);
            }
        }
    }

    return connection;

  error:
    LDi_mutex_lock(&context->lock);
    context->count--;
    LDi_mutex_unlock(&context->lock);

    if (connection) {
        if (connection->connection) {
            redisFree(connection->connection);
        }

        LDFree(connection);
    }

    return NULL;
}

static void
returnConnection(struct Context *const context,
    struct Connection *const connection)
{
    LD_ASSERT(context);

    if (!connection) {
        return;
    }

    LDi_mutex_lock(&context->lock);

    if (connection->connection->err == 0) {
        LD_LOG(LD_LOG_TRACE, "returning redis connection");

        LL_PREPEND(context->connections, connection);
    } else {
        LD_LOG(LD_LOG_TRACE, "deleting failed redis context");

        redisFree(connection->connection);

        context->count--;

        LDFree(connection);
    }

    LDi_mutex_unlock(&context->lock);

    LDi_cond_signal(&context->condition);
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
storeInit(void *const contextRaw,
    const struct LDStoreCollectionState *collections,
    const unsigned int collectionCount)
{
    struct Context *context;
    redisReply *reply;
    struct Connection *connection;
    bool success;
    unsigned int x;

    LD_LOG(LD_LOG_TRACE, "redis storeInit");

    LD_ASSERT(contextRaw);
    LD_ASSERT(collections || collectionCount == 0);

    connection = NULL;
    context    = (struct Context *)contextRaw;
    reply      = NULL;
    success    = false;

    if (!(connection = borrowConnection(context))) {
        goto cleanup;
    }

    reply = redisCommand(connection->connection, "MULTI");

    if (!redisCheckStatus(reply, "OK")) {
        goto cleanup;
    }

    resetReply(&reply);

    for (x = 0; x < collectionCount; x++) {
        const struct LDStoreCollectionState *collection;
        unsigned int y;

        collection = &(collections[x]);

        reply = redisCommand(connection->connection, "DEL %s:%s",
            LDRedisConfigGetPrefix(context->config), collection->kind);

        if (!redisCheckStatus(reply, "QUEUED")) {
            goto cleanup;
        }

        resetReply(&reply);

        for (y = 0; y < collection->itemCount; y++) {
            struct LDStoreCollectionStateItem *item;

            item = &(collection->items[y]);

            reply = redisCommand(connection->connection, "HSET %s:%s %s %s",
                LDRedisConfigGetPrefix(context->config), collection->kind,
                item->key, item->item.buffer);

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
    resetReply(&reply);

    returnConnection(context, connection);

    return success;
}

static bool
storeGet(void *const contextRaw, const char *const kind,
    const char *const key, struct LDStoreCollectionItem *const result)
{
    struct Context *context;
    struct Connection *connection;
    struct LDJSON *feature;
    redisReply *reply;
    bool success;

    LD_LOG(LD_LOG_TRACE, "redis storeGet");

    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(key);
    LD_ASSERT(result);

    context    = (struct Context *)contextRaw;
    connection = NULL;
    feature    = NULL;
    reply      = NULL;
    success    = false;

    if (!(connection = borrowConnection(context))) {
        goto cleanup;
    }

    reply = redisCommand(connection->connection, "HGET %s:%s %s",
        LDRedisConfigGetPrefix(context->config), kind, key);

    if (!reply) {
        goto cleanup;
    } else if (reply->type == REDIS_REPLY_NIL) {
        result->buffer = NULL;

        success = true;

        goto cleanup;
    } else if (!redisCheckReply(reply, REDIS_REPLY_STRING)) {
        goto cleanup;
    } else if (!(feature = LDJSONDeserialize(reply->str))) {
        goto cleanup;
    } else if (!(result->buffer = LDStrDup(reply->str))) {
        goto cleanup;
    }

    if (result->buffer) {
        result->bufferSize = strlen(reply->str);
        result->version    = LDi_getFeatureVersion(feature);
    } else {
        result->bufferSize = 0;
        result->version    = 0;
    }

    success = true;

  cleanup:
    LDJSONFree(feature);

    resetReply(&reply);

    returnConnection(context, connection);

    return success;
}

static bool
storeAll(void *const contextRaw, const char *const kind,
    struct LDStoreCollectionItem **const result,
    unsigned int *const resultCount)
{
    struct Context *context;
    redisReply *reply;
    struct Connection *connection;
    bool success;
    unsigned int i;
    struct LDStoreCollectionItem *collection, *collectionIter;
    size_t resultBytes;
    struct LDJSON *feature;

    LD_LOG(LD_LOG_TRACE, "redis storeAll");

    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(result);

    context      = (struct Context *)contextRaw;
    reply        = NULL;
    success      = false;
    *result      = NULL;
    *resultCount = 0;
    collection   = NULL;
    resultBytes  = 0;
    feature      = NULL;

    if (!(connection = borrowConnection(context))) {
        goto cleanup;
    }

    reply = redisCommand(connection->connection, "HGETALL %s:%s",
        LDRedisConfigGetPrefix(context->config), kind);

    if (reply->type == REDIS_REPLY_NIL) {
        success = true;

        goto cleanup;
    }

    if (!redisCheckReply(reply, REDIS_REPLY_ARRAY)) {
        goto cleanup;
    }

    *resultCount = reply->elements / 2;
    resultBytes = sizeof(struct LDStoreCollectionItem) * (*resultCount);

    if (!(collection = (struct LDStoreCollectionItem *)LDAlloc(resultBytes))) {
        LD_LOG(LD_LOG_ERROR, "LDAlloc failed");

        goto cleanup;
    }

    memset(collection, 0, resultBytes);

    collectionIter = collection;

    for (i = 0; i < reply->elements; i++) {
        const char *raw;
        /* skip name field */
        i++;

        if (!redisCheckReply(reply->element[i], REDIS_REPLY_STRING)) {
            LD_LOG(LD_LOG_ERROR, "not a string");

            goto cleanup;
        }

        raw = reply->element[i]->str;
        LD_ASSERT(raw);

        if (!(feature = LDJSONDeserialize(raw))) {
            goto cleanup;
        }

        if (!(collectionIter->buffer = LDStrDup(raw))) {
            goto cleanup;
        }

        collectionIter->bufferSize = strlen(raw);
        collectionIter->version    = LDi_getFeatureVersion(feature);

        LDJSONFree(feature);
        feature = NULL;

        collectionIter++;
    }

    *result = collection;
    success = true;

  cleanup:
    LDJSONFree(feature);

    resetReply(&reply);
    returnConnection(context, connection);

    if (!success) {
        for (i = 0; i < *resultCount; i++) {
            LDFree(collection[i].buffer);
        }
        LDFree(collection);
    }

    return success;
}

bool
storeUpsertInternal(void *const contextRaw, const char *const kind,
    const struct LDStoreCollectionItem *const feature,
    const char *const featureKey,
    void (*const hook)())
{
    struct Context *context;
    redisReply *reply;
    struct LDJSON *existing;
    struct Connection *connection;
    char *serialized;
    bool success;

    LD_LOG(LD_LOG_TRACE, "redis storeUpsertInternal");

    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(feature);
    LD_ASSERT(featureKey);

    context    = (struct Context *)contextRaw;
    reply      = NULL;
    serialized = NULL;
    existing   = NULL;
    connection = NULL;
    success    = false;

    if (!(connection = borrowConnection(context))) {
        goto cleanup;
    }

    while (true) {
        reply = redisCommand(connection->connection, "WATCH %s:%s",
            LDRedisConfigGetPrefix(context->config), kind);

        if (!redisCheckStatus(reply, "OK")) {
            goto cleanup;
        }

        resetReply(&reply);

        reply = redisCommand(connection->connection, "HGET %s:%s %s",
            LDRedisConfigGetPrefix(context->config), kind, featureKey);

        if (!reply) {
            goto cleanup;
        } else if (reply->type == REDIS_REPLY_NIL) {
            /* does not exist */
        } else if (!redisCheckReply(reply, REDIS_REPLY_STRING)) {
            goto cleanup;
        } else if ((existing = LDJSONDeserialize(reply->str))) {
            if (LDi_isFeatureDeleted(existing)) {
                LDJSONFree(existing);

                existing = NULL;
            }
        } else {
            goto cleanup;
        }

        resetReply(&reply);

        if (existing && LDi_getFeatureVersion(existing) >=
            feature->version)
        {
            success = true;

            goto cleanup;
        }

        LDJSONFree(existing);
        existing = NULL;

        if (feature->buffer) {
            serialized = feature->buffer;
        } else {
            struct LDJSON *placeholder;

            if (!(placeholder =
                LDi_makeDeleted(featureKey, feature->version)))
            {
                goto cleanup;
            }

            serialized = LDJSONSerialize(placeholder);

            LDJSONFree(placeholder);

            if (!serialized) {
                goto cleanup;
            }
        }

        if (hook) {
            hook();
        }

        reply = redisCommand(connection->connection, "MULTI");

        if (!redisCheckStatus(reply, "OK")) {
            LD_LOG(LD_LOG_ERROR, "Redis MULTI failed");

            goto cleanup;
        }

        resetReply(&reply);

        reply = redisCommand(connection->connection, "HSET %s:%s %s %s",
            LDRedisConfigGetPrefix(context->config), kind, featureKey,
            serialized);

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

                resetReply(&reply);
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
    if ((feature && (serialized != feature->buffer)) || !feature) {
        LDFree(serialized);
    }

    LDJSONFree(existing);

    resetReply(&reply);

    returnConnection(context, connection);

    return success;
}

static bool
storeUpsert(void *const contextRaw, const char *const kind,
    const struct LDStoreCollectionItem *const feature,
    const char *const featureKey)
{
    LD_ASSERT(contextRaw);
    LD_ASSERT(kind);
    LD_ASSERT(feature);
    LD_ASSERT(featureKey);

    return storeUpsertInternal(contextRaw, kind, feature, featureKey, NULL);
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

    context    = (struct Context *)contextRaw;
    connection = NULL;
    reply      = NULL;

    if (!(connection = borrowConnection(context))) {
        return false;
    }

    reply = redisCommand(connection->connection, "EXISTS %s:%s",
        LDRedisConfigGetPrefix(context->config), initedKey);

    initialized = !redisCheckReply(reply, REDIS_REPLY_INTEGER)
        || reply->integer;

    resetReply(&reply);

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

            tmp = context->connections;
            LD_ASSERT(tmp);

            redisFree(tmp->connection);

            LL_DELETE(context->connections, tmp);

            LDFree(tmp);
        }

        LDi_mutex_destroy(&context->lock);
        LDi_cond_destroy(&context->condition);

        LDRedisConfigFree(context->config);

        LDFree(context);
    }
}

struct LDStoreInterface *
LDStoreInterfaceRedisNew(struct LDRedisConfig *const config)
{
    struct LDStoreInterface *handle;
    struct Context *context;

    LD_ASSERT_API(config);

    handle  = NULL;
    context = NULL;

    if (!(context = (struct Context *)LDAlloc(sizeof(struct Context)))) {
        goto error;
    }

    if (!(handle =
        (struct LDStoreInterface *)LDAlloc(sizeof(struct LDStoreInterface))))
    {
        goto error;
    }

    context->count       = 0;
    context->connections = NULL;
    context->config      = config;

    LDi_mutex_init(&context->lock);
    LDi_cond_init(&context->condition);

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
