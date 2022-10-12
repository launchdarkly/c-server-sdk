#include "test-utils/flags.h"

#include "assertion.h"

struct LDJSON *
makeMinimalFlag(
    const char *const  key,
    const unsigned int version,
    const LDBoolean    on,
    const LDBoolean    trackEvents)
{
    struct LDJSON *flag;

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(version)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(on)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
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

void
addPrerequisite(struct LDJSON *const flag, struct LDJSON *const prereq, unsigned int expectedVariation)
{
    struct LDJSON *req;
    struct LDJSON *reqs;

    if (!(reqs = LDObjectLookup(flag, "prerequisites"))) {
        reqs = LDNewArray();
        LDObjectSetKey(flag, "prerequisites", reqs);
    }

    req = LDNewObject();
    LDObjectSetKey(req, "key", LDNewText(LDGetText(LDObjectLookup(prereq, "key"))));
    LDObjectSetKey(req, "variation", LDNewNumber(expectedVariation));
    LDArrayPush(reqs, req);
}

void
addVariations1(struct LDJSON *const flag)
{
    addVariation(flag, LDNewText("fall"));
    addVariation(flag, LDNewText("off"));
    addVariation(flag, LDNewText("on"));
}

void
addVariations2(struct LDJSON *const flag)
{
    addVariation(flag, LDNewText("nogo"));
    addVariation(flag, LDNewText("go"));
}

struct LDJSON *
makeFlagToMatchUser(
    const char *const key, struct LDJSON *const variationOrRollout)
{
    struct LDJSON *flag, *clause, *tmp, *rule;

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("key")));
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayPush(tmp, LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(clause, "values", tmp));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "id", LDNewText("rule-id")));
    LD_ASSERT(LDObjectMerge(rule, variationOrRollout));
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayPush(tmp, clause));
    LD_ASSERT(LDObjectSetKey(rule, "clauses", tmp));

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    addVariations1(flag);
    setFallthrough(flag, 0);
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayPush(tmp, rule));
    LD_ASSERT(LDObjectSetKey(flag, "rules", tmp));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(3)));

    LDJSONFree(variationOrRollout);

    return flag;
}
