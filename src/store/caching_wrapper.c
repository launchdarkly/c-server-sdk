#include <stdio.h>
#include <string.h>

#include "launchdarkly/memory.h"
#include "launchdarkly/store.h"

#include "assertion.h"
#include "caching_wrapper.h"
#include "concurrency.h"
#include "internal_store.h"
#include "utility.h"
#include "memory_cache.h"
#include "store_utilities.h"
#include "persistent_store_collection.h"

#define PS_CONTEXT(ptr) (struct PersistentStoreContext *) ptr;

typedef enum {
    LD_EXP_ERROR,
    LD_EXP_CURRENT,
    LD_EXP_EXPIRED
} ExpirationState;

/* The motivation behind $initChecked is to reduce queries to the store backend
 * while it remains uninitialized. Otherwise, an application evaluating many flags would end up
 * hitting the backend many times to check if it is initialized.
 *
 * This key represents a sentinel value that may be stored in the cache.
 * If it is present in the cache, and unexpired, then the store is considered uninitialized (even if it is at that moment.)
 *
 * Once the value reaches expiry, the store can be queried again for its true initialization status.
 * At that point $initChecked can be deleted from the cache if the store is initialized, otherwise it can be re-inserted
 * with a new expiry in the future.
 * */
static const char *const INIT_CHECKED_KEY = "$initChecked";

/* region Structure definitions */

struct PersistentStoreContext {
    struct LDStoreInterface *persistentStore;
    struct LDMemoryContext *cache;
    unsigned int cacheMilliseconds;
};

/* endregion */

/* region Forward declarations for internal methods. */

static LDBoolean
memoryInit(struct PersistentStoreContext *const context, struct LDJSON *const newData);

static LDBoolean
upsertMemory(
        struct PersistentStoreContext *const store,
        enum FeatureKind kind,
        struct LDJSONRC *const replacement);

static ExpirationState
getExpirationState(const struct PersistentStoreContext *const store, const struct LDCacheItem *const item);

static LDBoolean
getSingleItemFromBackend(
        struct PersistentStoreContext *const store,
        enum FeatureKind kind,
        const char *const key,
        struct LDJSONRC **const result);

static LDBoolean
getAllItemsFromBackend(
        struct PersistentStoreContext *const   store,
        enum FeatureKind kind,
        struct LDJSONRC **const result);

static LDBoolean
cacheContainsUnexpired(struct PersistentStoreContext const *const store, const char *key);

static LDBoolean
quickCheckInitialization(struct PersistentStoreContext const *const store, LDBoolean *out_status);

static void
queryAndUpdateInitialization(struct PersistentStoreContext *const store, LDBoolean *out_initialized);

static void
memoryDestructor(struct LDMemoryContext *const context);

/* endregion */

/* region Cache Keys */

static char *
featureStoreAllCacheKey(const char* kind)
{
    char *result = NULL;
    int bytes;
    int status;

    LD_ASSERT(kind);

    bytes = 4 + strlen(kind) + 1;

    if (!(result = LDAlloc(bytes))) {
        return NULL;
    }

    status = snprintf(result, bytes, "all:%s", kind);
    LD_ASSERT(status == bytes - 1);

    return result;
}

static char *
featureStoreCacheKey(const char* kind, const char *const key)
{
    char *result = NULL;
    int   bytes;
    int   status;

    LD_ASSERT(kind);
    LD_ASSERT(key);

    bytes = strlen(kind) + 1 + strlen(key) + 1;

    result = LDAlloc(bytes);
    LD_ASSERT(result);

    status = snprintf(result, bytes, "%s:%s", kind, key);
    LD_ASSERT(status == bytes - 1);

    return result;
}

/* endregion */

/* region LDInternalStoreInterface Implementation */

static LDBoolean
storeInit(void *const contextRaw, struct LDJSON *const newData) {
    struct PersistentStoreContext *psCtx = NULL;
    struct LDStoreCollectionState *collections = NULL;
    unsigned int collectionCount = 0;
    LDBoolean success = LDBooleanFalse;

    LD_ASSERT(contextRaw);

    psCtx = PS_CONTEXT(contextRaw);

    LD_ASSERT(psCtx->persistentStore);
    LD_ASSERT(psCtx->persistentStore->init);

    LDi_makeCollections(newData, &collections, &collectionCount);

    if (psCtx->persistentStore->init(psCtx->persistentStore->context, collections, collectionCount)) {
        success = LDBooleanTrue;
    }

    LDi_freeCollections(collections, collectionCount);
    collections = NULL;

    if (success) {
        success = memoryInit(psCtx, newData);
    }

    LDJSONFree(newData);
    return success;
}

static LDBoolean
storeGet(
        void *const contextRaw,
        enum FeatureKind kind,
        const char *const key,
        struct LDJSONRC **const result)
{
    struct PersistentStoreContext *psCtx = NULL;
    struct LDCacheItem *item = NULL;
    char* cacheKey = NULL;

    LD_ASSERT(key);
    LD_ASSERT(result);
    LD_ASSERT(contextRaw);

    cacheKey = featureStoreCacheKey(featureKindToString(kind), key);

    *result = NULL;

    psCtx = PS_CONTEXT(contextRaw);
    LD_ASSERT(psCtx->cache);

    LDi_rwlock_rdlock(&psCtx->cache->lock);

    LDi_memoryCacheGetCollectionItem(psCtx->cache,
                                     cacheKey,
                                     &item);
    LDFree(cacheKey);

    if(item != NULL) {
        ExpirationState state = getExpirationState(psCtx, item);

        switch(state) {
            case LD_EXP_EXPIRED:
                /* The item was expired, so we want to get it from the backend. */
                break;
            case LD_EXP_ERROR:
                /* We encountered an error determining if the item is expired.
                 * Report failure to access the store. */
                LDi_rwlock_rdunlock(&psCtx->cache->lock);
                return LDBooleanFalse;
            case LD_EXP_CURRENT:
                /* The item was current, so we can use it.
                 * If it was deleted, then we just need to say that access was a success.
                 * We don't need to return a result.
                 * If it is current, and not deleted, then we can assign the result. */
                if (LDi_isDataDeleted(LDJSONRCGet(item->feature))) {
                    LDi_rwlock_rdunlock(&psCtx->cache->lock);
                    /* It was deleted, so we don't assign a result. */
                    return LDBooleanTrue;
                } else {
                    LDJSONRCRetain(item->feature);

                    *result = item->feature;

                    LDi_rwlock_rdunlock(&psCtx->cache->lock);

                    return LDBooleanTrue;
                }
        }
    }
    /* The item was not in the cache or it was expired. */
    LDi_rwlock_rdunlock(&psCtx->cache->lock);

    return getSingleItemFromBackend(psCtx, kind, key, result);
}

static LDBoolean
storeAll(
        void *const contextRaw,
        enum FeatureKind kind,
        struct LDJSONRC **const result)
{
    struct PersistentStoreContext *psCtx = NULL;
    struct LDCacheItem *item = NULL;
    char* allCacheKey = NULL;

    LD_ASSERT(result);
    LD_ASSERT(contextRaw);

    allCacheKey = featureStoreAllCacheKey(featureKindToString(kind));

    psCtx = PS_CONTEXT(contextRaw);
    LD_ASSERT(psCtx->cache);

    LDi_rwlock_rdlock(&psCtx->cache->lock);

    LDi_memoryCacheGetCollectionItem(psCtx->cache, allCacheKey, &item);
    LDFree(allCacheKey);

    if(item) {
        ExpirationState state = getExpirationState(psCtx, item);

        switch(state) {
            case LD_EXP_EXPIRED:
                /* The item was expired, so we want to get it from the backend. */
                break;
            case LD_EXP_ERROR:
                LDi_rwlock_rdunlock(&psCtx->cache->lock);
                return LDBooleanFalse;
            case LD_EXP_CURRENT:
                LDJSONRCRetain(item->feature);

                *result = item->feature;

                LDi_rwlock_rdunlock(&psCtx->cache->lock);

                return LDBooleanTrue;
        }
    }

    LDi_rwlock_rdunlock(&psCtx->cache->lock);

    return getAllItemsFromBackend(psCtx, kind, result);
}

static LDBoolean
storeUpsert(
        void *const contextRaw,
        enum FeatureKind kind,
        const char *const key,
        struct LDJSON *const item)
{
    struct PersistentStoreContext *psCtx = NULL;
    struct LDJSONRC *rcItem = NULL;
    char *serialized;
    struct LDStoreCollectionItem collectionItem;
    LDBoolean status = LDBooleanFalse;

    LD_ASSERT(key);
    LD_ASSERT(contextRaw);

    psCtx = PS_CONTEXT(contextRaw);
    LD_ASSERT(psCtx->cache);

    LD_ASSERT(psCtx->persistentStore);
    LD_ASSERT(psCtx->persistentStore->upsert);

    if (!(serialized = LDJSONSerialize(item))) {
        LDJSONFree(item);
        return LDBooleanFalse;
    }

    collectionItem.buffer     = (void *)serialized;
    collectionItem.bufferSize = strlen(serialized);
    collectionItem.version    = LDi_getDataVersion(item);

    status = psCtx->persistentStore->upsert(
            psCtx->persistentStore->context,
            featureKindToString(kind),
            &collectionItem,
            LDi_getDataKey(item));

    if(!status) {
        LDJSONFree(item);
        LDFree(serialized);
        return LDBooleanFalse;
    }

    LDFree(serialized);

    LDi_rwlock_wrlock(&psCtx->cache->lock);
    rcItem = LDJSONRCNew(item);
    LD_ASSERT(rcItem);
    status = upsertMemory(psCtx, kind, rcItem);
    /* We made it, so we need to decrement our usage. */
    LDJSONRCRelease(rcItem);
    LDi_rwlock_wrunlock(&psCtx->cache->lock);

    return status;
}

static LDBoolean
storeInitialized(void *const contextRaw)
{
    struct PersistentStoreContext *psCtx = NULL;
    LDBoolean initialized;
    LDBoolean checkSucceeded;

    LD_ASSERT(contextRaw);

    psCtx = PS_CONTEXT(contextRaw);
    LD_ASSERT(psCtx->cache);

    LDi_rwlock_rdlock(&psCtx->cache->lock);
    checkSucceeded = quickCheckInitialization(psCtx, &initialized);
    LDi_rwlock_rdunlock(&psCtx->cache->lock);

    if (checkSucceeded) {
        return initialized;
    }

    LDi_rwlock_wrlock(&psCtx->cache->lock);
    queryAndUpdateInitialization(psCtx, &initialized);
    LDi_rwlock_wrunlock(&psCtx->cache->lock);

    return initialized;
}

static void
storeDestructor(void *const contextRaw)
{
    struct PersistentStoreContext *psCtx = NULL;
    if(contextRaw) {
        psCtx = PS_CONTEXT(contextRaw);
        memoryDestructor(psCtx->cache);
        if(psCtx->persistentStore) {
            if(psCtx->persistentStore->destructor) {
                psCtx->persistentStore->destructor(psCtx->persistentStore->context);
            }
            LDFree(psCtx->persistentStore);
        }
        LDFree(contextRaw);
    }
}

static void
storeExpireAll(void *const contextRaw) {
    struct PersistentStoreContext *psCtx = NULL;

    psCtx = PS_CONTEXT(contextRaw);
    LD_ASSERT(psCtx);

    LDi_memoryCacheExpireAll(psCtx->cache);
}

/* endregion */

struct LDInternalStoreInterface *
LDStoreCachingWrapperNew(struct LDStoreInterface *persistentStore, unsigned int cacheMilliseconds) {
    struct LDInternalStoreInterface *wrapper = NULL;
    struct PersistentStoreContext *context = NULL;
    struct LDMemoryContext *cache = NULL;

    wrapper = LDAlloc(sizeof(struct LDInternalStoreInterface));
    LD_ASSERT(wrapper);
    memset(wrapper, 0, sizeof *wrapper);


    context = LDAlloc(sizeof (struct PersistentStoreContext));
    LD_ASSERT(context);
    memset(context, 0, sizeof *context);

    cache = LDAlloc(sizeof(struct LDMemoryContext));
    LD_ASSERT(cache);
    memset(cache, 0, sizeof *cache);

    cache->initialized = LDBooleanFalse;
    cache->items = NULL;
    LDi_rwlock_init(&cache->lock);

    context->cache = cache;
    context->cacheMilliseconds = cacheMilliseconds;
    context->persistentStore = persistentStore;

    wrapper->context = context;
    wrapper->init = storeInit;
    wrapper->get = storeGet;
    wrapper->all = storeAll;
    wrapper->upsert = storeUpsert;
    wrapper->initialized = storeInitialized;
    wrapper->destructor = storeDestructor;
    wrapper->ldi_expireAll = storeExpireAll;

    return wrapper;
}

static LDBoolean
memoryInit(struct PersistentStoreContext *const store, struct LDJSON *const sets)
{
    struct LDJSON *dataKindsIter, *dataKindsNext;

    LD_ASSERT(store);
    LD_ASSERT(store->cache);
    LD_ASSERT(sets);
    LD_ASSERT(LDJSONGetType(sets) == LDObject);

    LDi_rwlock_wrlock(&store->cache->lock);

    LDi_memoryCacheFlush(store->cache);

    /* For each data kind (Features/Segments/??). */
    for(dataKindsIter = LDGetIter(sets); dataKindsIter; dataKindsIter = dataKindsNext) {
        struct LDCacheItem *newAllCacheForKind;
        struct LDJSON *itemsIter, *itemsNext, *items;
        const char* kind = LDIterKey(dataKindsIter);

        /* The cache key is owned memory. */
        char* allCacheKeyForKind = featureStoreAllCacheKey(kind);

        /* Need to get the next iter before detaching. */
        dataKindsNext = LDIterNext(dataKindsIter);

        /* Detaching means we now own it. */
        items = LDCollectionDetachIter(sets, dataKindsIter);

        /* We need a duplicate of all the items, to go to the "all" cache for that kind. */
        newAllCacheForKind = LDi_makeCacheItem(allCacheKeyForKind, LDJSONDuplicate(items));
        LD_ASSERT(newAllCacheForKind);
        LDi_addToCache(store->cache, newAllCacheForKind);

        /* For each item of the kind. */
        for (itemsIter = LDGetIter(items); itemsIter; itemsIter = itemsNext) {
            struct LDCacheItem *newItem;
            char* cacheKey = featureStoreCacheKey(kind, LDi_getDataKey(itemsIter));
            /* Get the next item before detaching from the collection. */
            itemsNext = LDIterNext(itemsIter);

            newItem = LDi_makeCacheItem(cacheKey, LDCollectionDetachIter(items, itemsIter));
            LD_ASSERT(newItem);

            LDi_addToCache(store->cache, newItem);

            LDFree(cacheKey);
        }

        LDFree(allCacheKeyForKind);
        LDJSONFree(items);
    }

    LDi_rwlock_wrunlock(&store->cache->lock);

    return LDBooleanTrue;
}

static LDBoolean
upsertMemory(
        struct PersistentStoreContext *const store,
        enum FeatureKind kind,
        struct LDJSONRC *const replacement)
{
    LDBoolean success;
    struct LDCacheItem *currentItem, *replacementItem, *allItems;
    char * cacheKey;
    char * allCacheKey;

    LD_ASSERT(store);
    LD_ASSERT(store->cache);
    LD_ASSERT(replacement);

    success = LDBooleanFalse;
    currentItem = NULL;
    replacementItem = NULL;
    cacheKey = NULL;

    allCacheKey = featureStoreAllCacheKey(featureKindToString(kind));
    LD_ASSERT(allCacheKey);

    cacheKey =
            featureStoreCacheKey(featureKindToString(kind), LDi_getDataKey(LDJSONRCGet(replacement)));
    LD_ASSERT(cacheKey);

    LDi_memoryCacheGetCollectionItem(store->cache, cacheKey, &currentItem);

    if (currentItem) {
        ExpirationState expired;
        struct LDJSON *current;

        current = NULL;

        if ((expired = getExpirationState(store, currentItem)) == LD_EXP_ERROR) {
            goto cleanup;
        }

        current = LDJSONRCGet(currentItem->feature);
        LD_ASSERT(current);

        /* There is an item in the cache, and the item in the cache is the same version or newer. */
        if (expired == LD_EXP_CURRENT && LDi_getDataVersion(current) >=
                                         LDi_getDataVersion(LDJSONRCGet(replacement)))
        {
            success = LDBooleanTrue;

            goto cleanup;
        }
    }

    replacementItem = LDi_makeCacheItemFromRc(cacheKey, replacement);
    LD_ASSERT(replacementItem);

    LDi_memoryCacheGetCollectionItem(store->cache, allCacheKey, &allItems);

    /* There is currently an allItems cache, that is now not valid. */
    if (allItems) {
        /* Note: If an infinite TTL is added, then the "all" items cache should get updated instead. */
        LDi_deleteAndRemoveCacheItem(&store->cache->items, allItems);
    }

    if (currentItem) {
        LDi_deleteAndRemoveCacheItem(&store->cache->items, currentItem);
    }

    LDi_addToCache(store->cache, replacementItem);

    replacementItem = NULL;

    success = LDBooleanTrue;

    cleanup:
    LDFree(allCacheKey);
    LDFree(cacheKey);
    LDi_deleteCacheItem(replacementItem);

    return success;
}

/**
 * Check the expiration state of a cached item.
 * @param[in] store Store containing the timeout information.
 * @param[in] item The item to check the status of.
 * @return LD_EXP_EXPIRED if the item is expired, LD_EXP_CURRENT if the item is not expired, or LD_EXP_ERROR if
 * monotonic time cannot be accessed.
 */
static ExpirationState
getExpirationState(const struct PersistentStoreContext *const store, const struct LDCacheItem *const item)
{
    double now;

    LD_ASSERT(store);
    LD_ASSERT(item);

    if (store->cacheMilliseconds == 0) {
        return LD_EXP_EXPIRED;
    }

    if (LDi_getMonotonicMilliseconds(&now)) {
        if ((now - item->updatedOn) > store->cacheMilliseconds) {
           return LD_EXP_EXPIRED;
        } else {
            return LD_EXP_CURRENT;
        }
    } else {
        return LD_EXP_ERROR;
    }
}

static LDBoolean
getSingleItemFromBackend(
        struct PersistentStoreContext *const   store,
        enum FeatureKind kind,
        const char *const key,
        struct LDJSONRC **const result)
{
    struct LDStoreCollectionItem collectionItem;
    LDBoolean status;

    LD_ASSERT(store);
    LD_ASSERT(key);
    LD_ASSERT(result);

    *result = NULL;
    memset(&collectionItem, 0, sizeof(struct LDStoreCollectionItem));

    LD_ASSERT(store->persistentStore->get);

    if (!store->persistentStore->get(
            store->persistentStore->context, featureKindToString(kind), key, &collectionItem)) {
        return LDBooleanFalse;
    }

    /* If there is no buffer, then that means the item should be treated as deleted. */
    if (collectionItem.buffer) {
        struct LDJSON *deserialized;
        struct LDJSONRC *deserializedRef;

        if (!(deserialized =
                      LDJSONDeserialize((const char *)collectionItem.buffer))) {
            LD_LOG(LD_LOG_ERROR, "getSingleItemFromBackend failed to deserialize JSON");

            LDFree(collectionItem.buffer);

            return LDBooleanFalse;
        }

        LDFree(collectionItem.buffer);

        if (!LDi_validateData(deserialized)) {
            LD_LOG(LD_LOG_ERROR, "getSingleItemFromBackend invalid feature from backend");

            LDJSONFree(deserialized);

            return LDBooleanFalse;
        }

        if (LDi_isDataDeleted(deserialized)) {
            deserializedRef = LDJSONRCNew(deserialized);
            LD_ASSERT(deserializedRef);

            LDi_rwlock_wrlock(&store->cache->lock);
            status = upsertMemory(store, kind, deserializedRef);
            /* We are not returning to the caller, so we don't need the extra ref. */
            LDJSONRCRelease(deserializedRef);
            LDi_rwlock_wrunlock(&store->cache->lock);

            return status;
        } else {
            /* Using an RC here, then adding that RC to the cache, prevents us from needing to duplicate
             * the cJSON value. */
            deserializedRef = LDJSONRCNew(deserialized);
            LD_ASSERT(deserializedRef);

            *result = deserializedRef;

            LDi_rwlock_wrlock(&store->cache->lock);
            status = upsertMemory(store, kind, deserializedRef);
            LDi_rwlock_wrunlock(&store->cache->lock);

            return status;
        }
    } else {
        struct LDJSON *placeholder;
        struct LDJSONRC *placeholderRef;

        LD_ASSERT(store->cache);

        placeholder = LDi_makeDeletedData(key, collectionItem.version);
        LD_ASSERT(placeholder);

        placeholderRef = LDJSONRCNew(placeholder);
        LD_ASSERT(placeholderRef);
        LDi_rwlock_wrlock(&store->cache->lock);
        status = upsertMemory(store, kind, placeholderRef);
        /* We are not returning to the caller, so we don't need the extra ref. */
        LDJSONRCRelease(placeholderRef);
        LDi_rwlock_wrunlock(&store->cache->lock);

        return status;
    }
}

static LDBoolean
getAllItemsFromBackend(
        struct PersistentStoreContext *const store,
        enum FeatureKind kind,
        struct LDJSONRC **const result)
{
    LDBoolean                     success;
    struct LDStoreCollectionItem *collectionItemsFromStore;
    unsigned int itemCount, i;
    struct LDJSON *rawItems;
    struct LDJSONRC *itemsRC;
    char *allCacheKey;
    struct LDCacheItem *allCacheItem;

    LD_ASSERT(store);
    LD_ASSERT(result);

    success = LDBooleanFalse;
    *result = NULL;
    collectionItemsFromStore = NULL;
    itemCount = 0;
    i = 0;
    rawItems = NULL;
    itemsRC = NULL;
    allCacheKey = NULL;
    allCacheItem = NULL;

    LD_ASSERT(store->persistentStore->all);

    if (!store->persistentStore->all(
            store->persistentStore->context, featureKindToString(kind), &collectionItemsFromStore, &itemCount))
    {
        goto cleanup;
    }

    rawItems = LDNewObject();
    LD_ASSERT(rawItems);

    for (i = 0; i < itemCount; i++) {
        const char *   key;
        struct LDJSON *deserialized;

        deserialized = NULL;

        if (!collectionItemsFromStore[i].buffer) {
            continue;
        }

        if (!(deserialized = LDJSONDeserialize(collectionItemsFromStore[i].buffer))) {
            goto cleanup;
        }

        if (!LDi_validateData(deserialized)) {
            LD_LOG(LD_LOG_ERROR, "LDStoreAll invalid feature from backend");

            LDJSONFree(deserialized);

            continue;
        }

        if (LDi_isDataDeleted(deserialized)) {
            LDJSONFree(deserialized);

            continue;
        }

        key = LDGetText(LDObjectLookup(deserialized, "key"));
        LD_ASSERT(key);

        if (!LDObjectSetKey(rawItems, key, deserialized)) {
            goto cleanup;
        }
    }

    LDi_rwlock_wrlock(&store->cache->lock);

    allCacheKey = featureStoreAllCacheKey(featureKindToString(kind));
    LD_ASSERT(allCacheKey);

    itemsRC = LDJSONRCNew(rawItems);
    LD_ASSERT(itemsRC);

    /* Insert the existing RC item into the cache. Allowing it to both be returned, and populate the cache. */
    allCacheItem = LDi_makeCacheItemFromRc(allCacheKey, itemsRC);
    LD_ASSERT(allCacheItem);

    LDi_addToCache(store->cache, allCacheItem);

    LDi_rwlock_wrunlock(&store->cache->lock);

    rawItems = NULL;
    *result = itemsRC;
    success = LDBooleanTrue;

    cleanup:
    LDFree(rawItems);
    LDFree(allCacheKey);

    for (i = 0; i < itemCount; i++) {
        LDFree(collectionItemsFromStore[i].buffer);
    }

    LDFree(collectionItemsFromStore);

    return success;
}

/* For readability within quickCheckInitialization. */

#define INITIALIZED LDBooleanTrue
#define NOT_INITIALIZED LDBooleanFalse

/* Returns true if a value corresponding to a key exists in the cache, and that value has not
 * yet expired.
 * Expects a cache lock to be held.
 * */
static LDBoolean
cacheContainsUnexpired(struct PersistentStoreContext const *const store, const char *key) {
    struct LDCacheItem *item;
    double now;

    /* If the cache was configured to have 0 TTL, then the value is considered expired (hit the database). */
    if (store->cacheMilliseconds == 0) {
        return LDBooleanFalse;
    }

    LDi_memoryCacheGetCollectionItem(store->cache, key, &item);

    if (!item) {
        return LDBooleanFalse;
    }

    if (LDi_getMonotonicMilliseconds(&now)) {
        if ((now - item->updatedOn) > store->cacheMilliseconds) {
            return LDBooleanFalse; /* expired */
        } else {
            return LDBooleanTrue; /* not expired */
        }
    }

    /* safe default if the time check fails: not expired (do not hit database) */
    return LDBooleanTrue;
}

/* Checks if the store is initialized using only read operations.
 * Intended to complete quickly without hitting a database.
 *
 * Initialization status is available in out_status on success.
 *
 * Returns true if the routine succeeded; Otherwise, returns false.
 * Expects a cache lock to be held.
 * */
static LDBoolean
quickCheckInitialization(struct PersistentStoreContext const *const store, LDBoolean *out_status) {

    if (store->cache->initialized) {
        *out_status = INITIALIZED;
        return LDBooleanTrue;
    }

    if (cacheContainsUnexpired(store, INIT_CHECKED_KEY)) {
        *out_status = NOT_INITIALIZED;
        return LDBooleanTrue;
    }

    return LDBooleanFalse;
}

/* Checks if the store is initialized by querying the database.
 * Performs write operations on the PersistentStoreContext's cache structure.
 *
 * Initialization status is available in out_status.
 * Expects a cache write lock to be held.
 * */
static void
queryAndUpdateInitialization(struct PersistentStoreContext *const store, LDBoolean *out_initialized) {
    LDi_findAndRemoveCacheItem(store->cache, INIT_CHECKED_KEY);

    *out_initialized = store->persistentStore->initialized(store->persistentStore->context);

    if (*out_initialized) {
        store->cache->initialized = LDBooleanTrue;
        return;
    }

    /* Add the cache-checked key. We only care about the presence or absence of the key. */
    LDi_addToCache(store->cache, LDi_makeCacheItem(INIT_CHECKED_KEY, NULL));
}

static void
memoryDestructor(struct LDMemoryContext *const context)
{
    LD_ASSERT(context);

    LDi_memoryCacheFlush(context);

    LDi_rwlock_destroy(&context->lock);

    LDFree(context);
}
