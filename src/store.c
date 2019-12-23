#include "uthash.h"

#include <launchdarkly/api.h>

#include "misc.h"
#include "store.h"

/* **** Forward Declarations **** */

struct MemoryContext;

static const char *const LD_SS_FEATURES = "features";
static const char *const LD_SS_SEGMENTS = "segments";

static bool memoryInit(void *const rawcontext, struct LDJSON *const sets);

static bool memoryGet(void *const rawcontext, const char *const kind,
    const char *const key, struct LDJSONRC **const result);

static bool memoryAll(void *const rawcontext, const char *const kind,
    struct LDJSONRC ***const result);

static bool memoryUpsert(void *const rawcontext, const char *const kind,
    struct LDJSON *replacement);

static bool memoryInitialized(void *const rawcontext);

static void memoryDestructor(void *const rawcontext);

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

    if (!LDi_mtxinit(&result->lock)) {
        LDFree(result);

        return NULL;
    }

    result->value = json;
    result->count = 1;

    return result;
}

void
LDJSONRCIncrement(struct LDJSONRC *const rc)
{
    LD_ASSERT(rc);

    LD_ASSERT(LDi_mtxlock(&rc->lock));
    rc->count++;
    LD_ASSERT(LDi_mtxunlock(&rc->lock));
}

static void
destroyJSONRC(struct LDJSONRC *const rc)
{
    if (rc) {
        LDJSONFree(rc->value);
        LD_ASSERT(LDi_mtxdestroy(&rc->lock));
        LDFree(rc);
    }
}

void
LDJSONRCDecrement(struct LDJSONRC *const rc)
{
    if (rc) {
        LD_ASSERT(LDi_mtxlock(&rc->lock));
        LD_ASSERT(rc->count > 0);
        rc->count--;

        if (rc->count == 0) {
            LD_ASSERT(LDi_mtxunlock(&rc->lock));

            destroyJSONRC(rc);
        } else {
            LD_ASSERT(LDi_mtxunlock(&rc->lock));
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
struct FeatureCollectionItem {
    struct LDJSONRC *feature;
    UT_hash_handle hh;
};

/* Namespace -> Feature Set Hashtable */
struct FeatureCollection {
    char *itemNamespace;
    struct FeatureCollectionItem *items;
    UT_hash_handle hh;
};

static struct FeatureCollectionItem *
makeFeatureCollectionItem(struct LDJSONRC *const feature)
{
    struct FeatureCollectionItem *result;

    LD_ASSERT(feature);

    if (!(result = (struct FeatureCollectionItem *)
        LDAlloc(sizeof(struct FeatureCollectionItem))))
    {
        return NULL;
    }

    memset(result, 0, sizeof(struct FeatureCollectionItem));

    result->feature = feature;

    return result;
}

struct MemoryContext {
    bool initialized;
    /* hash table, not single element */
    struct FeatureCollection *collections;
    ld_rwlock_t lock;
};

static bool
addFeatures(struct FeatureCollection **const collections,
    struct LDJSON *const features)
{
    struct LDJSONRC *rc;
    struct FeatureCollection *collection;
    struct FeatureCollectionItem *item;
    struct LDJSON *iter, *next;

    LD_ASSERT(collections);
    LD_ASSERT(features);

    if (!(collection =
        (struct FeatureCollection *)LDAlloc(sizeof(struct FeatureCollection))))
    {
        return false;
    }

    memset(collection, 0, sizeof(struct FeatureCollection));

    if (!(collection->itemNamespace = LDStrDup(LDIterKey(features)))) {
        LDFree(collection);

        return false;
    }

    HASH_ADD_KEYPTR(hh, *collections, collection->itemNamespace,
        strlen(collection->itemNamespace), collection);

    for (iter = LDGetIter(features); iter;) {
        const char *key;

        next = LDIterNext(iter);

        if (!LDi_validateFeature(iter)) {
            LD_LOG(LD_LOG_ERROR,
                "memory addFeatures failed to validate feature");

            return false;
        }

        LD_ASSERT(key = LDGetText(LDObjectLookup(iter, "key")));

        if (!(rc = LDJSONRCNew(LDCollectionDetachIter(features, iter)))) {
            return false;
        }

        if (!(item = makeFeatureCollectionItem(rc))) {
            destroyJSONRC(rc);

            return false;
        }

        HASH_ADD_KEYPTR(hh, collection->items, key, strlen(key), item);

        iter = next;
    }

    return true;
}

static void
memoryClearCollections(struct MemoryContext *const context)
{
    struct FeatureCollection *collection, *collectiontmp;
    struct FeatureCollectionItem *item, *itemtmp;

    LD_ASSERT(context);

    HASH_ITER(hh, context->collections, collection, collectiontmp) {
        HASH_DEL(context->collections, collection);

        HASH_ITER(hh, collection->items, item, itemtmp) {
            HASH_DEL(collection->items, item);

            LDJSONRCDecrement(item->feature);

            LDFree(item);
        }

        LDFree(collection->itemNamespace);
        LDFree(collection);
    }

    context->collections = NULL;
}

static bool
memoryInit(void *const rawcontext, struct LDJSON *const sets)
{
    struct MemoryContext *context;
    struct LDJSON *iter;

    LD_ASSERT(rawcontext);
    LD_ASSERT(sets);
    LD_ASSERT(LDJSONGetType(sets) == LDObject);

    context = (struct MemoryContext *)rawcontext;

    LD_ASSERT(LDi_wrlock(&context->lock));

    if (context->initialized){
        memoryClearCollections(context);
    }

    context->initialized = false;

    for (iter = LDGetIter(sets); iter; iter = LDIterNext(iter)) {
        if (!addFeatures(&context->collections, iter)) {
            memoryClearCollections(context);

            LD_ASSERT(LDi_wrunlock(&context->lock));

            LDJSONFree(sets);

            return false;
        }
    }

    context->initialized = true;

    LD_ASSERT(LDi_wrunlock(&context->lock));

    LDJSONFree(sets);

    return true;
}

static bool
memoryGet(void *const rawcontext, const char *const kind,
    const char *const key, struct LDJSONRC **const result)
{
    struct FeatureCollection *collection;
    struct FeatureCollectionItem *current;
    struct MemoryContext *context;

    LD_ASSERT(rawcontext);
    LD_ASSERT(key);
    LD_ASSERT(result);
    LD_ASSERT(kind);

    current    = NULL;
    context    = (struct MemoryContext *)rawcontext;
    *result    = NULL;
    collection = NULL;

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (!context->collections) {
        LD_LOG(LD_LOG_ERROR, "tried memoryGet before initialized");

        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    HASH_FIND_STR(context->collections, kind, collection);

    if (!collection) {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    HASH_FIND_STR(collection->items, key, current);

    if (current) {
        if (LDi_isFeatureDeleted(LDJSONRCGet(current->feature))) {
            LD_ASSERT(LDi_rdunlock(&context->lock));

            return true;
        } else {
            LDJSONRCIncrement(current->feature);

            *result = current->feature;

            LD_ASSERT(LDi_rdunlock(&context->lock));

            return true;
        }
    } else {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return true;
    }
}

static bool
memoryAll(void *const rawcontext, const char *const kind,
    struct LDJSONRC ***const result)
{
    struct MemoryContext *context;
    struct FeatureCollection *set;
    struct FeatureCollectionItem *current, *tmp;
    struct LDJSONRC **collection, **collectioniter;
    unsigned int count;

    LD_ASSERT(rawcontext);
    LD_ASSERT(result);

    *result    = NULL;
    context    = (struct MemoryContext *)rawcontext;
    collection = NULL;
    count      = 0;
    set        = NULL;

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (!context->collections) {
        LD_LOG(LD_LOG_ERROR, "tried memoryAll before initialized");

        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    HASH_FIND_STR(context->collections, kind, set);

    if (!set) {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    HASH_ITER(hh, set->items, current, tmp) {
        if (!LDi_isFeatureDeleted(LDJSONRCGet(current->feature))) {
            count++;
        }
    }

    if (count == 0) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return true;
    }

    if (!(collection =
        (struct LDJSONRC **)LDAlloc(sizeof(struct LDJSONRC *) * (count + 1))))
    {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    collectioniter = collection;

    HASH_ITER(hh, set->items, current, tmp) {
        if (!LDi_isFeatureDeleted(LDJSONRCGet(current->feature))) {
            *collectioniter = current->feature;
            LDJSONRCIncrement(current->feature);
            collectioniter++;
        }
    }

    *collectioniter = NULL;

    LD_ASSERT(LDi_rdunlock(&context->lock));

    *result = collection;

    return true;
}

static bool
memoryUpsert(void *const rawcontext, const char *const kind,
    struct LDJSON *replacement)
{
    struct MemoryContext *context;
    const char *key;
    struct FeatureCollection *collection;
    struct FeatureCollectionItem *current;
    bool success;

    LD_ASSERT(rawcontext);
    LD_ASSERT(replacement);

    current    = NULL;
    context    = (struct MemoryContext *)rawcontext;
    key        = NULL;
    collection = NULL;
    success    = false;

    LD_ASSERT(LDi_wrlock(&context->lock));

    if (!context->collections) {
        LD_LOG(LD_LOG_ERROR, "tried memoryUpsert before initialized");

        goto cleanup;
    }

    HASH_FIND_STR(context->collections, kind, collection);

    if (!collection) {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    LD_ASSERT(key = LDi_getFeatureKeyTrusted(replacement));

    HASH_FIND_STR(collection->items, key, current);

    if (current) {
        struct LDJSON *feature;
        struct LDJSONRC *replacementrc;
        struct FeatureCollectionItem *replacementcollectionitem;

        replacementrc = NULL;
        feature       = NULL;

        LD_ASSERT(feature = LDJSONRCGet(current->feature));

        if (LDi_getFeatureVersionTrusted(feature) >=
            LDi_getFeatureVersionTrusted(replacement))
        {
            success = true;

            goto cleanup;
        }

        if (!(replacementrc = LDJSONRCNew(replacement))) {
            goto cleanup;
        }

        replacement = NULL;

        if (!(replacementcollectionitem =
            makeFeatureCollectionItem(replacementrc)))
        {
            LDJSONRCDecrement(replacementrc);

            goto cleanup;
        }

        LDJSONRCDecrement(current->feature);

        HASH_DEL(collection->items, current);
        LDFree(current);

        HASH_ADD_KEYPTR(hh, collection->items, key, strlen(key),
            replacementcollectionitem);
    } else {
        struct LDJSONRC *replacementrc;
        struct FeatureCollectionItem *replacementcollectionitem;

        if (!(replacementrc = LDJSONRCNew(replacement))) {
            goto cleanup;
        }

        replacement = NULL;

        if (!(replacementcollectionitem =
            makeFeatureCollectionItem(replacementrc)))
        {
            LDJSONRCDecrement(replacementrc);

            goto cleanup;
        }

        HASH_ADD_KEYPTR(hh, collection->items, key, strlen(key),
            replacementcollectionitem);
    }

    success = true;

  cleanup:
    LD_ASSERT(LDi_wrunlock(&context->lock));

    LDJSONFree(replacement);

    return success;
}

static bool
memoryInitialized(void *const rawcontext)
{
    struct MemoryContext *context;

    LD_ASSERT(rawcontext);

    context = (struct MemoryContext *)rawcontext;

    return context->initialized;
}

static void
memoryDestructor(void *const rawcontext)
{
    struct MemoryContext *context;

    LD_ASSERT(rawcontext);

    context = (struct MemoryContext *)rawcontext;

    memoryClearCollections(context);

    LDi_rwlockdestroy(&context->lock);

    LDFree(context);
}

struct LDStore *
LDStoreNew(struct LDStoreInterface *const storeInterface)
{
    struct LDStore *store;
    struct MemoryContext *cache;

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

    if (!LDi_rwlockinit(&cache->lock)) {
        goto error;
    }

    cache->initialized = false;
    cache->collections = NULL;

    store->cache       = cache;
    store->backend     = storeInterface;

    return store;

  error:
    LDFree(store);
    LDFree(cache);

    return NULL;
}

/* **** Store API Operations **** */

bool
LDStoreInit(const struct LDStore *const store, struct LDJSON *const sets)
{
    LD_ASSERT(store);
    LD_ASSERT(sets);
    LD_ASSERT(LDJSONGetType(sets) == LDObject);

    if (store->backend) {
        bool success;
        struct LDJSON *set, *setItem;
        struct LDStoreCollectionState *collections, *collectionsIter;
        unsigned int x;

        LD_ASSERT(store->backend->context);
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

        LDJSONFree(sets);

        return success;
    } else {
        LD_ASSERT(store->cache);

        return memoryInit(store->cache, sets);
    }
}

bool
LDStoreGet(const struct LDStore *const store, const enum FeatureKind kind,
    const char *const key, struct LDJSONRC **const result)
{
    LD_ASSERT(store);
    LD_ASSERT(key);
    LD_ASSERT(result);

    if (store->backend) {
        struct LDStoreCollectionItem collectionItem;

        LD_ASSERT(store->backend->context)
        LD_ASSERT(store->backend->get);

        memset(&collectionItem, 0, sizeof(struct LDStoreCollectionItem));

        if (store->backend->get(store->backend->context,
            featureKindToString(kind), key, &collectionItem))
        {
            if (collectionItem.buffer) {
                struct LDJSON *deserialized;
                struct LDJSONRC *deserializedRef;

                if (!(deserialized = LDJSONDeserialize(
                    (const char *)collectionItem.buffer)))
                {
                    LD_LOG(LD_LOG_ERROR,
                        "LDStoreGet failed to deserializen JSON");

                    LDFree(collectionItem.buffer);

                    return false;
                }

                LDFree(collectionItem.buffer);

                if (LDi_isFeatureDeleted(deserialized)) {
                    *result = NULL;

                    LDJSONFree(deserialized);

                    return true;
                }

                if (!(deserializedRef = LDJSONRCNew(deserialized))) {
                    LDJSONFree(deserialized);

                    return false;
                }

                *result = deserializedRef;

                return true;
            }

            *result = NULL;

            return true;
        } else {
            return false;
        }
    } else {
        LD_ASSERT(store->cache);

        return memoryGet(store->cache, featureKindToString(kind), key, result);
    }
}

bool
LDStoreAll(const struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSONRC ***const result)
{
    LD_ASSERT(store);
    LD_ASSERT(result);

    if (store->backend) {
        struct LDStoreCollectionItem *rawFlags;
        unsigned int rawFlagsCount;

        LD_ASSERT(store->backend->context);
        LD_ASSERT(store->backend->all);

        rawFlags      = NULL;
        rawFlagsCount = 0;

        if (store->backend->all(store->backend->context, LD_SS_FEATURES,
            &rawFlags, &rawFlagsCount))
        {
            struct LDJSONRC **activeItems, **activeItemsIter;
            unsigned int allocatedBytes;
            bool success;
            unsigned int i;

            activeItems      = NULL;
            activeItemsIter  = NULL;
            allocatedBytes   = 0;
            success          = false;
            i                = 0;

            allocatedBytes = sizeof(struct LDJSONRC *) * (rawFlagsCount + 1);

            if (!(activeItems = (struct LDJSONRC **)LDAlloc(allocatedBytes))) {
                goto cleanup;
            }

            memset(activeItems, 0, allocatedBytes);

            activeItemsIter = activeItems;

            for (i = 0; i < rawFlagsCount; i++) {
                struct LDJSON *deserialized;
                struct LDJSONRC *deserializedRC;

                deserialized   = NULL;
                deserializedRC = NULL;

                if (!rawFlags[i].buffer) {
                    continue;
                }

                if (!(deserialized = LDJSONDeserialize(rawFlags[i].buffer))) {
                    goto cleanup;
                }

                if (LDi_isFeatureDeleted(deserialized)) {
                    LDJSONFree(deserialized);

                    continue;
                }

                if (!(deserializedRC = LDJSONRCNew(deserialized))) {
                    LDJSONFree(deserialized);

                    goto cleanup;
                }

                *activeItemsIter = deserializedRC;

                activeItemsIter++;
            }

            *result = activeItems;
            success = true;

          cleanup:
            if (!success) {
                for (activeItemsIter = activeItems; *activeItemsIter;
                    activeItemsIter++)
                {
                    LDJSONRCDecrement(*activeItemsIter);
                }

                LDFree(activeItems);
            }

            for (i = 0; i < rawFlagsCount; i++) {
                LDFree(rawFlags[i].buffer);
            }

            LDFree(rawFlags);

            return success;
        } else {
            return false;
        }
    } else {
        LD_ASSERT(store->cache);

        return memoryAll(store->cache, featureKindToString(kind), result);
    }
}

bool
LDStoreRemove(const struct LDStore *const store, const enum FeatureKind kind,
    const char *const key, const unsigned int version)
{
    LD_ASSERT(store);
    LD_ASSERT(key);

    if (store->backend) {
        struct LDStoreCollectionItem item;

        LD_ASSERT(store->backend->context);
        LD_ASSERT(store->backend->upsert);

        item.buffer     = NULL;
        item.bufferSize = 0;
        item.version    = version;

        return store->backend->upsert(store->backend->context,
            featureKindToString(kind), &item, key);
    } else {
        struct LDJSON *placeholder;

        LD_ASSERT(store->cache);

        if (!(placeholder = LDi_makeDeleted(key, version))) {
            return false;
        }

        return memoryUpsert(store->cache, featureKindToString(kind),
            placeholder);
    }
}

bool
LDStoreUpsert(const struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSON *const feature)
{
    LD_ASSERT(store);
    LD_ASSERT(feature);

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

        LD_ASSERT(store->backend->context);
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

        LDJSONFree(feature);
        LDFree(serialized);

        return success;
    } else {
        LD_ASSERT(store->cache);

        return memoryUpsert(store->cache, featureKindToString(kind), feature);
    }
}

bool
LDStoreInitialized(const struct LDStore *const store)
{
    LD_ASSERT(store);

    if (store->backend) {
        LD_ASSERT(store->backend->context);
        LD_ASSERT(store->backend->initialized);

        return store->backend->initialized(store->backend->context);
    } else {
        LD_ASSERT(store->cache);

        return memoryInitialized(store->cache);
    }
}

void
LDStoreDestroy(struct LDStore *const store)
{
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
    struct LDJSON *tmp, *values;
    bool success;

    LD_ASSERT(store);

    tmp     = NULL;
    values  = NULL;
    success = false;

    if (!(values = LDNewObject())) {
        goto cleanup;
    }

    if (!(tmp = LDNewObject())) {
        goto cleanup;
    }

    if (!(LDObjectSetKey(values, "segments", tmp))) {
        goto cleanup;
    }

    if (!(tmp = LDNewObject())) {
        goto cleanup;
    }

    if (!(LDObjectSetKey(values, "features", tmp))) {
        goto cleanup;
    }

    tmp = NULL;

    success = LDStoreInit(store, values);
    /* values consumed even on failure */
    values = NULL;

  cleanup:
    LDJSONFree(tmp);
    LDJSONFree(values);

    return success;
}
