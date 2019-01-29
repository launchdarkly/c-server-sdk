#include "ldinternal.h"
#include "ldstore.h"
#include "ldschema.h"

struct LDFeatureStore *
prepareEmptyStore()
{
    struct LDVersionedSet *sets = NULL;
    struct LDFeatureStore *store = NULL;

    const char *namespace = NULL;
    struct LDVersionedSet *set = NULL;

    LD_ASSERT(store = makeInMemoryStore());
    LD_ASSERT(!store->initialized(store->context));

    LD_ASSERT(set = malloc(sizeof(struct LDVersionedSet)));
    set->kind = getSegmentKind();
    set->elements = NULL;
    LD_ASSERT(namespace = set->kind.getNamespace());
    HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), set);

    set = malloc(sizeof(struct LDVersionedSet));
    LD_ASSERT(set);
    set->kind = getFlagKind();
    set->elements = NULL;
    LD_ASSERT(namespace = set->kind.getNamespace());
    HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), set);

    LD_ASSERT(store->init(store->context, sets));
    LD_ASSERT(store->initialized(store->context));

    return store;
}

void
allocateAndFree()
{
    struct LDFeatureStore *const store = prepareEmptyStore();

    freeStore(store);
}

void
deletedSegment()
{
    struct LDFeatureStore *store = NULL;
    struct Segment *segment = NULL;
    struct LDVersionedData *versioned = NULL;
    struct LDVersionedData *lookup = NULL;
    struct LDVersionedDataKind kind = getSegmentKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(segment = segmentMakeDeleted("abc", 123));

    LD_ASSERT(versioned = segmentToVersioned(segment));

    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT(!(lookup = store->get(store->context, "abc", kind)));

    store->finalizeGet(store->context, lookup);

    freeStore(store);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    allocateAndFree();
    deletedSegment();

    return 0;
}
