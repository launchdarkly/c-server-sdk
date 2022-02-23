#include <launchdarkly/flag_state.h>

#include "assertion.h"
#include "all_flags_state.h"

void
LDAllFlagsStateFree(struct LDAllFlagsState *const allFlags) {
    LDi_freeAllFlagsState(allFlags);
}


LDBoolean
LDAllFlagsStateValid(struct LDAllFlagsState *const flags) {
    LD_ASSERT_API(flags);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (flags == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlagsStateValid NULL flags");

        return LDBooleanFalse;
    }
#endif

    return LDi_allFlagsStateValid(flags);
}

char*
LDAllFlagsStateSerializeJSON(struct LDAllFlagsState *const flags) {
    LD_ASSERT_API(flags);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (flags == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlagsStateSerializeJSON NULL flags");

        return LDBooleanFalse;
    }
#endif

    return LDi_allFlagsStateJSON(flags);
}


struct LDDetails*
LDAllFlagsStateGetDetails(struct LDAllFlagsState *flags, const char* key) {
    LD_ASSERT_API(flags);
    LD_ASSERT_API(key);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (flags == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlagsStateGetFlag NULL flags");

        return NULL;
    }

    if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlagsStateGetFlag NULL key");

        return NULL;
    }
#endif

    return LDi_allFlagsStateDetails(flags, key);
}

struct LDJSON*
LDAllFlagsStateGetValue(struct LDAllFlagsState* flags, const char* key) {
    LD_ASSERT_API(flags);
    LD_ASSERT_API(key);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (flags == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlagsStateGetValue NULL flags");

        return NULL;
    }

    if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlagsStateGetValue NULL key");

        return NULL;
    }
#endif

    return LDi_allFlagsStateValue(flags, key);
}

struct LDJSON*
LDAllFlagsStateToValuesMap(struct LDAllFlagsState *flags) {
    LD_ASSERT_API(flags);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (flags == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlagsStateToValuesMap NULL flags");

        return NULL;
    }
#endif

    return LDi_allFlagsStateValuesMap(flags);
}
