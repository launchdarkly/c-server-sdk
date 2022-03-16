#pragma once

#include <launchdarkly/boolean.h>

struct LDStore;

struct LDDataSource
{
    /**
     * @brief Used to store implementation specific data
     */
    void *context;

    /**
     * @brief Initialize the data source
     * @param[in] context Implementation specific context.
     * @return True on success, False on failure.
     */
    LDBoolean (*init)(void *const context, struct LDStore *const store);

    /**
     * @brief Do any client specific cleanup
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @return Void.
     */
    void (*close)(void *const context);

    /**
     * @brief Destroy the implementation specific context associated.
     * @param[in] context Implementation specific context.
     * May not be NULL (assert).
     * @return Void.
     */
    void (*destructor)(void *const context);
};

