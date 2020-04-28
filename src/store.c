#include "uthash.h"

#include <launchdarkly/api.h>

#include "assertion.h"
#include "concurrency.h"
#include "utility.h"
#include "store.h"

/* **** Forward Declarations **** */

struct MemoryContext;
struct CacheItem;

static const char *const LD_SS_FEATURES   = "features";
static const char *const LD_SS_SEGMENTS   = "segments";
static const char *const INIT_CHECKED_KEY = "$initChecked";

static bool memoryInit(struct LDStore *const context,
    struct LDJSON *const sets);

static void memoryDestructor(struct MemoryContext *const context);

static int isExpired(const struct LDStore *const store,
    const struct CacheItem *const item);

/* **** Flag Utilities **** */

bool
LDi_validateFeature(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if (LDJSONGetType(feature) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "feature is not an object");

        return false;
    }

    if (!(tmp = LDObjectLookup(feature, "version"))) {
        LD_LOG(LD_LOG_ERROR, "feature missing version");

        return false;
    }

    if (LDJSONGetType(tmp) != LDNumber) {
        LD_LOG(LD_LOG_ERROR, "feature version is not a number");

        return false;
    }

    if (!(tmp = LDObjectLookup(feature, "key"))) {
        LD_LOG(LD_LOG_ERROR, "feature missing key");

        return false;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "feature key is not a string");

        return false;
    }

    if ((tmp = LDObjectLookup(feature, "deleted"))) {
        if (LDJSONGetType(tmp) != LDBool) {
            LD_LOG(LD_LOG_ERROR, "feature deleted field is not boolean");

            return false;
        }
    }

    return true;
}

unsigned int
LDi_getFeatureVersion(const struct LDJSON *const feature)
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

static const char *
LDi_getFeatureKeyTrusted(const struct LDJSON *const feature)
{
    LD_ASSERT(feature);

    return LDGetText(LDObjectLookup(feature, "key"));
}

unsigned int
LDi_getFeatureVersionTrusted(const struct LDJSON *const feature)
{
    LD_ASSERT(feature);

    return LDGetNumber(LDObjectLookup(feature, "version"));
}

static const char *
featureKindToString(const enum FeatureKind kind)
{
    switch (kind) {
        case LD_FLAG:    return LD_SS_FEATURES;
        case LD_SEGMENT: return LD_SS_SEGMENTS;
        default:
            LD_LOG(LD_LOG_FATAL, "invalid feature kind");

            LD_ASSERT(false);
    }
}

bool
LDi_isFeatureDeleted(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if ((tmp = LDObjectLookup(feature, "deleted"))) {
        if (LDJSONGetType(tmp) != LDBool) {
            LD_LOG(LD_LOG_ERROR, "feature deletion status is not boolean");

            return true;
        }

        return LDGetBool(tmp);
    }

    return false;
}

struct LDJSON *
LDi_makeDeleted(const char *const key, const unsigned int version)
{
    struct LDJSON *placeholder, *tmp;

    if (!(placeholder = LDNewObject())) {
        return NULL;
    }

    if (!(tmp = LDNewText(key))) {
        LDFree(placeholder);

        return NULL;
    }

    if (!LDObjectSetKey(placeholder, "key", tmp)) {
        LDFree(placeholder);
        LDFree(tmp);

        return NULL;
    }

    if (!(tmp = LDNewNumber(version))) {
        LDFree(placeholder);

        return NULL;
    }

    if (!LDObjectSetKey(placeholder, "version", tmp)) {
        LDFree(placeholder);
        LDFree(tmp);

        return NULL;
    }

    if (!(tmp = LDNewBool(true))) {
        LDFree(placeholder);

        return NULL;
    }

    if (!LDObjectSetKey(placeholder, "deleted", tmp)) {
        LDFree(placeholder);
        LDFree(tmp);

        return NULL;
    }

    return placeholder;
}

/* **** LDStore **** */

struct LDStore {
    struct MemoryContext *cache;
    struct LDStoreInterface *backend;
    unsigned int cacheMilliseconds;
};

/* ***** Reference counting **** */

struct LDJSONRC {
    struct LDJSON *value;
    ld_mutex_t lock;
    unsigned int count;
};

struct LDJSONRC *
LDJSONRCNew(struct LDJSON *const json)
{
    struct LDJSONRC *result;

    LD_ASSERT(json);

    if (!(result = (struct LDJSONRC *)LDAlloc(sizeof(struct LDJSONRC)))) {
        return NULL;
    }

    LDi_mutex_init(&result->lock);

    result->value = json;
    result->count = 1;

    return result;
}

void
LDJSONRCIncrement(struct LDJSONRC *const rc)
{
    LD_ASSERT(rc);

    LDi_mutex_lock(&rc->lock);
    rc->count++;
    LDi_mutex_unlock(&rc->lock);
}

static void
destroyJSONRC(struct LDJSONRC *const rc)
{
    if (rc) {
        LDJSONFree(rc->value);
        LDi_mutex_destroy(&rc->lock);
        LDFree(rc);
    }
}

void
LDJSONRCDecrement(struct LDJSONRC *const rc)
{
    if (rc) {
        LDi_mutex_lock(&rc->lock);
        LD_ASSERT(rc->count > 0);
        rc->count--;

        if (rc->count == 0) {
            LDi_mutex_unlock(&rc->lock);

            destroyJSONRC(rc);
        } else {
            LDi_mutex_unlock(&rc->lock);
        }
    }
}

struct LDJSON *
LDJSONRCGet(struct LDJSONRC *const rc)
{
    LD_ASSERT(rc);

    return rc->value;
}

/* **** Memory Implementation **** */

/* Feature Key -> JSON */
struct CacheItem {
    char *key;
    struct LDJSONRC *feature;
    UT_hash_handle hh;
    /* monotonic milliseconds */
    double updatedOn;
};

static void
deleteCacheItem(struct CacheItem *const item)
{
    if (item) {
        LDFree(item->key);
        LDJSONRCDecrement(item->feature);
        LDFree(item);
    }
}

static void
deleteAndRemoveCacheItem(struct CacheItem **const collection,
    struct CacheItem *const item)
{
    if (collection && item) {
        LDFree(item->key);
        LDJSONRCDecrement(item->feature);
        HASH_DEL(*collection, item);
        LDFree(item);
    }
}

static struct CacheItem *
makeCacheItem(const char *const key, struct LDJSON *value)
{
    char *keyDupe;
    struct CacheItem *item;
    struct LDJSONRC *valueRC;

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

    if (!(item = (struct CacheItem *)
        LDAlloc(sizeof(struct CacheItem))))
    {
        goto error;
    }

    memset(item, 0, sizeof(struct CacheItem));

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

struct MemoryContext {
    bool initialized;
    /* ut hash table */
    struct CacheItem *items;
    ld_rwlock_t lock;
};

static char *
featureStoreCacheKey(const char *const kind, const char *const key)
{
    char *result;
    int bytes;
    int status;

    LD_ASSERT(kind);
    LD_ASSERT(key);

    bytes = strlen(kind) + 1 + strlen(key) + 1;

    if (!(result = LDAlloc(bytes))) {
        return NULL;
    }

    status = snprintf(result, bytes, "%s:%s", kind, key);
    LD_ASSERT(status == bytes - 1);

    return result;
}

static char *
featureStoreAllCacheKey(const char *const kind)
{
    char *result;
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

/* expects write lock */
static bool
upsertMemory(struct LDStore *const store, const char *const kind,
    struct LDJSON *replacement)
{
    bool success;
    struct LDJSON *weakReplacementRef;
    struct CacheItem *currentItem, *replacementItem, *allItems;
    char *cacheKey;
    char *allCacheKey;

    LD_ASSERT(store);
    LD_ASSERT(store->cache);
    LD_ASSERT(kind);
    LD_ASSERT(replacement);

    success            = false;
    currentItem        = NULL;
    replacementItem    = NULL;
    cacheKey           = NULL;
    weakReplacementRef = replacement;

    allCacheKey = featureStoreAllCacheKey(kind);
    LD_ASSERT(allCacheKey);

    cacheKey = featureStoreCacheKey(kind,
        LDi_getFeatureKeyTrusted(replacement));
    LD_ASSERT(cacheKey);

    HASH_FIND_STR(store->cache->items, cacheKey, currentItem);

    if (currentItem) {
        int expired;
        struct LDJSON *current;

        expired = 0;
        current = NULL;

        if ((expired = isExpired(store, currentItem)) < 0) {
            goto cleanup;
        }

        current = LDJSONRCGet(currentItem->feature);
        LD_ASSERT(current);

        if (expired == 0 && LDi_getFeatureVersionTrusted(current) >=
            LDi_getFeatureVersionTrusted(replacement))
        {
            success = true;

            goto cleanup;
        }
    }

    if (!(replacementItem = makeCacheItem(cacheKey, replacement))) {
        goto cleanup;
    }

    replacement = NULL;

    HASH_FIND_STR(store->cache->items, allCacheKey, allItems);

    if (!store->backend) {
        struct LDJSON *itemDupe;

        itemDupe = NULL;

        if (!LDi_isFeatureDeleted(weakReplacementRef)) {
            if (!(itemDupe = LDJSONDuplicate(weakReplacementRef))) {
                goto cleanup;
            }
        }

        if (allItems) {
            struct LDJSON *allDupe;
            struct CacheItem *allDupeItem;

            if (!(allDupe = LDJSONDuplicate(LDJSONRCGet(allItems->feature)))) {
                goto cleanup;
            }

            if (itemDupe) {
                if (!LDObjectSetKey(allDupe,
                    LDi_getFeatureKeyTrusted(weakReplacementRef), itemDupe))
                {
                    goto cleanup;
                }
            } else {
                LDObjectDeleteKey(allDupe,
                    LDi_getFeatureKeyTrusted(weakReplacementRef));
            }

            if (!(allDupeItem = makeCacheItem(allCacheKey, allDupe))) {
                goto cleanup;
            }

            deleteAndRemoveCacheItem(&store->cache->items, allItems);

            HASH_ADD_KEYPTR(hh, store->cache->items, allDupeItem->key,
                strlen(allDupeItem->key), allDupeItem);
        } else if (itemDupe) {
            struct LDJSON *singleton;
            struct CacheItem *singletonItem;

            if (!(singleton = LDNewObject())) {
                goto cleanup;
            }

            if (!LDObjectSetKey(singleton,
                LDi_getFeatureKeyTrusted(weakReplacementRef), itemDupe))
            {
                goto cleanup;
            }

            if (!(singletonItem =
                makeCacheItem(allCacheKey, singleton)))
            {
                goto cleanup;
            }

            HASH_ADD_KEYPTR(hh, store->cache->items, singletonItem->key,
                strlen(singletonItem->key), singletonItem);
        }
    } else if (allItems) {
        deleteAndRemoveCacheItem(&store->cache->items, allItems);
    }

    if (currentItem) {
        deleteAndRemoveCacheItem(&store->cache->items, currentItem);
    }

    HASH_ADD_KEYPTR(hh, store->cache->items, replacementItem->key,
        strlen(replacementItem->key), replacementItem);

    replacementItem = NULL;

    success = true;

  cleanup:
    LDFree(allCacheKey);
    LDFree(cacheKey);
    LDJSONFree(replacement);
    deleteCacheItem(replacementItem);

    return success;
}

/* expects write lock */
static bool
filterAndCacheItems(struct LDStore *const store, const char *const kind,
    struct LDJSON *const features, struct LDJSON **const result)
{
    bool success;
    struct LDJSON *filteredItems, *featuresIter, *dupe, *next;

    LD_ASSERT(store);
    LD_ASSERT(kind);
    LD_ASSERT(features);
    LD_ASSERT(LDJSONGetType(features) == LDObject);

    success       = false;
    filteredItems = NULL;
    dupe          = NULL;

    if (!(filteredItems = LDNewObject())) {
        goto cleanup;
    }

    for (featuresIter = LDGetIter(features); featuresIter; featuresIter = next)
    {
        next = LDIterNext(featuresIter);

        if (!LDi_isFeatureDeleted(featuresIter)) {
            if (!(dupe = LDJSONDuplicate(featuresIter))) {
                goto cleanup;
            }

            if (!LDObjectSetKey(filteredItems, LDi_getFeatureKeyTrusted(dupe),
                dupe))
            {
                goto cleanup;
            }

            dupe = NULL;
        }

        if (!upsertMemory(store, kind,
            LDCollectionDetachIter(features, featuresIter)))
        {
            goto cleanup;
        }
    }

    success = true;

    if (result) {
        *result       = filteredItems;
        filteredItems = NULL;
    }

  cleanup:
    LDJSONFree(dupe);
    LDJSONFree(filteredItems);
    LDJSONFree(features);

    return success;
}

static void
memoryCacheFlush(struct MemoryContext *const context)
{
    struct CacheItem *item, *itemTmp;

    LD_ASSERT(context);

    item    = NULL;
    itemTmp = NULL;

    HASH_ITER(hh, context->items, item, itemTmp) {
        deleteAndRemoveCacheItem(&context->items, item);
    }

    context->items = NULL;
}

static bool
memoryInit(struct LDStore *const store, struct LDJSON *const sets)
{
    struct LDJSON *iter, *next;

    LD_ASSERT(store);
    LD_ASSERT(store->cache);
    LD_ASSERT(sets);
    LD_ASSERT(LDJSONGetType(sets) == LDObject);

    LDi_rwlock_wrlock(&store->cache->lock);

    memoryCacheFlush(store->cache);

    for (iter = LDGetIter(sets); iter; iter = next) {
        next = LDIterNext(iter);

        if (!filterAndCacheItems(store, LDIterKey(iter),
            LDCollectionDetachIter(sets, iter), NULL))
        {
            memoryCacheFlush(store->cache);

            LDi_rwlock_wrunlock(&store->cache->lock);

            LDJSONFree(sets);

            return false;
        }
    }

    if (!store->backend) {
        store->cache->initialized = true;
    }

    LDi_rwlock_wrunlock(&store->cache->lock);

    LDJSONFree(sets);

    return true;
}

static bool
memoryGetCollectionItem(struct MemoryContext *const context,
    const char *const kind, const char *const key,
    struct CacheItem **result)
{
    char *cacheKey;
    struct CacheItem *current;

    LD_ASSERT(context);
    LD_ASSERT(kind);
    LD_ASSERT(key);
    LD_ASSERT(result);

    cacheKey = NULL;
    *result  = NULL;

    if (!(cacheKey = featureStoreCacheKey(kind, key))) {
        return false;
    }

    HASH_FIND_STR(context->items, cacheKey, current);

    LDFree(cacheKey);

    *result = current;

    return true;
}

/* -1 error, 0 not expired, 1 expired */
static int
isExpired(const struct LDStore *const store,
    const struct CacheItem *const item)
{
    double now;

    LD_ASSERT(store);
    LD_ASSERT(item);

    if (!store->backend) {
        return 0;
    }

    if (store->cacheMilliseconds == 0) {
        return 1;
    }

    if (LDi_getMonotonicMilliseconds(&now)) {
        if ((now - item->updatedOn) > store->cacheMilliseconds) {
            return 1;
        } else {
            return 0;
        }
    } else {
        return -1;
    }
}

/* if there is a backend use it to fetch all features */
static bool
tryGetAllBackend(struct LDStore *const store, const char *const kind,
    struct LDJSONRC **const result)
{
    bool success;
    struct LDStoreCollectionItem *rawFeatureItems;
    unsigned int rawFeaturesCount, i;
    struct LDJSON *active, *rawFeatures, *activeDupe;
    struct LDJSONRC *activeRC;
    char *cacheKey;
    struct CacheItem *cacheItem;

    LD_ASSERT(store);
    LD_ASSERT(kind);
    LD_ASSERT(result);

    success          = false;
    *result          = NULL;
    rawFeatureItems  = NULL;
    rawFeaturesCount = 0;
    i                = 0;
    active           = NULL;
    rawFeatures      = NULL;
    activeRC         = NULL;
    cacheKey         = NULL;
    cacheItem        = NULL;
    activeDupe       = NULL;

    if (!store->backend) {
        success = true;

        goto cleanup;
    }

    LD_ASSERT(store->backend->all);

    if (!store->backend->all(store->backend->context, kind, &rawFeatureItems,
        &rawFeaturesCount))
    {
        goto cleanup;
    }

    if (!(rawFeatures = LDNewObject())) {
        goto cleanup;
    }

    for (i = 0; i < rawFeaturesCount; i++) {
        const char *key;
        struct LDJSON *deserialized;

        deserialized = NULL;

        if (!rawFeatureItems[i].buffer) {
            continue;
        }

        if (!(deserialized = LDJSONDeserialize(rawFeatureItems[i].buffer))) {
            goto cleanup;
        }

        if (!LDi_validateFeature(deserialized)) {
            LD_LOG(LD_LOG_ERROR, "LDStoreAll invalid feature from backend");

            LDJSONFree(deserialized);

            continue;
        }

        if (LDi_isFeatureDeleted(deserialized)) {
            LDJSONFree(deserialized);

            continue;
        }

        key = LDGetText(LDObjectLookup(deserialized, "key"));
        LD_ASSERT(key);

        if (!LDObjectSetKey(rawFeatures, key, deserialized)) {
            goto cleanup;
        }
    }

    LDi_rwlock_wrlock(&store->cache->lock);
    if (!filterAndCacheItems(store, kind, rawFeatures, &active)) {
        LDi_rwlock_wrunlock(&store->cache->lock);
        rawFeatures = NULL;
        goto cleanup;
    }
    rawFeatures = NULL;

    if (!(cacheKey = featureStoreAllCacheKey(kind))) {
        goto cleanup;
    }

    if (!(activeDupe = LDJSONDuplicate(active))) {
        goto cleanup;
    }

    if (!(cacheItem = makeCacheItem(cacheKey, activeDupe))) {
        goto cleanup;
    }
    activeDupe = NULL;

    HASH_ADD_KEYPTR(hh, store->cache->items, cacheItem->key,
        strlen(cacheItem->key), cacheItem);

    LDi_rwlock_wrunlock(&store->cache->lock);

    if (!(activeRC = LDJSONRCNew(active))) {
        goto cleanup;
    }

    active  = NULL;
    *result = activeRC;
    success = true;

  cleanup:
    LDFree(active);
    LDFree(rawFeatures);
    LDFree(cacheKey);
    LDFree(activeDupe);

    for (i = 0; i < rawFeaturesCount; i++) {
        LDFree(rawFeatureItems[i].buffer);
    }

    LDFree(rawFeatureItems);

    return success;
}

static bool
tryGetBackend(struct LDStore *const store, const char *const kind,
    const char *const key, struct LDJSONRC **const result)
{
    struct LDStoreCollectionItem collectionItem;

    LD_ASSERT(store);
    LD_ASSERT(kind);
    LD_ASSERT(key);
    LD_ASSERT(result);

    *result = NULL;
    memset(&collectionItem, 0, sizeof(struct LDStoreCollectionItem));

    if (!store->backend) {
        return true;
    }

    LD_ASSERT(store->backend->get);

    if (!store->backend->get(store->backend->context, kind, key,
        &collectionItem))
    {
        return false;
    }

    if (collectionItem.buffer) {
        struct LDJSON *deserialized, *dupe;
        struct LDJSONRC *deserializedRef;

        if (!(deserialized = LDJSONDeserialize(
            (const char *)collectionItem.buffer)))
        {
            LD_LOG(LD_LOG_ERROR, "LDStoreGet failed to deserialize JSON");

            LDFree(collectionItem.buffer);

            return false;
        }

        LDFree(collectionItem.buffer);

        if (!LDi_validateFeature(deserialized)) {
            LD_LOG(LD_LOG_ERROR, "LDStoreGet invalid feature from backend");

            LDJSONFree(deserialized);

            return false;
        }

        if (LDi_isFeatureDeleted(deserialized)) {
            return upsertMemory(store, kind, deserialized);
        } else {
            if (!(deserializedRef = LDJSONRCNew(deserialized))) {
                LDJSONFree(deserialized);

                return false;
            }

            if (!(dupe = LDJSONDuplicate(deserialized))) {
                LDJSONRCDecrement(deserializedRef);

                return false;
            }

            *result = deserializedRef;

            return upsertMemory(store, kind, dupe);
        }
    } else {
        bool status;
        struct LDJSON *placeholder;

        LD_ASSERT(store->cache);

        if (!(placeholder = LDi_makeDeleted(key, collectionItem.version))) {
            return false;
        }

        LDi_rwlock_wrlock(&store->cache->lock);
        status = upsertMemory(store, kind, placeholder);
        LDi_rwlock_wrunlock(&store->cache->lock);

        return status;
    }

    return false;
}

static bool
memoryAllCollectionItem(struct MemoryContext *const context,
    const char *const kind, struct CacheItem **const result)
{
    char *key;
    struct CacheItem *filtered;

    LD_ASSERT(context);
    LD_ASSERT(kind);
    LD_ASSERT(result);

    if (!(key = featureStoreAllCacheKey(kind))) {
        return false;
    }

    HASH_FIND_STR(context->items, key, filtered);

    LDFree(key);

    *result = filtered;

    return true;
}

/* used for testing */
void
LDi_expireAll(struct LDStore *const store)
{
    struct CacheItem *item, *itemTmp;

    LD_ASSERT(store);

    item    = NULL;
    itemTmp = NULL;

    LD_ASSERT(LDi_rwlock_wrlock(&store->cache->lock));

    HASH_ITER(hh, store->cache->items, item, itemTmp) {
        item->updatedOn = 0;
    }

    LD_ASSERT(LDi_rwlock_wrunlock(&store->cache->lock));
}

static void
memoryDestructor(struct MemoryContext *const context)
{
    LD_ASSERT(context);

    memoryCacheFlush(context);

    LDi_rwlock_destroy(&context->lock);

    LDFree(context);
}

struct LDStore *
LDStoreNew(const struct LDConfig *const config)
{
    struct LDStore *store;
    struct MemoryContext *cache;

    LD_ASSERT(config);

    store = NULL;
    cache = NULL;

    if (!(store = (struct LDStore *)LDAlloc(sizeof(struct LDStore)))) {
        goto error;
    }

    if (!(cache =
        (struct MemoryContext *)LDAlloc(sizeof(struct MemoryContext))))
    {
        goto error;
    }

    LDi_rwlock_init(&cache->lock);

    cache->initialized       = false;
    cache->items             = NULL;

    store->cache             = cache;
    store->backend           = config->storeBackend;
    store->cacheMilliseconds = config->storeCacheMilliseconds;

    return store;

  error:
    LDFree(store);
    LDFree(cache);

    return NULL;
}

/* **** Store API Operations **** */

bool
LDStoreInit(struct LDStore *const store, struct LDJSON *const sets)
{
    LD_ASSERT(store);
    LD_ASSERT(store->cache);
    LD_ASSERT(sets);
    LD_ASSERT(LDJSONGetType(sets) == LDObject);

    LD_LOG(LD_LOG_TRACE, "LDStoreInit");

    if (store->backend) {
        bool success;
        struct LDJSON *set, *setItem;
        struct LDStoreCollectionState *collections, *collectionsIter;
        unsigned int x;

        LD_ASSERT(store->backend->init);

        success         = false;
        set             = NULL;
        setItem         = NULL;
        collections     = NULL;
        collectionsIter = NULL;

        if (!(collections = (struct LDStoreCollectionState *)
            LDAlloc(sizeof(struct LDStoreCollectionState) *
            LDCollectionGetSize(sets))))
        {
            LD_LOG(LD_LOG_ERROR, "LDAlloc failed");

            goto cleanup;
        }

        collectionsIter = collections;

        for (set = LDGetIter(sets); set; set = LDIterNext(set)) {
            struct LDStoreCollectionStateItem *itemIter;

            LD_ASSERT(LDJSONGetType(set) == LDObject);

            collectionsIter->kind      = LDIterKey(set);
            collectionsIter->itemCount = LDCollectionGetSize(set);
            collectionsIter->items     = NULL;

            if (collectionsIter->itemCount > 0) {
                unsigned int allocated;

                allocated = sizeof(struct LDStoreCollectionStateItem) *
                    LDCollectionGetSize(set);

                if (!(collectionsIter->items =
                    (struct LDStoreCollectionStateItem *)LDAlloc(allocated)))
                {
                    LD_LOG(LD_LOG_ERROR, "LDAlloc failed");

                    goto cleanup;
                }

                memset(collectionsIter->items, 0, allocated);

                itemIter = collectionsIter->items;

                for (setItem = LDGetIter(set); setItem;
                    setItem = LDIterNext(setItem))
                {
                    char *serialized;

                    serialized = NULL;

                    if (!LDi_validateFeature(setItem)) {
                        LD_LOG(LD_LOG_ERROR,
                            "LDStoreInit failed to validate feature");

                        goto next;
                    }

                    if (!(serialized = LDJSONSerialize(setItem))) {
                        goto cleanup;
                    }

                    itemIter->key = LDi_getFeatureKeyTrusted(setItem);

                    itemIter->item.buffer     = (void *)serialized;
                    itemIter->item.bufferSize = strlen(serialized);
                    itemIter->item.version    = LDi_getFeatureVersion(setItem);

                  next:
                    itemIter++;
                }
            }

            collectionsIter++;
        }

        success = store->backend->init(store->backend->context, collections,
            LDCollectionGetSize(sets));

      cleanup:
        if (collections) {
            for (x = 0; x < LDCollectionGetSize(sets); x++) {
                struct LDStoreCollectionState *collection;
                unsigned int y;

                collection = &(collections[x]);

                for (y = 0; y < collection->itemCount; y++) {
                    struct LDStoreCollectionStateItem *item;

                    item = &(collection->items[y]);
                    /* this function owns the buffer the cast is safe */
                    LDFree((void *)item->item.buffer);
                }

                LDFree(collection->items);
            }

            LDFree(collections);
        }

        if (!success) {
            LDJSONFree(sets);

            return false;
        }
    }

    return memoryInit(store, sets);
}

bool
LDStoreGet(struct LDStore *const store, const enum FeatureKind kind,
    const char *const key, struct LDJSONRC **const result)
{
    struct CacheItem *item;

    LD_LOG(LD_LOG_TRACE, "LDStoreGet");

    LD_ASSERT(store);
    LD_ASSERT(store->cache);
    LD_ASSERT(key);
    LD_ASSERT(result);

    item    = NULL;
    *result = NULL;

    LDi_rwlock_rdlock(&store->cache->lock);

    if (!memoryGetCollectionItem(store->cache, featureKindToString(kind),
        key, &item))
    {
        LDi_rwlock_rdunlock(&store->cache->lock);

        return false;
    }

    if (item) {
        int expired;

        expired = isExpired(store, item);

        if (expired < 0) {
            LDi_rwlock_rdunlock(&store->cache->lock);

            return false;
        } else if (expired == 0) {
            if (LDi_isFeatureDeleted(LDJSONRCGet(item->feature))) {
                LDi_rwlock_rdunlock(&store->cache->lock);

                return true;
            } else {
                LDJSONRCIncrement(item->feature);

                *result = item->feature;

                LDi_rwlock_rdunlock(&store->cache->lock);

                return true;
            }
        } else if (expired > 0) {
            LDi_rwlock_rdunlock(&store->cache->lock);
            /* When there is no backend a flag will never be expired */
            return tryGetBackend(store, featureKindToString(kind), key, result);
        }
    } else {
        LDi_rwlock_rdunlock(&store->cache->lock);

        if (store->backend) {
            return tryGetBackend(store, featureKindToString(kind), key, result);
        } else {
            return true;
        }
    }

    return false;
}

bool
LDStoreAll(struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSONRC **const result)
{
    struct CacheItem *item;

    LD_LOG(LD_LOG_TRACE, "LDStoreAll");

    LD_ASSERT(store);
    LD_ASSERT(store->cache);
    LD_ASSERT(result);

    item    = NULL;
    *result = NULL;

    LDi_rwlock_rdlock(&store->cache->lock);

    if (!memoryAllCollectionItem(store->cache, featureKindToString(kind),
        &item))
    {
        LDi_rwlock_rdunlock(&store->cache->lock);

        return false;
    }

    if (item) {
        int expired;

        expired = isExpired(store, item);

        if (expired < 0) {
            LDi_rwlock_rdunlock(&store->cache->lock);

            return false;
        } else if (expired == 0) {
            LDJSONRCIncrement(item->feature);

            *result = item->feature;

            LDi_rwlock_rdunlock(&store->cache->lock);

            return true;
        } else if (expired > 0) {
            LDi_rwlock_rdunlock(&store->cache->lock);
            /* When there is no backend a flag will never be expired */
            return tryGetAllBackend(store, featureKindToString(kind), result);
        }
    } else {
        LDi_rwlock_rdunlock(&store->cache->lock);

        return tryGetAllBackend(store, featureKindToString(kind), result);
    }

    /* work around invalid warning, execution should never get here  */
    LD_ASSERT(false);

    return false;
}

bool
LDStoreRemove(struct LDStore *const store, const enum FeatureKind kind,
    const char *const key, const unsigned int version)
{
    bool status;
    struct LDJSON *placeholder;

    LD_LOG(LD_LOG_TRACE, "LDStoreRemove");

    LD_ASSERT(store);
    LD_ASSERT(key);

    status      = false;
    placeholder = NULL;

    if (store->backend) {
        struct LDStoreCollectionItem item;

        LD_ASSERT(store->backend->upsert);

        item.buffer     = NULL;
        item.bufferSize = 0;
        item.version    = version;

        if (!store->backend->upsert(store->backend->context,
            featureKindToString(kind), &item, key))
        {
            return false;
        }
    }

    if (!(placeholder = LDi_makeDeleted(key, version))) {
        return false;
    }

    LDi_rwlock_wrlock(&store->cache->lock);
    status = upsertMemory(store, featureKindToString(kind), placeholder);
    LDi_rwlock_wrunlock(&store->cache->lock);

    return status;
}

bool
LDStoreUpsert(struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSON *const feature)
{
    bool status;

    LD_ASSERT(store);
    LD_ASSERT(feature);

    LD_LOG(LD_LOG_TRACE, "LDStoreUpsert");

    if (!LDi_validateFeature(feature)) {
        LD_LOG(LD_LOG_ERROR, "LDStoreUpsert failed to validate feature");

        LDJSONFree(feature);

        return false;
    }

    if (store->backend) {
        bool success;
        char *serialized;
        struct LDStoreCollectionItem collectionItem;

        success    = false;
        serialized = NULL;

        LD_ASSERT(store->backend->upsert);

        if (!(serialized = LDJSONSerialize(feature))) {
            LDJSONFree(feature);

            return false;
        }

        collectionItem.buffer     = (void *)serialized;
        collectionItem.bufferSize = strlen(serialized);
        collectionItem.version    = LDi_getFeatureVersionTrusted(feature);

        success = store->backend->upsert(store->backend->context,
            featureKindToString(kind), &collectionItem,
            LDi_getFeatureKeyTrusted(feature));

        LDFree(serialized);

        if (!success) {
            LDJSONFree(feature);

            return false;
        }
    }

    LDi_rwlock_wrlock(&store->cache->lock);
    status = upsertMemory(store, featureKindToString(kind), feature);
    LDi_rwlock_wrunlock(&store->cache->lock);

    return status;
}

bool
LDStoreInitialized(struct LDStore *const store)
{
    bool isInitialized;
    struct CacheItem *item;

    LD_ASSERT(store);

    LD_LOG(LD_LOG_TRACE, "LDStoreInitialized");

    LDi_rwlock_rdlock(&store->cache->lock);
    isInitialized = store->cache->initialized;

    if (isInitialized || !store->backend) {
        LDi_rwlock_rdunlock(&store->cache->lock);

        return isInitialized;
    }

    HASH_FIND_STR(store->cache->items, INIT_CHECKED_KEY, item);

    if (item) {
        int expired = isExpired(store, item);

        if (expired < 0) {
            LDi_rwlock_rdunlock(&store->cache->lock);

            return false;
        } else if (expired == 0) {
            LDi_rwlock_rdunlock(&store->cache->lock);

            return false;
        } else if (expired > 0) {
            deleteAndRemoveCacheItem(&store->cache->items, item);
        }
    }

    LDi_rwlock_rdunlock(&store->cache->lock);

    isInitialized = store->backend->initialized(store->backend->context);

    if (isInitialized) {
        LDi_rwlock_wrlock(&store->cache->lock);
        store->cache->initialized = true;
        LDi_rwlock_wrunlock(&store->cache->lock);
    } else {
        if (!(item = makeCacheItem(INIT_CHECKED_KEY, NULL))) {
            return false;
        }

        LDi_rwlock_wrlock(&store->cache->lock);
        HASH_ADD_KEYPTR(hh, store->cache->items, item->key, strlen(item->key),
            item);
        LDi_rwlock_wrunlock(&store->cache->lock);
    }

    return isInitialized;
}

void
LDStoreDestroy(struct LDStore *const store)
{
    LD_LOG(LD_LOG_TRACE, "LDStoreDestroy");

    if (store) {
        memoryDestructor(store->cache);

        if (store->backend) {
            if (store->backend->destructor) {
                store->backend->destructor(store->backend->context);
            }

            LDFree(store->backend);
        }

        LDFree(store);
    }
}

bool
LDStoreInitEmpty(struct LDStore *const store)
{
    struct LDJSON *values;
    bool success;

    LD_ASSERT(store);

    success = false;

    if (!(values = LDNewObject())) {
        goto cleanup;
    }

    success = LDStoreInit(store, values);
    /* values consumed even on failure */
    values = NULL;

  cleanup:
    LDJSONFree(values);

    return success;
}
