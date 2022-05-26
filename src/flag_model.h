#pragma once

#include "all_flags_state.h"

#include <launchdarkly/json.h>
#include <launchdarkly/boolean.h>

/** @brief Represents the client-side availability of a flag. */
struct LDClientSideAvailability {
    /** usingMobile key indicates that this flag is available to clients using the mobile key for
     * authorization (includes most desktop and mobile clients.) */
    LDBoolean usingMobileKey;
    /** usingEnvironmentID indicates that this flag is available to clients using the environment id to identify an
     * environment (includes client-side javascript clients). */
    LDBoolean usingEnvironmentID;

    /** usingExplicitSchema is true if, when serializing this flag, all the LDClientSideAvailability properties should
    * be included. If it is false, then an older schema is used in which this object is entirely omitted,
    * usingEnvironmentID is stored in a deprecated property, and usingMobileKey is assumed to be true.
    *
    * This field exists to ensure that flag representations remain consistent when sent and received
    * even though the clientSideAvailability property may not be present in the JSON data. It is false
    * if the flag was deserialized from an older JSON schema that did not include that property.
    *
    * Similarly, when deserializing a flag, if it used the older schema then usingExplicitSchema will be false and
    * usingMobileKey will be true.
    * */
    LDBoolean usingExplicitSchema;
};

/** @brief Represents the attributes that comprise a flag. */
struct LDFlagModel {
    const char* key; /* non-owned */
    unsigned int version;
    struct LDClientSideAvailability clientSideAvailability;
    LDBoolean trackEvents;
    double debugEventsUntilDate;
};

/** @brief Initializes the given model from a JSON representation.
 * @param model @ref LDFlagModel handle.
 * @param json JSON representation of the flag.
 */
void
LDi_initFlagModel(struct LDFlagModel *model, const struct LDJSON* json);

/** @brief Populates an @ref LDFlagState from the given model.
 * @param model @ref LDFlagModel handle.
 * @param flag @LDFlagState to populate.
 */
void
LDi_flagModelPopulate(struct LDFlagModel *model, struct LDFlagState *flag);
