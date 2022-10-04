#include "all_flags_state.h"

#include "json_internal_helpers.h"
#include "assertion.h"
#include "utility.h"
#include "cJSON.h"

#include <launchdarkly/memory.h>
#include <launchdarkly/json.h>
#include <launchdarkly/variations.h>
#include <launchdarkly/flag_state.h>

/* LDAllFlagsState implements the opaque type returned to callers of LDAllFlagsState().
 * */
struct LDAllFlagsState {
    struct LDFlagState *hash;
    struct LDJSON *map;
    LDBoolean valid;
};

/* Returned whenever a call to LDAllFlagsState must return an invalid state, to avoid
 * any allocation failures. */
static struct LDAllFlagsState invalidSingleton = {NULL, NULL, LDBooleanFalse};

/* LDAllFlagsBuilder implements the builder pattern for LDAllFlagsState in order to cut down on the complexity
 * of transforming each flag into an LDFlagState.
 * Ownership of 'hash' is transferred from the builder into an instance of LDAllFlagsState.
 * */
struct LDAllFlagsBuilder {
    struct LDAllFlagsState* state;
    /* Include evaluation reason.
     * By default, these aren't included to save space when passing data to the frontend. */
    LDBoolean includeReasons;
    /* Omit details (such as flag version and eval reasons) for flags that do not have event
     * tracking or debugging enabled. */
    LDBoolean detailsOnlyForTrackedFlags;
};

struct LDFlagState*
LDi_newFlagState(const char* key) {
    struct LDFlagState *state = NULL;
    char *clonedKey = NULL;

    if (!(state = LDAlloc(sizeof(struct LDFlagState)))) {
        LD_LOG(LD_LOG_ERROR, "LDFlagState alloc error");

        return NULL;
    }

    if (!(clonedKey = LDStrDup(key))) {
        LD_LOG(LD_LOG_ERROR, "LDFlagState key alloc error");

        LDFree(state);
        return NULL;
    }

    LDDetailsInit(&state->details);

    state->key = clonedKey;
    state->value = NULL;
    state->debugEventsUntilDate = 0;
    state->trackEvents = LDBooleanFalse;
    state->version = 0;
    state->omitDetails = LDBooleanFalse;

    return state;
}

void
LDi_freeFlagState(struct LDFlagState *const flag) {
    if (flag) {
        LDFree(flag->key);
        LDJSONFree(flag->value);
        LDDetailsClear(&flag->details);
        LDFree(flag);
    }
}

static LDBoolean
buildFlagState(const struct LDAllFlagsState *flags, struct LDJSON *object);

struct LDAllFlagsState*
LDi_newAllFlagsState(LDBoolean valid) {

    struct LDAllFlagsState *state = NULL;
    struct LDJSON *map = NULL;

    if (!valid) {
        return &invalidSingleton;
    }

    if (!(state = LDAlloc(sizeof(struct LDAllFlagsState)))) {
        LD_LOG(LD_LOG_ERROR, "LDAllFlagsState alloc error");

        return NULL;
    }

    if (!(map = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "LDFlagState map alloc error");
        LDFree(state);

        return NULL;
    }

    state->map = map;
    state->hash = NULL;
    state->valid = LDBooleanTrue;
    return state;
}

void
LDi_freeAllFlagsState(struct LDAllFlagsState *const flags) {
    struct LDFlagState *iter, *tmp1;

    iter = NULL;
    tmp1 = NULL;

    if (flags == &invalidSingleton) {

        return;
    }

    if (flags) {
        HASH_ITER(hh, flags->hash, iter, tmp1) {
            HASH_DEL(flags->hash, iter);
            LDi_freeFlagState(iter);
        }
        LDJSONFree(flags->map);
        LDFree(flags);
    }
}

/* Adds the flag to the internal hashtable, and also inserts it into a pre-existing
 * LDJSON map. This map is available to the user via ToValuesMap.
 *
 * An alternate implementation could build the values map on-demand, but this would require the user to
 * manage the memory of it separately from the LDAllFlagsState.
 * */
LDBoolean
LDi_allFlagsStateAdd(struct LDAllFlagsState *const state, struct LDFlagState *const flag) {
    struct LDJSON *null = NULL;

    if (flag->value != NULL) {
        if (!LDObjectSetReference(state->map, flag->key, flag->value)) {
            LD_LOG(LD_LOG_ERROR, "set flag reference");

            return LDBooleanFalse;
        }
    } else {
        if (!(null = LDNewNull())) {
            LD_LOG(LD_LOG_ERROR, "alloc LDNull json object");

            return LDBooleanFalse;
        }

        if (!LDObjectSetKey(state->map, flag->key, null)) {
            LD_LOG(LD_LOG_ERROR, "set null reference");
            LDJSONFree(null);

            return LDBooleanFalse;
        }
    }

    HASH_ADD_KEYPTR(hh, state->hash, flag->key, strlen(flag->key), flag);

    return LDBooleanTrue;
}

LDBoolean
LDi_allFlagsStateValid(const struct LDAllFlagsState *const flags) {
    return flags->valid;
}

char *
LDi_allFlagsStateJSON(const struct LDAllFlagsState *const flags) {
    struct LDJSON *baseObj = NULL;
    char *json = NULL;

    if (!(baseObj = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc base object");

        return NULL;
    }

    if (!buildFlagState(flags, baseObj)) {
        LD_LOG(LD_LOG_ERROR, "unable to build AllFlagsState json object");

        LDJSONFree(baseObj);
        return NULL;
    }

    if (!(json = LDJSONSerialize(baseObj))) {
        LD_LOG(LD_LOG_ERROR, "unable to serialize JSON");

        LDJSONFree(baseObj);
        return NULL;
    }

    LDJSONFree(baseObj);
    return json;
}

static LDBoolean buildFlagState(const struct LDAllFlagsState *const flags, struct LDJSON *const object) {
    struct LDJSON *flagsState = NULL;
    struct LDFlagState *flag = NULL;

    if (!LDObjectSetBool(object, "$valid", flags->valid)) {
        LD_LOG(LD_LOG_ERROR, "set key");

        return LDBooleanFalse;
    }

    for (flag = flags->hash; flag != NULL; flag = flag->hh.next) {
        struct LDJSON *null = NULL;

        if (flag->value == NULL) {

            if (!(null = LDNewNull())) {
                LD_LOG(LD_LOG_ERROR, "alloc null placeholder");

                return LDBooleanFalse;
            }

            if (!LDObjectSetKey(object, flag->key, null)) {
                LD_LOG(LD_LOG_ERROR, "set key");

                LDJSONFree(null);
                return LDBooleanFalse;
            }

            continue;
        }

        if (!LDObjectSetReference(object, flag->key, flag->value)) {
            LD_LOG(LD_LOG_ERROR, "set key");

            return LDBooleanFalse;
        }
    }

    if (!(flagsState = LDObjectNewChild(object, "$flagsState"))) {
        LD_LOG(LD_LOG_ERROR, "set key");

        return LDBooleanFalse;
    }

    for (flag = flags->hash; flag != NULL; flag = flag->hh.next) {
        struct LDJSON *flagObj = NULL;

        if (!(flagObj = LDObjectNewChild(flagsState, flag->key))) {
            LD_LOG(LD_LOG_ERROR, "set key");

            return LDBooleanFalse;
        }

        if (flag->details.hasVariation) {
            if (!LDObjectSetNumber(flagObj, "variation", flag->details.variationIndex)) {
                LD_LOG(LD_LOG_ERROR, "set key");

                return LDBooleanFalse;
            }
        }


        if (!flag->omitDetails) {
            if (!LDObjectSetNumber(flagObj, "version", flag->version)) {
                LD_LOG(LD_LOG_ERROR, "set key");

                return LDBooleanFalse;
            }
        }

        if (flag->details.reason != LD_UNKNOWN && !flag->omitDetails) {
            struct LDJSON* reason = NULL;

            if (!(reason = LDReasonToJSON(&flag->details))) {
                LD_LOG(LD_LOG_ERROR, "alloc");

                return LDBooleanFalse;
            }

            if (!LDObjectSetKey(flagObj, "reason", reason)) {
                LD_LOG(LD_LOG_ERROR, "set key");

                LDJSONFree(reason);
                return LDBooleanFalse;
            }
        }

        if (flag->trackEvents) {
            if (!LDObjectSetBool(flagObj, "trackEvents", flag->trackEvents)) {
                LD_LOG(LD_LOG_ERROR, "set key");

                return LDBooleanFalse;
            }
        }

        if (flag->debugEventsUntilDate > 0) {
            if (!LDObjectSetNumber(flagObj, "debugEventsUntilDate", flag->debugEventsUntilDate)) {
                LD_LOG(LD_LOG_ERROR, "set key");

                return LDBooleanFalse;
            }
        }
    }

    return LDBooleanTrue;
}



struct LDDetails*
LDi_allFlagsStateDetails(const struct LDAllFlagsState *const flags, const char* key) {
    struct LDFlagState* foundFlag = NULL;

    HASH_FIND_STR(flags->hash, key, foundFlag);

    if (foundFlag == NULL) {
        return NULL;
    }

    return &foundFlag->details;
}

struct LDJSON*
LDi_allFlagsStateValue(const struct LDAllFlagsState *const flags, const char* key) {
    struct LDFlagState* foundFlag = NULL;

    HASH_FIND_STR(flags->hash, key, foundFlag);

    if (foundFlag == NULL) {
        return NULL;
    }

    return foundFlag->value;
}


struct LDJSON*
LDi_allFlagsStateValuesMap(const struct LDAllFlagsState *const flags) {
    return flags->map;
}

struct LDAllFlagsBuilder *
LDi_newAllFlagsBuilder(unsigned int options)
{
    struct LDAllFlagsBuilder *const builder = LDAlloc(sizeof(struct LDAllFlagsBuilder));

    if (builder == NULL) {
        return NULL;
    }

    if (!(builder->state = LDi_newAllFlagsState(LDBooleanTrue))) {
        LDFree(builder);

        return NULL;
    }

    builder->includeReasons = (LDBoolean) (options & LD_INCLUDE_REASON);
    builder->detailsOnlyForTrackedFlags = (LDBoolean) (options & LD_DETAILS_ONLY_FOR_TRACKED_FLAGS);

    return builder;
}

void
LDi_freeAllFlagsBuilder(struct LDAllFlagsBuilder *const builder) {
    if (builder) {
        LDi_freeAllFlagsState(builder->state);
        LDFree(builder);
    }
}

LDBoolean
LDi_allFlagsBuilderAdd(struct LDAllFlagsBuilder *const b, struct LDFlagState *const flag)
{

    LD_ASSERT(b);

    if (b->detailsOnlyForTrackedFlags) {
        double now;

        LDi_getMonotonicMilliseconds(&now);
        if (!flag->trackEvents && !(flag->debugEventsUntilDate != 0 && flag->debugEventsUntilDate > now)) {
            flag->omitDetails = LDBooleanTrue;
        }
    }

    if (!b->includeReasons) {
        if (flag->details.reason == LD_RULE_MATCH) {
            LDFree(flag->details.extra.rule.id);
        } else if (flag->details.reason == LD_PREREQUISITE_FAILED) {
            LDFree(flag->details.extra.prerequisiteKey);
        }
        flag->details.reason = LD_UNKNOWN;
    }

    return LDi_allFlagsStateAdd(b->state, flag);
}

struct LDAllFlagsState*
LDi_allFlagsBuilderBuild(struct LDAllFlagsBuilder *const builder) {
    struct LDAllFlagsState* allFlags = NULL;

    allFlags = builder->state;
    builder->state = NULL;

    return allFlags;
}
