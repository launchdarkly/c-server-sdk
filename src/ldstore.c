#include "ldinternal.h"
#include "ldstore.h"

/* **** Forward Declarations **** */

static bool memoryInit(void *const rawcontext, struct LDVersionedSet *const sets);

static struct LDVersionedData *memoryGet(void *const rawcontext, const char *const key, const struct LDVersionedDataKind kind);

static struct LDVersionedData *memoryAll(void *const rawcontext, const struct LDVersionedDataKind kind);

static bool memoryDelete(void *const rawcontext, const struct LDVersionedDataKind kind, const char *const key, const unsigned int version);

static bool memoryUpsert(void *const rawcontext, const struct LDVersionedDataKind kind, struct LDVersionedData *replacement);

static bool memoryInitialized(void *const rawcontext);

static void memoryDestructor(void *const rawcontext);

static void memoryFinalizeAll(void *const context, struct LDVersionedData *const all);

static void memoryFinalizeGet(void *const context, struct LDVersionedData *const value);

/* **** Kind Interfaces **** */

static const char *
flagKindNamespace() {
    return "features";
}

struct LDVersionedDataKind
getFlagKind()
{
    struct LDVersionedDataKind interface;
    interface.getNamespace = flagKindNamespace;

    return interface;
}

static bool
flagIsDeleted(const void *const flagraw)
{
    const struct FeatureFlag *const flag = flagraw;

    LD_ASSERT(flag);

    return flag->deleted;
}

static unsigned int
flagGetVersion(const void *const flagraw)
{
    const struct FeatureFlag *const flag = flagraw;

    LD_ASSERT(flag);

    return flag->version;
}

const char *
flagGetKey(const void *const flagraw)
{
    const struct FeatureFlag *const flag = flagraw;

    LD_ASSERT(flag);

    return flag->key;
}

static char *
flagSerialize(const void *const flagraw)
{
    const struct FeatureFlag *const flag = flagraw;

    LD_ASSERT(flag);

    return NULL;
}

static void
flagDestructor(void *const flagraw)
{
    struct FeatureFlag *const flag = flagraw;

    LD_ASSERT(flag);

    featureFlagFree(flag);
}

struct LDVersionedData *
flagToVersioned(struct FeatureFlag *const flag)
{
    struct LDVersionedData *interface = NULL;

    LD_ASSERT(flag);

    if (!(interface = malloc(sizeof(struct LDVersionedData)))) {
        return NULL;
    }

    memset(interface, 0, sizeof(struct LDVersionedData));

    interface->data       = flag;
    interface->isDeleted  = flagIsDeleted;
    interface->getVersion = flagGetVersion;
    interface->getKey     = flagGetKey;
    interface->serialize  = flagSerialize;
    interface->destructor = flagDestructor;

    return interface;
}

static const char *
segmentKindNamespace() {
    return "segments";
}

struct LDVersionedDataKind
getSegmentKind()
{
    struct LDVersionedDataKind interface;
    interface.getNamespace = segmentKindNamespace;

    return interface;
}

static bool
segmentIsDeleted(const void *const segmentraw)
{
    const struct Segment *const segment = segmentraw;

    LD_ASSERT(segment);

    return segment->deleted;
}

static unsigned int
segmentGetVersion(const void *const segmentraw)
{
    const struct Segment *const segment = segmentraw;

    LD_ASSERT(segment);

    return segment->version;
}

const char *
segmentGetKey(const void *const segmentraw)
{
    const struct Segment *const segment = segmentraw;

    LD_ASSERT(segment);

    return segment->key;
}

static char *
segmentSerialize(const void *const segmentraw)
{
    const struct Segment *const segment = segmentraw;

    LD_ASSERT(segment);

    return NULL;
}

static void
segmentDestructor(void *const segmentraw)
{
    struct Segment *const segment = segmentraw;

    LD_ASSERT(segment);

    segmentFree(segment);
}

struct LDVersionedData *
segmentToVersioned(struct Segment *const segment)
{
    struct LDVersionedData *interface = NULL;

    LD_ASSERT(segment);

    if (!(interface = malloc(sizeof(struct LDVersionedData)))) {
        return NULL;
    }

    memset(interface, 0, sizeof(struct LDVersionedData));

    interface->data       = segment;
    interface->isDeleted  = segmentIsDeleted;
    interface->getVersion = segmentGetVersion;
    interface->getKey     = segmentGetKey;
    interface->serialize  = segmentSerialize;
    interface->destructor = segmentDestructor;

    return interface;
}

/* **** Memory Implementation **** */

struct MemoryContext {
    bool initialized;
    struct LDVersionedSet *store;
    ld_rwlock_t lock;
};

static void
memoryFreeStore(struct LDVersionedSet *store)
{
    struct LDVersionedSet *set = NULL, *tmpset = NULL;
    struct LDVersionedData *data = NULL, *tmpdata = NULL;;

    HASH_ITER(hh, store, set, tmpset) {
        HASH_DEL(store, set);

        HASH_ITER(hh, set->elements, data, tmpdata) {
            HASH_DEL(set->elements, data);

            data->destructor(data->data);

            free(data);
        }

        free(set);
    }
}

static bool
memoryInit(void *const rawcontext, struct LDVersionedSet *const sets)
{
    struct MemoryContext *const context = rawcontext;

    LD_ASSERT(context);

    LD_ASSERT(LDi_rdlock(&context->lock));

    if (context->store){
        memoryFreeStore(context->store);
    }

    context->initialized = true;
    context->store       = sets;

    LD_ASSERT(LDi_rdunlock(&context->lock));

    return true;
}

static struct LDVersionedData *
memoryGet(void *const rawcontext, const char *const key, const struct LDVersionedDataKind kind)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedSet *set = NULL;
    struct LDVersionedData *current = NULL;

    LD_ASSERT(context); LD_ASSERT(key);

    LD_ASSERT(LDi_rdlock(&context->lock));

    HASH_FIND_STR(context->store, kind.getNamespace(), set);

    if (!set) {
        return NULL;
    }

    HASH_FIND_STR(set->elements, key, current);

    if (!current || (current && current->isDeleted(current->data))) {
        return NULL;
    } else {
        return current;
    }
}

static void
memoryFinalizeGet(void *const rawcontext, __attribute__((unused)) struct LDVersionedData *const value)
{
    struct MemoryContext *const context = rawcontext;

    LD_ASSERT(context);

    LD_ASSERT(LDi_rdunlock(&context->lock));

    /* no need to free all in memory context */
}

static struct LDVersionedData *
memoryAll(void *const rawcontext, const struct LDVersionedDataKind kind)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedSet *set = NULL;
    struct LDVersionedData *result = NULL, *iter = NULL, *tmp = NULL;

    LD_ASSERT(context);

    LD_ASSERT(LDi_rdlock(&context->lock));

    HASH_FIND_STR(context->store, kind.getNamespace(), set);

    if (!set) {
        LD_ASSERT(LDi_rdunlock(&context->lock));

        return false;
    }

    HASH_ITER(hh, set->elements, iter, tmp) {
        struct LDVersionedData *const duplicate = NULL;
        const char *const key = duplicate->getKey(duplicate->data);

        if (!duplicate) {
            LD_ASSERT(LDi_rdunlock(&context->lock));
            /* TODO free already duplicated elements */
            return NULL;
        }

        HASH_ADD_KEYPTR(hh, result, key, key, duplicate);
    }

    LD_ASSERT(LDi_rdunlock(&context->lock));

    return result;
}

static void
memoryFinalizeAll(void *const rawcontext, __attribute__((unused)) struct LDVersionedData *const all)
{
    struct MemoryContext *const context = rawcontext;

    LD_ASSERT(context);

    LD_ASSERT(LDi_rdunlock(&context->lock));

    /* no need to free all in memory context */
}

static bool
memoryDelete(void *const rawcontext, const struct LDVersionedDataKind kind, const char *const key, const unsigned int version)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedData *placeholder = NULL;

    LD_ASSERT(context); LD_ASSERT(key);

    placeholder = kind.makeDeletedItem(key, version);

    if (!placeholder) {
        return false;
    }

    return memoryUpsert(rawcontext, kind, placeholder);
}

static bool
memoryUpsert(void *const rawcontext, const struct LDVersionedDataKind kind, struct LDVersionedData *replacement)
{
    struct MemoryContext *const context = rawcontext;

    struct LDVersionedSet *set = NULL;
    struct LDVersionedData *current = NULL;

    LD_ASSERT(context); LD_ASSERT(replacement);

    LD_ASSERT(LDi_wrlock(&context->lock));

    HASH_FIND_STR(context->store, kind.getNamespace(), set);

    if (!set) {
        LD_ASSERT(LDi_wrunlock(&context->lock));

        return false;
    }

    HASH_FIND_STR(set->elements, replacement->getKey(replacement->data), current);

    if (!current || current->isDeleted(current->data) || current->getVersion(current->data) < replacement->getVersion(replacement->data)) {
        const char *const key = replacement->getKey(replacement->data);

        if (current) {
            HASH_DEL(set->elements, current);

            current->destructor(current->data);

            free(current);
        }

        HASH_ADD_KEYPTR(hh, set->elements, key, strlen(key), replacement);
    } else {
        replacement->destructor(replacement->data);

        free(replacement);
    }

    LD_ASSERT(LDi_wrunlock(&context->lock));

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

    memoryFreeStore(context->store);

    LDi_rwlockdestroy(&context->lock);

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
    store->finalizeGet   = memoryFinalizeGet;
    store->all           = memoryAll;
    store->finalizeAll   = memoryFinalizeAll;
    store->delete        = memoryDelete;
    store->upsert        = memoryUpsert;
    store->initialized   = memoryInitialized;
    store->destructor    = memoryDestructor;

    return store;
}

void
freeStore(struct LDFeatureStore *const store)
{
    if (store) {
        if (store->destructor) {
            store->destructor(store->context);
        }

        free(store);
    }
}
