#include <uthash.h>

#include "memory_cache.h"
#include "ldjsonrc.h"
#include "assertion.h"
#include "utility.h"

void
LDi_deleteAndRemoveCacheItem(struct LDCacheItem **const collection, struct LDCacheItem *const item)
{
    if (collection && item) {
        LDFree(item->key);
        LDJSONRCRelease(item->feature);
        HASH_DEL(*collection, item);
        LDFree(item);
    }
}

void
LDi_findAndRemoveCacheItem(struct LDMemoryContext * cache, const char *key) {
    struct LDCacheItem *item = NULL;
    LDi_memoryCacheGetCollectionItem(cache, key, &item);
    if (item) {
        LDi_deleteAndRemoveCacheItem(&cache->items, item);
    }
}

void
LDi_memoryCacheFlush(struct LDMemoryContext *const context)
{
    struct LDCacheItem *item, *itemTmp;

    LD_ASSERT(context);

    item    = NULL;
    itemTmp = NULL;

    HASH_ITER(hh, context->items, item, itemTmp)
    {
        LDi_deleteAndRemoveCacheItem(&context->items, item);
    }

    context->items = NULL;
}

void
LDi_memoryCacheGetCollectionItem(
        struct LDMemoryContext *const context,
        const char *const cacheKey,
        struct LDCacheItem **result)
{
    struct LDCacheItem *current;

    LD_ASSERT(context);
    LD_ASSERT(cacheKey);
    LD_ASSERT(result);

    *result  = NULL;

    HASH_FIND_STR(context->items, cacheKey, current);

    *result = current;
}

struct LDCacheItem *
LDi_makeCacheItemFromRc(const char *const key, struct LDJSONRC *valueRC) {
    char *            keyDupe;
    struct LDCacheItem *item;

    LD_ASSERT(key);
    LD_ASSERT(valueRC);

    keyDupe = NULL;
    item    = NULL;

    if (!(keyDupe = LDStrDup(key))) {
        goto error;
    }

    if (!(item = (struct LDCacheItem *)LDAlloc(sizeof(struct LDCacheItem)))) {
        goto error;
    }

    memset(item, 0, sizeof(struct LDCacheItem));

    if (!LDi_getMonotonicMilliseconds(&item->updatedOn)) {
        goto error;
    }

    item->key     = keyDupe;
    item->feature = valueRC;

    /* The RC existed and we are a new reference to it. */
    LDJSONRCRetain(item->feature);

    return item;

    error:
    LDFree(item);
    LDFree(keyDupe);

    return NULL;
}

struct LDCacheItem *
LDi_makeCacheItem(const char *const key, struct LDJSON *value)
{
    char *            keyDupe;
    struct LDCacheItem *item;
    struct LDJSONRC * valueRC;

    LD_ASSERT(key);

    keyDupe = NULL;
    item    = NULL;
    valueRC = NULL;

    if (!(keyDupe = LDStrDup(key))) {
        goto error;
    }

    if (value) {
        if (!(valueRC = LDJSONRCNew(value))) {
            goto error;
        }

        value = NULL;
    }

    if (!(item = (struct LDCacheItem *)LDAlloc(sizeof(struct LDCacheItem)))) {
        goto error;
    }

    memset(item, 0, sizeof(struct LDCacheItem));

    if (!LDi_getMonotonicMilliseconds(&item->updatedOn)) {
        goto error;
    }

    item->key     = keyDupe;
    item->feature = valueRC;

    return item;

    error:
    LDFree(item);
    LDFree(keyDupe);
    LDJSONFree(value);

    return NULL;
}

void
LDi_addToCache(struct LDMemoryContext *const context, struct LDCacheItem * item)
{
    HASH_ADD_KEYPTR(
            hh,
            context->items,
            item->key,
            strlen(item->key),
            item);
}

void
LDi_deleteCacheItem(struct LDCacheItem *const item)
{
    if (item != NULL) {
        LDFree(item->key);
        LDJSONRCRelease(item->feature);
        LDFree(item);
    }
}

/* used for testing */
void
LDi_memoryCacheExpireAll(struct LDMemoryContext *const cache)
{
    struct LDCacheItem *item, *itemTmp;

    LD_ASSERT(cache);

    item    = NULL;
    itemTmp = NULL;

    LD_ASSERT(LDi_rwlock_wrlock(&cache->lock));

    HASH_ITER(hh, cache->items, item, itemTmp)
    {
        item->updatedOn = 0;
    }

    LD_ASSERT(LDi_rwlock_wrunlock(&cache->lock));
}
