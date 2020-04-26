#include "sha1.h"
#include "hexify.h"

#include <launchdarkly/api.h>

#include "assertion.h"
#include "network.h"
#include "operators.h"
#include "evaluate.h"
#include "user.h"
#include "client.h"
#include "utility.h"
#include "store.h"
#include "event_processor.h"

bool
LDi_isEvalError(const EvalStatus status)
{
    return status == EVAL_MEM || status == EVAL_SCHEMA || status == EVAL_STORE;
}

static EvalStatus
maybeNegate(const struct LDJSON *const clause, const EvalStatus status)
{
    const struct LDJSON *negate;

    LD_ASSERT(clause);

    negate = NULL;

    if (LDi_isEvalError(status)) {
        return status;
    }

    if (LDi_notNull(negate = LDObjectLookup(clause, "negate"))) {
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
    struct LDJSON **const o_value, const bool recordReason)
{
    EvalStatus substatus;
    const struct LDJSON *iter, *rules, *targets, *on;
    const struct LDJSON *index;
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

        details->reason = LD_OFF;

        if (!(addValue(flag, o_value, details, offVariation))) {
            LD_LOG(LD_LOG_ERROR, "failed to add value");

            return EVAL_MEM;
        }

        return EVAL_MISS;
    }

    /* prerequisites */
    if (LDi_isEvalError(substatus =
        LDi_checkPrerequisites(client, flag, user, store, &failedKey,
            o_events, recordReason)))
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

        details->reason = LD_PREREQUISITE_FAILED;
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

                details->reason = LD_TARGET_MATCH;

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

    if (rules) {
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
                struct LDJSON *ruleid;
                const struct LDJSON *variation;

                variation = NULL;
                ruleid    = NULL;

                details->reason = LD_RULE_MATCH;
                details->extra.rule.ruleIndex = index;
                details->extra.rule.id = NULL;

                if (!LDi_getIndexForVariationOrRollout(flag, iter, user,
                    &variation))
                {
                    LD_LOG(LD_LOG_ERROR, "schema error");

                    return EVAL_SCHEMA;
                }

                if (!(addValue(flag, o_value, details, variation))) {
                    LD_LOG(LD_LOG_ERROR, "failed to add value");

                    return EVAL_MEM;
                }

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
                }

                return EVAL_MATCH;
            }

            index++;
        }
    }

    /* fallthrough */
    details->reason = LD_FALLTHROUGH;

    if (!LDi_getIndexForVariationOrRollout(flag,
        LDObjectLookup(flag, "fallthrough"), user, &index))
    {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(addValue(flag, o_value, details, index))) {
        LD_LOG(LD_LOG_ERROR, "failed to add value");

        return EVAL_MEM;
    }

    return EVAL_MATCH;
}

EvalStatus
LDi_checkPrerequisites(struct LDClient *const client,
    const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    const char **const failedKey, struct LDJSON **const events,
    const bool recordReason)
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
        struct LDDetails details, *detailsRef;
        struct LDJSONRC *preflagrc;
        unsigned long now;

        value            = NULL;
        preflag          = NULL;
        key              = NULL;
        variation        = NULL;
        variationNumRef  = NULL;
        event            = NULL;
        subevents        = NULL;
        keyText          = NULL;
        detailsRef       = NULL;
        preflagrc        = NULL;

        LDDetailsInit(&details);
        LDi_getUnixMilliseconds(&now);

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

        if (!LDStoreGet(store, LD_FLAG, keyText, &preflagrc)) {
            LD_LOG(LD_LOG_ERROR, "store lookup error");

            return EVAL_STORE;
        }

        if (preflagrc) {
            preflag = LDJSONRCGet(preflagrc);
        }

        if (!preflag) {
            LD_LOG(LD_LOG_ERROR, "cannot find flag in store");

            return EVAL_MISS;
        }

        if (LDi_isEvalError(status = LDi_evaluate(client, preflag, user, store,
            &details, &subevents, &value, recordReason)))
        {
            LDJSONRCDecrement(preflagrc);
            LDJSONFree(value);
            LDDetailsClear(&details);
            LDJSONFree(subevents);

            return status;
        }

        if (!value) {
            LD_LOG(LD_LOG_ERROR, "sub error with result");
        }

        if (details.hasVariation) {
            variationNumRef = &details.variationIndex;
        }

        if (recordReason) {
            detailsRef = &details;
        }

        event = LDi_newFeatureRequestEvent(client->eventProcessor,
            keyText, user, variationNumRef, value, NULL,
            LDGetText(LDObjectLookup(flag, "key")), preflag, detailsRef, now);

        if (!event) {
            LDJSONRCDecrement(preflagrc);
            LDJSONFree(value);
            LDDetailsClear(&details);
            LDJSONFree(subevents);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        if (!(*events)) {
            if (!(*events = LDNewArray())) {
                LDJSONRCDecrement(preflagrc);
                LDJSONFree(value);
                LDDetailsClear(&details);
                LDJSONFree(subevents);

                LD_LOG(LD_LOG_ERROR, "alloc error");

                return EVAL_MEM;
            }
        }

        if (subevents) {
            if (!LDArrayAppend(*events, subevents)) {
                LDJSONRCDecrement(preflagrc);
                LDJSONFree(value);
                LDDetailsClear(&details);
                LDJSONFree(subevents);

                LD_LOG(LD_LOG_ERROR, "alloc error");

                return EVAL_MEM;
            }

            LDJSONFree(subevents);
        }

        if (!LDArrayPush(*events, event)) {
            LDJSONRCDecrement(preflagrc);
            LDJSONFree(value);
            LDDetailsClear(&details);

            LD_LOG(LD_LOG_ERROR, "alloc error");

            return EVAL_MEM;
        }

        if (status == EVAL_MISS) {
            LDJSONRCDecrement(preflagrc);
            LDJSONFree(value);
            LDDetailsClear(&details);

            return EVAL_MISS;
        }

        {
            struct LDJSON *on;
            bool variationMatch = false;

            if (!(on = LDObjectLookup(preflag, "on"))) {
                LDJSONRCDecrement(preflagrc);
                LDJSONFree(value);
                LDDetailsClear(&details);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (LDJSONGetType(on) != LDBool) {
                LDJSONRCDecrement(preflagrc);
                LDJSONFree(value);
                LDDetailsClear(&details);

                LD_LOG(LD_LOG_ERROR, "schema error");

                return EVAL_SCHEMA;
            }

            if (details.hasVariation) {
                variationMatch = details.variationIndex ==
                    LDGetNumber(variation);
            }

            if (!LDGetBool(on) || !variationMatch) {
                LDJSONRCDecrement(preflagrc);
                LDJSONFree(value);
                LDDetailsClear(&details);

                return EVAL_MISS;
            }
        }

        LDJSONRCDecrement(preflagrc);
        LDJSONFree(value);
        LDDetailsClear(&details);
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
                struct LDJSONRC *segmentrc;

                segmentrc = NULL;
                segment   = NULL;

                if (!LDStoreGet(store, LD_SEGMENT, LDGetText(iter),
                    &segmentrc))
                {
                    LD_LOG(LD_LOG_ERROR, "store lookup error");

                    return EVAL_STORE;
                }

                if (segmentrc) {
                    segment = LDJSONRCGet(segmentrc);
                }

                if (!segment) {
                    LD_LOG(LD_LOG_WARNING, "segment not found in store");

                    continue;
                }

                if (LDi_isEvalError(
                    evalstatus = LDi_segmentMatchesUser(segment, user)))
                {
                    LD_LOG(LD_LOG_ERROR, "sub error");

                    LDJSONRCDecrement(segmentrc);

                    return evalstatus;
                }

                LDJSONRCDecrement(segmentrc);

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

static EvalStatus
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
    struct LDJSON *operatorJSON, *attributeValue, *attribute;
    const struct LDJSON *values;
    LDJSONType type;

    LD_ASSERT(clause);
    LD_ASSERT(user);

    fn             = NULL;
    operatorText   = NULL;
    operatorJSON   = NULL;
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

    if (!(operatorJSON = LDObjectLookup(clause, "op"))) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (LDJSONGetType(operatorJSON) != LDText) {
        LD_LOG(LD_LOG_ERROR, "schema error");

        return EVAL_SCHEMA;
    }

    if (!(operatorText = LDGetText(operatorJSON))) {
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
        char raw[256], bucketableBuffer[256];
        const char *bucketable;

        bucketable = NULL;

        if (LDJSONGetType(attributeValue) == LDText) {
            bucketable = LDGetText(attributeValue);
        } else if (LDJSONGetType(attributeValue) == LDNumber) {
            if (snprintf(bucketableBuffer, sizeof(bucketableBuffer), "%f",
                LDGetNumber(attributeValue)) >= 0)
            {
                bucketable = bucketableBuffer;
            }
        }

        if (!bucketable) {
            LDJSONFree(attributeValue);

            return false;
        }

        if (snprintf(raw, sizeof(raw), "%s.%s.%s", segmentKey,
            salt, bucketable) >= 0)
        {
            int status;
            char digest[21], encoded[17];
            const float longScale = 0xFFFFFFFFFFFFFFF;

            SHA1(digest, raw, strlen(raw));

            /* encodes to hex, and shortens, 16 characters in hex 8 bytes */
            status = hexify((unsigned char *)digest,
                sizeof(digest) - 1, encoded, sizeof(encoded));
            LD_ASSERT(status == 16);

            encoded[15] = 0;

            *bucket = (float)strtoll(encoded, NULL, 16) / longScale;

            LDJSONFree(attributeValue);

            return true;
        }

        LDJSONFree(attributeValue);
    }

    return false;
}

bool
LDi_variationIndexForUser(const struct LDJSON *const varOrRoll,
    const struct LDUser *const user, const char *const key,
    const char *const salt, const struct LDJSON **const index)
{
    struct LDJSON *variation, *rollout, *variations, *weight, *subvariation;
    float userBucket, sum;

    LD_ASSERT(varOrRoll);
    LD_ASSERT(index);

    variation    = NULL;
    rollout      = NULL;
    variations   = NULL;
    userBucket   = 0;
    sum          = 0;
    weight       = NULL;
    subvariation = NULL;

    variation = LDObjectLookup(varOrRoll, "variation");

    if (LDi_notNull(variation)) {
        if (LDJSONGetType(variation) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        *index = variation;

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

    variation = LDGetIter(variations);
    LD_ASSERT(variation);

    if (!LDi_bucketUser(user, key, "key", salt, &userBucket)) {
        LD_LOG(LD_LOG_ERROR, "failed to bucket user");

        return false;
    }

    for (; variation; variation = LDIterNext(variation)) {
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

        subvariation = LDObjectLookup(variation, "variation");

        if (!LDi_notNull(subvariation)) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(subvariation) != LDNumber) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (userBucket < sum) {
            *index = subvariation;

            return true;
        }
    }

    /* The user's bucket value was greater than or equal to the end of the last
    bucket. This could happen due to a rounding error, or due to the fact that
    we are scaling to 100000 rather than 99999, or the flag data could contain
    buckets that don't actually add up to 100000. Rather than returning an error
    in this case (or changing the scaling, which would potentially change the
    results for *all* users), we will simply put the user in the last bucket.
    The loop ensures subvariation is the last element, and a size check above
    ensures there is at least one element. */

    *index = subvariation;

    return true;
}

bool
LDi_getIndexForVariationOrRollout(const struct LDJSON *const flag,
    const struct LDJSON *const varOrRoll,
    const struct LDUser *const user, const struct LDJSON **const result)
{
    const struct LDJSON *jkey, *jsalt;
    const char *key, *salt;

    LD_ASSERT(flag);
    LD_ASSERT(varOrRoll);
    LD_ASSERT(result);

    jkey    = NULL;
    jsalt   = NULL;
    key     = NULL;
    salt    = NULL;
    *result = NULL;

    if (LDi_notNull(jkey = LDObjectLookup(flag, "key"))) {
        if (LDJSONGetType(jkey) != LDText) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        key = LDGetText(jkey);
    }

    if (LDi_notNull(jsalt = LDObjectLookup(flag, "salt"))) {
        if (LDJSONGetType(jsalt) != LDText) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return false;
        }

        salt = LDGetText(jsalt);
    }

    if (!LDi_variationIndexForUser(varOrRoll, user, key, salt, result)) {
        LD_LOG(LD_LOG_ERROR, "failed to get variation index");

        return false;
    }

    return true;
}
