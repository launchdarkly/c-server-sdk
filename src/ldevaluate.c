#include "sha1.h"
#include "hexify.h"

#include "ldinternal.h"
#include "ldevaluate.h"

static bool
addReason(struct LDJSON *result, const char *const reason)
{
    struct LDJSON *tmpcollection;
    struct LDJSON *tmp;

    LD_ASSERT(result);
    LD_ASSERT(reason);

    if (!(tmpcollection = LDNewObject())) {
        LDi_log(LD_LOG_ERROR, "allocation error 1");

        return false;
    }

    if (!(tmp = LDNewText(reason))) {
        LDi_log(LD_LOG_ERROR, "allocation error 2");

        return false;
    }

    if (!(LDObjectSetKey(tmpcollection, "kind", tmp))) {
        LDi_log(LD_LOG_ERROR, "allocation error 3");

        return false;
    }

    if (!(LDObjectSetKey(result, "reason", tmpcollection))) {
        LDi_log(LD_LOG_ERROR, "allocation error 4");

        return false;
    }

    return true;
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
            LDi_log(LD_LOG_ERROR, "schema error 5");

            return false;
        }

        if (!(tmp = LDNewNumber(LDGetNumber(index)))) {
            LDi_log(LD_LOG_ERROR, "allocation error6");

            return false;
        }
    } else {
        if (!(tmp = LDNewNull())) {
            LDi_log(LD_LOG_ERROR, "allocation error7");

            return false;
        }
    }

    if (!(LDObjectSetKey(result, "variationIndex", tmp))) {
        LDi_log(LD_LOG_ERROR, "allocation error8");

        return false;
    }

    if (index) {
        if (!(variations = LDObjectLookup(flag, "variations"))) {
            LDi_log(LD_LOG_ERROR, "schema error9");

            return false;
        }

        if (LDJSONGetType(variations) != LDArray) {
            LDi_log(LD_LOG_ERROR, "schema error10");

            return false;
        }

        if (!(variation = LDArrayLookup(variations,
            LDGetNumber(index))))
        {
            LDi_log(LD_LOG_ERROR, "schema error11");

            return false;
        }

        if (!(tmp = LDJSONDuplicate(variation))) {
            LDi_log(LD_LOG_ERROR, "allocation error12");

            return false;
        }
    } else {
        if (!(tmp = LDNewNull())) {
            LDi_log(LD_LOG_ERROR, "allocation error13");

            return false;
        }
    }

    if (!(LDObjectSetKey(result, "value", tmp))) {
        LDi_log(LD_LOG_ERROR, "allocation error14");

        return false;
    }

    return true;
}

bool
evaluate(const struct LDJSON *const flag, const struct LDUser *const user,
    struct LDStore *const store, struct LDJSON **const result)
{
    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(result);

    if (!(*result = LDNewObject())) {
        LDi_log(LD_LOG_ERROR, "allocation error15");

        return false;
    }

    if (LDJSONGetType(flag) != LDObject) {
        LDi_log(LD_LOG_ERROR, "schema error16");

        return false;
    }

    {
        const struct LDJSON *on = NULL;

        if (!(on = LDObjectLookup(flag, "on"))) {
            LDi_log(LD_LOG_ERROR, "schema error17");

            return false;
        }

        if (LDJSONGetType(on) != LDBool) {
            LDi_log(LD_LOG_ERROR, "schema error19");

            return false;
        }

        if (!LDGetBool(on)) {
            const struct LDJSON *offVariation =
                LDObjectLookup(flag, "offVariation");

            if (!addReason(*result, "OFF")) {
                LDi_log(LD_LOG_ERROR, "failed to add reason19");

                return false;
            }

            if (!(addValue(flag, *result, offVariation))) {
                LDi_log(LD_LOG_ERROR, "failed to add value20");

                return false;
            }

            return true;
        }
    }

    {
        bool submatch;

        if (!checkPrerequisites(flag, user, store, &submatch)) {
            LDi_log(LD_LOG_ERROR, "sub error error21");

            return false;
        }

        if (!submatch) {
            const struct LDJSON *offVariation =
                LDObjectLookup(flag, "offVariation");

            if (!addReason(*result, "PREREQUISITE_FAILED")) {
                LDi_log(LD_LOG_ERROR, "failed to add reason22");

                return false;
            }

            if (!(addValue(flag, *result, offVariation))) {
                LDi_log(LD_LOG_ERROR, "failed to add value23");

                return false;
            }

            return true;
        }
    }
    /*
    {
        const struct LDJSON *const targets = LDObjectLookup(flag, "targets");

        LD_ASSERT(LDJSONGetType(targets) == LDArray);

        {
            const struct LDJSON *iter = NULL;

            for (iter = LDGetIter(targets); iter; iter = LDIterNext(iter)) {
                const struct LDJSON *values = NULL;

                LD_ASSERT(LDJSONGetType(iter) == LDObject);

                values = LDObjectLookup(iter, "values");

                LD_ASSERT(values); LD_ASSERT(LDJSONGetType(values) == LDArray);

                if (textInArray(values, user->key)) {
                    // TODO return isTarget
                }
            }
        }
    }
    */

    while (true) {
        const struct LDJSON *const rules = LDObjectLookup(flag, "rules");

        if (rules && LDJSONGetType(rules) != LDArray) {
            LDi_log(LD_LOG_ERROR, "schema error24");

            return false;
        }

        if (!rules || LDArrayGetSize(rules) == 0) {
            const struct LDJSON *fallthrough = NULL;

            if (!(fallthrough = LDObjectLookup(flag, "fallthrough"))) {
                LDi_log(LD_LOG_ERROR, "schema error25");

                return false;
            }

            fallthrough = LDObjectLookup(fallthrough, "variation");

            if (!addReason(*result, "FALLTHROUGH")) {
                LDi_log(LD_LOG_ERROR, "failed to add reason26");

                return false;
            }

            if (!(addValue(flag, *result, fallthrough))) {
                LDi_log(LD_LOG_ERROR, "failed to add value27");

                return false;
            }

            return true;
        }

        if (!rules) {
            break;
        }

        {
            const struct LDJSON *iter = NULL;

            for (iter = LDGetIter(rules); iter; iter = LDIterNext(iter)) {
                bool submatch;

                LD_ASSERT(LDJSONGetType(iter) == LDObject);

                if (!ruleMatchesUser(iter, user, &submatch)) {
                    LDi_log(LD_LOG_ERROR, "sub error28");

                    return false;
                }

                if (submatch) {
                    /* TODO return ruleMatch */

                    return true;
                }
            }
        }

        break;
    }

    return true;
}

bool
checkPrerequisites(const struct LDJSON *const flag,
    const struct LDUser *const user, struct LDStore *const store,
    bool *const matches)
{
    struct LDJSON *prerequisites = NULL;
    struct LDJSON *iter = NULL;

    LD_ASSERT(flag);
    LD_ASSERT(user);
    LD_ASSERT(store);
    LD_ASSERT(matches);

    if (LDJSONGetType(flag) != LDObject) {
        LDi_log(LD_LOG_ERROR, "schema error29");

        return false;
    }

    prerequisites = LDObjectLookup(flag, "prerequisites");

    if (!prerequisites) {
        *matches = true;

        return true;
    }

    if (LDJSONGetType(prerequisites) != LDArray) {
        LDi_log(LD_LOG_ERROR, "schema error30");

        return false;
    }

    for (iter = LDGetIter(prerequisites); iter; iter = LDIterNext(iter)) {
        struct LDJSON *result = NULL;
        struct LDJSON *preflag = NULL;
        const struct LDJSON *key = NULL;
        const struct LDJSON *variation = NULL;

        if (LDJSONGetType(iter) != LDObject) {
            LDi_log(LD_LOG_ERROR, "schema error31");

            return false;
        }

        if (!(key = LDObjectLookup(iter, "key"))) {
            LDi_log(LD_LOG_ERROR, "schema error32");

            return false;
        }

        if (LDJSONGetType(key) != LDText) {
            LDi_log(LD_LOG_ERROR, "schema error33");

            return false;
        }

        if (!(variation = LDObjectLookup(iter, "variation"))) {
            LDi_log(LD_LOG_ERROR, "schema error34");

            return false;
        }

        if (LDJSONGetType(variation) != LDNumber) {
            LDi_log(LD_LOG_ERROR, "schema error35");

            return false;
        }

        if (!(preflag = LDStoreGet(store, "flags", LDGetText(key)))) {
            LDi_log(LD_LOG_ERROR, "store lookup error36");

            return false;
        }

        if (!evaluate(preflag, user, store, &result)) {
            LDJSONFree(preflag);

            return false;
        }

        if (!result) {
            LDJSONFree(preflag);

            LDi_log(LD_LOG_ERROR, "sub error with result37");
        }

        {
            struct LDJSON *on;
            struct LDJSON *variationIndex;

            if (!(on = LDObjectLookup(preflag, "on"))) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LDi_log(LD_LOG_ERROR, "schema error38");

                return false;
            }

            if (LDJSONGetType(on) != LDBool) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LDi_log(LD_LOG_ERROR, "schema error39");

                return false;
            }

            if (!(variationIndex = LDObjectLookup(result, "variationIndex"))) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LDi_log(LD_LOG_ERROR, "schema error40");

                return false;
            }

            if (LDJSONGetType(variationIndex) != LDNumber) {
                LDJSONFree(preflag);
                LDJSONFree(result);

                LDi_log(LD_LOG_ERROR, "schema error41");

                return false;
            }

            if (!LDGetBool(on) ||
                LDGetNumber(variationIndex) != LDGetNumber(variation))
            {
                LDJSONFree(preflag);
                LDJSONFree(result);

                *matches = false;

                return true;
            }
        }

        LDJSONFree(preflag);
        LDJSONFree(result);
    }

    *matches = true;

    return true;
}

bool
ruleMatchesUser(const struct LDJSON *const rule,
    const struct LDUser *const user, bool *const matches)
{
    const struct LDJSON *clauses = NULL;
    const struct LDJSON *iter = NULL;

    LD_ASSERT(rule);
    LD_ASSERT(user);
    LD_ASSERT(matches);

    if (!(clauses = LDObjectLookup(rule, "clauses"))) {
        LDi_log(LD_LOG_ERROR, "schema error42");

        return false;
    }

    if (LDJSONGetType(clauses) != LDArray) {
        LDi_log(LD_LOG_ERROR, "schema error43");

        return false;
    }

    for (iter = LDGetIter(clauses); iter; iter = LDIterNext(iter)) {
        bool submatch;

        if (LDJSONGetType(iter) != LDObject) {
            LDi_log(LD_LOG_ERROR, "schema error44");

            return false;
        }

        if (!clauseMatchesUser(iter, user, &submatch)) {
            LDi_log(LD_LOG_ERROR, "schema error45");

            return false;
        }

        if (!submatch) {
            *matches = false;

            return true;
        }
    }

    *matches = true;

    return true;
}

bool
clauseMatchesUser(const struct LDJSON *const clause,
    const struct LDUser *const user, bool *const matches)
{
    const struct LDJSON *op = NULL;

    LD_ASSERT(clause);
    LD_ASSERT(user);

    if (LDJSONGetType(clause) != LDObject) {
        LDi_log(LD_LOG_ERROR, "schema error46");

        return false;
    }

    if (!(op = LDObjectLookup(clause, "op"))) {
        LDi_log(LD_LOG_ERROR, "schema error47");

        return false;
    }

    if (LDJSONGetType(op) != LDText) {
        LDi_log(LD_LOG_ERROR, "schema error48");

        return false;
    }

    if (strcmp(LDGetText(op), "segmentMatch") == 0) {
        const struct LDJSON *values = NULL;
        const struct LDJSON *iter = NULL;
        const struct LDJSON *negate = NULL;

        if (!(values = LDObjectLookup(clause, "values"))) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(values) != LDArray) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (!(negate = LDObjectLookup(clause, "negate"))) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(negate) != LDBool) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        for (iter = LDGetIter(values); iter; iter = LDIterNext(iter)) {
            if (LDJSONGetType(iter) == LDText) {
                const struct LDJSON *const segment = NULL;
                bool submatch;

                if (!segmentMatchesUser(segment, user, &submatch)) {
                    LDi_log(LD_LOG_ERROR, "sub error");

                    return false;
                }

                if (submatch) {
                    *matches = LDGetBool(negate) ? false : true;

                    return true;
                }
            }
        }

        *matches = LDGetBool(negate) ? true : true;

        return true;
    }

    {
        bool submatch;

        if (!clauseMatchesUserNoSegments(clause, user, &submatch)) {
            LDi_log(LD_LOG_ERROR, "sub error");

            return false;
        }

        *matches = submatch;

        return true;
    }
}

bool
segmentMatchesUser(const struct LDJSON *const segment,
    const struct LDUser *const user, bool *const matches)
{
    const struct LDJSON *included = NULL;
    const struct LDJSON *excluded = NULL;

    LD_ASSERT(segment);
    LD_ASSERT(user);

    if (!(included = LDObjectLookup(segment, "included"))) {
        LDi_log(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDJSONGetType(included) != LDArray) {
        LDi_log(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (textInArray(included, user->key)) {
        *matches = true;

        return true;
    } else if (textInArray(excluded, user->key)) {
        *matches = false;

        return true;
    } else {
        const struct LDJSON *segmentRules = NULL;
        const struct LDJSON *iter = NULL;

        if (!(segmentRules = LDObjectLookup(segment, "included"))) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(segmentRules) != LDArray) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        for (iter = LDGetIter(segmentRules); iter; iter = LDIterNext(iter)) {
            const struct LDJSON *key = NULL;
            const struct LDJSON *salt = NULL;
            bool submatches;

            if (LDJSONGetType(iter) != LDObject) {
                LDi_log(LD_LOG_ERROR, "schema error");

                return false;
            }

            if (!(key = LDObjectLookup(iter, "key"))) {
                LDi_log(LD_LOG_ERROR, "schema error");

                return false;
            }

            if (LDJSONGetType(key) != LDText) {
                LDi_log(LD_LOG_ERROR, "schema error");

                return false;
            }

            if (!(salt = LDObjectLookup(iter, "salt"))) {
                LDi_log(LD_LOG_ERROR, "schema error");

                return false;
            }

            if (LDJSONGetType(salt) != LDText) {
                LDi_log(LD_LOG_ERROR, "schema error");

                return false;
            }

            if (!segmentRuleMatchUser(iter, LDGetText(key), user,
                LDGetText(salt), &submatches))
            {
                return false;
            }

            if (submatches) {
                *matches = submatches;

                return true;
            }
        }

        *matches = false;

        return true;
    }
}

bool
segmentRuleMatchUser(const struct LDJSON *const segmentRule,
    const char *const segmentKey, const struct LDUser *const user,
    const char *const salt, bool *const matches)
{
    const struct LDJSON *clauses = NULL;
    const struct LDJSON *clause = NULL;

    LD_ASSERT(segmentRule);
    LD_ASSERT(segmentKey);
    LD_ASSERT(user);
    LD_ASSERT(salt);
    LD_ASSERT(matches);

    if (!(clauses = LDObjectLookup(segmentRule, "clauses"))) {
        LDi_log(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDJSONGetType(clauses) != LDArray) {
        LDi_log(LD_LOG_ERROR, "schema error");

        return false;
    }

    for (clause = LDGetIter(clauses); clause; clause = LDIterNext(clause)) {
        bool submatches;

        if (!clauseMatchesUserNoSegments(clause, user, &submatches)) {
            return false;
        }

        if (!submatches) {
            *matches = false;

            return true;
        }
    }

    {
        const struct LDJSON *weight = LDObjectLookup(segmentRule, "weight");

        if (!weight) {
            return true;
        }

        if (LDJSONGetType(weight) != LDNumber) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        {
            float bucket;

            const struct LDJSON *bucketBy =
                LDObjectLookup(segmentRule, "bucketBy");

            const char *const attribute = (bucketBy == NULL)
                ? "key" : LDGetText(bucketBy);

            if (!bucketUser(user, segmentKey, attribute, salt, &bucket)) {
                LDi_log(LD_LOG_ERROR, "bucketUser error");

                return false;
            }

            *matches = bucket < LDGetNumber(weight) / 100000;

            return true;
        }
    }
}

bool
clauseMatchesUserNoSegments(const struct LDJSON *const clause,
    const struct LDUser *const user, bool *const matches)
{
    const char *attributeText = NULL;
    struct LDJSON *attributeValue = NULL;
    struct LDJSON *attribute = NULL;
    LDJSONType type;

    LD_ASSERT(clause);
    LD_ASSERT(user);

    if (!(attribute = LDObjectLookup(clause, "attribute"))) {
        LDi_log(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (LDJSONGetType(attribute) != LDText) {
        LDi_log(LD_LOG_ERROR, "schema error");

        return false;
    }

    if (!(attributeText = LDGetText(attribute))) {
        LDi_log(LD_LOG_ERROR, "allocation error");

        return false;
    }

    if (!(attributeValue = valueOfAttribute(user, attributeText))) {
        LDi_log(LD_LOG_ERROR, "schema error");

        return false;
    }

    if ((type = LDJSONGetType(attributeValue)) == LDArray) {
        const struct LDJSON *negate = NULL;
        struct LDJSON *iter = LDGetIter(attributeValue);

        for (; iter; iter = LDIterNext(iter)) {
            type = LDJSONGetType(iter);

            if (type == LDObject || type == LDArray) {
                LDi_log(LD_LOG_ERROR, "schema error");

                LDJSONFree(attributeValue);

                return false;
            }
        }

        LDJSONFree(attributeValue);

        if (!(negate = LDObjectLookup(clause, "negate"))) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        if (LDJSONGetType(negate) != LDBool) {
            LDi_log(LD_LOG_ERROR, "schema error");

            return false;
        }

        *matches = LDGetBool(negate) ? false : true;

        return true;
    } else if (type != LDObject && type != LDArray) {

        /* match any */
        LDJSONFree(attributeValue);

        return true;
    } else {
        LDJSONFree(attributeValue);

        return false;
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
