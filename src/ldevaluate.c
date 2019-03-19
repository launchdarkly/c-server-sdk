#include "sha1.h"
#include "hexify.h"

#include "ldinternal.h"
#include "ldevaluate.h"
#include "ldoperators.h"
#include "ldevents.h"

bool
LDi_isEvalError(const EvalStatus status)
{
    return status == EVAL_MEM || status == EVAL_SCHEMA || status == EVAL_STORE;
}

static EvalStatus
maybeNegate(const struct LDJSON *const object, const EvalStatus status)
{
    const struct LDJSON *negate;

    LD_ASSERT(object);

    negate = NULL;

    if (LDi_isEvalError(status)) {
        return status;
    }

    if (LDi_notNull(negate = LDObjectLookup(object, "negate"))) {
        if (LDJSONGetType(negate) != LDBool) {
            return EVAL_SCHEMA;
        }

        if (LDGetBool(negate)) {
            if (status == EVAL_MATCH) {
                return EVAL_MISS;
            } else if (status == EVAL_MISS) {
                return EVAL_MATCH;
            }
        }
    }

    return status;
}

struct LDJSON *
LDi_addReason(struct LDJSON **const result, const char *const reason,
    struct LDJSON *const events)
{
    struct LDJSON *tmpcollection, *tmp;

    LD_ASSERT(reason);

    tmpcollection = NULL;
    tmp           = NULL;

    if (!(*result)) {
        if (!(*result = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "allocation error");

            return NULL;
        }
    }

    if (!(tmpcollection = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return NULL;
    }

    if (!(tmp = LDNewText(reason))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        LDJSONFree(tmpcollection);

        return NULL;
    }

    if (!LDObjectSetKey(tmpcollection, "kind", tmp)) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        LDJSONFree(tmpcollection);

        return NULL;
    }

    if (!LDObjectSetKey(*result, "reason", tmpcollection)) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        LDJSONFree(tmpcollection);

        return NULL;
    }

    if (events) {
        if (!LDObjectSetKey(*result, "events", events)) {
            LD_LOG(LD_LOG_ERROR, "events");

            return NULL;
        }
    }

    return tmpcollection;
}

struct LDJSON *
LDi_addErrorReason(struct LDJSON **const result, const char *const kind)
{
    struct LDJSON *tmpcollection, *tmp;

    LD_ASSERT(result);
    LD_ASSERT(kind);

    tmpcollection = NULL;
    tmp           = NULL;

    if (!(tmpcollection = LDi_addReason(result, "ERROR", NULL))) {
        LD_LOG(LD_LOG_ERROR, "LDi_addReason failed");

        return NULL;
    }

    if (!(tmp = LDNewText(kind))) {
        LDJSONFree(tmpcollection);

        LD_LOG(LD_LOG_ERROR, "allocation error");

        return NULL;
    }

    if (!(LDObjectSetKey(tmpcollection, "errorKind", tmp))) {
        LDJSONFree(tmpcollection);

        LD_LOG(LD_LOG_ERROR, "failed to set key");

        return NULL;
    }

    return tmpcollection;
}

static bool
addValue(const struct LDJSON *const flag, struct LDJSON *result,
    const struct LDJSON *const index)
{
    struct LDJSON *tmp, *variations, *variation;

    LD_ASSERT(flag);
    LD_ASSERT(result);

    tmp        = NULL;
    variations = NULL;
    variation  = NULL;

    if (LDi_notNull(index)) {
        if (LDJSONGetType(index) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (!(tmp = LDNewNumber(LDGetNumber(index)))) {
            LD_LOG(LD_LOG_ERROR, "allocation error");

            return false;
        }
    } else {
        if (!(tmp = LDNewNull())) {
            LD_LOG(LD_LOG_ERROR, "allocation error");

            return false;
        }
    }

    if (!(LDObjectSetKey(result, "variationIndex", tmp))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        LDJSONFree(tmp);

        return false;
    }

    if (LDi_notNull(index)) {
        if (!(variations = LDObjectLookup(flag, "variations"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(variations) != LDArray) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (!(variation = LDArrayLookup(variations,
            LDGetNumber(index))))
        {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (!(tmp = LDJSONDuplicate(variation))) {
            LD_LOG(LD_LOG_ERROR, "allocation error");

            return false;
        }
    } else {
        if (!(tmp = LDNewNull())) {
            LD_LOG(LD_LOG_ERROR, "allocation error");

            return false;
        }
    }

    if (!(LDObjectSetKey(result, "value", tmp))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        LDJSONFree(tmp);

        return false;
    }

    return true;
}

EvalStatus
LDi_evaluate(struct LDClient *const client, const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    struct LDJSON **const result)
{
    EvalStatus substatus;
    const struct LDJSON *iter, *rules, *targets, *on;
    struct LDJSON *events, *index;
    const char *failedKey;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(result);

    iter        = NULL;
    rules       = NULL;
    targets     = NULL;
    on          = NULL;
    failedKey   = NULL;
    events      = NULL;
    index       = NULL;

    if (LDJSONGetType(flag) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    /* on */
    if (!(on = LDObjectLookup(flag, "on"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(on) != LDBool) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!LDGetBool(on)) {
        const struct LDJSON *offVariation;

        offVariation = LDObjectLookup(flag, "offVariation");

        if (!LDi_addReason(result, "OFF", NULL)) {
            LD_LOG(LD_LOG_ERROR, "failed to add reason");

            return EVAL_MEM;
        }

        if (!(addValue(flag, *result, offVariation))) {
            LD_LOG(LD_LOG_ERROR, "failed to add value");

            return EVAL_MEM;
        }

        return EVAL_MISS;
    }

    /* prerequisites */
    if (LDi_isEvalError(substatus =
        LDi_checkPrerequisites(client, flag, user, store, &failedKey, &events)))
    {
        struct LDJSON *reason;

        LD_LOG(LD_LOG_ERROR, "sub error error");

        if (!(reason = LDi_addReason(result, "PREREQUISITE_FAILED", events))) {
            LD_LOG(LD_LOG_ERROR, "failed to add reason");

            return EVAL_MEM;
        }

        return substatus;
    }

    if (substatus == EVAL_MISS) {
        struct LDJSON *key, *reason;

        key    = NULL;
        reason = NULL;

        if (!(reason = LDi_addReason(result, "PREREQUISITE_FAILED", events))) {
            LD_LOG(LD_LOG_ERROR, "failed to add reason");

            return EVAL_MEM;
        }

        if (!(key = LDNewText(failedKey))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            return EVAL_MEM;
        }

        if (!LDObjectSetKey(reason, "prerequisiteKey", key)) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            return EVAL_MEM;
        }

        if (!(addValue(flag, *result, LDObjectLookup(flag, "offVariation")))) {
            LD_LOG(LD_LOG_ERROR, "failed to add value");

            return EVAL_MEM;
        }

        return EVAL_MISS;
    }

    /* targets */
    targets = LDObjectLookup(flag, "targets");

    if (targets && LDJSONGetType(targets) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (targets) {
        for (iter = LDGetIter(targets); iter; iter = LDIterNext(iter)) {
            const struct LDJSON *values = NULL;

            if (LDJSONGetType(iter) != LDObject) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (!(values = LDObjectLookup(iter, "values"))) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (LDJSONGetType(values) != LDArray) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (user->key && LDi_textInArray(values, user->key)) {
                const struct LDJSON *variation = NULL;

                variation = LDObjectLookup(iter, "variation");

                if (!LDi_addReason(result, "TARGET_MATCH", events)) {
                    LD_LOG(LD_LOG_ERROR, "failed to add reason");

                    return EVAL_MEM;
                }

                if (!(addValue(flag, *result, variation))) {
                    LD_LOG(LD_LOG_ERROR, "failed to add value");

                    return EVAL_MEM;
                }

                return EVAL_MATCH;
            }
        }
    }

    /* rules */
    rules = LDObjectLookup(flag, "rules");

    if (rules && LDJSONGetType(rules) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (rules && LDCollectionGetSize(rules) != 0) {
        unsigned int index = 0;
        for (iter = LDGetIter(rules); iter; iter = LDIterNext(iter)) {
            EvalStatus substatus;

            if (LDJSONGetType(iter) != LDObject) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (LDi_isEvalError(substatus = LDi_ruleMatchesUser(
                iter, user, store)))
            {
                LD_LOG(LD_LOG_ERROR, "sub error");

                return substatus;
            }

            if (substatus == EVAL_MATCH) {
                struct LDJSON *variation, *reason, *tmp, *ruleid;

                variation = NULL;
                reason    = NULL;
                tmp       = NULL;
                ruleid    = NULL;

                if (!(reason = LDi_addReason(result, "RULE_MATCH", events))) {
                    LD_LOG(LD_LOG_ERROR, "failed to add reason");

                    return EVAL_MEM;
                }

                variation = LDi_getIndexForVariationOrRollout(flag, iter, user);

                if (!(addValue(flag, *result, variation))) {
                    LD_LOG(LD_LOG_ERROR, "failed to add value");

                    LDJSONFree(variation);

                    return EVAL_MEM;
                }

                LDJSONFree(variation);

                if (!(tmp = LDNewNumber(index))) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

                    return EVAL_MEM;
                }

                if (!LDObjectSetKey(reason, "ruleIndex", tmp)) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

                    LDJSONFree(tmp);

                    return EVAL_MEM;
                }

                if (!(ruleid = LDObjectLookup(iter, "id"))) {
                    LD_LOG(LD_LOG_ERROR, "schema error");

                    return EVAL_SCHEMA;
                }

                if (LDJSONGetType(ruleid) != LDText) {
                    LD_LOG(LD_LOG_ERROR, "schema error");

                    return EVAL_SCHEMA;
                }

                if (!(tmp = LDNewText(LDGetText(ruleid)))) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

                    return EVAL_MEM;
                }

                if (!LDObjectSetKey(reason, "ruleId", tmp)) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

                    LDJSONFree(tmp);

                    return EVAL_MEM;
                }


                return EVAL_MATCH;
            }

            index++;
        }
    }

    /* fallthrough */
    if (!LDi_addReason(result, "FALLTHROUGH", events)) {
        LD_LOG(LD_LOG_ERROR, "failed to add reason");

        return EVAL_MEM;
    }

    index = LDi_getIndexForVariationOrRollout(flag,
        LDObjectLookup(flag, "fallthrough"), user);

    if (!(addValue(flag, *result, index))) {
        LD_LOG(LD_LOG_ERROR, "failed to add value");

        LDJSONFree(index);

        return EVAL_MEM;
    }

    LDJSONFree(index);

    return EVAL_MATCH;
}

EvalStatus
LDi_checkPrerequisites(struct LDClient *const client,
    const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    const char **const failedKey, struct LDJSON **const events)
{
    struct LDJSON *prerequisites, *iter;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(failedKey);
    LD_ASSERT(events);

    prerequisites = NULL;
    iter          = NULL;

    if (LDJSONGetType(flag) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    prerequisites = LDObjectLookup(flag, "prerequisites");

    if (!prerequisites) {
        return EVAL_MATCH;
    }

    if (LDJSONGetType(prerequisites) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    for (iter = LDGetIter(prerequisites); iter; iter = LDIterNext(iter)) {
        struct LDJSON *result, *preflag, *event, *subevents;
        const struct LDJSON *key, *variation, *variationNumJSON;
        unsigned int variationNum, *variationNumRef ;
        EvalStatus status;

        result           = NULL;
        preflag          = NULL;
        key              = NULL;
        variation        = NULL;
        variationNumRef  = NULL;
        variationNumJSON = NULL;
        event            = NULL;
        subevents        = NULL;

        if (LDJSONGetType(iter) != LDObject) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (!(key = LDObjectLookup(iter, "key"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDJSONGetType(key) != LDText) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        *failedKey = LDGetText(key);

        if (!(variation = LDObjectLookup(iter, "variation"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDJSONGetType(variation) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (!LDStoreGet(store, "flags", LDGetText(key), &preflag)) {
            LD_LOG(LD_LOG_ERROR, "store lookup error");

            return EVAL_STORE;
        }

        if (!preflag || LDi_isDeleted(preflag))
        {
            LD_LOG(LD_LOG_ERROR, "store lookup error");

            LDJSONFree(preflag);

            return EVAL_MISS;
        }

        if (LDi_isEvalError(status = LDi_evaluate(
            client, preflag, user, store, &result)))
        {
            LDJSONFree(preflag);

            return status;
        }

        if (!result) {
            LDJSONFree(preflag);

            LD_LOG(LD_LOG_ERROR, "sub error with result");
        }

        variationNumJSON = LDObjectLookup(result, "variationIndex");

        if (LDi_notNull(variationNumJSON)) {
            if (LDJSONGetType(variationNumJSON) != LDNumber) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                LDJSONFree(preflag);
                LDJSONFree(result);

                return EVAL_SCHEMA;
            }

            variationNum = LDGetNumber(variationNumJSON);

            variationNumRef = &variationNum;
        }

        event = LDi_newFeatureRequestEvent(client,
            LDGetText(key), user, variationNumRef,
            LDObjectLookup(result, "value"), NULL,
            LDGetText(LDObjectLookup(flag, "key")), preflag,
            LDObjectLookup(result, "reason"));

        if (!event) {
            LDJSONFree(preflag);
            LDJSONFree(result);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        if (!(*events)) {
            if (!(*events = LDNewArray())) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LD_LOG(LD_LOG_ERROR, "alloc error");

                return EVAL_MEM;
            }
        }

        if ((subevents = LDObjectLookup(result, "events"))) {
            if (!LDArrayAppend(*events, subevents)) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LD_LOG(LD_LOG_ERROR, "alloc error");

                return EVAL_MEM;
            }
        }

        if (!LDArrayPush(*events, event)) {
            LDJSONFree(preflag);
            LDJSONFree(result);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        {
            struct LDJSON *on;
            struct LDJSON *variationIndex;
            bool variationMatch = false;

            if (!(on = LDObjectLookup(preflag, "on"))) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (LDJSONGetType(on) != LDBool) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            variationIndex = LDObjectLookup(result, "variationIndex");

            if (LDi_notNull(variationIndex)) {
                if (LDJSONGetType(variationIndex) != LDNumber) {
                    LDJSONFree(preflag);
                    LDJSONFree(result);

                    LD_LOG(LD_LOG_ERROR, "schema error");

                    return EVAL_SCHEMA;
                }

                variationMatch = LDGetNumber(variationIndex) ==
                    LDGetNumber(variation);
            }

            if (status == EVAL_MISS || !LDGetBool(on) || !variationMatch)
            {
                LDJSONFree(preflag);
                LDJSONFree(result);

                return EVAL_MISS;
            }
        }

        LDJSONFree(preflag);
        LDJSONFree(result);
    }

    return EVAL_MATCH;
}

EvalStatus
LDi_ruleMatchesUser(const struct LDJSON *const rule,
    const struct LDUser *const user, struct LDStore *const store)
{
    const struct LDJSON *clauses = NULL;
    const struct LDJSON *iter = NULL;

    LD_ASSERT(rule);
    LD_ASSERT(user);

    if (!(clauses = LDObjectLookup(rule, "clauses"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(clauses) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    for (iter = LDGetIter(clauses); iter; iter = LDIterNext(iter)) {
        EvalStatus substatus;

        if (LDJSONGetType(iter) != LDObject) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDi_isEvalError(substatus = LDi_clauseMatchesUser(
            iter, user, store)))
        {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return substatus;
        }

        if (substatus == EVAL_MISS) {
            return EVAL_MISS;
        }
    }

    return EVAL_MATCH;
}

EvalStatus
LDi_clauseMatchesUser(const struct LDJSON *const clause,
    const struct LDUser *const user, struct LDStore *const store)
{
    const struct LDJSON *op;

    LD_ASSERT(clause);
    LD_ASSERT(user);

    op = NULL;

    if (LDJSONGetType(clause) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(op = LDObjectLookup(clause, "op"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(op) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (strcmp(LDGetText(op), "segmentMatch") == 0) {
        bool anysuccess;
        const struct LDJSON *values, *iter;

        values    = NULL;
        iter      = NULL;
        anysuccess = false;

        if (!(values = LDObjectLookup(clause, "values"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDJSONGetType(values) != LDArray) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        for (iter = LDGetIter(values); iter; iter = LDIterNext(iter)) {
            if (LDJSONGetType(iter) == LDText) {
                EvalStatus evalstatus;
                struct LDJSON *segment;

                if (!LDStoreGet(store, "segments", LDGetText(iter), &segment)) {
                    LD_LOG(LD_LOG_ERROR, "store lookup error");

                    return EVAL_STORE;
                }

                if (!segment || LDi_isDeleted(segment)) {
                    LD_LOG(LD_LOG_WARNING, "store lookup error");

                    continue;
                }

                anysuccess = true;

                if (LDi_isEvalError(
                    evalstatus = LDi_segmentMatchesUser(segment, user)))
                {
                    LD_LOG(LD_LOG_ERROR, "sub error");

                    LDJSONFree(segment);

                    return evalstatus;
                }

                LDJSONFree(segment);

                if (evalstatus == EVAL_MATCH) {
                    return maybeNegate(clause, EVAL_MATCH);
                }
            }
        }

        if (!anysuccess && LDGetIter(values)) {
            return EVAL_MISS;
        }

        return maybeNegate(clause, EVAL_MISS);
    }

    return LDi_clauseMatchesUserNoSegments(clause, user);
}

EvalStatus
LDi_segmentMatchesUser(const struct LDJSON *const segment,
    const struct LDUser *const user)
{
    const struct LDJSON *included, *excluded, *iter, *salt, *segmentRules, *key;

    LD_ASSERT(segment);
    LD_ASSERT(user);

    included     = NULL;
    excluded     = NULL;
    key          = NULL;
    salt         = NULL;
    segmentRules = NULL;
    iter         = NULL;

    if (!user->key) {
        return EVAL_MISS;
    }

    included = LDObjectLookup(segment, "included");

    if (LDi_notNull(included)) {
        if (LDJSONGetType(included) != LDArray) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDi_textInArray(included, user->key)) {
            return EVAL_MATCH;
        }
    }

    excluded = LDObjectLookup(segment, "excluded");

    if (LDi_notNull(excluded)) {
        if (LDJSONGetType(excluded) != LDArray) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDi_textInArray(excluded, user->key)) {
            return EVAL_MISS;
        }
    }

    if (!(segmentRules = LDObjectLookup(segment, "rules"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(segmentRules) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(key = LDObjectLookup(segment, "key"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(key) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(salt = LDObjectLookup(segment, "salt"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(salt) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    for (iter = LDGetIter(segmentRules); iter; iter = LDIterNext(iter)) {
        EvalStatus substatus;

        if (LDJSONGetType(iter) != LDObject) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDi_isEvalError(substatus = LDi_segmentRuleMatchUser(iter,
            LDGetText(key), user, LDGetText(salt))))
        {
            return substatus;
        }

        if (substatus == EVAL_MATCH) {
            return EVAL_MATCH;
        }
    }

    return EVAL_MISS;
}

EvalStatus
LDi_segmentRuleMatchUser(const struct LDJSON *const segmentRule,
    const char *const segmentKey, const struct LDUser *const user,
    const char *const salt)
{
    const struct LDJSON *clauses, *clause;

    LD_ASSERT(segmentRule);
    LD_ASSERT(segmentKey);
    LD_ASSERT(user);
    LD_ASSERT(salt);

    clauses = NULL;
    clause  = NULL;

    if (!(clauses = LDObjectLookup(segmentRule, "clauses"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(clauses) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    for (clause = LDGetIter(clauses); clause; clause = LDIterNext(clause)) {
        EvalStatus substatus;

        if (LDi_isEvalError(substatus =
            LDi_clauseMatchesUserNoSegments(clause, user)))
        {
            return substatus;
        }

        if (substatus == EVAL_MISS) {
            return EVAL_MISS;
        }
    }

    {
        const struct LDJSON *weight = LDObjectLookup(segmentRule, "weight");

        if (!LDi_notNull(weight)) {
            return EVAL_MATCH;
        }

        if (LDJSONGetType(weight) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        {
            float bucket;

            const struct LDJSON *bucketBy =
                LDObjectLookup(segmentRule, "bucketBy");

            const char *const attribute = (bucketBy == NULL)
                ? "key" : LDGetText(bucketBy);

            if (!LDi_bucketUser(user, segmentKey, attribute, salt, &bucket)) {
                LD_LOG(LD_LOG_ERROR, "LDi_bucketUser error");

                return EVAL_MEM;
            }

            if (bucket < LDGetNumber(weight) / 100000) {
                return EVAL_MATCH;
            } else {
                return EVAL_MISS;
            }
        }
    }
}

EvalStatus
matchAny(OpFn f, const struct LDJSON *const value,
    const struct LDJSON *const values)
{
    const struct LDJSON *iter;

    LD_ASSERT(f);
    LD_ASSERT(value);
    LD_ASSERT(values);

    for (iter = LDGetIter(values); iter; iter = LDIterNext(iter)) {
        if (f(value, iter)) {
            return EVAL_MATCH;
        }
    }

    return EVAL_MISS;
}

EvalStatus
LDi_clauseMatchesUserNoSegments(const struct LDJSON *const clause,
    const struct LDUser *const user)
{
    OpFn fn;
    const char *operatorText, *attributeText;
    struct LDJSON *operator, *attributeValue, *attribute;
    const struct LDJSON *values;
    LDJSONType type;

    LD_ASSERT(clause);
    LD_ASSERT(user);

    fn             = NULL;
    operatorText   = NULL;
    operator       = NULL;
    attributeText  = NULL;
    attributeValue = NULL;
    attribute      = NULL;
    values         = NULL;

    if (!(attribute = LDObjectLookup(clause, "attribute"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(attribute) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(attributeText = LDGetText(attribute))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return EVAL_SCHEMA;
    }

    if (!(operator = LDObjectLookup(clause, "op"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(operator) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(operatorText = LDGetText(operator))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return EVAL_SCHEMA;
    }

    if (!(values = LDObjectLookup(clause, "values"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(fn = LDi_lookupOperation(operatorText))) {
        LD_LOG(LD_LOG_WARNING, "unknown operator");

        return EVAL_MISS;
    }

    if (!(attributeValue = LDi_valueOfAttribute(user, attributeText))) {
        LD_LOG(LD_LOG_TRACE, "attribute does not exist");

        return EVAL_MISS;
    }

    type = LDJSONGetType(attributeValue);

    if (type == LDArray || type == LDObject) {
        struct LDJSON *iter;

        for (iter = LDGetIter(attributeValue); iter; iter = LDIterNext(iter)) {
            EvalStatus substatus;

            type = LDJSONGetType(iter);

            if (type == LDObject || type == LDArray) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                LDJSONFree(attributeValue);

                return EVAL_SCHEMA;
            }

            if (LDi_isEvalError(substatus = matchAny(fn, iter, values))) {
                LD_LOG(LD_LOG_ERROR, "sub error");

                LDJSONFree(attributeValue);

                return substatus;
            }

            if (substatus == EVAL_MATCH) {
                LDJSONFree(attributeValue);

                return maybeNegate(clause, EVAL_MATCH);
            }
        }

        LDJSONFree(attributeValue);

        return maybeNegate(clause, EVAL_MISS);
    } else {
        EvalStatus substatus;

        if (LDi_isEvalError(substatus = matchAny(fn, attributeValue, values))) {
            LD_LOG(LD_LOG_ERROR, "sub error");

            LDJSONFree(attributeValue);

            return substatus;
        }

        LDJSONFree(attributeValue);

        return maybeNegate(clause, substatus);
    }
}

bool
LDi_bucketUser(const struct LDUser *const user, const char *const segmentKey,
    const char *const attribute, const char *const salt, float *const bucket)
{
    struct LDJSON *attributeValue;

    LD_ASSERT(user);
    LD_ASSERT(segmentKey);
    LD_ASSERT(attribute);
    LD_ASSERT(salt);
    LD_ASSERT(bucket);

    attributeValue = NULL;

    if ((attributeValue = LDi_valueOfAttribute(user, attribute))) {
        char raw[256];

        char *const bucketable = LDi_bucketableStringValue(attributeValue);

        LDJSONFree(attributeValue);

        if (!bucketable) {
            return false;
        }

        if (snprintf(raw, sizeof(raw), "%s.%s.%s", segmentKey,
            salt, bucketable) >= 0)
        {
            char digest[20], encoded[17];
            const float longScale = 0xFFFFFFFFFFFFFFF;

            SHA1(digest, raw, strlen(raw));

            /* encodes to hex, and shortens, 16 characters in hex 8 bytes */
            LD_ASSERT(hexify((unsigned char *)digest,
                sizeof(digest), encoded, sizeof(encoded)) == 16);

            encoded[15] = 0;

            *bucket = (float)strtoll(encoded, NULL, 16) / longScale;

            LDFree(bucketable);

            return true;
        }
    }

    return false;
}

char *
LDi_bucketableStringValue(const struct LDJSON *const node)
{
    LD_ASSERT(node);

    if (LDJSONGetType(node) == LDText) {
        return LDStrDup(LDGetText(node));
    } else if (LDJSONGetType(node) == LDNumber) {
        char buffer[256];

        if (snprintf(buffer, sizeof(buffer), "%f", LDGetNumber(node)) < 0) {
            return NULL;
        } else {
            return LDStrDup(buffer);
        }
    } else {
        return NULL;
    }
}

bool
LDi_variationIndexForUser(const struct LDJSON *const varOrRoll,
    const struct LDUser *const user, const char *const key,
    const char *const salt, unsigned int *const index)
{
    struct LDJSON *variation, *rollout, *variations;
    float userBucket, sum;

    LD_ASSERT(varOrRoll);
    LD_ASSERT(index);

    variation  = NULL;
    rollout    = NULL;
    variations = NULL;
    userBucket = 0;
    sum        = 0;

    variation = LDObjectLookup(varOrRoll, "variation");

    if (LDi_notNull(variation)) {
        if (LDJSONGetType(variation) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        *index = LDGetNumber(variation);

        return true;
    }

    LD_ASSERT(user);
    LD_ASSERT(salt);

    rollout = LDObjectLookup(varOrRoll, "rollout");

    if (!LDi_notNull(rollout)) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDJSONGetType(rollout) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    variations = LDObjectLookup(rollout, "variations");

    if (!LDi_notNull(variations)) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDJSONGetType(variations) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDCollectionGetSize(variations) == 0) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return false;
    }

    LD_ASSERT(variation = LDGetIter(variations));

    if (!LDi_bucketUser(user, key, "key", salt, &userBucket)) {
        LD_LOG(LD_LOG_ERROR, "failed to bucket user");

        return false;
    }

    for (; variation; variation = LDIterNext(variation)) {
        struct LDJSON *weight;

        weight = LDObjectLookup(variation, "weight");

        if (!LDi_notNull(weight)) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(weight) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        sum += LDGetNumber(weight) / 100000.0;

        if (userBucket < sum) {
            struct LDJSON *subvariation;

            subvariation = LDObjectLookup(variation, "variation");

            if (!LDi_notNull(subvariation)) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return false;
            }

            if (LDJSONGetType(subvariation) != LDNumber) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return false;
            }

            *index = LDGetNumber(subvariation);

            return true;
        }
    }

    return false;
}

struct LDJSON *
LDi_getIndexForVariationOrRollout(const struct LDJSON *const flag,
    const struct LDJSON *const varOrRoll,
    const struct LDUser *const user)
{
    unsigned int cindex;
    const struct LDJSON *jkey, *jsalt;
    const char *key, *salt;

    LD_ASSERT(flag);
    LD_ASSERT(varOrRoll);

    cindex = 0;
    jkey   = NULL;
    jsalt  = NULL;
    key    = NULL;
    salt   = NULL;

    if (LDi_notNull(jkey = LDObjectLookup(flag, "key"))) {
        if (LDJSONGetType(jkey) != LDText) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return NULL;
        }

        key = LDGetText(jkey);
    }

    if (LDi_notNull(jsalt = LDObjectLookup(flag, "salt"))) {
        if (LDJSONGetType(jsalt) != LDText) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return NULL;
        }

        salt = LDGetText(jsalt);
    }

    if (!LDi_variationIndexForUser(varOrRoll, user, key, salt, &cindex)) {
        LD_LOG(LD_LOG_ERROR, "failed to get variation index");

        return NULL;
    }

    return LDNewNumber(cindex);
}
