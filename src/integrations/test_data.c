#include <stdio.h>
#include <string.h>
#include <launchdarkly/memory.h>

#include "cJSON.h"
#include <utlist.h>
#include "assertion.h"
#include "integrations/test_data.h"
#include "json_internal_helpers.h"
#include "store.h"

#define ALLOCATE(typ, res) \
    (res = (typ *)LDAlloc(sizeof(typ)))

#define TRUE_VARIATION_FOR_BOOLEAN 0
#define FALSE_VARIATION_FOR_BOOLEAN 1

#define RULE_ID_MAX_SIZE 16

static int variationForBoolean(LDBoolean aBoolean) {
    if(aBoolean) {
        return TRUE_VARIATION_FOR_BOOLEAN;
    } else {
        return FALSE_VARIATION_FOR_BOOLEAN;
    }
}

static void
flagBuilderUsersFree(struct LDFlagBuilderUser *users) {
    struct LDFlagBuilderUser *user, *tmpUser;
    LL_FOREACH_SAFE(users, user, tmpUser) {
        LL_DELETE(users,user);
        LDFree(user);
    }
}

static void
flagBuilderTargetsFree(struct LDFlagBuilderTarget *targets) {
    struct LDFlagBuilderTarget *target, *tmpTarget;
    HASH_ITER(hh, targets, target, tmpTarget) {
        HASH_DEL(targets, target);
        flagBuilderUsersFree(target->users);
        LDFree(target);
    }
}

static void
flagBuilderRulesFree(struct LDFlagRuleBuilder *rules) {
    struct LDFlagRuleBuilder *rule, *tmpRule;
    LL_FOREACH_SAFE(rules, rule, tmpRule) {
        struct LDFlagRuleBuilderClause *clause, *tmpClause;
        LL_DELETE(rules, rule);
        LL_FOREACH_SAFE(rule->clauses, clause, tmpClause) {
            LL_DELETE(rule->clauses, clause);
            LDJSONFree(clause->values);
            LDFree(clause);
        }
        LDFree(rule);
    }
}

static void clearUserTargets(struct LDFlagBuilder *flagBuilder) {
    flagBuilderTargetsFree(flagBuilder->targets);
}

static void clearRules(struct LDFlagBuilder *flagBuilder) {
    flagBuilderRulesFree(flagBuilder->rules);
}

LDBoolean LDi_isBooleanFlag(struct LDFlagBuilder *flagBuilder) {
    struct LDJSON *variations = flagBuilder->variations;

    if(!variations || LDJSONGetType(variations) != LDArray) {
         return LDBooleanFalse;
    }

    if (LDCollectionGetSize(variations) != 2) {
        return LDBooleanFalse;
    }

    {
        struct LDJSON *variation = LDArrayLookup(variations, TRUE_VARIATION_FOR_BOOLEAN);
        if(LDJSONGetType(variation) != LDBool || LDGetBool(variation) != LDBooleanTrue) {
            return LDBooleanFalse;
        }
    }

    {
        struct LDJSON *variation = LDArrayLookup(variations, FALSE_VARIATION_FOR_BOOLEAN);
        if(LDJSONGetType(variation) != LDBool || LDGetBool(variation) != LDBooleanFalse) {
            return LDBooleanFalse;
        }
    }

    return LDBooleanTrue;
}

struct LDTestData *
LDTestDataInit() {
    struct LDTestData *res;
    struct LDJSON *currentFlags;
    if(!ALLOCATE(struct LDTestData, res)) {
        return NULL;
    }
    memset(res, 0, sizeof(struct LDTestData));

    if(!(currentFlags = LDNewObject())) {
        LDFree(res);
        return NULL;
    }
    res->currentFlags = currentFlags;
    LDi_rwlock_init(&res->lock);
    return res;
}

void
LDTestDataFree(struct LDTestData *testData) {
    if(!testData) {
        return;
    }

    {
        struct LDFlagBuilder *flag, *tmp;

        HASH_ITER(hh, testData->flagBuilders, flag, tmp) {
            HASH_DEL(testData->flagBuilders, flag);
            LDFlagBuilderFree(flag);
        }
    }

    {
        struct LDTestDataInstance *instance, *tmp;
        LL_FOREACH_SAFE(testData->instances, instance, tmp) {
            LL_DELETE(testData->instances, instance);
            LDFree(instance);
        }
    }

    LDi_rwlock_destroy(&testData->lock);
    LDJSONFree(testData->currentFlags);
    LDFree(testData);
}

void
LDFlagBuilderFree(struct LDFlagBuilder *flagBuilder) {
    clearUserTargets(flagBuilder);
    clearRules(flagBuilder);
    LDJSONFree(flagBuilder->variations);
    LDFree(flagBuilder);
}

/**
 * Create a deep copy of a FlagBuilderTarget HashMap.
 *
 * Traverse the targets map and generate a deep copy of each target list.
 * The resulting Map should be free'd using flagBuilderTargetsFree;
 *
 * This function does NOT take ownership of the source being copied.
 *
 * @param src The HashMap to copy
 */
static struct LDFlagBuilderTarget *
flagBuilderTargetsDuplicate(struct LDFlagBuilderTarget *src) {
    struct LDFlagBuilderTarget *res, *target, *tmp, *newTarget;
    res = NULL;

    HASH_ITER(hh, src, target, tmp) {
        struct LDFlagBuilderUser *user, *tmpUser;

        if(!ALLOCATE(struct LDFlagBuilderTarget, newTarget)) {
            flagBuilderTargetsFree(res);
            return NULL;
        }
        memset(newTarget, 0, sizeof(struct LDFlagBuilderUser));
        newTarget->variation = target->variation;
        HASH_ADD_INT(res, variation, newTarget);

        LL_FOREACH(target->users, user) {
            if(!ALLOCATE(struct LDFlagBuilderUser, tmpUser)) {
                flagBuilderTargetsFree(res);
                return NULL;
            }
            memcpy(tmpUser, user, sizeof(struct LDFlagBuilderUser));
            LL_APPEND(newTarget->users, tmpUser);
        }

    }

    return res;
}

/**
 * Create a deep copy of a FlagRuleBuilder list.
 *
 * Traverse the rule list and for each entry create a new RuleBuilder for the new parent.
 * Creates a deep copy of the clause list for each entry.
 *
 * This function does NOT take ownership of the source being copied.
 *
 * The resulting list should be free'd using flagBuilderRulesFree.
 *
 * @param flagBuilder The new LDFlagBuilder to parent these LDFlagRuleBuilders
 * @param src The rule list to copy
 */
static struct LDFlagRuleBuilder *
flagBuilderRulesDuplicate(struct LDFlagBuilder *flagBuilder, struct LDFlagRuleBuilder *src) {
    struct LDFlagRuleBuilder *res, *rule, *newRule;
    struct LDFlagRuleBuilderClause *clause, *newClause;
    res = NULL;
    LL_FOREACH(src, rule) {
        if(!ALLOCATE(struct LDFlagRuleBuilder, newRule)) {
            flagBuilderRulesFree(res);
            return NULL;
        }
        memset(newRule, 0, sizeof(struct LDFlagRuleBuilder));
        newRule->variation = rule->variation;
        newRule->flag = flagBuilder;
        LL_APPEND(res, newRule);

        LL_FOREACH(rule->clauses, clause) {
            if(!ALLOCATE(struct LDFlagRuleBuilderClause, newClause)) {
                flagBuilderRulesFree(res);
                return NULL;
            }
            memcpy(newClause, clause, sizeof(struct LDFlagRuleBuilderClause));
            newClause->next = NULL;
            LL_APPEND(newRule->clauses, newClause);
        }
    }
    return res;
}

/**
 * Create a deep copy of a LDFlagBuilder.
 *
 * Create a new deep copy of the variations, targets and rules of the source builder
 *
 * This function does NOT take ownership of the source being copied.
 *
 * The resulting LDFlagBuilder should be free'd using LDFlagBuilderFree
 *
 * @param src The flag builder to copy
 */
static struct LDFlagBuilder *
flagBuilderDuplicate(struct LDFlagBuilder *src) {
    struct LDFlagBuilder *res;
    if(!ALLOCATE(struct LDFlagBuilder, res)) {
        return NULL;
    }
    memcpy(res, src, sizeof(struct LDFlagBuilder));
    if(res->variations && !(res->variations = LDJSONDuplicate(src->variations))) {
        LDFree(res);
        return NULL;
    }
    if(res->targets && !(res->targets = flagBuilderTargetsDuplicate(src->targets))) {
        LDJSONFree(res->variations);
        LDFree(res);
        return NULL;
    }

    if(res->rules && !(res->rules = flagBuilderRulesDuplicate(res, src->rules))) {
        LDJSONFree(res->variations);
        flagBuilderTargetsFree(res->targets);
        LDFree(res);
        return NULL;
    }

    return res;
}

struct LDFlagBuilder *
LDTestDataFlag(struct LDTestData *testData, const char *key) {
    struct LDFlagBuilder *res, *tmp;

    LDi_rwlock_rdlock(&testData->lock);
    HASH_FIND_STR(testData->flagBuilders, key, tmp);
    LDi_rwlock_rdunlock(&testData->lock);

    if(tmp) {
        return flagBuilderDuplicate(tmp);
    }

    if(!ALLOCATE(struct LDFlagBuilder, res)) {
        return NULL;
    }
    memset(res, 0, sizeof(struct LDFlagBuilder));

    res->key = key;
    LDFlagBuilderOn(res, LDBooleanTrue);
    if(!LDFlagBuilderBooleanFlag(res)) {
        LDFree(res);
        return NULL;
    }

    return res;
}

LDBoolean
LDTestDataUpdate(struct LDTestData *testData, struct LDFlagBuilder *flagBuilder) {
    struct LDJSON *newFlag;
    LD_ASSERT(flagBuilder);

    LDi_rwlock_wrlock(&testData->lock);
    if(!(newFlag = LDi_createNewFlag(testData, flagBuilder))) {
        LDi_rwlock_wrunlock(&testData->lock);
        return LDBooleanFalse;
    }
    if(!LDi_updateCurrentFlagsMap(testData->currentFlags, flagBuilder->key, newFlag)) {
        LDi_rwlock_wrunlock(&testData->lock);
        LDJSONFree(newFlag);
        return LDBooleanFalse;
    }

    LDi_updateFlagBuildersMap(testData, flagBuilder);

    LDi_rwlock_wrunlock(&testData->lock);

    return LDi_notifyDataInstances(testData->instances, newFlag);
}

struct LDJSON *
LDi_createNewFlag(struct LDTestData *testData, struct LDFlagBuilder *flagBuilder) {
    struct LDJSON *newFlag;
    int oldVersion = LDi_getPreviousFlagVersion(testData, flagBuilder);
    if(!(newFlag = LDFlagBuilderBuild(flagBuilder, oldVersion + 1))) {
        return NULL;
    }
    return newFlag;
}

int
LDi_getPreviousFlagVersion(struct LDTestData *testData, struct LDFlagBuilder *flagBuilder) {
    struct LDJSON *oldFlag;
    int oldVersion = 0;

    if((oldFlag = LDObjectLookup(testData->currentFlags, flagBuilder->key))) {
        struct LDJSON *jsonVersion;
        if((jsonVersion = LDObjectLookup(oldFlag, "version"))) {
           if(LDNumber == LDJSONGetType(jsonVersion)) {
               oldVersion = LDGetNumber(jsonVersion);
           }
        }
    }

    return oldVersion;
}

LDBoolean
LDi_updateCurrentFlagsMap(struct LDJSON *currentFlags, const char *key, struct LDJSON *newFlag) {
    return LDObjectSetKey(currentFlags, key, newFlag);
}

void
LDi_updateFlagBuildersMap(struct LDTestData *testData, struct LDFlagBuilder *flagBuilder) {
    struct LDFlagBuilder *tmp;
    HASH_FIND_STR(testData->flagBuilders, flagBuilder->key, tmp);
    if(tmp) {
       HASH_DEL(testData->flagBuilders, tmp);
       LDFlagBuilderFree(tmp);
    }
    HASH_ADD_KEYPTR(hh, testData->flagBuilders, flagBuilder->key, strlen(flagBuilder->key), flagBuilder);
}

LDBoolean
LDi_notifyDataInstances(struct LDTestDataInstance *instances, struct LDJSON *newFlag) {
    struct LDTestDataInstance *instance;

    LL_FOREACH(instances, instance) {
        if(instance->store) {
            struct LDJSON *newFlagForStore;
            if(!(newFlagForStore = LDJSONDuplicate(newFlag))) {
                return LDBooleanFalse;
            }
            LDStoreUpsert(instance->store, LD_FLAG, newFlagForStore);
        }
    }

    return LDBooleanTrue;
}

static LDBoolean
start(void *const context, struct LDStore *const store) {
    struct LDJSON *set, *flags;
    struct LDTestDataInstance *instance;
    instance = (struct LDTestDataInstance *)context;

    if(!(set = LDNewObject())) {
        return LDBooleanFalse;
    }

    LDi_rwlock_rdlock(&instance->testData->lock);
    if(!(flags = LDJSONDuplicate(instance->testData->currentFlags))) {
        LDi_rwlock_rdunlock(&instance->testData->lock);
        LDJSONFree(set);
        return LDBooleanFalse;
    }
    LDi_rwlock_rdunlock(&instance->testData->lock);
    if(!LDObjectSetKey(set, "features", flags)) {
        LDJSONFree(set);
        LDJSONFree(flags);
        return LDBooleanFalse;
    }

    if(!LDStoreInit(store, set)) {
        LDJSONFree(set);
        return LDBooleanFalse;
    }

    instance->store = store;

    return LDBooleanTrue;
}

static void
close(void *const context) {
    struct LDTestDataInstance *instance;
    instance = (struct LDTestDataInstance *)context;
    LDi_rwlock_wrlock(&instance->testData->lock);
    LL_DELETE(instance->testData->instances, instance);
    LDi_rwlock_wrunlock(&instance->testData->lock);
    LDFree(instance);
}

static void
destructor(void *const context) {
    (void)context;
}

struct LDDataSource *
LDTestDataCreateDataSource(struct LDTestData *testData) {
    struct LDTestDataInstance *instance;
    struct LDDataSource *dataSource;

    if(!ALLOCATE(struct LDTestDataInstance, instance)) {
        return NULL;
    }
    memset(instance, 0, sizeof(struct LDTestDataInstance));

    if(!(ALLOCATE(struct LDDataSource, dataSource))) {
        LDFree(instance);
    }
    memset(dataSource, 0, sizeof(struct LDDataSource));

    instance->testData = testData;

    LDi_rwlock_wrlock(&testData->lock);
    LL_PREPEND(testData->instances, instance);
    LDi_rwlock_wrunlock(&testData->lock);

    dataSource->context = (void *)instance;
    dataSource->init = start;
    dataSource->close = close;
    dataSource->destructor = destructor;

    return dataSource;
}


struct LDJSON *
LDFlagBuilderBuild(struct LDFlagBuilder *flagBuilder, int version) {
    struct LDJSON *res;

    if(!(res = LDNewObject())) {
        return NULL;
    }

    if(!LDObjectSetString(res, "key", flagBuilder->key)) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetString(res, "salt", "salt")) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetNumber(res, "version", version)) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetBool(res, "on", flagBuilder->on)) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetNumber(res, "offVariation", flagBuilder->offVariation)) {
        LDJSONFree(res);
        return NULL;
    }

    {
        struct LDJSON *variationsDuplicate;
        if(!(variationsDuplicate = LDJSONDuplicate(flagBuilder->variations))) {
            LDJSONFree(res);
            return NULL;
        }
        if(!LDObjectSetKey(res, "variations", variationsDuplicate)) {
            LDJSONFree(variationsDuplicate);
            LDJSONFree(res);
            return NULL;
        }
    }

    {
        struct LDJSON *fallthrough;
        if(!(fallthrough = LDObjectNewChild(res, "fallthrough"))) {
            LDJSONFree(res);
            return NULL;
        }
        if(!LDObjectSetNumber(fallthrough, "variation", flagBuilder->fallthroughVariation)) {
            LDJSONFree(res);
            return NULL;
        }
    }

    if(flagBuilder->targets) {
        struct LDJSON *targets;
        if(!(targets = LDi_buildTargetsJSON(flagBuilder->targets))) {
            LDJSONFree(res);
            return NULL;
        }
        if(!LDObjectSetKey(res, "targets", targets)) {
            LDJSONFree(targets);
            LDJSONFree(res);
            return NULL;
        }
    }

    if(flagBuilder->rules) {
        struct LDJSON *jsonRules;
        if(!(jsonRules = LDi_buildRulesJSON(flagBuilder->rules))) {
            LDJSONFree(res);
            return NULL;
        }
        if(!LDObjectSetKey(res, "rules", jsonRules)) {
            LDJSONFree(jsonRules);
            LDJSONFree(res);
            return NULL;
        }
    }

    return res;
}

struct LDJSON *
LDi_buildTargetsJSON(struct LDFlagBuilderTarget *targets) {
    struct LDJSON *res;
    struct LDFlagBuilderTarget *target, *tmpTarget;

    if(!(res = LDNewArray())) {
        LDJSONFree(res);
        return NULL;
    }

    HASH_ITER(hh, targets, target, tmpTarget) {
        struct LDJSON *jsonTarget;
        if(!(jsonTarget = LDi_buildTargetJSON(target))) {
            LDJSONFree(res);
            return NULL;
        }

        if(!LDArrayPush(res, jsonTarget)) {
            LDJSONFree(jsonTarget);
            LDJSONFree(res);
            return NULL;
        }
    }

    return res;
}

struct LDJSON *
LDi_buildTargetJSON(struct LDFlagBuilderTarget *target) {
    struct LDJSON *res;
    struct LDJSON *users;

    if(!(res = LDNewObject())) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetNumber(res, "variation", target->variation)) {
        LDJSONFree(res);
        return NULL;
    }

    if(!(users = LDi_buildUsersJSON(target->users))) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetKey(res, "values", users)) {
        LDJSONFree(users);
        LDJSONFree(res);
        return NULL;
    }

    return res;
}

struct LDJSON *
LDi_buildUsersJSON(struct LDFlagBuilderUser *users) {
    struct LDJSON *res;
    struct LDFlagBuilderUser *user;

    if(!(res = LDNewArray())) {
        return NULL;
    }

    LL_FOREACH(users, user) {
        struct LDJSON *userKey;
        if(!(userKey = LDNewText(user->userKey))) {
            LDJSONFree(res);
            return NULL;
        }
        if(!LDArrayPush(res, userKey)) {
            LDJSONFree(userKey);
            LDJSONFree(res);
            return NULL;
        }
    }

    return res;
}

struct LDJSON *
LDi_buildRulesJSON(struct LDFlagRuleBuilder *rules) {
    struct LDJSON *res;
    struct LDFlagRuleBuilder *rule;
    int ri = 0;

    if(!(res = LDNewArray())) {
        return NULL;
    }

    LL_FOREACH(rules, rule) {
        struct LDJSON *jsonRule;
        char ruleId[RULE_ID_MAX_SIZE];
        snprintf(ruleId, RULE_ID_MAX_SIZE, "rule%d", ri++);
        if(!(jsonRule = LDi_buildRuleJSON(rule, ruleId))) {
            LDJSONFree(res);
            return NULL;
        }
        if(!(LDArrayPush(res, jsonRule))) {
            LDJSONFree(jsonRule);
            LDJSONFree(res);
            return NULL;
        }
    }

    return res;
}

struct LDJSON *
LDi_buildRuleJSON(struct LDFlagRuleBuilder *rule, const char *ruleId) {
    struct LDJSON *res;

    if(!(res = LDNewObject())) {
        return NULL;
    }

    if(!LDObjectSetString(res, "id", ruleId)) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetNumber(res, "variation", rule->variation)) {
        LDJSONFree(res);
        return NULL;
    }

    {
        struct LDJSON *jsonClauses;
        if(!(jsonClauses = LDi_buildClausesJSON(rule->clauses))) {
            LDJSONFree(res);
            return NULL;
        }

        if(!LDObjectSetKey(res, "clauses", jsonClauses)) {
            LDJSONFree(jsonClauses);
            LDJSONFree(res);
            return NULL;
        }
    }

    return res;
}

struct LDJSON *
LDi_buildClausesJSON(struct LDFlagRuleBuilderClause *clauses) {
    struct LDJSON *res;
    struct LDFlagRuleBuilderClause *clause;
    if(!(res = LDNewArray())) {
        return NULL;
    }

    LL_FOREACH(clauses, clause) {
        struct LDJSON *jsonClause;
        if(!(jsonClause = LDi_buildClauseJSON(clause))) {
            LDJSONFree(res);
            return NULL;
        }
        if(!(LDArrayPush(res, jsonClause))) {
            LDJSONFree(res);
            return NULL;
        }
    }

    return res;
}

struct LDJSON *
LDi_buildClauseJSON(struct LDFlagRuleBuilderClause *clause) {
    struct LDJSON *res, *jsonValues;

    if(!(res = LDNewObject())) {
        return NULL;
    }

    if(!LDObjectSetString(res, "attribute", clause->attribute)) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetString(res, "op", "in")) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetBool(res, "negate", clause->negate)) {
        LDJSONFree(res);
        return NULL;
    }

    if(!(jsonValues = LDJSONDuplicate(clause->values))) {
        LDJSONFree(res);
        return NULL;
    }

    if(!LDObjectSetKey(res, "values", jsonValues)) {
        LDJSONFree(jsonValues);
        LDJSONFree(res);
        return NULL;
    }

    return res;
}


LDBoolean
LDFlagBuilderBooleanFlag(struct LDFlagBuilder *flagBuilder) {
    struct LDJSON *variations, *tmp;
    if (LDi_isBooleanFlag(flagBuilder)) {
        return LDBooleanTrue;
    }

    if(!(variations = LDNewArray())) {
        return LDBooleanFalse;
    }

    if(!(tmp = LDNewBool(LDBooleanTrue))) {
        LDJSONFree(variations);
        return LDBooleanFalse;
    }

    if(!LDArrayPush(variations, tmp)) {
        LDJSONFree(tmp);
        LDJSONFree(variations);
        return LDBooleanFalse;
    }

    if(!(tmp = LDNewBool(LDBooleanFalse))) {
        LDJSONFree(variations);
        return LDBooleanFalse;
    }

    if(!LDArrayPush(variations, tmp)) {
        LDJSONFree(tmp);
        LDJSONFree(variations);
        return LDBooleanFalse;
    }

    if(!LDFlagBuilderVariations(flagBuilder, variations)) {
        LDJSONFree(variations);
        return LDBooleanFalse;
    }

    LDFlagBuilderFallthroughVariation(flagBuilder, TRUE_VARIATION_FOR_BOOLEAN);
    LDFlagBuilderOffVariation(flagBuilder, FALSE_VARIATION_FOR_BOOLEAN);

    return LDBooleanTrue;
}

void
LDFlagBuilderOn(struct LDFlagBuilder *flagBuilder, LDBoolean on) {
    flagBuilder->on = on;
}

void
LDFlagBuilderFallthroughVariation(struct LDFlagBuilder *flagBuilder, int variationIndex) {
    flagBuilder->fallthroughVariation = variationIndex;
}

LDBoolean
LDFlagBuilderFallthroughVariationBoolean(struct LDFlagBuilder *flagBuilder, LDBoolean value) {
    if(!LDFlagBuilderBooleanFlag(flagBuilder)) {
        return LDBooleanFalse;
    }

    LDFlagBuilderFallthroughVariation(flagBuilder, variationForBoolean(value));
    return LDBooleanTrue;
}

void
LDFlagBuilderOffVariation(struct LDFlagBuilder *flagBuilder, int variationIndex) {
    flagBuilder->offVariation = variationIndex;
}

LDBoolean
LDFlagBuilderOffVariationBoolean(struct LDFlagBuilder *flagBuilder, LDBoolean value) {
    if(!LDFlagBuilderBooleanFlag(flagBuilder)) {
        return LDBooleanFalse;
    }

    LDFlagBuilderOffVariation(flagBuilder, variationForBoolean(value));
    return LDBooleanTrue;
}

void
LDFlagBuilderVariationForAllUsers(struct LDFlagBuilder *flagBuilder, int variationIndex) {
    clearUserTargets(flagBuilder);
    clearRules(flagBuilder);
    LDFlagBuilderOn(flagBuilder, LDBooleanTrue);
    LDFlagBuilderFallthroughVariation(flagBuilder, variationIndex);
}

LDBoolean
LDFlagBuilderVariationForAllUsersBoolean(struct LDFlagBuilder *flagBuilder, LDBoolean value) {
    if(!LDFlagBuilderBooleanFlag(flagBuilder)) {
        return LDBooleanFalse;
    }
    LDFlagBuilderVariationForAllUsers(flagBuilder, variationForBoolean(value));
    return LDBooleanTrue;
}

void
LDFlagBuilderValueForAllUsers(struct LDFlagBuilder *flagBuilder, struct LDJSON *value) {
    LDFlagBuilderVariations(flagBuilder, value);
    LDFlagBuilderVariationForAllUsers(flagBuilder, 0);
}

static struct LDFlagBuilderUser *
detachUserFromOtherTargets(struct LDFlagBuilder *flagBuilder, const char *userKey, int variationIndex) {
    struct LDFlagBuilderTarget *target, *tmpTarget;
    HASH_ITER(hh, flagBuilder->targets, target, tmpTarget) {
        struct LDFlagBuilderUser *user, *tmpUser;

        if(target->variation == variationIndex) {
            continue;
        }

        LL_FOREACH_SAFE(target->users, user, tmpUser) {
            if(strcmp(user->userKey, userKey) == 0) {
                LL_DELETE(target->users,user);
                return user;
            }
        }
    }
    return NULL;
}

static int
compareUser(struct LDFlagBuilderUser *aUser, struct LDFlagBuilderUser *anotherUser) {
    return strcmp(aUser->userKey, anotherUser->userKey);
}

LDBoolean
LDFlagBuilderVariationForUser(struct LDFlagBuilder *flagBuilder, const char *userKey, int variationIndex) {
    struct LDFlagBuilderTarget *target;
    HASH_FIND_INT(flagBuilder->targets, &variationIndex, target);
    if(!target) {
        if (!ALLOCATE(struct LDFlagBuilderTarget, target)) {
            return LDBooleanFalse;
        }
        memset(target, 0, sizeof(struct LDFlagBuilderTarget));
        target->variation = variationIndex;
        HASH_ADD_INT(flagBuilder->targets, variation, target);
    }

    {
        struct LDFlagBuilderUser *user;
        struct LDFlagBuilderUser likeUser;
        likeUser.userKey = userKey;
        LL_SEARCH(target->users, user, &likeUser, compareUser);
        if(!user) {
            user = detachUserFromOtherTargets(flagBuilder, userKey, variationIndex);
            if(!user) {
                if(!ALLOCATE(struct LDFlagBuilderUser, user)) {
                    if(!target->users) {
                        HASH_DEL(flagBuilder->targets, target);
                    }
                    return LDBooleanFalse;
                }
                user->userKey = userKey;
            }

            LL_PREPEND(target->users, user);
        }
    }

    return LDBooleanTrue;
}

LDBoolean
LDFlagBuilderVariationForUserBoolean(struct LDFlagBuilder *flagBuilder, const char *userKey, LDBoolean value) {
    if(!LDFlagBuilderBooleanFlag(flagBuilder)) {
        return LDBooleanFalse;
    }
    return LDFlagBuilderVariationForUser(flagBuilder, userKey, variationForBoolean(value));
}

/**
 * Wrap a JSON value in an array if it is not already wrapped.
 *
 * If this function is successful ownership of value is transferred to the new JSON value
 *
 * @param value The JSON value to wrap
 */
static struct LDJSON *
asJSONArray(struct LDJSON *value) {
    struct LDJSON *array;

    if(LDJSONGetType(value) == LDArray) {
        array = value;
    } else { /* Wrap value in array */
        struct LDJSON *tmp;
        if(!(tmp = LDNewArray())) {
            return NULL;
        }
        if(!LDArrayPush(tmp, value)) {
            LDJSONFree(tmp);
            return NULL;
        }
        array = tmp;
    }

    return array;
}

LDBoolean
LDFlagBuilderVariations(struct LDFlagBuilder *flagBuilder, struct LDJSON *variations) {
    struct LDJSON *newVariations;
    LD_ASSERT(variations);
    if(!(newVariations = asJSONArray(variations))) {
        return LDBooleanFalse;
    }

    if(flagBuilder->variations) {
        LDJSONFree(flagBuilder->variations);
    }
    flagBuilder->variations = newVariations;
    return LDBooleanTrue;
}

static struct LDFlagRuleBuilderClause *
newFlagRuleBuilderClause(const char *const attribute, struct LDJSON *values, LDBoolean negate) {
    struct LDFlagRuleBuilderClause *clause;
    struct LDJSON *tmp;

    if(!ALLOCATE(struct LDFlagRuleBuilderClause, clause)) {
        return NULL;
    }
    memset(clause, 0, sizeof(struct LDFlagRuleBuilderClause));

    if(!(tmp = asJSONArray(values))) {
        LDFree(clause);
        return NULL;
    }

    clause->attribute = attribute;
    clause->negate = negate;
    clause->values = tmp;

    return clause;
}

struct LDFlagRuleBuilder *
LDFlagBuilderIfMatch(struct LDFlagBuilder *flagBuilder, const char *const attribute, struct LDJSON *values) {
    struct LDFlagRuleBuilder *ruleBuilder;

    if(!ALLOCATE(struct LDFlagRuleBuilder, ruleBuilder)) {
        return NULL;
    }
    memset(ruleBuilder, 0, sizeof(struct LDFlagRuleBuilder));

    ruleBuilder->flag = flagBuilder;

    if(!ruleBuilder) {
        return NULL;
    }

    if(!LDFlagRuleBuilderAndMatch(ruleBuilder, attribute, values)) {
        LDFree(ruleBuilder);
        return NULL;
    }

    LL_PREPEND(flagBuilder->rules, ruleBuilder);

    return ruleBuilder;
}

struct LDFlagRuleBuilder *
LDFlagBuilderIfNotMatch(struct LDFlagBuilder *flagBuilder, const char *const attribute, struct LDJSON *values) {
    struct LDFlagRuleBuilder *ruleBuilder;

    if(!ALLOCATE(struct LDFlagRuleBuilder, ruleBuilder)) {
        return NULL;
    }
    memset(ruleBuilder, 0, sizeof(struct LDFlagRuleBuilder));

    ruleBuilder->flag = flagBuilder;

    if(!ruleBuilder) {
        return NULL;
    }

    if(!LDFlagRuleBuilderAndNotMatch(ruleBuilder, attribute, values)) {
        LDFree(ruleBuilder);
        return NULL;
    }

    LL_PREPEND(flagBuilder->rules, ruleBuilder);

    return ruleBuilder;
}

LDBoolean
LDFlagRuleBuilderAndMatch(struct LDFlagRuleBuilder *ruleBuilder, const char *const attribute, struct LDJSON *values) {
    struct LDFlagRuleBuilderClause *clause;
    if(!(clause = newFlagRuleBuilderClause(attribute, values, LDBooleanFalse))) {
        return LDBooleanFalse;
    }
    LL_PREPEND(ruleBuilder->clauses, clause);
    return LDBooleanTrue;
}

LDBoolean
LDFlagRuleBuilderAndNotMatch(struct LDFlagRuleBuilder *ruleBuilder, const char *const attribute, struct LDJSON *values) {
    struct LDFlagRuleBuilderClause *clause;
    if(!(clause = newFlagRuleBuilderClause(attribute, values, LDBooleanTrue))) {
        return LDBooleanFalse;
    }
    LL_PREPEND(ruleBuilder->clauses, clause);
    return LDBooleanTrue;
}

void
LDFlagRuleBuilderThenReturn(struct LDFlagRuleBuilder *ruleBuilder, int variationIndex) {
    ruleBuilder->variation = variationIndex;
}

LDBoolean
LDFlagRuleBuilderThenReturnBoolean(struct LDFlagRuleBuilder *ruleBuilder, LDBoolean value) {
    if(!LDFlagBuilderBooleanFlag(ruleBuilder->flag)) {
        return LDBooleanFalse;
    }

    LDFlagRuleBuilderThenReturn(ruleBuilder, variationForBoolean(value));
    return LDBooleanTrue;
}

