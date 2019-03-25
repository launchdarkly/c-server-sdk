/*!
 * @file ldstore.h
 * @brief Public API Interface for Store implementatons
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "ldjson.h"
#include "ldinternal.h"

/***************************************************************************//**
 * @name Generic Store Interface
 * Used to provide all interaction with a feature store. Redis, Consul, Dynamo.
 * @{
 ******************************************************************************/

struct LDJSONRC;

struct LDJSONRC *LDJSONRCNew(struct LDJSON *const json);

void LDJSONRCIncrement(struct LDJSONRC *const rc);

void LDJSONRCDecrement(struct LDJSONRC *const rc);

struct LDJSON *LDJSONRCGet(struct LDJSONRC *const rc);

/*@}*/

/***************************************************************************//**
 * @name Generic Store Interface
 * Used to provide all interaction with a feature store. Redis, Consul, Dynamo.
 * @{
 ******************************************************************************/

/** @brief An Interface providing access to a store */
struct LDStore {
    /**
     * @brief Used to store implementation specific data
     */
    void *context;
    /**
     * @brief Initialize the feature store with a new data set
     * @param[in] sets A JSON object containing the new feature values.
     * This routine takes ownership of sets.
     * @return True on success, False on failure.
     */
    bool (*init)(void *const context, struct LDJSON *const sets);
    /**
     * @brief Fetch a feature from the store
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @param[in] kind The namespace to search in. May not be NULL (assert).
     * @param[in] key The key to return the value for. May not be NULL (assert).
     * @return Returns the feature, or NULL if it does not exist.
     */
    bool (*get)(void *const context, const char *const kind,
        const char *const key, struct LDJSONRC **const result);
    /**
     * @brief Fetch all features in a given namespace
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @param[in] kind The namespace to search in. May not be NULL (assert).
     * @return Returns an object map keys to features, NULL on failure.
     */
    bool (*all)(void *const context, const char *const kind,
        struct LDJSONRC ***const result);
    /**
     * @brief Mark an existing feature as deleted
     * (only virtually deletes to maintain version)
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @param[in] kind The namespace to search in. May not be NULL (assert).
     * @param[in] key The key to return the value for. May not be NULL (assert).
     * @param[in] version Version of event.
     * Only deletes if version is newer than the features.
     * @return True on success, False on failure.
     */
    bool (*delete)(void *const context, const char *const kind,
        const char *const key, const unsigned int version);
    /**
     * @brief Replace an existing feature with a newer one
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @param[in] kind The namespace to search in. May not be NULL (assert).
     * @param[in] feature The updated feature. Only deletes current if version
     * is newer. Takes ownership of feature.
     * @return True on success, False on failure.
     */
    bool (*upsert)(void *const context, const char *const kind,
        struct LDJSON *const feature);
    /**
     * @brief Determine if the store is initialized with features yet.
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @return True if the store is intialized, false otherwise.
     */
    bool (*initialized)(void *const context);
    /**
     * @brief Destroy the implementation specific context associated.
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @return Void.
     */
    void (*destructor)(void *const context);
};

/*@}*/

/***************************************************************************//**
 * @name Store convenience functions
 * Allows treating `LDStore` as more of an object
 * @{
 ******************************************************************************/

 enum FeatureKind {
    LD_FLAG,
    LD_SEGMENT
};

/** @brief A convenience wrapper around `store->init`. */
bool LDStoreInit(const struct LDStore *const store, struct LDJSON *const sets);

/** @brief A convenience wrapper around `store->get`. */
bool LDStoreGet(const struct LDStore *const store,
    const enum FeatureKind kind, const char *const key,
    struct LDJSONRC **const result);

/** @brief A convenience wrapper around `store->all`. */
bool LDStoreAll(const struct LDStore *const store,
    const enum FeatureKind kind, struct LDJSONRC ***const result);

/** @brief A convenience wrapper around `store->delete`. */
bool LDStoreDelete(const struct LDStore *const store,
    const enum FeatureKind kind, const char *const key,
    const unsigned int version);

/** @brief A convenience wrapper around `store->upsert`. */
bool LDStoreUpsert(const struct LDStore *const store,
    const enum FeatureKind kind, struct LDJSON *const feature);

/** @brief A convenience wrapper around `store->initialized`. */
bool LDStoreInitialized(const struct LDStore *const store);

/** @brief A convenience wrapper around `store->destructor.` */
void LDStoreDestroy(struct LDStore *const store);

/** @brief Calls LDStoreInit with empty sets. */
bool LDStoreInitEmpty(struct LDStore *const store);

/*@}*/

/***************************************************************************//**
 * @name Memory store
 * The default feature store with no external storage.
 * @{
 ******************************************************************************/

 /**
  * @brief The default store type.
  * @return The newly allocated store, or NULL on failure.
  */
struct LDStore *LDMakeInMemoryStore();

/*@}*/
