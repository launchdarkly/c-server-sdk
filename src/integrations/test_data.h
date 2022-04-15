#pragma once

#include <launchdarkly/boolean.h>
#include <launchdarkly/json.h>
#include <launchdarkly/store.h>
#include <launchdarkly/integrations/test_data.h>

#include "uthash.h"
#include "concurrency.h"

struct LDTestData {
    struct LDFlagBuilder* flagBuilders;
    struct LDJSON *currentFlags;
    struct LDTestDataInstance *instances;
    ld_rwlock_t lock;
};

struct LDTestDataInstance {
    struct LDTestData *testData;
    struct LDStore *store;
    struct LDTestDataInstance *next;
};

struct LDFlagBuilderUser {
    const char *userKey;
    struct LDFlagBuilderUser *next;
};

struct LDFlagBuilderTarget {
    int variation;
    struct LDFlagBuilderUser *users;
    UT_hash_handle hh;
};

struct LDFlagBuilder {
    const char *key;
    LDBoolean on;
    int fallthroughVariation;
    int offVariation;
    struct LDJSON *variations;
    struct LDFlagBuilderTarget *targets;
    struct LDFlagRuleBuilder *rules;
    UT_hash_handle hh;
};

struct LDFlagRuleBuilderClause {
    const char *attribute;
    struct LDJSON *values;
    LDBoolean negate;
    struct LDFlagRuleBuilderClause *next;
};

struct LDFlagRuleBuilder {
    int variation;
    struct LDFlagRuleBuilderClause *clauses;
    struct LDFlagRuleBuilder *next;
    struct LDFlagBuilder *flag;
};

LDBoolean
LDi_isBooleanFlag(struct LDFlagBuilder *flagBuilder);

void
LDFlagBuilderFree(struct LDFlagBuilder *flagBuilder);

struct LDJSON *
LDFlagBuilderBuild(struct LDFlagBuilder *flagBuilder, int version);


int
LDi_getPreviousFlagVersion(struct LDTestData *testData, struct LDFlagBuilder *flagBuilder);

struct LDJSON *
LDi_createNewFlag(struct LDTestData *testData, struct LDFlagBuilder *flagBuilder);

LDBoolean
LDi_updateCurrentFlagsMap(struct LDJSON *currentFlags, const char *key, struct LDJSON *newFlag);

void
LDi_updateFlagBuildersMap(struct LDTestData *testData, struct LDFlagBuilder *flagBuilder);

LDBoolean
LDi_notifyDataInstances(struct LDTestDataInstance *instances, struct LDJSON *newFlag);


struct LDJSON *
LDi_buildTargetsJSON(struct LDFlagBuilderTarget *targets);

struct LDJSON *
LDi_buildTargetJSON(struct LDFlagBuilderTarget *target);

struct LDJSON *
LDi_buildUsersJSON(struct LDFlagBuilderUser *users);

struct LDJSON *
LDi_buildRulesJSON(struct LDFlagRuleBuilder *rules);

struct LDJSON *
LDi_buildRuleJSON(struct LDFlagRuleBuilder *rule, const char *ruleId);

struct LDJSON *
LDi_buildClausesJSON(struct LDFlagRuleBuilderClause *clauses);

struct LDJSON *
LDi_buildClauseJSON(struct LDFlagRuleBuilderClause *clause);
