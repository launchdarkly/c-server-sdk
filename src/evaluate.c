#include <string.h>

#include "sha1.h"

#include <hexify.h>
#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "evaluate.h"
#include "event_processor.h"
#include "network.h"
#include "operators.h"
#include "store.h"
#include "user.h"
#include "utility.h"
#include "time_utils.h"

static const struct LDJSON *
lookupRequiredValueOfType(
    const struct LDJSON *const obj,
    const char *const          context,
    const char *const          key,
    const LDJSONType           expectedType)
{
    const struct LDJSON *tmp;

    LD_ASSERT(context);
    LD_ASSERT(key);
    LD_ASSERT(obj);
    LD_ASSERT(LDJSONGetType(obj) == LDObject);

    if (!(tmp = LDObjectLookup(obj, key))) {
        LD_LOG_2(LD_LOG_ERROR, "%s missing required field %s", context, key);

        return NULL;
    }

    if (LDJSONGetType(tmp) != expectedType) {
        LD_LOG_2(LD_LOG_ERROR, "%s.%s unexpected type", context, key);

        return NULL;
    }

    return tmp;
}

static LDBoolean
lookupOptionalValueOfType(
    const struct LDJSON *const  obj,
    const char *const           context,
    const char *const           key,
    const LDJSONType            expectedType,
    const struct LDJSON **const result)
{
    const struct LDJSON *tmp;
    LDJSONType           actualType;

    LD_ASSERT(context);
    LD_ASSERT(key);
    LD_ASSERT(obj);
    LD_ASSERT(LDJSONGetType(obj) == LDObject);
    LD_ASSERT(result);

    *result = NULL;

    if (!(tmp = LDObjectLookup(obj, key))) {
        return LDBooleanTrue;
    }

    actualType = LDJSONGetType(tmp);

    if (actualType == LDNull) {
        return LDBooleanTrue;
    }

    if (actualType != expectedType) {
        LD_LOG_2(LD_LOG_ERROR, "%s.%s unexpected type", context, key);

        return LDBooleanFalse;
    }

    *result = tmp;

    return LDBooleanTrue;
}

LDBoolean
LDi_isEvalError(const EvalStatus status)
{
    return status == EVAL_MEM || status == EVAL_SCHEMA || status == EVAL_STORE;
}

static EvalStatus
maybeNegate(const struct LDJSON *const clause, const EvalStatus status)
{
    const struct LDJSON *negate;

    LD_ASSERT(clause);

    if (LDi_isEvalError(status)) {
        return status;
    }

    if (!lookupOptionalValueOfType(clause, "clause", "negate", LDBool, &negate))
    {
        return EVAL_SCHEMA;
    }

    if (negate) {
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

static LDBoolean
getValue(
    const struct LDJSON *const flag,
    struct LDJSON **           result,
    const struct LDJSON *const index,
    EvalStatus *o_error,
    unsigned int *validatedVariationIndex)
{
    struct LDJSON *      variationCopy;
    const struct LDJSON *variations, *variation;

    double unvalidatedVariationIndex;

    LD_ASSERT(validatedVariationIndex);

    if (index == NULL) {
        LD_LOG(LD_LOG_ERROR, "null variation index");

        *o_error = EVAL_SCHEMA;
        return LDBooleanFalse;
    }

    if (LDJSONGetType(index) != LDNumber) {
        LD_LOG(LD_LOG_ERROR, "variation index expected number");

        *o_error = EVAL_SCHEMA;
        return LDBooleanFalse;
    }

    unvalidatedVariationIndex = LDGetNumber(index);
    if (unvalidatedVariationIndex < 0) {
        LD_LOG(LD_LOG_ERROR, "variation index negative");

        *o_error = EVAL_SCHEMA;
        return LDBooleanFalse;
    }

    *validatedVariationIndex = (unsigned int) unvalidatedVariationIndex;

    if (!(variations =
              lookupRequiredValueOfType(flag, "flag", "variations", LDArray)))
    {
        *o_error = EVAL_SCHEMA;
        return LDBooleanFalse;
    }

    if (!(variation = LDArrayLookup(variations, *validatedVariationIndex))) {
        LD_LOG(LD_LOG_ERROR, "variation index outside of bounds");

        *o_error = EVAL_SCHEMA;
        return LDBooleanFalse;
    }

    if (!(variationCopy = LDJSONDuplicate(variation))) {
        LD_LOG(LD_LOG_ERROR, "failed to allocate variation");

        *o_error = EVAL_MEM;
        return LDBooleanFalse;
    }

    *result = variationCopy;

    return LDBooleanTrue;
}

static LDBoolean  addValue(
        const struct LDJSON *const flag,
        struct LDJSON **           result,
        struct LDDetails *const    details,
        const struct LDJSON *const index,
        EvalStatus *o_error)
{
    unsigned int validatedVariationIndex = 0;

    LD_ASSERT(flag);
    LD_ASSERT(result);
    LD_ASSERT(details);
    LD_ASSERT(o_error);

    if(!getValue(flag, result, index, o_error, &validatedVariationIndex)) {

        LDDetailsClear(details);

        *result               = NULL;
        details->hasVariation = LDBooleanFalse;
        details->reason = LD_ERROR;
        details->extra.errorKind = LD_MALFORMED_FLAG;

        return LDBooleanFalse;
    }

    details->hasVariation = LDBooleanTrue;
    details->variationIndex = validatedVariationIndex;
    return LDBooleanTrue;
}

static const char *
LDi_getBucketAttribute(const struct LDJSON *const obj)
{
    const struct LDJSON *bucketBy;

    LD_ASSERT(obj);
    LD_ASSERT(LDJSONGetType(obj) == LDObject);

    if (!lookupOptionalValueOfType(
            obj, "rollout", "bucketBy", LDText, &bucketBy)) {
        return NULL;
    }

    if (bucketBy == NULL) {
        return "key";
    }

    return LDGetText(bucketBy);
}

EvalStatus
LDi_evaluate(
    struct LDClient *const     client,
    const struct LDJSON *const flag,
    const struct LDUser *const user,
    struct LDStore *const      store,
    struct LDDetails *const    details,
    struct LDJSON **const      o_events,
    struct LDJSON **const      o_value,
    const LDBoolean            recordReason)
{
    LDBoolean inExperiment;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(details);
    LD_ASSERT(o_events);
    LD_ASSERT(o_value);

    if (LDJSONGetType(flag) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "flag expected object");

        return EVAL_SCHEMA;
    }

    /* on */
    {
        const struct LDJSON *on;
        struct LDJSON* offVariationValue = NULL;
        EvalStatus status;

        if (!lookupOptionalValueOfType(flag, "flag", "on", LDBool, &on)) {
            return EVAL_SCHEMA;
        }

        if (on == NULL || !LDGetBool(on)) {
            details->reason = LD_OFF;

            offVariationValue = LDObjectLookup(flag, "offVariation");

            /* It is valid for the offVariation to either be unspecified or to be null. */
            if (offVariationValue == NULL || LDJSONGetType(offVariationValue) == LDNull) {
                *o_value = NULL;
            } else if (!(addValue(
                    flag,
                    o_value,
                    details,
                    offVariationValue, &status))) {
                LD_LOG(LD_LOG_ERROR, "failed to add value");

               return status;
            }

            return EVAL_MISS;
        }
    }

    /* prerequisites */
    {
        EvalStatus  substatus, status;
        const char *failedKey;

        failedKey = NULL;

        if (LDi_isEvalError(
                substatus = LDi_checkPrerequisites(
                    client,
                    flag,
                    user,
                    store,
                    &failedKey,
                    o_events,
                    recordReason)))
        {
            LD_LOG(LD_LOG_ERROR, "checkPrerequisites failed");

            return substatus;
        }

        if (substatus == EVAL_MISS) {
            char *key;

            if (!(key = LDStrDup(failedKey))) {
                LD_LOG(LD_LOG_ERROR, "failed to duplicate failed key");

                return EVAL_MEM;
            }

            details->reason                = LD_PREREQUISITE_FAILED;
            details->extra.prerequisiteKey = key;

            if (!(addValue(
                    flag,
                    o_value,
                    details,
                    LDObjectLookup(flag, "offVariation"),
                    &status))) {
                LD_LOG(LD_LOG_ERROR, "failed to add value");

                return status;
            }

            return EVAL_MISS;
        }
    }

    /* targets */
    {
        const struct LDJSON *targets;
        EvalStatus status;

        if (!lookupOptionalValueOfType(
                flag, "flag", "targets", LDArray, &targets)) {
            return EVAL_SCHEMA;
        }

        if (targets) {
            const struct LDJSON *target;

            for (target = LDGetIter(targets); target;
                 target = LDIterNext(target)) {
                const struct LDJSON *values = NULL;

                if (LDJSONGetType(target) != LDObject) {
                    LD_LOG(LD_LOG_ERROR, "target expected object");

                    return EVAL_SCHEMA;
                }

                if (!lookupOptionalValueOfType(
                        target, "target", "values", LDArray, &values)) {
                    return EVAL_SCHEMA;
                }

                if (values && LDi_textInArray(values, user->key)) {
                    const struct LDJSON *variation;

                    if (!(variation = lookupRequiredValueOfType(
                              target, "target", "variation", LDNumber)))
                    {
                        return EVAL_SCHEMA;
                    }

                    details->reason = LD_TARGET_MATCH;

                    if (!(addValue(flag, o_value, details, variation, &status))) {
                        LD_LOG(LD_LOG_ERROR, "failed to add value");

                        return status;
                    }

                    return EVAL_MATCH;
                }
            }
        }
    }

    /* rules */
    {
        const struct LDJSON *rules;

        if (!lookupOptionalValueOfType(flag, "flag", "rules", LDArray, &rules))
        {
            return EVAL_SCHEMA;
        }

        if (rules) {
            unsigned int         index;
            const struct LDJSON *rule;

            index = 0;

            for (rule = LDGetIter(rules); rule; rule = LDIterNext(rule)) {
                EvalStatus substatus;

                if (LDJSONGetType(rule) != LDObject) {
                    LD_LOG(LD_LOG_ERROR, "rule expected object");

                    return EVAL_SCHEMA;
                }

                if (LDi_isEvalError(
                        substatus = LDi_ruleMatchesUser(rule, user, store))) {
                    LD_LOG(LD_LOG_ERROR, "ruleMatchesUser Failed");

                    return substatus;
                }

                if (substatus == EVAL_MATCH) {
                    const struct LDJSON *ruleId, *variation;
                    EvalStatus status;

                    variation = NULL;

                    details->reason               = LD_RULE_MATCH;
                    details->extra.rule.ruleIndex = index;
                    details->extra.rule.id        = NULL;

                    if (!LDi_getIndexForVariationOrRollout(
                            flag, rule, user, &inExperiment, &variation))
                    {
                        LD_LOG(LD_LOG_ERROR, "schema error");

                        return EVAL_SCHEMA;
                    }

                    details->extra.rule.inExperiment = inExperiment;

                    if (!(addValue(flag, o_value, details, variation, &status))) {
                        LD_LOG(LD_LOG_ERROR, "failed to add value");

                        return status;
                    }

                    if (!lookupOptionalValueOfType(
                            rule, "rule", "id", LDText, &ruleId)) {
                        return EVAL_SCHEMA;
                    }

                    if (ruleId) {
                        char *text;

                        if (!(text = LDStrDup(LDGetText(ruleId)))) {
                            LD_LOG(LD_LOG_ERROR, "failed to duplicate rule id");

                            return EVAL_MEM;
                        }

                        details->extra.rule.id = text;
                    }

                    return EVAL_MATCH;
                }

                index++;
            }
        }
    }

    /* fallthrough */
    {
        const struct LDJSON *index;
        EvalStatus status;

        index = NULL;

        details->reason = LD_FALLTHROUGH;

        if (!LDi_getIndexForVariationOrRollout(
                flag,
                LDObjectLookup(flag, "fallthrough"),
                user,
                &inExperiment,
                &index))
        {
            LD_LOG(LD_LOG_ERROR, "schema error");

            return EVAL_SCHEMA;
        }

        details->extra.fallthrough.inExperiment = inExperiment;

        if (!(addValue(flag, o_value, details, index, &status))) {
            LD_LOG(LD_LOG_ERROR, "failed to add value");

            return status;
        }

        return EVAL_MATCH;
    }
}

EvalStatus
LDi_checkPrerequisites(
    struct LDClient *const     client,
    const struct LDJSON *const flag,
    const struct LDUser *const user,
    struct LDStore *const      store,
    const char **const         failedKey,
    struct LDJSON **const      events,
    const LDBoolean            recordReason)
{
    const struct LDJSON *prerequisites, *iter;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(failedKey);
    LD_ASSERT(events);
    LD_ASSERT(LDJSONGetType(flag) == LDObject);

    if (!lookupOptionalValueOfType(
            flag, "flag", "prerequisites", LDArray, &prerequisites))
    {
        return EVAL_SCHEMA;
    }

    if (!prerequisites) {
        return EVAL_MATCH;
    }

    for (iter = LDGetIter(prerequisites); iter; iter = LDIterNext(iter)) {
        struct LDJSON *      value, *preflag, *event, *subevents;
        const struct LDJSON *key, *variation;
        unsigned int *       variationNumRef;
        EvalStatus           status;
        const char *         keyText;
        struct LDDetails     details;
        struct LDJSONRC *    preflagrc;
        struct LDTimestamp  timestamp;

        value           = NULL;
        preflag         = NULL;
        key             = NULL;
        variation       = NULL;
        variationNumRef = NULL;
        event           = NULL;
        subevents       = NULL;
        keyText         = NULL;
        preflagrc       = NULL;

        LDDetailsInit(&details);
        LDTimestamp_InitNow(&timestamp);

        if (LDJSONGetType(iter) != LDObject) {
            LD_LOG(LD_LOG_ERROR, "prerequisite expected object");

            return EVAL_SCHEMA;
        }

        if (!(key = lookupRequiredValueOfType(
                  iter, "prerequisite", "key", LDText))) {
            return EVAL_SCHEMA;
        }

        if (!(variation = lookupRequiredValueOfType(
                  iter, "prerequisite", "variation", LDNumber)))
        {
            return EVAL_SCHEMA;
        }

        keyText    = LDGetText(key);
        *failedKey = keyText;

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

        if (LDi_isEvalError(
                status = LDi_evaluate(
                    client,
                    preflag,
                    user,
                    store,
                    &details,
                    &subevents,
                    &value,
                    recordReason)))
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

        event = LDi_newFeatureEvent(
                keyText,
                user,
                variationNumRef,
                value,
                NULL,
                LDGetText(LDObjectLookup(flag, "key")),
                preflag,
                &details,
                timestamp,
                client->config->inlineUsersInEvents,
                client->config->allAttributesPrivate,
                client->config->privateAttributeNames
        );

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
            const struct LDJSON *on;
            LDBoolean            variationMatch;

            variationMatch = LDBooleanFalse;

            if (!lookupOptionalValueOfType(preflag, "flag", "on", LDBool, &on))
            {
                LDJSONRCDecrement(preflagrc);
                LDJSONFree(value);
                LDDetailsClear(&details);

                return EVAL_SCHEMA;
            }

            if (details.hasVariation) {
                variationMatch =
                    details.variationIndex == LDGetNumber(variation);
            }

            if (on == NULL || !LDGetBool(on) || !variationMatch) {
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
LDi_ruleMatchesUser(
    const struct LDJSON *const rule,
    const struct LDUser *const user,
    struct LDStore *const      store)
{
    const struct LDJSON *clauses, *clause;

    LD_ASSERT(rule);
    LD_ASSERT(user);

    if (!lookupOptionalValueOfType(rule, "rule", "clauses", LDArray, &clauses))
    {
        return EVAL_SCHEMA;
    }

    if (clauses == NULL) {
        return EVAL_MATCH;
    }

    for (clause = LDGetIter(clauses); clause; clause = LDIterNext(clause)) {
        EvalStatus evalStatus;

        if (LDJSONGetType(clause) != LDObject) {
            LD_LOG(LD_LOG_ERROR, "clause expected object");

            return EVAL_SCHEMA;
        }

        if (LDi_isEvalError(
                evalStatus = LDi_clauseMatchesUser(clause, user, store))) {

            return evalStatus;
        }

        if (evalStatus == EVAL_MISS) {
            return EVAL_MISS;
        }
    }

    return EVAL_MATCH;
}

EvalStatus
LDi_clauseMatchesUser(
    const struct LDJSON *const clause,
    const struct LDUser *const user,
    struct LDStore *const      store)
{
    const struct LDJSON *op;

    LD_ASSERT(clause);
    LD_ASSERT(user);

    if (LDJSONGetType(clause) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "clause expected object");

        return EVAL_SCHEMA;
    }

    if (!(op = lookupRequiredValueOfType(clause, "clause", "op", LDText))) {
        return EVAL_SCHEMA;
    }

    if (strcmp(LDGetText(op), "segmentMatch") == 0) {
        const struct LDJSON *values, *iter;

        if (!lookupOptionalValueOfType(
                clause, "clause", "values", LDArray, &values)) {
            return EVAL_SCHEMA;
        }

        if (values == NULL) {
            return maybeNegate(clause, EVAL_MISS);
        }

        for (iter = LDGetIter(values); iter; iter = LDIterNext(iter)) {
            if (LDJSONGetType(iter) == LDText) {
                EvalStatus       evalStatus;
                struct LDJSON *  segment;
                struct LDJSONRC *segmentrc;

                segmentrc = NULL;
                segment   = NULL;

                if (!LDStoreGet(store, LD_SEGMENT, LDGetText(iter), &segmentrc))
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
                        evalStatus = LDi_segmentMatchesUser(segment, user))) {
                    LD_LOG(LD_LOG_ERROR, "segmentMatchesUser error");

                    LDJSONRCDecrement(segmentrc);

                    return evalStatus;
                }

                LDJSONRCDecrement(segmentrc);

                if (evalStatus == EVAL_MATCH) {
                    return maybeNegate(clause, EVAL_MATCH);
                }
            }
        }

        return maybeNegate(clause, EVAL_MISS);
    }

    return LDi_clauseMatchesUserNoSegments(clause, user);
}

EvalStatus
LDi_segmentMatchesUser(
    const struct LDJSON *const segment, const struct LDUser *const user)
{
    LD_ASSERT(segment);
    LD_ASSERT(user);

    /* included */
    {
        const struct LDJSON *included;

        if (!lookupOptionalValueOfType(
                segment, "segment", "included", LDArray, &included))
        {
            return EVAL_SCHEMA;
        }

        if (included && LDi_textInArray(included, user->key)) {
            return EVAL_MATCH;
        }
    }

    /* excluded */
    {
        const struct LDJSON *excluded;

        if (!lookupOptionalValueOfType(
                segment, "segment", "excluded", LDArray, &excluded))
        {
            return EVAL_SCHEMA;
        }

        if (excluded && LDi_textInArray(excluded, user->key)) {
            return EVAL_MISS;
        }
    }

    /* rules */
    {
        const struct LDJSON *iter, *salt, *segmentRules, *key;

        if (!lookupOptionalValueOfType(
                segment, "segment", "rules", LDArray, &segmentRules))
        {
            return EVAL_SCHEMA;
        }

        if (segmentRules == NULL) {
            return EVAL_MISS;
        }

        if (!(key =
                  lookupRequiredValueOfType(segment, "segment", "key", LDText)))
        {
            return EVAL_SCHEMA;
        }

        if (!(salt = lookupRequiredValueOfType(
                  segment, "segment", "salt", LDText))) {
            return EVAL_SCHEMA;
        }

        for (iter = LDGetIter(segmentRules); iter; iter = LDIterNext(iter)) {
            EvalStatus evalStatus;

            if (LDJSONGetType(iter) != LDObject) {
                LD_LOG(LD_LOG_ERROR, "segment rule expected object");

                return EVAL_SCHEMA;
            }

            if (LDi_isEvalError(
                    evalStatus = LDi_segmentRuleMatchUser(
                        iter, LDGetText(key), user, LDGetText(salt))))
            {
                return evalStatus;
            }

            if (evalStatus == EVAL_MATCH) {
                return EVAL_MATCH;
            }
        }

        return EVAL_MISS;
    }
}

EvalStatus
LDi_segmentRuleMatchUser(
    const struct LDJSON *const segmentRule,
    const char *const          segmentKey,
    const struct LDUser *const user,
    const char *const          salt)
{
    const struct LDJSON *clauses;

    LD_ASSERT(segmentRule);
    LD_ASSERT(segmentKey);
    LD_ASSERT(user);
    LD_ASSERT(salt);

    if (!lookupOptionalValueOfType(
            segmentRule, "segmentRule", "clauses", LDArray, &clauses))
    {
        return EVAL_SCHEMA;
    }

    if (clauses) {
        const struct LDJSON *clause;

        for (clause = LDGetIter(clauses); clause; clause = LDIterNext(clause)) {
            EvalStatus evalStatus;

            if (LDi_isEvalError(
                    evalStatus = LDi_clauseMatchesUserNoSegments(clause, user)))
            {
                return evalStatus;
            }

            if (evalStatus == EVAL_MISS) {
                return EVAL_MISS;
            }
        }
    }

    {
        const struct LDJSON *weight;
        float                bucket;
        const char *         attribute;

        if (!lookupOptionalValueOfType(
                segmentRule, "segmentRule", "weight", LDNumber, &weight))
        {
            return EVAL_SCHEMA;
        }

        if (weight == NULL) {
            return EVAL_MATCH;
        }

        attribute = LDi_getBucketAttribute(segmentRule);

        if (attribute == NULL) {
            LD_LOG(LD_LOG_ERROR, "failed to parse bucketBy");

            return EVAL_SCHEMA;
        }

        LDi_bucketUser(user, segmentKey, attribute, salt, NULL, &bucket);

        if (bucket < LDGetNumber(weight) / 100000) {
            return EVAL_MATCH;
        } else {
            return EVAL_MISS;
        }
    }
}

static EvalStatus
matchAny(
    OpFn f, const struct LDJSON *const value, const struct LDJSON *const values)
{
    const struct LDJSON *iter;

    LD_ASSERT(f);
    LD_ASSERT(value);

    if (values) {
        for (iter = LDGetIter(values); iter; iter = LDIterNext(iter)) {
            if (f(value, iter)) {
                return EVAL_MATCH;
            }
        }
    }

    return EVAL_MISS;
}

EvalStatus
LDi_clauseMatchesUserNoSegments(
    const struct LDJSON *const clause, const struct LDUser *const user)
{
    OpFn                 fn;
    struct LDJSON *      attributeValue;
    const struct LDJSON *values, *attribute, *op;
    LDJSONType           attributeType;

    LD_ASSERT(clause);
    LD_ASSERT(user);

    attributeValue = NULL;

    if (!(op = lookupRequiredValueOfType(clause, "clause", "op", LDText))) {
        return EVAL_SCHEMA;
    }

    if (!(fn = LDi_lookupOperation(LDGetText(op)))) {
        LD_LOG(LD_LOG_WARNING, "unknown operator");

        return EVAL_MISS;
    }

    if (!(attribute =
              lookupRequiredValueOfType(clause, "clause", "attribute", LDText)))
    {
        return EVAL_SCHEMA;
    }

    if (!lookupOptionalValueOfType(
            clause, "clause", "values", LDArray, &values)) {
        return EVAL_SCHEMA;
    }

    if (!(attributeValue = LDi_valueOfAttribute(user, LDGetText(attribute)))) {
        LD_LOG(LD_LOG_TRACE, "attribute does not exist");

        return EVAL_MISS;
    }

    attributeType = LDJSONGetType(attributeValue);

    if (attributeType == LDNull) {
        /* Null attributes are always non-matches. */

        return EVAL_MISS;
    }

    if (attributeType == LDArray) {
        const struct LDJSON *iter;

        for (iter = LDGetIter(attributeValue); iter; iter = LDIterNext(iter)) {
            EvalStatus evalStatus;

            attributeType = LDJSONGetType(iter);

            if (attributeType == LDObject || attributeType == LDArray) {
                LD_LOG(
                    LD_LOG_WARNING, "attribute value expected array or object");

                LDJSONFree(attributeValue);

                return EVAL_MISS;
            }

            if (LDi_isEvalError(evalStatus = matchAny(fn, iter, values))) {

                LD_LOG(LD_LOG_ERROR, "matchAny failed");

                LDJSONFree(attributeValue);

                return evalStatus;
            }

            if (evalStatus == EVAL_MATCH) {
                LDJSONFree(attributeValue);

                return maybeNegate(clause, EVAL_MATCH);
            }
        }

        LDJSONFree(attributeValue);

        return maybeNegate(clause, EVAL_MISS);
    } else {
        EvalStatus evalStatus;

        if (LDi_isEvalError(evalStatus = matchAny(fn, attributeValue, values)))
        {
            LD_LOG(LD_LOG_ERROR, "matchAny failed");

            LDJSONFree(attributeValue);

            return evalStatus;
        }

        LDJSONFree(attributeValue);

        return maybeNegate(clause, evalStatus);
    }
}

static float
LDi_hexToDecimal(const char *const input)
{
    float       acc;
    const char *i;

    LD_ASSERT(input);

    acc = 0;

    for (i = input; *i != '\0'; i++) {
        char charOffset;

        if (*i >= 48 && *i <= 57) {
            charOffset = *i - 48;
        } else if (*i >= 97 && *i <= 102) {
            charOffset = *i - 87;
        } else {
            return 0.0;
        }

        acc = (acc * 16) + charOffset;
    }

    return acc;
}

LDBoolean
LDi_bucketUser(
    const struct LDUser *const user,
    const char *const          segmentKey,
    const char *const          attribute,
    const char *const          salt,
    const int *const           seed,
    float *const               bucket)
{
    struct LDJSON *attributeValue;

    LD_ASSERT(user);
    LD_ASSERT(segmentKey);
    LD_ASSERT(attribute);
    LD_ASSERT(salt);
    LD_ASSERT(bucket);

    attributeValue = NULL;
    *bucket        = 0;

    if ((attributeValue = LDi_valueOfAttribute(user, attribute))) {
        char        raw[256], bucketableBuffer[256];
        const char *bucketable;
        int         snprintfStatus;

        bucketable = NULL;

        if (LDJSONGetType(attributeValue) == LDText) {
            bucketable = LDGetText(attributeValue);
        } else if (LDJSONGetType(attributeValue) == LDNumber) {
            if (snprintf(
                    bucketableBuffer,
                    sizeof(bucketableBuffer),
                    "%f",
                    LDGetNumber(attributeValue)) >= 0)
            {
                bucketable = bucketableBuffer;
            }
        }

        if (!bucketable) {
            LDJSONFree(attributeValue);

            return LDBooleanFalse;
        }

        if (seed) {
            if (user->secondary) {
                snprintfStatus = snprintf(
                    raw,
                    sizeof(raw),
                    "%d.%s.%s",
                    *seed,
                    bucketable,
                    user->secondary);
            } else {
                snprintfStatus =
                    snprintf(raw, sizeof(raw), "%d.%s", *seed, bucketable);
            }
        } else {
            if (user->secondary) {
                snprintfStatus = snprintf(
                    raw,
                    sizeof(raw),
                    "%s.%s.%s.%s",
                    segmentKey,
                    salt,
                    bucketable,
                    user->secondary);
            } else {
                snprintfStatus = snprintf(
                    raw, sizeof(raw), "%s.%s.%s", segmentKey, salt, bucketable);
            }
        }

        if (snprintfStatus >= 0 && (size_t)snprintfStatus < sizeof(raw)) {
            int         status;
            char        digest[21], encoded[17];
            const float longScale = 1152921504606846975.0;

            SHA1(digest, raw, strlen(raw));

            /* encodes to hex, and shortens, 16 characters in hex 8 bytes */
            status = hexify(
                (unsigned char *)digest,
                sizeof(digest) - 1,
                encoded,
                sizeof(encoded));
            LD_ASSERT(status == 16);

            encoded[15] = 0;

            *bucket = LDi_hexToDecimal(encoded) / longScale;

            LDJSONFree(attributeValue);

            return LDBooleanTrue;
        }

        LDJSONFree(attributeValue);
    }

    return LDBooleanFalse;
}

LDBoolean
LDi_variationIndexForUser(
    const struct LDJSON *const  varOrRoll,
    const struct LDUser *const  user,
    const char *const           key,
    const char *const           salt,
    LDBoolean *const            inExperiment,
    const struct LDJSON **const index)
{
    const struct LDJSON *variation, *rollout, *variations, *subVariation;
    float                userBucket, sum;
    LDBoolean            untrackedValue;

    LD_ASSERT(varOrRoll);
    LD_ASSERT(index);
    LD_ASSERT(inExperiment);
    LD_ASSERT(salt);
    LD_ASSERT(user);

    variations     = NULL;
    userBucket     = 0;
    sum            = 0;
    subVariation   = NULL;
    untrackedValue = LDBooleanFalse;
    *inExperiment  = LDBooleanFalse;

    /* if a specific variation */
    {
        const struct LDJSON *variation;

        if (!lookupOptionalValueOfType(
                varOrRoll,
                "variationOrRollout",
                "variation",
                LDNumber,
                &variation))
        {
            return LDBooleanFalse;
        }

        if (variation != NULL) {
            *index = variation;

            return LDBooleanTrue;
        }
    }

    if (!(rollout = lookupRequiredValueOfType(
              varOrRoll, "variationOrRollout", "rollout", LDObject)))
    {
        return LDBooleanFalse;
    }

    /* rollout kind */
    {
        const struct LDJSON *rolloutKind;

        if (!lookupOptionalValueOfType(
                rollout, "rollout", "kind", LDText, &rolloutKind))
        {
            return LDBooleanFalse;
        }

        if (rolloutKind && strcmp(LDGetText(rolloutKind), "experiment") == 0) {
            *inExperiment = LDBooleanTrue;
        }
    }

    if (!(variations = lookupRequiredValueOfType(
              rollout, "rollout", "variations", LDArray)))
    {
        return LDBooleanFalse;
    }

    if (LDCollectionGetSize(variations) == 0) {
        LD_LOG(LD_LOG_ERROR, "rollout variations must not be empty");

        return LDBooleanFalse;
    }

    {
        const char *         attribute;
        const struct LDJSON *seedJSON;
        int                  seedValue, *seedValueRef;

        seedValueRef = NULL;
        attribute    = LDi_getBucketAttribute(rollout);

        if (attribute == NULL) {
            LD_LOG(LD_LOG_ERROR, "failed to parse bucketBy");

            return LDBooleanFalse;
        }

        if (!lookupOptionalValueOfType(
                rollout, "rollout", "seed", LDNumber, &seedJSON)) {
            return LDBooleanFalse;
        }

        if (seedJSON != NULL) {
            seedValue    = (int)LDGetNumber(seedJSON);
            seedValueRef = &seedValue;
        }

        LDi_bucketUser(user, key, attribute, salt, seedValueRef, &userBucket);
    }

    for (variation = LDGetIter(variations); variation;
         variation = LDIterNext(variation))
    {
        const struct LDJSON *untracked, *weight;

        if (!(weight = lookupRequiredValueOfType(
                  variation, "weightedVariation", "weight", LDNumber)))
        {
            return LDBooleanFalse;
        }

        sum += LDGetNumber(weight) / 100000.0;

        if (!(subVariation = lookupRequiredValueOfType(
                  variation, "weightedVariation", "variation", LDNumber)))
        {
            return LDBooleanFalse;
        }

        untrackedValue = LDBooleanFalse;

        if (!lookupOptionalValueOfType(
                variation,
                "weightedVariation",
                "untracked",
                LDBool,
                &untracked))
        {
            return LDBooleanFalse;
        }

        if (untracked != NULL) {
            untrackedValue = LDGetBool(untracked);
        }

        if (userBucket < sum) {
            *index = subVariation;

            if (*inExperiment && untrackedValue) {
                *inExperiment = LDBooleanFalse;
            }

            return LDBooleanTrue;
        }
    }

    /* The user's bucket value was greater than or equal to the end of the last
    bucket. This could happen due to a rounding error, or due to the fact that
    we are scaling to 100000 rather than 99999, or the flag data could contain
    buckets that don't actually add up to 100000. Rather than returning an error
    in this case (or changing the scaling, which would potentially change the
    results for *all* users), we will simply put the user in the last bucket.
    The loop ensures subVariation is the last element, and a size check above
    ensures there is at least one element. */

    *index = subVariation;

    if (*inExperiment && untrackedValue) {
        *inExperiment = LDBooleanFalse;
    }

    return LDBooleanTrue;
}

LDBoolean
LDi_getIndexForVariationOrRollout(
    const struct LDJSON *const  flag,
    const struct LDJSON *const  varOrRoll,
    const struct LDUser *const  user,
    LDBoolean *const            inExperiment,
    const struct LDJSON **const result)
{
    const struct LDJSON *key, *salt;

    LD_ASSERT(flag);
    LD_ASSERT(varOrRoll);
    LD_ASSERT(inExperiment);
    LD_ASSERT(result);

    *result = NULL;

    if (!(key = lookupRequiredValueOfType(flag, "flag", "key", LDText))) {
        return LDBooleanFalse;
    }

    if (!(salt = lookupRequiredValueOfType(flag, "flag", "salt", LDText))) {
        return LDBooleanFalse;
    }

    if (!LDi_variationIndexForUser(
            varOrRoll,
            user,
            LDGetText(key),
            LDGetText(salt),
            inExperiment,
            result))
    {
        LD_LOG(LD_LOG_ERROR, "failed to get variation index");

        return LDBooleanFalse;
    }

    return LDBooleanTrue;
}
