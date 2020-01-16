#pragma once

#include <launchdarkly/api.h>

#include "config.h"

/***************************************************************************//**
 * @name Reference counted wrapper for JSON
 * Used as an optimization to reduce allocations.
 * @{
 ******************************************************************************/

struct LDJSONRC;

struct LDJSONRC *LDJSONRCNew(struct LDJSON *const json);

void LDJSONRCIncrement(struct LDJSONRC *const rc);

void LDJSONRCDecrement(struct LDJSONRC *const rc);

struct LDJSON *LDJSONRCGet(struct LDJSONRC *const rc);

/*@}*/

/* **** Internal Store Types *** */

struct LDStore;

struct LDStore *LDStoreNew(const struct LDConfig *const config);

/* **** Flag Utilities **** */

bool LDi_validateFeature(const struct LDJSON *const feature);

struct LDJSON *LDi_makeDeleted(const char *const key,
    const unsigned int version);

bool LDi_isFeatureDeleted(const struct LDJSON *const feature);

/** @brief Get version of a non validated feature value */
unsigned int LDi_getFeatureVersion(const struct LDJSON *const feature);

/** @brief Get version of a validated feature value */
unsigned int LDi_getFeatureVersionTrusted(const struct LDJSON *const feature);

/** @briefUsed for testing */
void LDi_expireAll(struct LDStore *const store);

/***************************************************************************//**
 * @name Store convenience functions
 * Allows treating `LDStore` as more of an object
 * @{
 ******************************************************************************/

enum FeatureKind {
    LD_FLAG,
    LD_SEGMENT
};

/** @brief A convenience wrapper around `store->init`.
 *
 * Input is consumed even on failure.
 */
bool LDStoreInit(struct LDStore *const store, struct LDJSON *const sets);

/** @brief A convenience wrapper around `store->get`. */
bool LDStoreGet(struct LDStore *const store,
    const enum FeatureKind kind, const char *const key,
    struct LDJSONRC **const result);

/** @brief A convenience wrapper around `store->all`. */
bool LDStoreAll(struct LDStore *const store,
    const enum FeatureKind kind, struct LDJSONRC **const result);

/** @brief A convenience wrapper around `store->remove`. */
bool LDStoreRemove(struct LDStore *const store,
    const enum FeatureKind kind, const char *const key,
    const unsigned int version);

/** @brief A convenience wrapper around `store->upsert`.
 *
 * Input is consumed even on failure.
 */
bool LDStoreUpsert(struct LDStore *const store,
    const enum FeatureKind kind, struct LDJSON *const feature);

/** @brief A convenience wrapper around `store->initialized`. */
bool LDStoreInitialized(struct LDStore *const store);

/** @brief A convenience wrapper around `store->destructor.` */
void LDStoreDestroy(struct LDStore *const store);

/** @brief Calls LDStoreInit with empty sets. */
bool LDStoreInitEmpty(struct LDStore *const store);

/*@}*/
