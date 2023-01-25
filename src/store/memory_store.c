#include <stdio.h>
#include <string.h>

#include <uthash.h>

#include "launchdarkly/memory.h"
#include "launchdarkly/store.h"

#include "assertion.h"
#include "concurrency.h"
#include "internal_store.h"
#include "memory_store.h"
#include "store_utilities.h"
#include "json_internal_helpers.h"

#define MS_CONTEXT(ptr) (struct MemoryStoreContext *) ptr;

/* region Structure definitions */

struct LDMemoryItem
{
    char *key;
    struct LDJSONRC *value;
    UT_hash_handle hh;
};

struct MemoryStoreContext {
    LDBoolean initialized;
    /* ut hash table */
    struct LDMemoryItem *features;
    /* ut hash table */
    struct LDMemoryItem *segments;
    ld_rwlock_t lock;
};

/* endregion */

static void clearItems(struct LDMemoryItem *items) {
    struct LDMemoryItem *item, *itemTmp;

    item    = NULL;
    itemTmp = NULL;

    HASH_ITER(hh, items, item, itemTmp)
    {
        LDFree(item->key);
        LDJSONRCRelease(item->value);
        HASH_DEL(items, item);
        LDFree(item);
    }
}

static void
removeAndDeleteItem(struct MemoryStoreContext* msCtx, enum FeatureKind kind, struct LDMemoryItem* item) {
    LD_ASSERT(msCtx);
    LD_ASSERT(item);

    switch(kind) {
        case LD_FLAG:
            if(msCtx->features) {
                /* HASH_DEL Must be called with the direct pointer of the items.
                 * It must be able to re-assign the pointer. */
                HASH_DEL(msCtx->features, item);
            }
            break;
        case LD_SEGMENT:
            if(msCtx->segments) {
                HASH_DEL(msCtx->segments, item);
            }
            break;
        default:
            break;
    }

    LDFree(item->key);
    LDJSONRCRelease(item->value);
    LDFree(item);
}

static void
clearStore(struct MemoryStoreContext *const context) {
    LD_ASSERT(context);

    if(context->features != NULL) {
        clearItems(context->features);
    }
    if(context->segments != NULL) {
        clearItems(context->segments);
    }

    context->features = NULL;
    context->segments = NULL;
}

static struct LDMemoryItem* makeMemoryItem(const char *const key, struct LDJSON *value) {
    char *keyDupe;
    struct LDMemoryItem *item;
    struct LDJSONRC * valueRC;

    LD_ASSERT(key);

    keyDupe = NULL;
    item    = NULL;
    valueRC = NULL;

    keyDupe = LDStrDup(key);
    LD_ASSERT(keyDupe);

    if (value) {
        valueRC = LDJSONRCNew(value);
        LD_ASSERT(valueRC);
    }

    item = (struct LDMemoryItem *)LDAlloc(sizeof(struct LDMemoryItem));
    LD_ASSERT(item);

    memset(item, 0, sizeof *item);

    item->key = keyDupe;
    item->value = valueRC;

    return item;
}

static void
addToStore(struct MemoryStoreContext* msCtx, enum FeatureKind kind, struct LDMemoryItem *item) {
    LD_ASSERT(msCtx);
    LD_ASSERT(item);

    switch(kind) {
        case LD_FLAG:
            HASH_ADD_KEYPTR(
                hh,
                msCtx->features,
                item->key,
                strlen(item->key),
                item);
            break;
        case LD_SEGMENT:
            HASH_ADD_KEYPTR(
                hh,
                msCtx->segments,
                item->key,
                strlen(item->key),
                item);
            break;
        default:
            break;
    }
}

static void
getFromStore(struct MemoryStoreContext* msCtx, enum FeatureKind kind, const char* key, struct LDMemoryItem **result) {
    LD_ASSERT(msCtx);
    LD_ASSERT(result);
    LD_ASSERT(key);

    *result  = NULL;

    switch(kind) {
        case LD_FLAG:
            HASH_FIND_STR(msCtx->features, key, *result);
            break;
        case LD_SEGMENT:
            HASH_FIND_STR(msCtx->segments, key, *result);
            break;
        default:
            break;
    }
}

/* region LDInternalStoreInterface Implementation */

static LDBoolean
storeInit(
        void *const contextRaw,
        struct LDJSON *const newData) {
    struct LDJSON *dataKindsIter = NULL;
    struct LDJSON *dataKindsNext = NULL;
    struct MemoryStoreContext* msCtx = MS_CONTEXT(contextRaw);
    LD_ASSERT(msCtx);

    /* An init can happen on any put, so we need to clear the store if we have one. */
    LDi_rwlock_wrlock(&msCtx->lock);

    clearStore(msCtx);

    /* For each data kind (Features/Segments/??). */
    for(dataKindsIter = LDGetIter(newData); dataKindsIter; dataKindsIter = dataKindsNext) {
        struct LDJSON *itemsIter, *itemsNext, *items;
        const char* kind = LDIterKey(dataKindsIter);
        enum FeatureKind featureKind;

        /* Need to get the next iter before detaching. */
        dataKindsNext = LDIterNext(dataKindsIter);

        stringToFeatureKind(kind, &featureKind);

        /* Don't do anything with unrecognized namespaces. */
        /* Converts to an enum kind, returns false if it could not be converted. */
        if(stringToFeatureKind(kind, &featureKind)) {
            /* Detaching means we now own it. */
            items = LDCollectionDetachIter(newData, dataKindsIter);

            /* For each item of the kind. */
            for (itemsIter = LDGetIter(items); itemsIter; itemsIter = itemsNext) {
                struct LDMemoryItem *newItem;
                /* Get the next item before detaching from the collection. */
                itemsNext = LDIterNext(itemsIter);

                newItem = makeMemoryItem(
                        LDi_getDataKey(itemsIter),
                        LDCollectionDetachIter(items,
                                               itemsIter));

                addToStore(msCtx, featureKind, newItem);
            }
            LDJSONFree(items);
        }
    }

    msCtx->initialized = LDBooleanTrue;

    LDi_rwlock_wrunlock(&msCtx->lock);
    LDJSONFree(newData);
    return LDBooleanTrue;
}

static LDBoolean
storeGet(
        void *const contextRaw,
        enum FeatureKind kind,
        const char *const key,
        struct LDJSONRC **const result)
{
    struct LDMemoryItem *item;
    struct MemoryStoreContext* msCtx = MS_CONTEXT(contextRaw);
    LD_ASSERT(msCtx);
    LD_ASSERT(key);
    LD_ASSERT(result);

    *result  = NULL;

    LDi_rwlock_rdlock(&msCtx->lock);

    getFromStore(msCtx, kind, key, &item);

    if(item) {
        /* If the item has a value, and it isn't deleted, then provide a result. */
        if(item->value != NULL && !LDi_isDataDeleted(LDJSONRCGet(item->value))) {
            LDJSONRCRetain(item->value);
                *result = item->value;
        }
    }

    LDi_rwlock_rdunlock(&msCtx->lock);

    /* Failure would be that the store was not working, not that we couldn't find the item. The memory store
     * is always functional, so always return true. */
    return LDBooleanTrue;
}

static LDBoolean
storeAll(
        void *const contextRaw,
        enum FeatureKind kind,
        struct LDJSONRC **const result)
{
    /* Collection to contain at most the number of references there are features.
    * Not all of them may be populated, because some items may be deleted, but having
    * a little extra space is better than having to implement vector behavior. */
    struct LDJSONRC** associatedRcItems = NULL;
    unsigned int associatedCount = 0;
    struct LDMemoryItem *item, *itemTmp;
    struct LDJSON* all = LDNewObject();
    struct MemoryStoreContext* msCtx = MS_CONTEXT(contextRaw);

    *result = NULL;

    LDi_rwlock_rdlock(&msCtx->lock);

    switch(kind) {
        case LD_FLAG: {
            unsigned int itemCount = HASH_COUNT(msCtx->features);
            associatedRcItems = (struct LDJSONRC **) LDAlloc(sizeof(struct LDJSONRC *) * itemCount);

            HASH_ITER(hh, msCtx->features, item, itemTmp) {
                struct LDJSON *value = LDJSONRCGet(item->value);
                if (!LDi_isDataDeleted(value)) {
                    LDObjectSetReference(all, item->key, value);
                    *(associatedRcItems + associatedCount) = item->value;
                    associatedCount++;
                }
            }
        }
            break;
        case LD_SEGMENT: {
            unsigned int itemCount = HASH_COUNT(msCtx->segments);
            associatedRcItems = (struct LDJSONRC **) LDAlloc(sizeof(struct LDJSONRC *) * itemCount);

            HASH_ITER(hh, msCtx->segments, item, itemTmp) {
                struct LDJSON *value = LDJSONRCGet(item->value);
                if (!LDi_isDataDeleted(value)) {
                    LDObjectSetReference(all, item->key, value);
                    *(associatedRcItems + associatedCount) = item->value;
                    associatedCount++;
                }
            }
        }
            break;
        default:
            break;
    }

    LDi_rwlock_rdunlock(&msCtx->lock);
    *result = LDJSONRCNew(all);
    /* The result from this method will have a set of associated items which are incremented/decremented with the
     * parent item. This allows for a reference counted shallow collection of flags/segments. */
    LDJSONRCAssociate(*result, associatedRcItems, associatedCount);

    /* Failure would be that the store was not working, not that we couldn't find the item. The memory store
     * is always functional, so always return true. */
    return LDBooleanTrue;
}

static LDBoolean
storeUpsert(
        void *const contextRaw,
        enum FeatureKind kind,
        const char *const key,
        struct LDJSON *const item)
{
    struct LDMemoryItem *existing;
    struct MemoryStoreContext* msCtx = MS_CONTEXT(contextRaw);

    LD_ASSERT(msCtx);
    LD_ASSERT(key);
    LD_ASSERT(item);

    if (!LDi_validateData(item)) {
        LD_LOG(LD_LOG_ERROR, "memory store storeUpsert invalid feature");

        LDJSONFree(item);

        return LDBooleanFalse;
    }

    LDi_rwlock_wrlock(&msCtx->lock);

    getFromStore(msCtx, kind, key, &existing);

    if(existing) {
        if(existing->value) {
            unsigned int itemVersion = LDi_getDataVersion(item);
            unsigned int existingVersion = LDi_getDataVersion(LDJSONRCGet(existing->value));
            if (existingVersion >= itemVersion) {
                /* The store contains the same or newer version, so no work needs to be done. */
                LDJSONFree(item);

                LDi_rwlock_wrunlock(&msCtx->lock);
                return LDBooleanTrue;
            }
        }
        removeAndDeleteItem(msCtx, kind, existing);
    }
    addToStore(msCtx, kind, makeMemoryItem(LDi_getDataKey(item), item));
    LDi_rwlock_wrunlock(&msCtx->lock);
    return LDBooleanTrue;
}

static LDBoolean
storeInitialized(void *const contextRaw)
{
    LDBoolean initialized;

    struct MemoryStoreContext* msCtx = MS_CONTEXT(contextRaw);
    LD_ASSERT(msCtx);

    LDi_rwlock_wrlock(&msCtx->lock);
    initialized = msCtx->initialized;
    LDi_rwlock_wrunlock(&msCtx->lock);
    return initialized;
}

static void
storeDestructor(void *const contextRaw)
{
    struct MemoryStoreContext *msCtx = NULL;
    if(contextRaw) {
        msCtx = MS_CONTEXT(contextRaw);

        clearStore(msCtx);

        LDi_rwlock_destroy(&msCtx->lock);

        LDFree(contextRaw);
    }
}
/* endregion */

struct LDInternalStoreInterface *
LDStoreMemoryNew(void) {
    struct LDInternalStoreInterface *memoryStore = NULL;
    struct MemoryStoreContext *context = NULL;

    memoryStore = LDAlloc(sizeof(struct LDInternalStoreInterface));
    LD_ASSERT(memoryStore);
    memset(memoryStore, 0, sizeof(struct LDInternalStoreInterface));

    context = LDAlloc(sizeof(struct MemoryStoreContext));
    LD_ASSERT(context);
    memset(context, 0, sizeof(struct MemoryStoreContext));

    context->initialized = LDBooleanFalse;
    context->features = NULL;
    context->segments = NULL;
    LDi_rwlock_init(&context->lock);

    memoryStore->context = context;
    memoryStore->init = storeInit;
    memoryStore->get = storeGet;
    memoryStore->all = storeAll;
    memoryStore->upsert = storeUpsert;
    memoryStore->initialized = storeInitialized;
    memoryStore->destructor = storeDestructor;

    return memoryStore;
}
