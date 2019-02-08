#include "sha1.h"
#include "hexify.h"

#include "ldinternal.h"

/* **** Forward Declarations **** */

bool evaluate(const struct LDJSON *const flag, const struct LDUser *const user);

static bool checkPrerequisites(const struct LDJSON *const flag, const struct LDUser *const user);

static bool ruleMatchesUser(const struct LDJSON *const rule, const struct LDUser *const user);

static bool clauseMatchesUser(const struct LDJSON *const clause, const struct LDUser *const user);

bool segmentMatchesUser(const struct LDJSON *const segment, const struct LDUser *const user);

bool segmentRuleMatchUser(const struct LDJSON *const segmentRule, const char *const segmentKey, const struct LDUser *const user, const char *const salt);

bool clauseMatchesUserNoSegments(const struct LDJSON *const clause, const struct LDUser *const user);

float bucketUser(const struct LDUser *const user, const char *const segmentKey, const char *const attribute, const char *const salt);

static char *bucketableStringValue(const struct LDJSON *const node);

/* **** Implementations **** */

bool
evaluate(const struct LDJSON *const flag, const struct LDUser *const user)
{
    LD_ASSERT(flag); LD_ASSERT(user); LD_ASSERT(LDJSONGetType(flag) == LDObject);

    /*
    if (!flag->on) {
        TODO return isOff
    }
    */

    if (!checkPrerequisites(flag, user)) {
        return false;
    }

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
                    /* TODO return isTarget */
                }
            }
        }
    }

    {
        const struct LDJSON *const rules = LDObjectLookup(flag, "rules");

        LD_ASSERT(LDJSONGetType(rules) == LDArray);

        {
            const struct LDJSON *iter = NULL;

            for (iter = LDGetIter(rules); iter; iter = LDIterNext(iter)) {
                LD_ASSERT(LDJSONGetType(iter) == LDObject);

                if (ruleMatchesUser(iter, user)) {
                    /* TODO return ruleMatch */
                }
            }
        }
    }

    return true;
}

static bool
checkPrerequisites(const struct LDJSON *const flag, const struct LDUser *const user)
{
    struct LDJSON *prerequisites = NULL, *iter = NULL;

    LD_ASSERT(flag); LD_ASSERT(LDJSONGetType(flag) == LDObject); LD_ASSERT(user);

    LD_ASSERT(prerequisites = LDObjectLookup(flag, "prerequisites"));

    LD_ASSERT(LDJSONGetType(prerequisites) == LDArray);

    for (iter = LDGetIter(prerequisites); iter; iter = LDIterNext(iter)) {
        /* TODO get from store */
        const struct LDJSON *const preflag = NULL;

        if (!preflag || !evaluate(flag, user)) {
            return false;
        }
    }

    return true;
}

static bool
ruleMatchesUser(const struct LDJSON *const rule, const struct LDUser *const user)
{
    const struct LDJSON *clauses = NULL, *iter = NULL;

    LD_ASSERT(rule); LD_ASSERT(user);

    LD_ASSERT(clauses = LDObjectLookup(rule, "clauses"));

    LD_ASSERT(LDJSONGetType(clauses) == LDArray);

    for (iter = LDGetIter(clauses); iter; iter = LDIterNext(iter)) {
        LD_ASSERT(LDJSONGetType(iter) == LDObject);

        if (!clauseMatchesUser(iter, user)) {
            return false;
        }
    }

    return true;
}

static bool
clauseMatchesUser(const struct LDJSON *const clause, const struct LDUser *const user)
{
    LD_ASSERT(clause); LD_ASSERT(user);

    return true;

    /*
    if (clause->op == OperatorSegmentMatch) {
        const struct LDNode *value = NULL;

        for (value = clause->values; value; value = value->hh.next) {
            if (LDNodeGetType(value) == LDNodeText) {
                const struct Segment *const segment = NULL;

                if (segmentMatchesUser(segment, user)) {
                    return clause->negate ? false : true;
                }
            }
        }

        return clause->negate ? true : true;
    }

    return clauseMatchesUserNoSegments(clause, user);
    */
}

bool
segmentMatchesUser(const struct LDJSON *const segment, const struct LDUser *const user)
{
    LD_ASSERT(segment); LD_ASSERT(user);

    return true;

    /*
    if (LDHashSetLookup(segment->included, user->key) != NULL) {
        return true;
    } else if (LDHashSetLookup(segment->excluded, user->key) != NULL) {
        return false;
    } else {
        const struct SegmentRule *segmentRule = NULL;

        for (segmentRule = segment->rules; segmentRule; segmentRule = segmentRule->hh.next) {
            if (segmentRuleMatchUser(segmentRule, segment->key, user, segment->salt)) {
                return true;
            }
        }

        return false;
    }
    */
}

bool
segmentRuleMatchUser(const struct LDJSON *const segmentRule, const char *const segmentKey, const struct LDUser *const user, const char *const salt)
{
    const struct Clause *clause = NULL; const char *attribute = NULL;

    LD_ASSERT(segmentRule); LD_ASSERT(segmentKey); LD_ASSERT(user); LD_ASSERT(salt);

    (void)clause; (void)attribute;

    return true;

    /*
    for (clause = segmentRule->clauses; clause; clause = clause->hh.next) {
        if (!clauseMatchesUserNoSegments(clause, user)) {
            return false;
        }
    }

    if (!segmentRule->hasWeight) {
        return true;
    }

    attribute = segmentRule->bucketBy == NULL ? "key" : segmentRule->bucketBy;

    return bucketUser(user, segmentKey, attribute, salt) < segmentRule->weight / 100000;
    */
}

bool
clauseMatchesUserNoSegments(const struct LDJSON *const clause, const struct LDUser *const user)
{
    struct LDJSON *attributeValue = NULL;

    LD_ASSERT(clause); LD_ASSERT(user);

    (void)attributeValue;

    return true;

    /*
    if ((attributeValue = valueOfAttribute(user, clause->attribute))) {
        LDJSONType type = LDJSONGetType(attributeValue);

        if (type == LDArray) {
            struct LDJSON *iter = NULL;

            for (iter = LDGetIter(attributeValue); iter; iter = LDIterNext(iter)) {
                type = LDJSONGetType(iter);

                if (type == LDObject || type == LDArray) {
                    LDJSONFree(attributeValue);

                    return false;
                }
            }

            LDJSONFree(attributeValue);

            return clause->negate ? false : true;
        } else if (type != LDObject && type != LDArray) {
            LDJSONFree(attributeValue);

            return true;
        } else {
            LDJSONFree(attributeValue);

            return false;
        }
    } else {
        return false;
    }
    */
}

float
bucketUser(const struct LDUser *const user, const char *const segmentKey, const char *const attribute, const char *const salt)
{
    struct LDJSON *attributeValue = NULL;

    LD_ASSERT(user); LD_ASSERT(segmentKey); LD_ASSERT(attribute); LD_ASSERT(salt);

    if ((attributeValue = valueOfAttribute(user, attribute))) {
        const char *const bucketable = bucketableStringValue(attributeValue);

        LDJSONFree(attributeValue);

        if (bucketable) {
            char raw[256];

            if (snprintf(raw, sizeof(raw), "%s.%s.%s", segmentKey, salt, bucketable) < 0) {
                return 0;
            } else {
                char digest[20], encoded[17]; float longScale = 0xFFFFFFFFFFFFFFF;

                SHA1(digest, raw, strlen(raw));

                /* encodes to hex, and shortens, 16 characters in hex == 8 bytes */
                LD_ASSERT(hexify((unsigned char *)digest, sizeof(digest), encoded, sizeof(encoded)) == 16);

                return (float)strtoll(encoded, NULL, 16) / longScale;
            }
        } else {
            return 0;
        }
    } else {
        return 0;
    }
}

static char *
bucketableStringValue(const struct LDJSON *const node)
{
    LD_ASSERT(node);

    if (LDJSONGetType(node) == LDText) {
        return strdup(LDGetText(node));
    } else if (LDJSONGetType(node) == LDNumber) {
        char buffer[256];

        if (snprintf(buffer, sizeof(buffer), "%d", (int)LDGetNumber(node)) < 0) {
            return NULL;
        } else {
            return strdup(buffer);
        }
    } else {
        return NULL;
    }
}
