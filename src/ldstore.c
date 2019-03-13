#include "ldinternal.h"
#include "ldstore.h"

/* **** Forward Declarations **** */

static bool memoryInit(void *const rawcontext, struct LDJSON *const sets);

static bool memoryGet(void *const rawcontext, const char *const kind,
    const char *const key, struct LDJSON **const result);

static bool memoryAll(void *const rawcontext, const char *const kind,
    struct LDJSON **const result);

static bool memoryDelete(void *const rawcontext, const char *const kind,
    const char *const key, const unsigned int version);

static bool memoryUpsert(void *const rawcontext, const char *const kind,
    struct LDJSON *const feature);

static bool memoryInitialized(void *const rawcontext);

static void memoryDestructor(void *const rawcontext);


/* **** Memory Implementation **** */

struct MemoryContext {
    bool initialized;
    struct LDJSON *store;
    ld_rwlock_t lock;
};

static bool
memoryInit(void *const rawcontext, struct LDJSON *const sets)
{
    struct MemoryContext *context;;

    LD_ASSERT(rawcontext);
    LD_ASSERT(sets);
    LD_ASSERT(LDJSONGetType(sets) == LDObject);

    context = rawcontext;

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (context->store){
        LDJSONFree(context->store);
    }

    context->initialized = true;
    context->store       = sets;

    LD_ASSERT(LDi_rdunlock(&context->lock));

    return true;
}

bool
isDeleted(const struct LDJSON *const feature)
{
    struct LDJSON *deleted = NULL;

    LD_ASSERT(feature);
    LD_ASSERT(LDJSONGetType(feature) == LDObject);

    deleted = LDObjectLookup(feature, "deleted");

    return deleted && LDJSONGetType(deleted) == LDBool && LDGetBool(deleted);
}

static bool
memoryGet(void *const rawcontext, const char *const kind, const char *const key,
    struct LDJSON **const result)
{
    struct LDJSON *set, *current;
    struct MemoryContext *context;

    LD_ASSERT(rawcontext);
    LD_ASSERT(kind);
    LD_ASSERT(key);
    LD_ASSERT(result);

    set     = NULL;
    current = NULL;
    context = rawcontext;
    *result = NULL;

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (!(set = LDObjectLookup(context->store, kind))) {
        return false;
    }

    if ((current = LDObjectLookup(set, key))) {
        struct LDJSON *const copy = LDJSONDuplicate(current);

        LD_ASSERT(LDi_rdunlock(&context->lock));

        if (copy) {
            *result = copy;

            return true;
        } else {
            return false;
        }
    } else {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return true;
    }
}

static bool
memoryAll(void *const rawcontext, const char *const kind,
    struct LDJSON **const result)
{
    struct MemoryContext *context;
    struct LDJSON *set, *iter, *object;

    LD_ASSERT(rawcontext);
    LD_ASSERT(kind);
    LD_ASSERT(result);

    set     = NULL;
    iter    = NULL;
    object  = NULL;
    *result = NULL;
    context = rawcontext;

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (!(set = LDObjectLookup(context->store, kind))) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    if (!(object = LDNewObject())) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    for (iter = LDGetIter(set); iter; iter = LDIterNext(iter)) {
        if (!isDeleted(iter)) {
            struct LDJSON *duplicate = NULL;

            if (!(duplicate = LDJSONDuplicate(iter))) {
                LDJSONFree(object);

                return false;
            }

            LDObjectSetKey(object, LDIterKey(iter), duplicate);
        }
    }

    LD_ASSERT(LDi_rdunlock(&context->lock));

    *result = object;

    return true;
}

static bool
memoryDelete(void *const rawcontext, const char *const kind,
    const char *const key, const unsigned int version)
{
    struct LDJSON *placeholder, *temp;

    LD_ASSERT(rawcontext);
    LD_ASSERT(kind);
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
        goto error;
    }

    if (!(temp = LDNewNumber(version))) {
        goto error;
    }

    if (!LDObjectSetKey(placeholder, "version", temp)) {
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
    struct LDJSON *set, *current, *key;

    LD_ASSERT(rawcontext);
    LD_ASSERT(kind);
    LD_ASSERT(replacement);

    set     = NULL;
    current = NULL;
    key     = NULL;
    context = rawcontext;

    LD_ASSERT(LDi_wrlock(&context->lock));

    if (!(set = LDObjectLookup(context->store, kind))) {
        goto error;
    }

    if (!(key = LDObjectLookup(replacement, "key"))) {
        goto error;
    }

    if ((current = LDObjectLookup(set, LDGetText(key)))) {
        struct LDJSON *currentversion, *replacementversion;

        currentversion     = NULL;
        replacementversion = NULL;

        if (!(currentversion = LDObjectLookup(current, "version"))) {
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

        LDObjectSetKey(set, LDGetText(key), replacement);
    } else {
        LDObjectSetKey(set, LDGetText(key), replacement);
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

    LDJSONFree(context->store);

    LDi_rwlockdestroy(&context->lock);

    LDFree(context);
}

struct LDStore *
makeInMemoryStore()
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
    context->store       = NULL;

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
LDStoreGet(const struct LDStore *const store, const char *const kind,
    const char *const key, struct LDJSON **const result)
{
    LD_ASSERT(store);
    LD_ASSERT(key);
    LD_ASSERT(result);

    return store->get(store->context, kind, key, result);
}

bool
LDStoreAll(const struct LDStore *const store, const char *const kind,
    struct LDJSON **const result)
{
    LD_ASSERT(store);
    LD_ASSERT(kind);
    LD_ASSERT(result);

    return store->all(store->context, kind, result);
}

bool
LDStoreDelete(const struct LDStore *const store, const char *const kind,
    const char *const key, const unsigned int version)
{
    LD_ASSERT(store);
    LD_ASSERT(kind);

    return store->delete(store->context, kind, key, version);
}

bool
LDStoreUpsert(const struct LDStore *const store, const char *const key,
    struct LDJSON *const feature)
{
    LD_ASSERT(store);
    LD_ASSERT(key);
    LD_ASSERT(feature);

    return store->upsert(store->context, key, feature);
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
