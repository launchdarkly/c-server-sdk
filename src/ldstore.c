#include "ldinternal.h"
#include "ldstore.h"

/* **** Forward Declarations **** */

static bool memoryInit(void *const rawcontext, struct LDJSON *const sets);

static struct LDJSON *memoryGet(void *const rawcontext, const char *const kind, const char *const key);

static struct LDJSON *memoryAll(void *const rawcontext, const char *const kind);

static bool memoryDelete(void *const rawcontext, const char *const kind, const char *const key, const unsigned int version);

static bool memoryUpsert(void *const rawcontext, const char *const kind, struct LDJSON *const feature);

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
    struct MemoryContext *const context = rawcontext;

    LD_ASSERT(context); LD_ASSERT(sets); LD_ASSERT(LDJSONGetType(sets) == LDArray);

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (context->store){
        LDJSONFree(context->store);
    }

    context->initialized = true;
    context->store       = sets;

    LD_ASSERT(LDi_rdunlock(&context->lock));

    return true;
}

static bool
isDeleted(const struct LDJSON *const feature)
{
    struct LDJSON *deleted = NULL;

    LD_ASSERT(deleted); LD_ASSERT(LDJSONGetType(deleted) == LDObject);

    deleted = LDObjectLookup(feature, "deleted");

    return deleted && LDJSONGetType(deleted) == LDBool && LDGetBool(deleted);
}

static struct LDJSON *
memoryGet(void *const rawcontext, const char *const kind, const char *const key)
{
    struct MemoryContext *const context = rawcontext;

    struct LDJSON *set = NULL, *current = NULL;

    LD_ASSERT(context); LD_ASSERT(kind); LD_ASSERT(key);

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (!(set = LDObjectLookup(context->store, kind))) {
        return NULL;
    }

    if ((current = LDObjectLookup(set, key)) && !isDeleted(current)) {
        struct LDJSON *const copy = LDJSONDuplicate(current);

        LD_ASSERT(LDi_rdunlock(&context->lock));

        return copy;
    } else {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return NULL;
    }
}

static struct LDJSON *
memoryAll(void *const rawcontext, const char *const kind)
{
    struct MemoryContext *const context = rawcontext;

    struct LDJSON *set = NULL, *result = NULL, *iter = NULL;

    LD_ASSERT(context);

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (!(set = LDObjectLookup(context->store, kind))) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return NULL;
    }

    if (!(result = LDNewObject())) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return NULL;
    }

    for (iter = LDGetIter(set); iter; iter = LDIterNext(iter)) {
        if (!isDeleted(iter)) {
            struct LDJSON *duplicate = NULL;

            if (!(duplicate = LDJSONDuplicate(iter))) {
                LDJSONFree(result);

                return NULL;
            }

            LDObjectSetKey(result, LDIterKey(iter), duplicate);
        }
    }

    LD_ASSERT(LDi_rdunlock(&context->lock));

    return result;
}

static bool
memoryDelete(void *const rawcontext, const char *const kind, const char *const key, const unsigned int version)
{
    struct MemoryContext *const context = rawcontext;

    struct LDJSON *placeholder = NULL, *temp = NULL;

    LD_ASSERT(context); LD_ASSERT(kind); LD_ASSERT(key);

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
memoryUpsert(void *const rawcontext, const char *const kind, struct LDJSON *const replacement)
{
    struct MemoryContext *const context = rawcontext;

    struct LDJSON *set = NULL, *current = NULL, *key = NULL;

    LD_ASSERT(context); LD_ASSERT(kind); LD_ASSERT(replacement);

    LD_ASSERT(LDi_wrlock(&context->lock));

    if (!(set = LDObjectLookup(context->store, kind))) {
        goto error;
    }

    if (!(key = LDObjectLookup(replacement, "key"))) {
        goto error;
    }

    if ((current = LDObjectLookup(set, LDGetText(key)))) {
        struct LDJSON *currentversion = NULL, *replacementversion = NULL;

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

        if (LDGetNumber(currentversion) < LDGetNumber(replacementversion)) {
            LDObjectSetKey(set, LDGetText(key), replacement);
        }
    } else {
        LDObjectSetKey(set, LDGetText(key), replacement);
    }

    LD_ASSERT(LDi_wrunlock(&context->lock));

    return true;

  error:
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

    free(context);
}

struct LDStore *
makeInMemoryStore()
{
    struct MemoryContext *context = NULL;
    struct LDStore *const store = malloc(sizeof(struct LDStore));

    if (!store) {
        return NULL;
    }

    if (!(context = malloc(sizeof(struct MemoryContext)))) {
        free(store);

        return NULL;
    }

    if (!LDi_rwlockinit(&context->lock)) {
        free(store);

        free(context);

        return NULL;
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
