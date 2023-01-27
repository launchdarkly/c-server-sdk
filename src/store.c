#include <stdio.h>

#include <uthash.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "store.h"
#include "store/caching_wrapper.h"
#include "store/memory_store.h"
#include "store/internal_store.h"
#include "store/store_utilities.h"

/* **** Forward Declarations **** */

struct LDMemoryContext;
struct LDCacheItem;

/* **** LDStore **** */

struct LDStore
{
    struct LDInternalStoreInterface *implementation;
};

struct LDStore *
LDStoreNew(const struct LDConfig *const config)
{
    struct LDStore *      store;

    LD_ASSERT(config);

    store = NULL;

    store = (struct LDStore *)LDAlloc(sizeof(struct LDStore));
    LD_ASSERT(store);
    memset(store, 0, sizeof(struct LDStore));

    if(config->storeBackend) {
        /* There is a configured back-end. We should wrap it in a caching wrapper. */
        store->implementation = LDStoreCachingWrapperNew(config->storeBackend, config->storeCacheMilliseconds);
    } else {
        /* There is no back-end, so we want a memory store. */
        store->implementation = LDStoreMemoryNew();
    }

    LD_ASSERT(store->implementation);

    return store;
}

/* **** Store API Operations **** */

LDBoolean
LDStoreInit(struct LDStore *const store, struct LDJSON *const sets)
{
    LD_LOG(LD_LOG_TRACE, "LDStoreInit");

    LD_ASSERT(store);
    LD_ASSERT(store->implementation);
    LD_ASSERT(store->implementation->init);
    LD_ASSERT(store->implementation->context);

    return store->implementation->init(store->implementation->context, sets);
}

LDBoolean
LDStoreGet(
    struct LDStore *const   store,
    const enum FeatureKind  kind,
    const char *const       key,
    struct LDJSONRC **const result)
{
    LD_LOG(LD_LOG_TRACE, "LDStoreGet");

    LD_ASSERT(store);
    LD_ASSERT(store->implementation);
    LD_ASSERT(store->implementation->get);
    LD_ASSERT(store->implementation->context);

    return store->implementation->get(store->implementation->context, kind, key, result);
}

LDBoolean
LDStoreAll(
    struct LDStore *const   store,
    const enum FeatureKind  kind,
    struct LDJSONRC **const result)
{
    LD_LOG(LD_LOG_TRACE, "LDStoreAll");

    LD_ASSERT(store);
    LD_ASSERT(store->implementation);
    LD_ASSERT(store->implementation->all);
    LD_ASSERT(store->implementation->context);

    return store->implementation->all(store->implementation->context, kind, result);
}

LDBoolean
LDStoreRemove(
    struct LDStore *const  store,
    const enum FeatureKind kind,
    const char *const      key,
    const unsigned int     version)
{
    struct LDJSON* item;

    LD_LOG(LD_LOG_TRACE, "LDStoreRemove");

    LD_ASSERT(store);
    LD_ASSERT(key);
    LD_ASSERT(store->implementation);
    LD_ASSERT(store->implementation->upsert);

    item = LDNewObject();
    LDObjectSetKey(item, "version", LDNewNumber(version));
    LDObjectSetKey(item, "key", LDNewText(key));
    LDObjectSetKey(item, "deleted", LDNewBool(LDBooleanTrue));

    return  store->implementation->upsert(store->implementation->context, kind, key, item);
}

LDBoolean
LDStoreUpsert(
    struct LDStore *const  store,
    const enum FeatureKind kind,
    struct LDJSON *const   feature)
{
    LD_LOG(LD_LOG_TRACE, "LDStoreUpsert");

    LD_ASSERT(store);
    LD_ASSERT(store->implementation);
    LD_ASSERT(store->implementation->upsert);

    if(LDi_validateData(feature)) {
        return  store->implementation->upsert(store->implementation->context, kind,
                                              LDi_getDataKey(feature), feature);
    } else {
        LDJSONFree(feature);
    }

    return LDBooleanFalse;
}

LDBoolean
LDStoreInitialized(struct LDStore *const store) {
    LD_LOG(LD_LOG_TRACE, "LDStoreInitialized");

    LD_ASSERT(store);
    LD_ASSERT(store->implementation);
    LD_ASSERT(store->implementation->context);
    LD_ASSERT(store->implementation->initialized);

    return store->implementation->initialized(store->implementation->context);
}

void
LDStoreDestroy(struct LDStore *const store)
{
    LD_LOG(LD_LOG_TRACE, "LDStoreDestroy");

    if (store) {
        if (store->implementation) {
            if (store->implementation->destructor) {
                store->implementation->destructor(store->implementation->context);
            }

            LDFree(store->implementation);
        }

        LDFree(store);
    }
}

LDBoolean
LDStoreInitEmpty(struct LDStore *const store)
{
    struct LDJSON *values;
    LDBoolean      success;

    LD_ASSERT(store);

    success = LDBooleanFalse;

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

void
LDi_expireAll(struct LDStore *const store) {
    LD_ASSERT(store);
    LD_ASSERT(store->implementation);
    LD_ASSERT(store->implementation->context);

    if(store->implementation->ldi_expireAll) {
        store->implementation->ldi_expireAll(store->implementation->context);
    }
}
