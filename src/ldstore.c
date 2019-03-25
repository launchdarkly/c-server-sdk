#include "uthash.h"

#include "ldinternal.h"
#include "ldstore.h"

/* **** Forward Declarations **** */

static const char *const LD_SS_FEATURES = "features";
static const char *const LD_SS_SEGMENTS = "segments";

static bool memoryInit(void *const rawcontext, struct LDJSON *const sets);

static bool memoryGet(void *const rawcontext, const char *const kind,
    const char *const key, struct LDJSONRC **const result);

static bool memoryAll(void *const rawcontext, const char *const kind,
    struct LDJSONRC ***const result);

static bool memoryDelete(void *const rawcontext, const char *const kind,
    const char *const key, const unsigned int version);

static bool memoryUpsert(void *const rawcontext, const char *const kind,
    struct LDJSON *const feature);

static bool memoryInitialized(void *const rawcontext);

static void memoryDestructor(void *const rawcontext);

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

    if (!(result = LDAlloc(sizeof(struct LDJSONRC)))) {
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

void
LDJSONRCDestroy(struct LDJSONRC *const rc)
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

            LDJSONRCDestroy(rc);
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
    char *namespace;
    struct FeatureCollectionItem *items;
    UT_hash_handle hh;
};

static struct FeatureCollectionItem *
makeFeatureCollection(struct LDJSONRC *const feature)
{
    struct FeatureCollectionItem *result;

    LD_ASSERT(feature);

    if (!(result = LDAlloc(sizeof(struct FeatureCollectionItem)))) {
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

    if (!(collection = LDAlloc(sizeof(struct FeatureCollection)))) {
        return false;
    }

    memset(collection, 0, sizeof(struct FeatureCollection));

    if (!(collection->namespace = LDStrDup(LDIterKey(features)))) {
        LDFree(collection);

        return false;
    }

    HASH_ADD_KEYPTR(hh, *collections, collection->namespace,
        strlen(collection->namespace), collection);

    for (iter = LDGetIter(features); iter;) {
        const char *key;
        struct LDJSON *keyjson;

        next = LDIterNext(iter);

        if (!(keyjson = LDObjectLookup(iter, "key"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(keyjson) != LDText) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        LD_ASSERT(key = LDGetText(keyjson));

        if (!(rc = LDJSONRCNew(LDCollectionDetachIter(features, iter)))) {
            return false;
        }

        if (!(item = makeFeatureCollection(rc))) {
            LDJSONRCDestroy(rc);

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

        LDFree(collection->namespace);
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

    context = rawcontext;

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

bool
LDi_isDeleted(const struct LDJSON *const feature)
{
    struct LDJSON *deleted;

    LD_ASSERT(feature);
    LD_ASSERT(LDJSONGetType(feature) == LDObject);

    deleted = LDObjectLookup(feature, "deleted");

    return deleted && LDJSONGetType(deleted) == LDBool && LDGetBool(deleted);
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
    context    = rawcontext;
    *result    = NULL;
    collection = NULL;

    LD_ASSERT(LDi_rdlock(&context->lock));

    HASH_FIND_STR(context->collections, kind, collection);

    if (!collection) {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    HASH_FIND_STR(collection->items, key, current);

    if (current) {
        if (LDi_isDeleted(LDJSONRCGet(current->feature))) {
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
    context    = rawcontext;
    collection = NULL;
    count      = 0;
    set        = NULL;

    LD_ASSERT(LDi_rdlock(&context->lock));

    HASH_FIND_STR(context->collections, kind, set);

    if (!set) {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    HASH_ITER(hh, set->items, current, tmp) {
        if (!LDi_isDeleted(LDJSONRCGet(current->feature))) {
            count++;
        }
    }

    if (count == 0) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return true;
    }

    if (!(collection = malloc(sizeof(struct LDJSONRC *) * (count + 1)))) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    collectioniter = collection;

    HASH_ITER(hh, set->items, current, tmp) {
        if (!LDi_isDeleted(LDJSONRCGet(current->feature))) {
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
memoryDelete(void *const rawcontext, const char *const kind,
    const char *const key, const unsigned int version)
{
    struct LDJSON *placeholder, *temp;

    LD_ASSERT(rawcontext);
    LD_ASSERT(key);

    placeholder = NULL;
    temp        = NULL;

    if (!(placeholder = LDNewObject())) {
        return false;
    }

    if (!(temp = LDNewBool(true))) {
        goto error;
    }

    if (!LDObjectSetKey(placeholder, "deleted", temp)) {
        goto error;
    }

    if (!(temp = LDNewText(key))) {
        goto error;
    }

    if (!LDObjectSetKey(placeholder, "key", temp)) {
        LDJSONFree(temp);

        goto error;
    }

    if (!(temp = LDNewNumber(version))) {
        goto error;
    }

    if (!LDObjectSetKey(placeholder, "version", temp)) {
        LDJSONFree(temp);

        goto error;
    }

    return memoryUpsert(rawcontext, kind, placeholder);

  error:
    LDJSONFree(placeholder);

    return false;
}

static bool
memoryUpsert(void *const rawcontext, const char *const kind,
    struct LDJSON *const replacement)
{
    struct MemoryContext *context;
    struct LDJSON *keyjson;
    const char *key;
    struct FeatureCollection *collection;
    struct FeatureCollectionItem *current;

    LD_ASSERT(rawcontext);
    LD_ASSERT(replacement);

    current    = NULL;
    keyjson    = NULL;
    context    = rawcontext;
    key        = NULL;
    collection = NULL;

    LD_ASSERT(LDi_wrlock(&context->lock));

    HASH_FIND_STR(context->collections, kind, collection);

    if (!collection) {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    if (!(keyjson = LDObjectLookup(replacement, "key"))) {
        goto error;
    }

    if (LDJSONGetType(keyjson) != LDText) {
        goto error;
    }

    LD_ASSERT(key = LDGetText(keyjson));

    HASH_FIND_STR(collection->items, key, current);

    if (current) {
        struct LDJSON *currentversion, *replacementversion, *feature;
        struct LDJSONRC *replacementrc;
        struct FeatureCollectionItem *replacementcollection;

        feature = LDJSONRCGet(current->feature);

        currentversion     = NULL;
        replacementversion = NULL;
        replacementrc      = NULL;

        if (!(currentversion = LDObjectLookup(feature, "version"))) {
            goto error;
        }

        if (LDJSONGetType(currentversion) != LDNumber) {
            goto error;
        }

        if (!(replacementversion = LDObjectLookup(replacement, "version"))) {
            goto error;
        }

        if (LDJSONGetType(replacementversion) != LDNumber) {
            goto error;
        }

        if (LDGetNumber(currentversion) >= LDGetNumber(replacementversion)) {
            LD_ASSERT(LDi_wrunlock(&context->lock));

            LDJSONFree(replacement);

            return true;
        }

        if (!(replacementrc = LDJSONRCNew(replacement))) {
            goto error;
        }

        if (!(replacementcollection = makeFeatureCollection(replacementrc))) {
            LDJSONRCDecrement(replacementrc);

            LDJSONFree(replacement);

            return false;
        }

        LDJSONRCDecrement(current->feature);

        HASH_DEL(collection->items, current);
        LDFree(current);

        HASH_ADD_KEYPTR(hh, collection->items, key, strlen(key),
            replacementcollection);
    } else {
        struct LDJSONRC *replacementrc;
        struct FeatureCollectionItem *replacementcollection;

        if (!(replacementrc = LDJSONRCNew(replacement))) {
            goto error;
        }

        if (!(replacementcollection = makeFeatureCollection(replacementrc))) {
            LDJSONRCDecrement(replacementrc);

            LDJSONFree(replacement);

            return false;
        }

        HASH_ADD_KEYPTR(hh, collection->items, key, strlen(key),
            replacementcollection);
    }

    LD_ASSERT(LDi_wrunlock(&context->lock));

    return true;

  error:
    LD_ASSERT(LDi_wrunlock(&context->lock));

    LDJSONFree(replacement);

    return false;
}

static bool
memoryInitialized(void *const rawcontext)
{
    struct MemoryContext *const context = rawcontext;

    LD_ASSERT(context);

    return context->initialized;
}

static void
memoryDestructor(void *const rawcontext)
{
    struct MemoryContext *const context = rawcontext;

    LD_ASSERT(context);

    memoryClearCollections(context);

    LDi_rwlockdestroy(&context->lock);

    LDFree(context);
}

struct LDStore *
LDMakeInMemoryStore()
{
    struct LDStore *store;
    struct MemoryContext *context;

    context = NULL;
    store   = NULL;

    if (!(store = LDAlloc(sizeof(struct LDStore)))) {
        goto error;
    }

    if (!(context = LDAlloc(sizeof(struct MemoryContext)))) {
        goto error;
    }

    if (!LDi_rwlockinit(&context->lock)) {
        goto error;
    }

    context->initialized = false;
    context->collections = NULL;

    store->context       = context;
    store->init          = memoryInit;
    store->get           = memoryGet;
    store->all           = memoryAll;
    store->delete        = memoryDelete;
    store->upsert        = memoryUpsert;
    store->initialized   = memoryInitialized;
    store->destructor    = memoryDestructor;

    return store;

  error:
    LDFree(store);
    LDFree(context);

    return NULL;
}

/* **** Covenience Operations **** */

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
LDStoreInit(const struct LDStore *const store, struct LDJSON *const sets)
{
    LD_ASSERT(store);
    LD_ASSERT(sets);

    return store->init(store->context, sets);
}

bool
LDStoreGet(const struct LDStore *const store, const enum FeatureKind kind,
    const char *const key, struct LDJSONRC **const result)
{
    LD_ASSERT(store);
    LD_ASSERT(key);
    LD_ASSERT(result);

    return store->get(store->context, featureKindToString(kind), key, result);
}

bool
LDStoreAll(const struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSONRC ***const result)
{
    LD_ASSERT(store);
    LD_ASSERT(result);

    return store->all(store->context, featureKindToString(kind), result);
}

bool
LDStoreDelete(const struct LDStore *const store, const enum FeatureKind kind,
    const char *const key, const unsigned int version)
{
    LD_ASSERT(store);

    return store->delete(store->context, featureKindToString(kind), key,
        version);
}

bool
LDStoreUpsert(const struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSON *const feature)
{
    LD_ASSERT(store);
    LD_ASSERT(feature);

    return store->upsert(store->context, featureKindToString(kind), feature);
}

bool
LDStoreInitialized(const struct LDStore *const store)
{
    LD_ASSERT(store);

    return store->initialized(store->context);
}

void
LDStoreDestroy(struct LDStore *const store)
{
    if (store) {
        if (store->destructor) {
            store->destructor(store->context);
        }

        LDFree(store);
    }
}

bool
LDStoreInitEmpty(struct LDStore *const store)
{
    struct LDJSON *tmp, *values;

    LD_ASSERT(store);

    tmp    = NULL;
    values = NULL;

    if (!(values = LDNewObject())) {
        return false;
    }

    if (!(tmp = LDNewObject())) {
        LDJSONFree(values);

        return false;
    }

    if (!(LDObjectSetKey(values, "segments", tmp))) {
        LDJSONFree(values);
        LDJSONFree(tmp);

        return false;
    }

    if (!(tmp = LDNewObject())) {
        LDJSONFree(values);

        return false;
    }

    if (!(LDObjectSetKey(values, "features", tmp))) {
        LDJSONFree(values);
        LDJSONFree(tmp);

        return false;
    }

    if (!(LDStoreInit(store, values))) {
        LDJSONFree(values);

        return false;
    }

    return true;
}
