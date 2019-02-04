#include "sha1.h"
#include "hexify.h"

#include "ldinternal.h"
#include "ldschema.h"

/* **** Forward Declarations **** */

bool evaluate(const struct FeatureFlag *const flag, const struct LDUser *const user);

static bool checkPrerequisites(const struct FeatureFlag *const flag, const struct LDUser *const user);

static bool ruleMatchesUser(const struct Rule *const rule, const struct LDUser *const user);

static bool clauseMatchesUser(const struct Clause *const clause, const struct LDUser *const user);

static bool segmentMatchesUser(const struct Segment *const segment, const struct LDUser *const user);

static bool segmentRuleMatchUser(const struct SegmentRule *const segmentRule, const char *const segmentKey, const struct LDUser *const user, const char *const salt);

static bool clauseMatchesUserNoSegments(const struct Clause *const clause, const struct LDUser *const user);

static float bucketUser(const struct LDUser *const user, const char *const segmentKey, const char *const attribute, const char *const salt);

static char *bucketableStringValue(const struct LDNode *const node);

/* **** Implementations **** */

bool
evaluate(const struct FeatureFlag *const flag, const struct LDUser *const user)
{
    LD_ASSERT(flag); LD_ASSERT(user);

    if (!flag->on) {
        /* TODO return isOff */
    }

    if (!checkPrerequisites(flag, user)) {
        return false;
    }

    if (flag->targets) {
        struct Target *target = NULL;

        for (target = flag->targets; target; target++) {
            if (LDHashSetLookup(target->values, user->key) != NULL) {
                /* TODO return isTarget */
            }
        }
    }

    if (flag->rules) {
        const struct Rule *rule = NULL;

        for (rule = flag->rules; rule; rule = rule->hh.next) {
            if (ruleMatchesUser(rule, user)) {
                /* TODO return ruleMatch */
            }
        }
    }

    return true;
}

static bool
checkPrerequisites(const struct FeatureFlag *const flag, const struct LDUser *const user)
{
    struct Prerequisite *prerequisite = NULL;

    LD_ASSERT(flag); LD_ASSERT(user);

    for (prerequisite = flag->prerequisites; prerequisite; prerequisite = prerequisite->hh.next) {
        /* TODO get from store */
        const struct FeatureFlag *const preflag = NULL;

        if (!preflag || !evaluate(flag, user)) {
            return false;
        }
    }

    return true;
}

static bool
ruleMatchesUser(const struct Rule *const rule, const struct LDUser *const user)
{
    const struct Clause *clause = NULL;

    LD_ASSERT(rule); LD_ASSERT(user);

    for (clause = rule->clauses; clause; clause = clause->hh.next) {
        if (!clauseMatchesUser(clause, user)) {
            return false;
        }
    }

    return true;
}

static bool
clauseMatchesUser(const struct Clause *const clause, const struct LDUser *const user)
{
    LD_ASSERT(clause); LD_ASSERT(user);

    if (clause->op == OperatorSegmentMatch) {
        const struct LDNode *value = NULL;

        for (value = clause->values; value; value = value->hh.next) {
            if (LDNodeGetType(value) == LDNodeText) {
                /* TODO get from store */
                const struct Segment *const segment = NULL;

                if (segmentMatchesUser(segment, user)) {
                    return clause->negate ? false : true;
                }
            }
        }

        return clause->negate ? true : true;
    }

    return clauseMatchesUserNoSegments(clause, user);
}

static bool
segmentMatchesUser(const struct Segment *const segment, const struct LDUser *const user)
{
    LD_ASSERT(segment); LD_ASSERT(user);

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
}

static bool
segmentRuleMatchUser(const struct SegmentRule *const segmentRule, const char *const segmentKey, const struct LDUser *const user, const char *const salt)
{
    const struct Clause *clause = NULL; const char *attribute = NULL;

    LD_ASSERT(segmentRule); LD_ASSERT(segmentKey); LD_ASSERT(user); LD_ASSERT(salt);

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
}

static bool
clauseMatchesUserNoSegments(const struct Clause *const clause, const struct LDUser *const user)
{
    struct LDNode *attributeValue = NULL;

    LD_ASSERT(clause); LD_ASSERT(user);

    if ((attributeValue = valueOfAttribute(user, clause->attribute))) {
        LDNodeType type = LDNodeGetType(attributeValue);

        if (type == LDNodeArray) {
            const struct LDNode *iter = NULL;

            for (iter = LDNodeArrayGetIterator(attributeValue); iter; iter = LDNodeAdvanceIterator(iter)) {
                type = LDNodeGetType(iter);

                if (type == LDNodeObject || type == LDNodeArray) {
                    LDNodeFree(attributeValue);

                    return false;
                }
            }

            LDNodeFree(attributeValue);

            return clause->negate ? false : true;
        } else if (type != LDNodeObject && type != LDNodeArray) {
            LDNodeFree(attributeValue);

            return true;
        } else {
            LDNodeFree(attributeValue);

            return false;
        }
    } else {
        return false;
    }
}

static float
bucketUser(const struct LDUser *const user, const char *const segmentKey, const char *const attribute, const char *const salt)
{
    struct LDNode *attributeValue = NULL;

    LD_ASSERT(user); LD_ASSERT(segmentKey); LD_ASSERT(attribute); LD_ASSERT(salt);

    if ((attributeValue = valueOfAttribute(user, attribute))) {
        const char *const bucketable = bucketableStringValue(attributeValue);

        LDNodeFree(attributeValue);

        if (bucketable) {
            char raw[256];

            if (snprintf(raw, sizeof(raw), "%s.%s.%s", segmentKey, salt, bucketable) < 0) {
                return 0;
            } else {
                char digest[20], encoded[17]; float longScale = 0xFFFFFFFFFFFFFFF;

                SHA1(digest, raw, strlen(raw));

                /* encodes to hex, and shortens, 16 characters in hex == 8 bytes */
                LD_ASSERT(hexify((unsigned char *)digest, sizeof(digest), encoded, sizeof(encoded)) == 16);

                /* TODO: check for size error (4 bytes only) */

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
bucketableStringValue(const struct LDNode *const node)
{
    LD_ASSERT(node);

    if (LDNodeGetType(node) == LDNodeText) {
        return strdup(LDNodeGetText(node));
    } else if (LDNodeGetType(node) == LDNodeNumber) {
        char buffer[256];

        if (snprintf(buffer, sizeof(buffer), "%d", (int)LDNodeGetNumber(node)) < 0) {
            return NULL;
        } else {
            return strdup(buffer);
        }
    } else {
        return NULL;
    }
}
