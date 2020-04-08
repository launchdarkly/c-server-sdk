#include <launchdarkly/api.h>

#include "assertion.h"

struct LDJSON *
makeMinimalFlag(const char *const key, const unsigned int version,
    const bool on, const bool trackEvents)
{
    struct LDJSON *flag;

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(version)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(on)));
    LD_ASSERT(LDObjectSetKey(flag, "trackEvents", LDNewBool(trackEvents)));

    return flag;
}

void
setFallthrough(struct LDJSON *const flag, const unsigned int variation)
{
    struct LDJSON *tmp;

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(variation)));
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", tmp));
}

void
addVariation(struct LDJSON *const flag, struct LDJSON *const variation)
{
    struct LDJSON *variations;

    if (!(variations = LDObjectLookup(flag, "variations"))) {
        LD_ASSERT(variations = LDNewArray());
        LDObjectSetKey(flag, "variations", variations);
    }

    LD_ASSERT(LDArrayPush(variations, variation));
}
