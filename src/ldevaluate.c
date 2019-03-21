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
LDi_addReason(struct LDJSON **const result, const char *const reason)
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

        LDJSONFree(tmp);
        LDJSONFree(tmpcollection);

        return NULL;
    }

    if (!LDObjectSetKey(*result, "reason", tmpcollection)) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        LDJSONFree(tmpcollection);

        return NULL;
    }

    return tmpcollection;
}

static bool
addValue(const struct LDJSON *const flag, struct LDJSON **result,
    struct LDDetails *const details, const struct LDJSON *const index)
{
    struct LDJSON *tmp, *variations, *variation;

    LD_ASSERT(flag);
    LD_ASSERT(result);
    LD_ASSERT(details);

    tmp        = NULL;
    variations = NULL;
    variation  = NULL;

    if (LDi_notNull(index)) {
        details->hasVariation = true;

        if (LDJSONGetType(index) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        details->variationIndex = LDGetNumber(index);

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

        *result = tmp;
    } else {
        *result = NULL;
        details->hasVariation = false;
    }

    return true;
}

EvalStatus
LDi_evaluate(struct LDClient *const client, const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    struct LDDetails *const details, struct LDJSON **const o_events,
    struct LDJSON **const o_value)
{
    EvalStatus substatus;
    const struct LDJSON *iter, *rules, *targets, *on;
    struct LDJSON *index;
    const char *failedKey;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(details);
    LD_ASSERT(o_events);
    LD_ASSERT(o_value);

    iter        = NULL;
    rules       = NULL;
    targets     = NULL;
    on          = NULL;
    failedKey   = NULL;
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

        details->kind = LD_OFF;

        if (!(addValue(flag, o_value, details, offVariation))) {
            LD_LOG(LD_LOG_ERROR, "failed to add value");

            return EVAL_MEM;
        }

        return EVAL_MISS;
    }

    /* prerequisites */
    if (LDi_isEvalError(substatus =
        LDi_checkPrerequisites(client, flag, user, store, &failedKey,
            o_events)))
    {
        LD_LOG(LD_LOG_ERROR, "checkPrequisites failed");

        return substatus;
    }

    if (substatus == EVAL_MISS) {
        char *key;

        if (!(key = LDStrDup(failedKey))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            return EVAL_MEM;
        }

        details->kind = LD_PREREQUISITE_FAILED;
        details->extra.prerequisiteKey = key;

        if (!(addValue(flag, o_value, details,
            LDObjectLookup(flag, "offVariation"))))
        {
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

            if (LDi_textInArray(values, user->key)) {
                const struct LDJSON *variation = NULL;

                variation = LDObjectLookup(iter, "variation");

                details->kind = LD_TARGET_MATCH;

                if (!(addValue(flag, o_value, details, variation))) {
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

    if (rules && LDGetIter(rules)) {
        unsigned int index;

        index = 0;

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
                struct LDJSON *variation, *ruleid;

                variation = NULL;
                ruleid    = NULL;

                details->kind = LD_RULE_MATCH;
                details->extra.rule.ruleIndex = index;
                details->extra.rule.hasId = false;

                variation = LDi_getIndexForVariationOrRollout(flag, iter, user);

                if (!(addValue(flag, o_value, details, variation))) {
                    LD_LOG(LD_LOG_ERROR, "failed to add value");

                    LDJSONFree(variation);

                    return EVAL_MEM;
                }

                LDJSONFree(variation);

                if (LDi_notNull(ruleid = LDObjectLookup(iter, "id"))) {
                    char *text;

                    if (LDJSONGetType(ruleid) != LDText) {
                        LD_LOG(LD_LOG_ERROR, "schema error");

                        return EVAL_SCHEMA;
                    }

                    if (!(text = LDStrDup(LDGetText(ruleid)))) {
                        LD_LOG(LD_LOG_ERROR, "memory error");

                        return EVAL_MEM;
                    }

                    details->extra.rule.id = text;
                    details->extra.rule.hasId = true;
                }

                return EVAL_MATCH;
            }

            index++;
        }
    }

    /* fallthrough */
    details->kind = LD_FALLTHROUGH;

    index = LDi_getIndexForVariationOrRollout(flag,
        LDObjectLookup(flag, "fallthrough"), user);

    if (!(addValue(flag, o_value, details, index))) {
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
    LD_ASSERT(LDJSONGetType(flag) == LDObject);

    prerequisites = NULL;
    iter          = NULL;

    prerequisites = LDObjectLookup(flag, "prerequisites");

    if (!prerequisites) {
        return EVAL_MATCH;
    }

    if (LDJSONGetType(prerequisites) != LDArray) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    for (iter = LDGetIter(prerequisites); iter; iter = LDIterNext(iter)) {
        struct LDJSON *value, *preflag, *event, *subevents;
        const struct LDJSON *key, *variation;
        unsigned int *variationNumRef;
        EvalStatus status;
        const char *keyText;
        struct LDDetails details;

        value            = NULL;
        preflag          = NULL;
        key              = NULL;
        variation        = NULL;
        variationNumRef  = NULL;
        event            = NULL;
        subevents        = NULL;
        keyText          = NULL;

        LDDetailsInit(&details);

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

        keyText = LDGetText(key);

        *failedKey = keyText;

        if (!(variation = LDObjectLookup(iter, "variation"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (LDJSONGetType(variation) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        if (!LDStoreGet(store, "flags", keyText, &preflag)) {
            LD_LOG(LD_LOG_ERROR, "store lookup error");

            return EVAL_STORE;
        }

        if (!preflag) {
            LD_LOG(LD_LOG_ERROR, "cannot find flag in store");

            return EVAL_MISS;
        }

        if (LDi_isEvalError(status = LDi_evaluate(
            client, preflag, user, store, &details, &subevents, &value)))
        {
            LDJSONFree(preflag);

            return status;
        }

        if (!value) {
            LDJSONFree(preflag);

            LD_LOG(LD_LOG_ERROR, "sub error with result");
        }

        if (details.hasVariation) {
            variationNumRef = &details.variationIndex;
        }

        event = LDi_newFeatureRequestEvent(client,
            keyText, user, variationNumRef, value, NULL,
            LDGetText(LDObjectLookup(flag, "key")), preflag, &details);

        if (!event) {
            LDJSONFree(preflag);
            LDJSONFree(value);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        if (!(*events)) {
            if (!(*events = LDNewArray())) {
                LDJSONFree(preflag);
                LDJSONFree(value);

                LD_LOG(LD_LOG_ERROR, "alloc error");

                return EVAL_MEM;
            }
        }

        if (subevents) {
            if (!LDArrayAppend(*events, subevents)) {
                LDJSONFree(preflag);
                LDJSONFree(value);

                LD_LOG(LD_LOG_ERROR, "alloc error");

                return EVAL_MEM;
            }
        }

        if (!LDArrayPush(*events, event)) {
            LDJSONFree(preflag);
            LDJSONFree(value);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        if (status == EVAL_MISS) {
            LDJSONFree(preflag);
            LDJSONFree(value);

            return EVAL_MISS;
        }

        {
            struct LDJSON *on;
            bool variationMatch = false;

            if (!(on = LDObjectLookup(preflag, "on"))) {
                LDJSONFree(preflag);
                LDJSONFree(value);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (LDJSONGetType(on) != LDBool) {
                LDJSONFree(preflag);
                LDJSONFree(value);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (details.hasVariation) {
                variationMatch = details.variationIndex ==
                    LDGetNumber(variation);
            }

            if (!LDGetBool(on) || !variationMatch) {
                LDJSONFree(preflag);
                LDJSONFree(value);

                return EVAL_MISS;
            }
        }

        LDJSONFree(preflag);
        LDJSONFree(value);
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
        const struct LDJSON *values, *iter;

        values = NULL;
        iter   = NULL;

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

                if (!segment) {
                    LD_LOG(LD_LOG_WARNING, "segment not found in store");

                    continue;
                }

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

            const struct LDJSON *const bucketBy =
                LDObjectLookup(segmentRule, "bucketBy");

            const char *const attribute = LDi_notNull(bucketBy)
                ? LDGetText(bucketBy) : "key";

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

    if (type == LDArray) {
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
        } else {
            LDFree(bucketable);
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
