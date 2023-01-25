#pragma once
#include "launchdarkly/boolean.h"
#include "ldjsonrc.h"
#include "store.h"

/**
 * This interface is used by the internal store mechanisms.
 * This is implemented by the memory store and by the caching wrapper.
 * Externally implemented interfaces should use LDStoreInterface and will
 * be adapted to this interface by the caching wrapper.
 *
 * store.c
 * Uses --> LDInternalStoreInterface
 *
 * memory_store.c
 * Implements --> LDInternalStoreInterface
 *
 * caching_wrapper.c
 * Implements --> LDInternalStoreInterface
 * Uses --> LDStoreInterface
 *
 * Externals Stores (redis.c)
 * Implements --> LDStoreInterface
 */
struct LDInternalStoreInterface {
    /**
     * @brief Implementation specific data.
     */
    void *context;

    /**
     * @brief Replaces the entire content of the store.
     * @param[in] context
     * @param[in] newData Set containing features/segments to put in the store.
     * Data format example.
     * ```js
     * newData = {
     *  features: {
     *      featureX: {...}
     *  },
     *  segments: {
     *      segmentY: {...}
     *  }
     * }
     * ```
     * @return LDBooleanTrue if the operation was a success.
     */
    LDBoolean (*init)(
            void *const context,
            struct LDJSON *const newData);

    /**
     * @brief Get an item from the store.
     * @param[in] context
     * @param[in] kind The kind (features/segments) to get.
     * @param[in] key The key to return a value for.
     * @param[out] result Returns an item, or NULL if it does not exist.
     * @return LDBooleanTrue if the operation was a success.
     */
    LDBoolean (*get)(
            void *const context,
            enum FeatureKind kind,
            const char *const key,
            struct LDJSONRC **const result);

    /**
     * @brief Get all items of a specific kind.
     * @param[in] context
     * @param[in] kind The kind (features/segments) to get.
     * @param[out] result Returns a collection of items.
     * @return LDBooleanTrue if the operation was a success.
     */
    LDBoolean (*all)(
            void *const context,
            enum FeatureKind kind,
            struct LDJSONRC **const result);

    /**
     * @brief  Insert/Replace an item in the store. If an existing item is of a more recent version, then the store
     * will not be changed.
     * @param[in] context
     * @param[in] kind The kind (feature/segment) to set.
     * @param[in] key The key of the item to set.
     * @param[in] item The value of the item.
     * @return LDBooleanTrue if the operation was a success.
     */
    LDBoolean (*upsert)(
            void *const context,
            enum FeatureKind kind,
            const char *const key,
            struct LDJSON *const item);

    /**
     * @brief Check if the store is initialized.
     * @param[in] context
     * @return LDBooleanTrue if the store is initialized.
     */
    LDBoolean (*initialized)(void *const context);

    /**
     * @brief Destroy the implementation specific context and any associated allocations.
     * @param [in]context
     */
    void (*destructor)(void *const context);

    /**
     * @brief Expire all cached items. After doing this operations should hit
     * the back-end instead of being handled from cache. This function is
     * for testing only.
     * @param context
     */
    void (*ldi_expireAll)(void *const context);
};
