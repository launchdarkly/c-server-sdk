/*!
 * @file flag_state.h
 * @brief Public API associated with LDAllFlagsState.
 */

#pragma once

#include <launchdarkly/boolean.h>
#include <launchdarkly/export.h>


/** @brief Opaque return value of LDAllFlagsState. */
struct LDAllFlagsState;

/** @brief Check if a call to @ref LDAllFlagsState succeeded.
 * @param[in] flags `LDAllFlagsState` handle.
 * @return True if the call to `LDAllFlagsState` succeeded. False if there was an
 * error (such as the data store not being available), in which case no flag data is in this object.
 * It is safe to call @ref LDAllFlagsStateSerializeJSON even if the state is invalid. */
LD_EXPORT(LDBoolean) LDAllFlagsStateValid(struct LDAllFlagsState *flags);

/** @brief Frees memory associated with `LDAllFlagsState`.
 * @param[in] flags `LDAllFlagsState` handle.*/
LD_EXPORT(void) LDAllFlagsStateFree(struct LDAllFlagsState *flags);

/** @brief Serializes flag data to JSON.
 * @param[in] flags `LDAllFlagsState` handle.
 * @return Pointer to JSON string. Caller is responsible for freeing with @ref LDFree. */
LD_EXPORT(char*) LDAllFlagsStateSerializeJSON(struct LDAllFlagsState *flags);

/** @brief  Returns evaluation details for an individual feature flag at the time the state was recorded.
 * @param[in] flags `LDAllFlagsState` handle.
 * @param[in] key Flag key.
 * @return @ref LDDetails structure if the flag was found, otherwise `NULL`. Caller must not free the returned structure
 * directly; instead, call @ref LDAllFlagsStateFree to free all memory associated with an `LDAllFlagsState`.
 */
LD_EXPORT(struct LDDetails*) LDAllFlagsStateGetDetails(struct LDAllFlagsState* flags, const char* key);

/** @brief Returns the JSON value of an individual feature flag at the time the state was recorded.
 * @param[in] flags `LDAllFlagsState` handle.
 * @param key Flag key.
 * @return JSON value of flag, or `NULL` if the flag was not found. Caller must not free the returned structure
 * directly; instead, call @ref LDAllFlagsStateFree to free all memory associated with an `LDAllFlagsState`.
 */
LD_EXPORT(struct LDJSON*) LDAllFlagsStateGetValue(struct LDAllFlagsState* flags, const char* key);

/** @brief Returns a JSON map of flag keys to flag values.
 *
 * Do not use this method if you are passing data to the front end to "bootstrap" the JavaScript client.
 * Instead, convert the state object to JSON using @ref LDAllFlagsStateSerializeJSON.
 *
 * @param[in] flags `LDAllFlagsState` handle.
 * @return JSON map of flag keys to flag values. The values are references into the LDAllFlagsState object.
 * Caller must not free the map; instead, call @ref LDAllFlagsStateFree.
 */
LD_EXPORT(struct LDJSON*) LDAllFlagsStateToValuesMap(struct LDAllFlagsState* flags);

/** @brief Options for use in @ref LDAllFlagsState. */
enum LDAllFlagsStateOption {
    /** Use default behavior. **/
    LD_ALLFLAGS_DEFAULT                  = 0,
    /** @brief Include evaluation reasons in the state object. By default, they are not. */
    LD_INCLUDE_REASON                    = (1 << 0),
    /** @brief Include detailed flag metadata only for flags with event tracking or debugging turned on.
     * This reduces the size of the JSON data if you are passing the flag state to the front end. */
    LD_DETAILS_ONLY_FOR_TRACKED_FLAGS    = (1 << 1),
    /** @brief Include only flags marked for use with the client-side SDK. By default, all flags are included. */
    LD_CLIENT_SIDE_ONLY                  = (1 << 2)
};
