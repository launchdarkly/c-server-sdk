#include "ldinternal.h"
#include "ldschema.h"

/* **** Forward Declarations **** */

bool evaluate(const struct FeatureFlag *const flag, const struct LDUser *const user);

static bool ruleMatchesUser(const struct Rule *const rule, const struct LDUser *const user);

static bool clauseMatchesUser(const struct Clause *const clause, const struct LDUser *const user);

static bool segmentMatchesUser(const struct Segment *const segment, const struct LDUser *const user);

static bool segmentRuleMatchUser(const struct SegmentRule *const segmentRule, const struct LDUser *const user, const char *const salt);

static bool clauseMatchesUserNoSegments(const struct Clause *const clause, const struct LDUser *const user);

/* **** Implementations **** */

bool
evaluate(const struct FeatureFlag *const flag, const struct LDUser *const user)
{
    LD_ASSERT(flag); LD_ASSERT(user);

    if (!flag->on) {
        /* TODO return isOff */
    }

    /* Prerequisite */

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
            if (segmentRuleMatchUser(segmentRule, user, segment->salt)) {
                return true;
            }
        }

        return false;
    }
}

static bool
segmentRuleMatchUser(const struct SegmentRule *const segmentRule, const struct LDUser *const user, const char *const salt)
{
    const struct Clause *clause = NULL;

    LD_ASSERT(segmentRule); LD_ASSERT(user); LD_ASSERT(salt);

    for (clause = segmentRule->clauses; clause; clause = clause->hh.next) {
        if (!clauseMatchesUserNoSegments(clause, user)) {
            return false;
        }
    }

    if (!segmentRule->hasWeight) {
        return true;
    }

    /* TODO: add bucketing */

    return true;
}

static bool
clauseMatchesUserNoSegments(const struct Clause *const clause, const struct LDUser *const user)
{
    LD_ASSERT(clause); LD_ASSERT(user);

    /* TODO: body */

    return false;
}
