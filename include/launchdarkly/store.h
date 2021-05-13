/*!
 * @file store.h
 * @brief Public API Interface for Store implementatons
 */

#pragma once

#include <stddef.h>

#include <launchdarkly/boolean.h>

/*******************************************************************************
 * @name Store collections and individual items.
 * Used to provide an opaque interface for interacting with feature values.
 * @{
 ******************************************************************************/

/** @brief Opaque value representing an item. */
struct LDStoreCollectionItem
{
    /** @brief May be NULL to indicate a deleted item */
    void *       buffer;
    size_t       bufferSize;
    unsigned int version;
};

/** @brief Stores a single item and its key */
struct LDStoreCollectionStateItem
{
    const char *                 key;
    struct LDStoreCollectionItem item;
};

/** @brief Stores the set of items in a single namespace */
struct LDStoreCollectionState
{
    const char *                       kind;
    struct LDStoreCollectionStateItem *items;
    unsigned int                       itemCount;
};

/*@}*/

/*******************************************************************************
 * @name Generic Store Interface
 * Used to provide all interaction with a feature store. Redis, Consul, Dynamo.
 *
 * The associated functions use a boolean result to indicate success, or error
 * outside of expected functionality. For example a memory allocation error
 * would return false, but `get` on a non existant item would return true.
 * @{
 ******************************************************************************/

/**
 * @struct LDStoreInterface
 * @brief An opaque client object.
 */

struct LDStoreInterface
{
    /**
     * @brief Used to store implementation specific data
     */
    void *context;
    /**
     * @brief Initialize the feature store with a new data set
     * @param[in] context Implementation specific context.
     * @param[in] collections An array of namespaces of store items.
     * @param[in] collectionCount The number of items in the array.
     * @return True on success, False on failure.
     */
    LDBoolean (*init)(
        void *const                          context,
        const struct LDStoreCollectionState *collections,
        const unsigned int                   collectionCount);
    /**
     * @brief Fetch a feature from the store
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @param[in] kind The namespace to search in.
     * May not be NULL (assert).
     * @param[in] featureKey The key to return the value for.
     * May not be NULL (assert).
     * @param[out] result Returns the feature, or NULL if it does not exist.
     * @return True on success, False on failure.
     */
    LDBoolean (*get)(
        void *const                         context,
        const char *const                   kind,
        const char *const                   featureKey,
        struct LDStoreCollectionItem *const result);
    /**
     * @brief Fetch all features in a given namespace
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @param[in] kind The namespace to search in.
     * May not be NULL (assert).
     * @param[out] result Returns an array of raw items.
     * @param[out] result Count Returns the number of items in the result array.
     * @return True on success, False on failure.
     */
    LDBoolean (*all)(
        void *const                          context,
        const char *const                    kind,
        struct LDStoreCollectionItem **const result,
        unsigned int *const                  resultCount);
    /**
     * @brief Replace an existing feature with a newer one or delete a feature
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @param[in] kind The namespace to search in.
     * May not be NULL (assert).
     * @param[in] feature The updated feature. Only deletes current if version
     * is newer. This is also used to delete a feature. May not be NULL
     * (assert).
     * @param[in] featureKey Key of the new item.
     * @return True on success, False on failure.
     */
    LDBoolean (*upsert)(
        void *const                               context,
        const char *const                         kind,
        const struct LDStoreCollectionItem *const feature,
        const char *const                         featureKey);
    /**
     * @brief Determine if the store is initialized with features yet.
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @return True if the store is intialized, false otherwise.
     */
    LDBoolean (*initialized)(void *const context);
    /**
     * @brief Destroy the implementation specific context associated.
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @return Void.
     */
    void (*destructor)(void *const context);
};

/*@}*/
