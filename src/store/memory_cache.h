#pragma once

#include "uthash.h"

#include "launchdarkly/api.h"

#include "concurrency.h"

struct LDCacheItem
{
    char *key;
    struct LDJSONRC *feature;
    UT_hash_handle hh;
    /* monotonic milliseconds */
    double updatedOn;
};

struct LDMemoryContext
{
    LDBoolean initialized;
    /* ut hash table */
    struct LDCacheItem *items;
    ld_rwlock_t lock;
};

void
LDi_deleteAndRemoveCacheItem(
        struct LDCacheItem **const collection, struct LDCacheItem *const item);

void
LDi_memoryCacheFlush(struct LDMemoryContext *const context);

struct LDCacheItem *
LDi_makeCacheItem(const char *const key, struct LDJSON *value);

struct LDCacheItem *
LDi_makeCacheItemFromRc(const char *const key, struct LDJSONRC *valueRC);

void
LDi_addToCache(struct LDMemoryContext *const context, struct LDCacheItem * item);

void
LDi_memoryCacheGetCollectionItem(
        struct LDMemoryContext *const context,
        const char *const cacheKey,
        struct LDCacheItem **result);

void
LDi_deleteCacheItem(struct LDCacheItem *const item);

void
LDi_findAndRemoveCacheItem(struct LDMemoryContext * cache, const char *key);

/* used for testing */
void
LDi_memoryCacheExpireAll(struct LDMemoryContext *const cache);
