#include "sha1.h"
#include "hexify.h"

#include "ldinternal.h"
#include "ldevaluate.h"
#include "ldoperators.h"
#include "ldevents.h"

bool
isEvalError(const EvalStatus status)
{
    return status == EVAL_MEM || status == EVAL_SCHEMA || status == EVAL_STORE;
}

struct LDJSON *
addReason(struct LDJSON **const result, const char *const reason,
    struct LDJSON *const events)
{
    struct LDJSON *tmpcollection;
    struct LDJSON *tmp;

    LD_ASSERT(reason);

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

        return NULL;
    }

    if (!LDObjectSetKey(tmpcollection, "kind", tmp)) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return NULL;
    }

    if (!LDObjectSetKey(*result, "reason", tmpcollection)) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

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
addErrorReason(struct LDJSON **const result, const char *const kind)
{
    struct LDJSON *tmpcollection;
    struct LDJSON *tmp;

    LD_ASSERT(result);
    LD_ASSERT(kind);

    if (!(tmpcollection = addReason(result, "ERROR", NULL))) {
        LD_LOG(LD_LOG_ERROR, "addReason failed");

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
    struct LDJSON *tmp;
    struct LDJSON *variations;
    struct LDJSON *variation;

    LD_ASSERT(flag);
    LD_ASSERT(result);

    if (index) {
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

        return false;
    }

    if (index) {
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

        return false;
    }

    return true;
}

EvalStatus
evaluate(const struct LDJSON *const flag, const struct LDUser *const user,
    struct LDStore *const store, struct LDJSON **const result)
{
    EvalStatus substatus;
    const struct LDJSON *iter;
    const struct LDJSON *rules;
    const struct LDJSON *targets;
    const struct LDJSON *on;
    const struct LDJSON *fallthrough;
    const char *failedKey;
    struct LDJSON *events = NULL;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(result);

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
        const struct LDJSON *offVariation =
            LDObjectLookup(flag, "offVariation");

        if (!addReason(result, "OFF", NULL)) {
            LD_LOG(LD_LOG_ERROR, "failed to add reason");

            return EVAL_MEM;
        }

        if (!(addValue(flag, *result, offVariation))) {
            LD_LOG(LD_LOG_ERROR, "failed to add value");

            return EVAL_MEM;
        }

        return EVAL_MATCH;
    }

    /* prerequisites */
    if (isEvalError(substatus =
        checkPrerequisites(flag, user, store, &failedKey, &events)))
    {
        LD_LOG(LD_LOG_ERROR, "sub error error");

        return substatus;
    }

    if (substatus == EVAL_MISS) {
        struct LDJSON *key;
        struct LDJSON *reason;

        if (!(reason = addReason(result, "PREREQUISITE_FAILED", events))) {
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

            LD_ASSERT(LDJSONGetType(iter) == LDObject);

            values = LDObjectLookup(iter, "values");

            LD_ASSERT(values); LD_ASSERT(LDJSONGetType(values) == LDArray);

            if (textInArray(values, user->key)) {
                const struct LDJSON *variation = NULL;

                variation = LDObjectLookup(iter, "variation");

                if (!addReason(result, "TARGET_MATCH", events)) {
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

    if (rules && LDArrayGetSize(rules) != 0) {
        unsigned int index = 0;
        for (iter = LDGetIter(rules); iter; iter = LDIterNext(iter)) {
            EvalStatus substatus;

            if (LDJSONGetType(iter) != LDObject) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (isEvalError(substatus = ruleMatchesUser(iter, user, store))) {
                LD_LOG(LD_LOG_ERROR, "sub error");

                return substatus;
            }

            if (substatus == EVAL_MATCH) {
                const struct LDJSON *variation = NULL;
                struct LDJSON *reason;
                struct LDJSON *tmp;
                struct LDJSON *ruleid;

                variation = LDObjectLookup(iter, "variation");

                if (!(reason = addReason(result, "RULE_MATCH", events))) {
                    LD_LOG(LD_LOG_ERROR, "failed to add reason");

                    return EVAL_MEM;
                }

                if (!(addValue(flag, *result, variation))) {
                    LD_LOG(LD_LOG_ERROR, "failed to add value");

                    return EVAL_MEM;
                }

                if (!(tmp = LDNewNumber(index))) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

                    return EVAL_MEM;
                }

                if (!LDObjectSetKey(reason, "ruleIndex", tmp)) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

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

                    return EVAL_MEM;
                }


                return EVAL_MATCH;
            }

            index++;
        }
    }

    /* fallthrough */
    if (!(fallthrough = LDObjectLookup(flag, "fallthrough"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    fallthrough = LDObjectLookup(fallthrough, "variation");

    if (!addReason(result, "FALLTHROUGH", events)) {
        LD_LOG(LD_LOG_ERROR, "failed to add reason");

        return EVAL_MEM;
    }

    if (!(addValue(flag, *result, fallthrough))) {
        LD_LOG(LD_LOG_ERROR, "failed to add value");

        return EVAL_MEM;
    }

    return EVAL_MATCH;
}

EvalStatus
checkPrerequisites(const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    const char **const failedKey, struct LDJSON **const events)
{
    struct LDJSON *prerequisites = NULL;
    struct LDJSON *iter = NULL;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(failedKey);
    LD_ASSERT(events);

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
        struct LDJSON *result = NULL;
        struct LDJSON *preflag = NULL;
        const struct LDJSON *key = NULL;
        const struct LDJSON *variation = NULL;
        unsigned int variationNum;
        struct LDJSON *event = NULL;

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

        if (!(preflag = LDStoreGet(store, "flags", LDGetText(key)))) {
            LD_LOG(LD_LOG_ERROR, "store lookup error");

            return EVAL_STORE;
        }

        if (evaluate(preflag, user, store, &result) != EVAL_MATCH) {
            LDJSONFree(preflag);

            return false;
        }

        if (!result) {
            LDJSONFree(preflag);

            LD_LOG(LD_LOG_ERROR, "sub error with result");
        }

        if (!(*events)) {
            if (!(*events = LDNewArray())) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LD_LOG(LD_LOG_ERROR, "alloc error");

                return EVAL_MEM;
            }
        }

        variationNum = LDGetNumber(LDObjectLookup(result, "variationIndex"));

        event = newFeatureRequestEvent(LDGetText(key), user, &variationNum,
            LDObjectLookup(result, "value"), NULL,
            LDGetText(LDObjectLookup(flag, "key")), preflag,
            LDObjectLookup(result, "reason"));

        if (!event) {
            LDJSONFree(preflag);
            LDJSONFree(result);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        if (!LDArrayAppend(*events, event)) {
            LDJSONFree(preflag);
            LDJSONFree(result);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        {
            struct LDJSON *on;
            struct LDJSON *variationIndex;

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

            if (!(variationIndex = LDObjectLookup(result, "variationIndex"))) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (LDJSONGetType(variationIndex) != LDNumber) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (!LDGetBool(on) ||
                LDGetNumber(variationIndex) != LDGetNumber(variation))
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
ruleMatchesUser(const struct LDJSON *const rule,
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

        if (isEvalError(substatus = clauseMatchesUser(iter, user, store))) {
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
clauseMatchesUser(const struct LDJSON *const clause,
    const struct LDUser *const user, struct LDStore *const store)
{
    const struct LDJSON *op = NULL;

    LD_ASSERT(clause);
    LD_ASSERT(user);

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
        bool negate = false;
        const struct LDJSON *values = NULL;
        const struct LDJSON *iter = NULL;
        const struct LDJSON *negateJSON = NULL;

        if (!(values = LDObjectLookup(clause, "values"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDJSONGetType(values) != LDArray) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if ((negateJSON = LDObjectLookup(clause, "negate"))) {
            if (LDJSONGetType(negateJSON) != LDBool) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            negate = LDGetBool(negateJSON);
        }

        bool anysuccess = false;

        for (iter = LDGetIter(values); iter; iter = LDIterNext(iter)) {
            if (LDJSONGetType(iter) == LDText) {
                EvalStatus evalstatus;
                struct LDJSON *segment;

                segment = LDStoreGet(store, "segments", LDGetText(iter));

                if (!segment) {
                    LD_LOG(LD_LOG_WARNING, "store lookup error");

                    continue;
                }

                anysuccess = true;

                if (isEvalError(
                    evalstatus = segmentMatchesUser(segment, user)))
                {
                    LD_LOG(LD_LOG_ERROR, "sub error");

                    LDJSONFree(segment);

                    return evalstatus;
                }

                LDJSONFree(segment);

                if (evalstatus == EVAL_MATCH) {
                    if (!negate) {
                        return EVAL_MATCH;
                    } else {
                        return EVAL_MISS;
                    }
                }
            }
        }

        if (!anysuccess && LDGetIter(values)) {
            return EVAL_MISS;
        }

        if (!negate) {
            return EVAL_MATCH;
        } else {
            return EVAL_MISS;
        }
    }

    return clauseMatchesUserNoSegments(clause, user);
}

EvalStatus
segmentMatchesUser(const struct LDJSON *const segment,
    const struct LDUser *const user)
{
    const struct LDJSON *included = NULL;
    const struct LDJSON *excluded = NULL;

    const struct LDJSON *key = NULL;
    const struct LDJSON *salt = NULL;

    const struct LDJSON *segmentRules = NULL;
    const struct LDJSON *iter = NULL;

    LD_ASSERT(segment);
    LD_ASSERT(user);

    if ((included = LDObjectLookup(segment, "included"))) {
        if (LDJSONGetType(included) != LDArray) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (textInArray(included, user->key)) {
            return EVAL_MATCH;
        }
    }

    if ((excluded = LDObjectLookup(segment, "excluded"))) {
        if (LDJSONGetType(excluded) != LDArray) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (textInArray(excluded, user->key)) {
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

        if (isEvalError(substatus = segmentRuleMatchUser(iter,
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
segmentRuleMatchUser(const struct LDJSON *const segmentRule,
    const char *const segmentKey, const struct LDUser *const user,
    const char *const salt)
{
    const struct LDJSON *clauses = NULL;
    const struct LDJSON *clause = NULL;

    LD_ASSERT(segmentRule);
    LD_ASSERT(segmentKey);
    LD_ASSERT(user);
    LD_ASSERT(salt);

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

        if (isEvalError(substatus =
            clauseMatchesUserNoSegments(clause, user)))
        {
            return substatus;
        }

        if (substatus == EVAL_MISS) {
            return substatus = EVAL_MISS;
        }
    }

    {
        const struct LDJSON *weight = LDObjectLookup(segmentRule, "weight");

        if (!weight) {
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

            if (!bucketUser(user, segmentKey, attribute, salt, &bucket)) {
                LD_LOG(LD_LOG_ERROR, "bucketUser error");

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
clauseMatchesUserNoSegments(const struct LDJSON *const clause,
    const struct LDUser *const user)
{
    OpFn fn;
    const char *operatorText;
    struct LDJSON *operator;
    const char *attributeText;
    struct LDJSON *attributeValue;
    struct LDJSON *attribute;
    const struct LDJSON *values;
    bool negate;
    struct LDJSON *negateJSON;
    LDJSONType type;

    LD_ASSERT(clause);
    LD_ASSERT(user);

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

        return EVAL_MEM;
    }

    if (!(values = LDObjectLookup(clause, "values"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_MEM;
    }

    if ((negateJSON = LDObjectLookup(clause, "negate"))) {
        if (LDJSONGetType(negateJSON) != LDBool) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        negate = LDGetBool(negateJSON);
    } else {
        negate = false;
    }

    if (!(fn = lookupOperation(operatorText))) {
        LD_LOG(LD_LOG_WARNING, "unknown operator");

        return EVAL_MISS;
    }

    if (!(attributeValue = valueOfAttribute(user, attributeText))) {
        return EVAL_MISS;
    }

    type = LDJSONGetType(attributeValue);

    if (type == LDArray || type == LDObject) {
        struct LDJSON *iter = LDGetIter(attributeValue);

        for (; iter; iter = LDIterNext(iter)) {
            EvalStatus substatus;

            type = LDJSONGetType(iter);

            if (type == LDObject || type == LDArray) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                LDJSONFree(attributeValue);

                return EVAL_SCHEMA;
            }

            if (isEvalError(substatus = matchAny(fn, iter, values))) {
                LD_LOG(LD_LOG_ERROR, "sub error");

                LDJSONFree(attributeValue);

                return substatus;
            }

            if (substatus == EVAL_MATCH) {
                LDJSONFree(attributeValue);

                if (!negate) {
                    return EVAL_MATCH;
                } else {
                    return EVAL_MISS;
                }
            }
        }

        LDJSONFree(attributeValue);

        if (!negate) {
            return EVAL_MATCH;
        } else {
            return EVAL_MISS;
        }
    } else {
        EvalStatus substatus;

        if (isEvalError(substatus = matchAny(fn, attributeValue, values))) {
            LD_LOG(LD_LOG_ERROR, "sub error");

            LDJSONFree(attributeValue);

            return substatus;
        }

        LDJSONFree(attributeValue);

        if (negate ? !(substatus == EVAL_MATCH) : (substatus == EVAL_MATCH)) {
            return EVAL_MATCH;
        } else {
            return EVAL_MISS;
        }
    }
}

bool
bucketUser(const struct LDUser *const user, const char *const segmentKey,
    const char *const attribute, const char *const salt, float *const bucket)
{
    struct LDJSON *attributeValue = NULL;

    LD_ASSERT(user);
    LD_ASSERT(segmentKey);
    LD_ASSERT(attribute);
    LD_ASSERT(salt);
    LD_ASSERT(bucket);

    if ((attributeValue = valueOfAttribute(user, attribute))) {
        char raw[256];

        char *const bucketable = bucketableStringValue(attributeValue);

        LDJSONFree(attributeValue);

        if (!bucketable) {
            return false;
        }

        if (snprintf(raw, sizeof(raw), "%s.%s.%s", segmentKey,
            salt, bucketable) >= 0)
        {
            char digest[20];
            char encoded[17];
            float longScale = 0xFFFFFFFFFFFFFFF;

            SHA1(digest, raw, strlen(raw));

            /* encodes to hex, and shortens, 16 characters in hex 8 bytes */
            LD_ASSERT(hexify((unsigned char *)digest,
                sizeof(digest), encoded, sizeof(encoded)) == 16);

            encoded[15] = 0;

            *bucket = (float)strtoll(encoded, NULL, 16) / longScale;

            free(bucketable);

            return true;
        }
    }

    return false;
}

char *
bucketableStringValue(const struct LDJSON *const node)
{
    LD_ASSERT(node);

    if (LDJSONGetType(node) == LDText) {
        return strdup(LDGetText(node));
    } else if (LDJSONGetType(node) == LDNumber) {
        char buffer[256];

        if (snprintf(buffer, sizeof(buffer), "%f", LDGetNumber(node)) < 0) {
            return NULL;
        } else {
            return strdup(buffer);
        }
    } else {
        return NULL;
    }
}
