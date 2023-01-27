#pragma once

#include <launchdarkly/api.h>

#include "config.h"
#include "store/ldjsonrc.h"

/* **** Internal Store Types *** */

struct LDStore;

struct LDStore *
LDStoreNew(const struct LDConfig *const config);

/*******************************************************************************
 * @name Store convenience functions
 * Allows treating `LDStore` as more of an object
 * @{
 *******************************************************************************/

enum FeatureKind
{
    LD_FLAG,
    LD_SEGMENT
};

/** @brief A convenience wrapper around `store->init`.
 *
 * Input is consumed even on failure.
 */
LDBoolean
LDStoreInit(struct LDStore *const store, struct LDJSON *const sets);

/** @brief A convenience wrapper around `store->get`. */
LDBoolean
LDStoreGet(
    struct LDStore *const   store,
    const enum FeatureKind  kind,
    const char *const       key,
    struct LDJSONRC **const result);

/** @brief A convenience wrapper around `store->all`. */
LDBoolean
LDStoreAll(
    struct LDStore *const   store,
    const enum FeatureKind  kind,
    struct LDJSONRC **const result);

/** @brief A convenience wrapper around `store->remove`. */
LDBoolean
LDStoreRemove(
    struct LDStore *const  store,
    const enum FeatureKind kind,
    const char *const      key,
    const unsigned int     version);

/** @brief A convenience wrapper around `store->upsert`.
 *
 * Input is consumed even on failure.
 */
LDBoolean
LDStoreUpsert(
    struct LDStore *const  store,
    const enum FeatureKind kind,
    struct LDJSON *const   feature);

/** @brief A convenience wrapper around `store->initialized`. */
LDBoolean
LDStoreInitialized(struct LDStore *const store);

/** @brief A convenience wrapper around `store->destructor.` */
void
LDStoreDestroy(struct LDStore *const store);

/** @brief Calls LDStoreInit with empty sets. */
LDBoolean
LDStoreInitEmpty(struct LDStore *const store);

void
LDi_expireAll(struct LDStore *const store);

/*@}*/
