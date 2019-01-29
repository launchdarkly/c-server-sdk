#include "ldinternal.h"
#include "ldstore.h"

/* **** Forward Declarations **** */

static bool memoryInit(void *const rawcontext, struct LDVersionedSet *const sets);

static struct LDVersionedData *memoryGet(void *const rawcontext, const char *const key, const struct LDVersionedDataKind *const kind);

static struct LDVersionedData *memoryAll(void *const rawcontext, const struct LDVersionedDataKind *const kind);

static bool memoryDelete(void *const rawcontext, const struct LDVersionedDataKind *const kind, const char *const key, const unsigned int version);

static bool memoryUpsert(void *const rawcontext, const struct LDVersionedDataKind *const kind, struct LDVersionedData *replacement);

static bool memoryInitialized(void *const rawcontext);

static void memoryDestructor(void *const rawcontext);

/* **** Memory Implementation **** */

struct MemoryContext {
    bool initialized;
    struct LDVersionedSet *store;
};

static bool
memoryInit(void *const rawcontext, struct LDVersionedSet *const sets)
{
    struct MemoryContext *const context = rawcontext;

    LD_ASSERT(context);

    context->initialized = true;
    context->store       = sets;

    return true;
}

static struct LDVersionedData *
memoryGet(void *const rawcontext, const char *const key, const struct LDVersionedDataKind *const kind)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedSet *set = NULL;
    struct LDVersionedData *current = NULL;

    LD_ASSERT(context); LD_ASSERT(key); LD_ASSERT(kind);

    HASH_FIND_STR(context->store, kind->getNamespace(), set);

    if (!set) {
        return NULL;
    }

    HASH_FIND_STR(set->elements, key, current);

    if (!current || (current && current->isDeleted(current->data))) {
        return NULL;
    } else {
        return current->deepCopy(current->data);
    }
}

static struct LDVersionedData *
memoryAll(void *const rawcontext, const struct LDVersionedDataKind *const kind)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedSet *set = NULL;
    struct LDVersionedData *result = NULL, *iter = NULL, *tmp = NULL;

    LD_ASSERT(context); LD_ASSERT(kind);

    HASH_FIND_STR(context->store, kind->getNamespace(), set);

    if (!set) {
        return false;
    }

    HASH_ITER(hh, set->elements, iter, tmp) {
        struct LDVersionedData *const duplicate = iter->deepCopy(iter->data);
        const char *const key = duplicate->getKey(duplicate->data);

        if (!duplicate) {
            /* TODO free already duplicated elements */
            return NULL;
        }

        HASH_ADD_KEYPTR(hh, result, key, key, duplicate);
    }

    return result;
}

static bool
memoryDelete(void *const rawcontext, const struct LDVersionedDataKind *const kind, const char *const key, const unsigned int version)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedData *placeholder = NULL;

    LD_ASSERT(context); LD_ASSERT(kind); LD_ASSERT(key);

    placeholder = kind->makeDeletedItem(key, version);

    if (!placeholder) {
        return false;
    }

    return memoryUpsert(rawcontext, kind, placeholder);
}

static bool
memoryUpsert(void *const rawcontext, const struct LDVersionedDataKind *const kind, struct LDVersionedData *replacement)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedSet *set = NULL;
    struct LDVersionedData *current = NULL;

    LD_ASSERT(context); LD_ASSERT(kind); LD_ASSERT(replacement);

    HASH_FIND_STR(context->store, kind->getNamespace(), set);

    if (!set) {
        return false;
    }

    HASH_FIND_STR(set->elements, replacement->getKey(replacement->data), current);

    if (!current || current->isDeleted(current->data) || current->getVersion(current->data) < replacement->getVersion(replacement->data)) {
        const char *const key = replacement->getKey(replacement->data);

        if (current) {
            HASH_DEL(set->elements, current);
            current->destructor(current->data);
        }

        HASH_ADD_KEYPTR(hh, set->elements, key, strlen(key), replacement);
    } else {
        replacement->destructor(replacement->data);
    }

    return true;
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

    struct LDVersionedSet *set = NULL, *tmp = NULL;

    LD_ASSERT(context);

    HASH_ITER(hh, context->store, set, tmp) {
        HASH_DEL(context->store, set);
    }

    free(context);
}

struct LDFeatureStore *
makeInMemoryStore()
{
    struct MemoryContext *context = NULL;
    struct LDFeatureStore *const store = malloc(sizeof(struct LDFeatureStore));

    if (!store) {
        return NULL;
    }

    if (!(context = malloc(sizeof(struct MemoryContext)))) {
        free(store);

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
freeInMemoryStore(struct LDFeatureStore *const store)
{
    if (store) {
        if (store->destructor) {
            store->destructor(store->context);
        }

        free(store);
    }
}
