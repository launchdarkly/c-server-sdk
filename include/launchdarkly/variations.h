/*!
 * @file variations.h
 * @brief Public API Interface for evaluation variations
 */

#pragma once

#include <launchdarkly/boolean.h>
#include <launchdarkly/client.h>
#include <launchdarkly/export.h>
#include <launchdarkly/json.h>
#include <launchdarkly/user.h>

/** @brief The reason an evaluation occured */
enum LDEvalReason
{
    /** @brief A default unset reason */
    LD_UNKNOWN = 0,
    /** @brief Indicates that the flag could not be evaluated, e.g. because it
     * does not exist or due to an unexpected error. In this case the result
     * value will be the default value that the caller passed to the client.*/
    LD_ERROR,
    /** @brief Indicates that the flag was off and therefore returned its
     * configured off value.*/
    LD_OFF,
    /** @brief Indicates that the flag was considered off because it had at
     * least one prerequisite flag that either was off or did not return the
     * desired variation.*/
    LD_PREREQUISITE_FAILED,
    /** @brief Indicates that the user key was specifically targeted for this
     * flag.*/
    LD_TARGET_MATCH,
    /** @brief Indicates that the user matched one of the flag's rules. */
    LD_RULE_MATCH,
    /** @brief Indicates that the flag was on but the user did not match any
     * targets or rules. */
    LD_FALLTHROUGH
};

/** @brief Details about the type of error that caused an evaluation to fail */
enum LDEvalErrorKind
{
    /** @brief Indicates that the caller tried to evaluate a flag before the
     * client had successfully initialized */
    LD_CLIENT_NOT_READY,
    /** @brief Indicates a `NULL` flag key was used */
    LD_NULL_KEY,
    /** @brief Indicates an internal exception with the flag store */
    LD_STORE_ERROR,
    /** @brief Indicates that the caller provided a flag key that did not match
     * any known flag */
    LD_FLAG_NOT_FOUND,
    /** @brief Indicates that a `NULL` user was passed for the user parameter */
    LD_USER_NOT_SPECIFIED,
    /** @brief Indicates that a `NULL` client was passed for the client
     * parameter */
    LD_CLIENT_NOT_SPECIFIED,
    /** @brief Indicates that there was an internal inconsistency in the flag
     * data, a rule specified a nonexistent variation. */
    LD_MALFORMED_FLAG,
    /** @brief Indicates that the result value was not of the requested type,
     * e.g. you called `LDBoolVariation` but the value was an integer. */
    LD_WRONG_TYPE,
    /** @brief Evaluation failed because the client ran out of memory */
    LD_OOM
};

/** @brief Indicates which rule matched a user */
struct LDDetailsRule
{
    /** @brief The index of the rule that was matched */
    unsigned int ruleIndex;
    /** @brief The unique identifier of the rule that was matched. */
    char *id;
    /** @brief Whether the evaluation was part of an experiment. Is
     * true if the evaluation resulted in an experiment rollout *and* served one
     * of the variations in the experiment. Otherwise false. */
    LDBoolean inExperiment;
};

/** @brief Extra information when reason == LD_FALLTHROUGH */
struct LDDetailsFallthrough
{
    /** @brief Whether the evaluation was part of an experiment. Is
     * true if the evaluation resulted in an experiment rollout *and* served one
     * of the variations in the experiment. Otherwise false. */
    LDBoolean inExperiment;
};

struct LDDetails
{
    /** @brief The index of the returned value within the flag's list of
     * variations. */
    unsigned int variationIndex;
    /** @brief True if there is a `variationIndex`, false if the default
     * value was returned. */
    LDBoolean hasVariation;
    /** @brief The reason an evaluation occured */
    enum LDEvalReason reason;
    /** @brief Extra information depending on the evaluation reason */
    union {
        /** @brief when reason == LD_ERROR */
        enum LDEvalErrorKind errorKind;
        /** @brief when reason == LD_PREREQUISITE_FAILED */
        char *prerequisiteKey;
        /** @brief when reason == LD_RULE_MATCH */
        struct LDDetailsRule rule;
        /** @brief when reason == LD_FALLTHROUGH */
        struct LDDetailsFallthrough fallthrough;
    } extra;
};

/** @brief Initialize `details` to a default value */
LD_EXPORT(void) LDDetailsInit(struct LDDetails *const details);

/** @brief Free any resources associated with `details` */
LD_EXPORT(void) LDDetailsClear(struct LDDetails *const details);

/** @brief Convert an `LDEvalReason` enum to an equivalent string */

/**
 * @brief Converts an `LDEvalReason` enum to an equivalent string
 * @param[in] kind The reason kind to convert.
 * @return A string, or `NULL` on invalid input.
 */
LD_EXPORT(const char *) LDEvalReasonKindToString(const enum LDEvalReason kind);

/**
 * @brief Converts an `LDEvalErrorKind` enum to an equivalent string
 * @param[in] kind The error kind to convert.
 * @return A string, or `NULL` on invalid input.
 */
LD_EXPORT(const char *)
LDEvalErrorKindToString(const enum LDEvalErrorKind kind);

/**
 * @brief Marshal just the evaluation reason portion of `LDDetails` to JSON.
 * @param[in] details The structure to marshal. Ownership is not transferred.
 * May not be `NULL` (assert)
 * @return A JSON representation, or `NULL` on `malloc` failure.
 */
LD_EXPORT(struct LDJSON *)
LDReasonToJSON(const struct LDDetails *const details);

/**
 * @brief Evaluate a boolean flag
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] user The user to evaluate the flag against. May not be `NULL`.
 * @param[in] key The key of the flag to evaluate. May not be `NULL`.
 * @param[in] fallback The value to return on error
 * @param[out] details A struct where the evaluation explanation will be put.
 * If `NULL` no explanation will be generated.
 * @return The fallback will be returned on any error.
 */
LD_EXPORT(LDBoolean)
LDBoolVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const LDBoolean            fallback,
    struct LDDetails *const    details);

/**
 * @brief Evaluate a integer flag
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] user The user to evaluate the flag against. May not be `NULL`.
 * @param[in] key The key of the flag to evaluate. May not be `NULL`.
 * @param[in] fallback The value to return on error
 * @param[out] details A struct where the evaluation explanation will be put.
 * If `NULL` no explanation will be generated.
 * @return The fallback will be returned on any error.
 */
LD_EXPORT(int)
LDIntVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const int                  fallback,
    struct LDDetails *const    details);

/**
 * @brief Evaluate a double flag
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] user The user to evaluate the flag against. May not be `NULL`.
 * @param[in] key The key of the flag to evaluate. May not be `NULL`.
 * @param[in] fallback The value to return on error
 * @param[out] details A struct where the evaluation explanation will be put.
 * If `NULL` no explanation will be generated.
 * @return The fallback will be returned on any error.
 */
LD_EXPORT(double)
LDDoubleVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const double               fallback,
    struct LDDetails *const    details);

/**
 * @brief Evaluate a text flag
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] user The user to evaluate the flag against. May not be `NULL`.
 * @param[in] key The key of the flag to evaluate. May not be `NULL`.
 * @param[in] fallback The value to return on error. Ownership is not
 * transferred. May be `NULL`.
 * @param[out] details A struct where the evaluation explanation will be put.
 * If `NULL` no explanation will be generated.
 * @return The fallback will be returned on any error but may be `NULL` on
 * allocation failure.
 * The result must be cleaned up with `LDFree`.
 */
LD_EXPORT(char *)
LDStringVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const char *const          fallback,
    struct LDDetails *const    details);

/**
 * @brief Evaluate a JSON flag
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] user The user to evaluate the flag against. May not be `NULL`.
 * @param[in] key The key of the flag to evaluate. May not be `NULL``
 * @param[in] fallback The fallback to return on error. Ownership is not
 * transferred. May be `NULL`.
 * @param[out] details A struct where the evaluation explanation will be put.
 * If `NULL` no explanation will be generated.
 * @return The fallback will be returned on any error but may be `NULL` on
 * allocation failure.
 * The result must be cleanud up with `LDJSONFree`.
 */
LD_EXPORT(struct LDJSON *)
LDJSONVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const struct LDJSON *const fallback,
    struct LDDetails *const    details);

/**
 * @brief Returns a map from feature flag keys to values for a given user.
 * This does not send analytics events back to LaunchDarkly.
 * @param[in] client The client to use. May not be `NULL`.
 * @param[in] user The user to evaluate flags for. Ownership is not transferred.
 * May not be `NULL`.
 * @return A JSON object, or `NULL` on failure.
 */
LD_EXPORT(struct LDJSON *)
LDAllFlags(struct LDClient *const client, const struct LDUser *const user);
