#include <string.h>
#include "assertion.h"
#include "flag_model.h"


/** Utility to help parse the 'clientSide' and 'clientSideAvailability' properties out of a flag's
 * JSON representation. See the comment on LDi_initFlagModel for more information.
 */
struct AvailabilityParser {
    /** Explicit availability properties */
    LDBoolean usingEnvironmentId;
    LDBoolean usingMobileKey;

    /** Deprecated availability property */
    LDBoolean clientSide;

    /** Indicates if the explicit properties were encountered in JSON or not. */
    LDBoolean isExplicit;
};


/** Initialize the parser.
 *
  * Note: If no properties related to client-side availability are encountered when parsing flag JSON,
  * then according to this function:
  *
  * isExplicitSchema = false, meaning 'clientSide' is the relevant property to inspect, and:
  * clientSide = false, meaning this flag is not available to clients using an Environment ID (such as JS SDK.)
  */
static void initParser(struct AvailabilityParser *const p) {
    p->usingEnvironmentId = LDBooleanFalse;
    p->usingMobileKey = LDBooleanFalse;
    p->clientSide = LDBooleanFalse;
    p->isExplicit = LDBooleanFalse;
}

/** Reads the 'clientSideAvailability' properties from JSON.
 * The actual value of the object in JSON may be NULL.
 *
 * If it is not NULL, then isExplicitSchema is set to true.
 */
static void parserReadExplicit(struct AvailabilityParser *const p, const struct LDJSON *const object) {
    LDJSONType type;
    const struct LDJSON* iter;

    type = LDJSONGetType(object);

    p->isExplicit = (LDBoolean) (type == LDObject);

    if (type == LDObject) {
        for (iter = LDGetIter(object); iter;
             iter = LDIterNext(iter)) {
            const char* prop;

            prop = LDIterKey(iter);

            if (strcmp(prop, "usingEnvironmentId") == 0) {
                p->usingEnvironmentId = LDGetBool(iter);
            } else if (strcmp(prop, "usingMobileKey") == 0) {
                p->usingMobileKey = LDGetBool(iter);
            }
        }
    }
}

/** Reads the deprecated 'clientSide' property from JSON.
 * The actual value of the object in JSON may be NULL.
 * If NULL, 'clientSide' will be parsed as false.
 */
static void parserReadDeprecated(struct AvailabilityParser *const p, const struct LDJSON *const object) {
    if (LDJSONGetType(object) == LDBool) {
        p->clientSide = LDGetBool(object);
    }
}

/** Stores parsed values into an LDClientAvailability structure.
 *
 * If the parser encountered the explicit schema, the properties are directly copied over.
 * Otherwise, the SDK interprets the deprecated schema into the format of the explicit schema.
 */
static void parserStoreAvailability(struct AvailabilityParser *const p, struct LDClientSideAvailability *const avail) {
    if (p->isExplicit) {
        avail->usingExplicitSchema = LDBooleanTrue;
        avail->usingMobileKey = p->usingMobileKey;
        avail->usingEnvironmentID = p->usingEnvironmentId;
    } else {
        avail->usingExplicitSchema = LDBooleanFalse;
        avail->usingMobileKey = LDBooleanTrue;  /* always assumed to be true in old schema */
        avail->usingEnvironmentID = p->clientSide;
    }
}

/** Initializes an LDFeatureFlag structure from its JSON object representation. Does not allocate.
 *
 * This function iterates over all properties in the object, parsing values
 * of interest. In the worst case, it will perform C*N string comparisons, where N is the number of properties in
 * the object, and C is the constant number of properties that we are interested in.
 *
 * Of particular note is the presence or absence of the 'clientSide' and 'clientSideAvailability' properties.
 *
 * In an older version of the JSON schema, 'clientSide' indicated whether a flag was made available
 * to clients utilizing an environment ID, such as the Javascript-based SDKs.
 *
 * In the current schema, 'clientSide' is deprecated in favor of 'clientSideAvailability', an object that
 * explicitly defines the availability of the flag.
 *
 * */
void
LDi_initFlagModel(struct LDFlagModel *const model, const struct LDJSON *const json) {

    struct AvailabilityParser parser;
    const struct LDJSON* iter;

    initParser(&parser);

    model->key = NULL;
    model->version = 0;
    model->trackEvents = LDBooleanFalse;
    model->debugEventsUntilDate = 0;

    for (iter = LDGetIter(json); iter;
         iter = LDIterNext(iter)) {

        const char* prop = LDIterKey(iter);

        if (strcmp(prop, "key") == 0) {
            model->key = LDGetText(iter);
        } else if (strcmp(prop, "version") == 0) {
            model->version = (unsigned int) LDGetNumber(iter);
        } else if (strcmp(prop, "trackEvents") == 0) {
            /* optional */
            if (LDJSONGetType(iter) == LDBool) {
                model->trackEvents = LDGetBool(iter);
            }
        } else if (strcmp(prop, "debugEventsUntilDate") == 0) {
            /* optional */
            if (LDJSONGetType(iter) == LDNumber) {
                model->debugEventsUntilDate = LDGetNumber(iter);
            }
        } else if (strcmp(prop, "clientSideAvailability") == 0) {
            parserReadExplicit(&parser, iter);
        } else if (strcmp(prop, "clientSide") == 0) {
            parserReadDeprecated(&parser, iter);
        }
    }

    LD_ASSERT(model->key);

    parserStoreAvailability(&parser, &model->clientSideAvailability);
}

void LDi_flagModelPopulate(struct LDFlagModel *const model, struct LDFlagState *const flag) {
    flag->version = model->version;
    flag->debugEventsUntilDate = model->debugEventsUntilDate;
    flag->trackEvents = model->trackEvents;
}
