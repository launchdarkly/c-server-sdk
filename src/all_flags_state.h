/*!
 * @file all_flags_state.h
 * @brief Functionality related to AllFlagsState method.
 */
#pragma once

#include <launchdarkly/variations.h>
#include "uthash.h"
#include "uthash.h"

/* undefine the uthash defaults */
#undef uthash_malloc
#undef uthash_free

/* re-define, specifying alternate functions */
#define uthash_malloc(sz) LDAlloc(sz)
#define uthash_free(ptr, sz) LDFree(ptr)

struct LDAllFlagsBuilder;

struct LDFlagState
{
    /* Makes this struct into a hashtable so users can look up flags quickly.
    * The conventional name 'hh' allows for simpler usage of uthash's macros.
    * */
    UT_hash_handle  hh;

    /* Key of flag. Must be freed. */
    char *key;

    /* JSON object containing flag evaluation. Must be freed. */
    struct LDJSON* value;

    /* Details may contain heap allocated memory. Must be freed. */
    struct LDDetails details;

    unsigned int version;

    LDBoolean trackEvents;

    double debugEventsUntilDate;
};

/** @brief Returns a pointer to an `LDAllFlagsState` with specified validity.
 * @param[in] valid Pass True to indicate this state is valid, otherwise pass False.
 * @return New `LDAllFlagsState` object if valid is specified & allocation succeeds, otherwise `NULL`.
 * If invalid is specified, returns a pointer to a global instance rather than allocating.
 */
struct LDAllFlagsState*
LDi_newAllFlagsState(LDBoolean valid);

/** @brief Frees an `LDAllFlagsState`.
 * @param[in] flags `LDAllFlagsState` handle.
 */
void
LDi_freeAllFlagsState(struct LDAllFlagsState *flags);

/** @brief Adds a flag by key.
 * @param[in] state `LDAllFlags` handle.
 * @param[in] flag Flag value. Ownership is transferred on success.
 * @return True if adding the flag succeeded, otherwise False.
 * If False is returned, the flag must be freed by the caller.
 */
LDBoolean
LDi_allFlagsStateAdd(struct LDAllFlagsState* state, struct LDFlagState* flag);

/** @brief Checks if the given `LDAllFlagsState` is valid or not.
 * @param[in] flags `LDAllFlagsState` handle.
 * @return True if valid, otherwise False.
 */
LDBoolean
LDi_allFlagsStateValid(const struct LDAllFlagsState *flags);

/** @brief Serializes the given `LDAllFlagsState` to JSON.
 * @param[in] flags `LDAllFlagsState` handle.
 * @return Newly allocated string containing JSON representation.
 * Caller is responsible for freeing with @ref LDFree.
 */
char *
LDi_allFlagsStateJSON(const struct LDAllFlagsState *flags);

/** @brief Retrieves details of a given flag.
 * @param[in] flags `LDAllFlagsState` handle.
 * @param[in] key Key of flag.
 * @return Pointer to @ref LDDetails if found, otherwise `NULL`.
 */
struct LDDetails*
LDi_allFlagsStateDetails(const struct LDAllFlagsState* flags, const char* key);

/** @brief Retrieves value of a given flag.
 * @param[in] flags `LDAllFlagsState` handle.
 * @param[in] key Key of flag.
 * @return Value of flag, which may be `NULL`. If flag is not found, returns `NULL`.
 */
struct LDJSON*
LDi_allFlagsStateValue(const struct LDAllFlagsState* flags, const char* key);

/** @brief Returns a reference to a JSON representation of `LDAllFlagsState`, where each flag is mapped
 * from flag key to flag value.
 * @param[in] flags `LDAllFlagsState` handle.
 * @return JSON map from flag key to flag value.
 * If there are no flags contained in the state, the map will be valid but empty.
 * The caller must not free the returned LDJSON value.
 */
struct LDJSON*
LDi_allFlagsStateValuesMap(const struct LDAllFlagsState* flags);

/** @brief Allocates an instance of @ref LDAllFlagsBuilder with the given options./
 * @param[in] options Builder options; see @ref LDAllFlagsStateOption.
 * Multiple options can be passed using bitwise operations.
 * @return New @ref LDAllFlagsBuilder if allocation succeeds, otherwise `NULL`.
 */
struct LDAllFlagsBuilder*
LDi_newAllFlagsBuilder(unsigned int options);


/** @brief Frees an @ref LDAllFlagsBuilder.
 * @param[in] builder @ref LDAllFlagsBuilder handle.
 */
void
LDi_freeAllFlagsBuilder(struct LDAllFlagsBuilder *builder);

/** @brief Adds a flag to the builder.
 * @param[in] @ref LDAllFlagsBuilder handle.
 * @param[in] flag Flag. Ownership is transferred on success.
 * @return True on success, otherwise False.
 * If False is returned, caller must free flag.
 */
LDBoolean
LDi_allFlagsBuilderAdd(struct LDAllFlagsBuilder *builder, struct LDFlagState* flag);


/** @brief Extract a complete `LDAllFlagsState` from the given builder.
 * @param builder @ref LDAllFlagsBuilder handle.
 * @return Pointer to new `LDAllFlagsState`. This function does not allocate.
 */
struct LDAllFlagsState*
LDi_allFlagsBuilderBuild(struct LDAllFlagsBuilder *builder);


/** @brief Allocates an @ref LDFlagState.
 * @param[in] key Flag's key. The key is cloned.
 */
struct LDFlagState*
LDi_newFlagState(const char* key);

/** @brief Frees an @ref LDFlagState.
 * @param[in] flag @ref LDFlagState handle.
 */
void
LDi_freeFlagState(struct LDFlagState* flag);
