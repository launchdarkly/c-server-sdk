#include <math.h>
#include <pcre.h>
#include <string.h>
#include <time.h>

#include "semver.h"

#include <launchdarkly/api.h>

#include "assertion.h"
#include "operators.h"
#include "utility.h"

#define CHECKSTRING(uvalue, cvalue)                                            \
    if (LDJSONGetType(uvalue) != LDText || LDJSONGetType(cvalue) != LDText) {  \
        return LDBooleanFalse;                                                 \
    }

#define CHECKNUMBER(uvalue, cvalue)                                            \
    if (LDJSONGetType(uvalue) != LDNumber ||                                   \
        LDJSONGetType(cvalue) != LDNumber) {                                   \
        return LDBooleanFalse;                                                 \
    }

static LDBoolean
operatorInFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    return LDJSONCompare(uvalue, cvalue);
}

static LDBoolean
operatorStartsWithFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    size_t ulen, clen;

    CHECKSTRING(uvalue, cvalue);

    ulen = strlen(LDGetText(uvalue));
    clen = strlen(LDGetText(cvalue));

    if (clen > ulen) {
        return LDBooleanFalse;
    }

    return strncmp(LDGetText(uvalue), LDGetText(cvalue), clen) == 0;
}

static LDBoolean
operatorEndsWithFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    size_t ulen, clen;

    CHECKSTRING(uvalue, cvalue);

    ulen = strlen(LDGetText(uvalue));
    clen = strlen(LDGetText(cvalue));

    if (clen > ulen) {
        return LDBooleanFalse;
    }

    return strcmp(LDGetText(uvalue) + ulen - clen, LDGetText(cvalue)) == 0;
}

static LDBoolean
operatorMatchesFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    LDBoolean   matches;
    pcre *      context;
    const char *subject, *error, *regex;
    int         errorOffset;

    CHECKSTRING(uvalue, cvalue);

    matches     = LDBooleanFalse;
    context     = NULL;
    subject     = NULL;
    error       = NULL;
    regex       = NULL;
    errorOffset = 0;

    subject = LDGetText(uvalue);
    LD_ASSERT(subject);
    regex = LDGetText(cvalue);
    LD_ASSERT(regex);

    context =
        pcre_compile(regex, PCRE_JAVASCRIPT_COMPAT, &error, &errorOffset, NULL);

    if (!context) {
        LD_LOG_3(
            LD_LOG_ERROR,
            "failed to compile regex '%s' got error '%s' with offset %d",
            regex,
            error,
            errorOffset);

        return LDBooleanFalse;
    }

    matches =
        pcre_exec(context, NULL, subject, strlen(subject), 0, 0, NULL, 0) >= 0;

    pcre_free(context);

    return matches;
}

static LDBoolean
operatorContainsFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    CHECKSTRING(uvalue, cvalue);

    return strstr(LDGetText(uvalue), LDGetText(cvalue)) != NULL;
}

static LDBoolean
operatorLessThanFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) < LDGetNumber(cvalue);
}

static LDBoolean
operatorLessThanOrEqualFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) <= LDGetNumber(cvalue);
}

static LDBoolean
operatorGreaterThanFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) > LDGetNumber(cvalue);
}

static LDBoolean
operatorGreaterThanOrEqualFn(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) >= LDGetNumber(cvalue);
}

static double
floorAtMagnitude(const double n, const unsigned int magnitude)
{
    return n - fmod(n, magnitude);
}

LDBoolean
LDi_parseTime(const struct LDJSON *const json, timestamp_t *result)
{
    LD_ASSERT(json);
    LD_ASSERT(result);

    if (LDJSONGetType(json) == LDNumber) {
        const double original = LDGetNumber(json);
        const double rounded  = floorAtMagnitude(original, 1000);

        result->sec    = rounded / 1000;
        result->nsec   = (original - rounded) * 1000000;
        result->offset = 0;

        return LDBooleanTrue;
    } else if (LDJSONGetType(json) == LDText) {
        const char *text;

        text = LDGetText(json);
        LD_ASSERT(text);

        if (timestamp_parse(text, strlen(text), result)) {
            LD_LOG(LD_LOG_ERROR, "failed to parse date uvalue");

            return LDBooleanFalse;
        }

        return LDBooleanTrue;
    }

    return LDBooleanFalse;
}

static LDBoolean
compareTime(
    const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue,
    LDBoolean (*op)(double, double))
{
    timestamp_t ustamp;
    timestamp_t cstamp;

    if (LDi_parseTime(uvalue, &ustamp)) {
        if (LDi_parseTime(cvalue, &cstamp)) {
            return op(timestamp_compare(&ustamp, &cstamp), 0);
        }
    }

    return LDBooleanFalse;
}

static LDBoolean
fnLT(const double l, const double r)
{
    return l < r;
}

static LDBoolean
fnGT(const double l, const double r)
{
    return l > r;
}

static LDBoolean
operatorBefore(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    return compareTime(uvalue, cvalue, fnLT);
}

static LDBoolean
operatorAfter(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    return compareTime(uvalue, cvalue, fnGT);
}

static LDBoolean
compareSemVer(
    const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue,
    int (*op)(semver_t, semver_t))
{
    LDBoolean result;
    semver_t  usem, csem;

    CHECKSTRING(uvalue, cvalue);

    memset(&usem, 0, sizeof(usem));
    memset(&csem, 0, sizeof(csem));

    if (semver_parse(LDGetText(uvalue), &usem)) {
        LD_LOG(LD_LOG_ERROR, "failed to parse uvalue");

        return LDBooleanFalse;
    }

    if (semver_parse(LDGetText(cvalue), &csem)) {
        LD_LOG(LD_LOG_ERROR, "failed to parse cvalue");

        semver_free(&usem);

        return LDBooleanFalse;
    }

    result = op(usem, csem);

    semver_free(&usem);
    semver_free(&csem);

    return result;
}

static LDBoolean
operatorSemVerEqual(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    return compareSemVer(uvalue, cvalue, semver_eq);
}

static LDBoolean
operatorSemVerLessThan(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    return compareSemVer(uvalue, cvalue, semver_lt);
}

static LDBoolean
operatorSemVerGreaterThan(
    const struct LDJSON *const uvalue, const struct LDJSON *const cvalue)
{
    return compareSemVer(uvalue, cvalue, semver_gt);
}

OpFn
LDi_lookupOperation(const char *const operation)
{
    LD_ASSERT(operation);

    if (strcmp(operation, "in") == 0) {
        return operatorInFn;
    } else if (strcmp(operation, "endsWith") == 0) {
        return operatorEndsWithFn;
    } else if (strcmp(operation, "startsWith") == 0) {
        return operatorStartsWithFn;
    } else if (strcmp(operation, "matches") == 0) {
        return operatorMatchesFn;
    } else if (strcmp(operation, "contains") == 0) {
        return operatorContainsFn;
    } else if (strcmp(operation, "lessThan") == 0) {
        return operatorLessThanFn;
    } else if (strcmp(operation, "lessThanOrEqual") == 0) {
        return operatorLessThanOrEqualFn;
    } else if (strcmp(operation, "greaterThan") == 0) {
        return operatorGreaterThanFn;
    } else if (strcmp(operation, "greaterThanOrEqual") == 0) {
        return operatorGreaterThanOrEqualFn;
    } else if (strcmp(operation, "before") == 0) {
        return operatorBefore;
    } else if (strcmp(operation, "after") == 0) {
        return operatorAfter;
    } else if (strcmp(operation, "semVerEqual") == 0) {
        return operatorSemVerEqual;
    } else if (strcmp(operation, "semVerLessThan") == 0) {
        return operatorSemVerLessThan;
    } else if (strcmp(operation, "semVerGreaterThan") == 0) {
        return operatorSemVerGreaterThan;
    }

    return NULL;
}
