#include "uthash.h"

#include "ldinternal.h"
#include "ldstore.h"

/* **** Forward Declarations **** */

static bool memoryInit(void *const rawcontext, struct LDJSON *const sets);

static bool memoryGet(void *const rawcontext, const enum FeatureKind kind,
    const char *const key, struct LDJSONRC **const result);

static bool memoryAll(void *const rawcontext, const enum FeatureKind kind,
    struct LDJSON **const result);

static bool memoryDelete(void *const rawcontext, const enum FeatureKind kind,
    const char *const key, const unsigned int version);

static bool memoryUpsert(void *const rawcontext, const enum FeatureKind kind,
    struct LDJSON *const feature);

static bool memoryInitialized(void *const rawcontext);

static void memoryDestructor(void *const rawcontext);

/* ***** Reference counting **** */

struct LDJSONRC {
    struct LDJSON *value;
    ld_mutex_t lock;
    unsigned int count;
};

struct FeatureCollection {
    struct LDJSONRC *feature;
    UT_hash_handle hh;
};

struct FeatureCollection *
makeFeatureCollection(struct LDJSONRC *feature)
{
    struct FeatureCollection *result;

    LD_ASSERT(feature);

    if (!(result = malloc(sizeof(struct FeatureCollection)))) {
        return NULL;
    }

    memset(result, 0, sizeof(struct FeatureCollection));

    result->feature = feature;

    return result;
}

struct LDJSONRC *
LDJSONRCNew(struct LDJSON *const json)
{
    struct LDJSONRC *result;

    if (!(result = malloc(sizeof(struct LDJSONRC)))) {
        return NULL;
    }

    if (!LDi_mtxinit(&result->lock)) {
        LDFree(result);
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
        rc->count++;

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

struct MemoryContext {
    bool initialized;
    struct FeatureCollection *flags;
    struct FeatureCollection *segments;
    ld_rwlock_t lock;
};

static bool
addFeatures(struct FeatureCollection **set,
    struct LDJSON *const features)
{
    struct LDJSONRC *rc;
    struct FeatureCollection *item;
    struct LDJSON *keyjson, *iter;
    const char *key;

    for (iter = LDGetIter(features); iter; iter = LDIterNext(iter)) {
        if (!(keyjson = LDObjectLookup(iter, "key"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(keyjson) != LDText) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        LD_ASSERT(key = LDGetText(keyjson));

        if (!(rc = LDJSONRCNew(iter))) {
            return false;
        }

        if (!(item = makeFeatureCollection(rc))) {
            LDJSONRCDestroy(rc);

            return false;
        }

        HASH_ADD_KEYPTR(hh, *set, key, strlen(key), item);
    }

    return true;
}

static bool
memoryInit(void *const rawcontext, struct LDJSON *const sets)
{
    struct MemoryContext *context;
    struct LDJSON *flags, *segments;

    LD_ASSERT(rawcontext);
    LD_ASSERT(sets);
    LD_ASSERT(LDJSONGetType(sets) == LDObject);

    context = rawcontext;

    if (!(flags = LDObjectLookup(sets, "flags"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDJSONGetType(flags) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (!(segments = LDObjectLookup(sets, "segments"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDJSONGetType(flags) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (context->initialized){
        struct FeatureCollection *current, *tmp;

        HASH_ITER(hh, context->flags, current, tmp) {
            HASH_DEL(context->flags, current);

            LDJSONRCDecrement(current->feature);

            LDFree(current);
        }

        HASH_ITER(hh, context->segments, current, tmp) {
            HASH_DEL(context->segments, current);

            LDJSONRCDecrement(current->feature);

            LDFree(current);
        }
    }

    context->initialized = false;
    context->flags       = NULL;
    context->segments    = NULL;

    /* fill store from sets */

    addFeatures(&context->flags, flags);
    addFeatures(&context->segments, segments);

    context->initialized = true;

    LD_ASSERT(LDi_rdunlock(&context->lock));

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
memoryGet(void *const rawcontext, const enum FeatureKind kind,
    const char *const key, struct LDJSONRC **const result)
{
    struct FeatureCollection **set, *current;
    struct MemoryContext *context;

    LD_ASSERT(rawcontext);
    LD_ASSERT(key);
    LD_ASSERT(result);

    set     = NULL;
    current = NULL;
    context = rawcontext;
    *result = NULL;

    if (kind == LD_FLAG) {
        set = &context->flags;
    } else if (kind == LD_SEGMENT) {
        set = &context->segments;
    } else {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    LD_ASSERT(LDi_rdlock(&context->lock));

    HASH_FIND_STR(*set, key, current);

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
memoryAll(void *const rawcontext, const enum FeatureKind kind,
    struct LDJSON **const result)
{
    struct MemoryContext *context;
    struct FeatureCollection **set, *current, *tmp;
    struct LDJSON *object, *item;

    LD_ASSERT(rawcontext);
    LD_ASSERT(result);

    object  = NULL;
    *result = NULL;
    context = rawcontext;

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (kind == LD_FLAG) {
        set = &context->flags;
    } else if (kind == LD_SEGMENT) {
        set = &context->segments;
    } else {
        LD_LOG(LD_LOG_FATAL, "invalid feature kind");

        LD_ASSERT(false);
    }

    if (!(object = LDNewObject())) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    HASH_ITER(hh, *set, current, tmp) {
        item = LDJSONRCGet(current->feature);

        if (!LDi_isDeleted(item)) {
            struct LDJSON *duplicate;
            const char *key;

            if (!(duplicate = LDJSONDuplicate(item))) {
                LDJSONFree(object);

                LD_ASSERT(LDi_rdunlock(&context->lock));

                return false;
            }

            LD_ASSERT(key = LDGetText(LDObjectLookup(item, "key")));

            if (!LDObjectSetKey(object, key, duplicate)) {
                LDJSONFree(object);
                LDJSONFree(duplicate);

                LD_ASSERT(LDi_rdunlock(&context->lock));

                return false;
            }
        }
    }

    LD_ASSERT(LDi_rdunlock(&context->lock));

    *result = object;

    return true;
}

static bool
memoryDelete(void *const rawcontext, const enum FeatureKind kind,
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
memoryUpsert(void *const rawcontext, const enum FeatureKind kind,
    struct LDJSON *const replacement)
{
    struct MemoryContext *context;
    struct LDJSON *keyjson;
    const char *key;
    struct FeatureCollection **set, *current;

    LD_ASSERT(rawcontext);
    LD_ASSERT(replacement);

    set     = NULL;
    current = NULL;
    keyjson = NULL;
    context = rawcontext;
    key     = NULL;

    LD_ASSERT(LDi_wrlock(&context->lock));

    if (kind == LD_FLAG) {
        set = &context->flags;
    } else if (kind == LD_SEGMENT) {
        set = &context->segments;
    } else {
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

    HASH_FIND_STR(*set, key, current);

    if (current) {
        struct LDJSON *currentversion, *replacementversion, *feature;
        struct LDJSONRC *replacementrc;
        struct FeatureCollection *replacementcollection;

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

        HASH_DEL(*set, current);
        LDFree(current);

        HASH_ADD_KEYPTR(hh, *set, key, strlen(key), replacementcollection);
    } else {
        struct LDJSONRC *replacementrc;
        struct FeatureCollection *replacementcollection;

        if (!(replacementrc = LDJSONRCNew(replacement))) {
            goto error;
        }

        if (!(replacementcollection = makeFeatureCollection(replacementrc))) {
            LDJSONRCDecrement(replacementrc);

            LDJSONFree(replacement);

            return false;
        }

        HASH_ADD_KEYPTR(hh, *set, key, strlen(key), replacementcollection);
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
    struct FeatureCollection *current, *tmp;

    HASH_ITER(hh, context->flags, current, tmp) {
        HASH_DEL(context->flags, current);

        LDJSONRCDecrement(current->feature);

        LDFree(current);
    }

    HASH_ITER(hh, context->segments, current, tmp) {
        HASH_DEL(context->segments, current);

        LDJSONRCDecrement(current->feature);

        LDFree(current);
    }

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
    context->flags       = NULL;
    context->segments    = NULL;

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
    free(store);
    free(context);
    return NULL;
}

/* **** Covenience Operations **** */

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

    return store->get(store->context, kind, key, result);
}

bool
LDStoreAll(const struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSON **const result)
{
    LD_ASSERT(store);
    LD_ASSERT(result);

    return store->all(store->context, kind, result);
}

bool
LDStoreDelete(const struct LDStore *const store, const enum FeatureKind kind,
    const char *const key, const unsigned int version)
{
    LD_ASSERT(store);

    return store->delete(store->context, kind, key, version);
}

bool
LDStoreUpsert(const struct LDStore *const store, const enum FeatureKind kind,
    struct LDJSON *const feature)
{
    LD_ASSERT(store);
    LD_ASSERT(feature);

    return store->upsert(store->context, kind, feature);
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

        free(store);
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

    if (!(LDObjectSetKey(values, "flags", tmp))) {
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
