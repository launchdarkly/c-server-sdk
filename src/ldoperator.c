#include "ldinternal.h"
#include "ldoperators.h"

#include "semver.h"

#include <time.h>
#include <pcre.h>
#include <math.h>
#include <gmp.h>

typedef bool (*StringOpFn)(const char *const uvalue, const char *const cvalue);

typedef bool (*NumberOpFn)(const float uvalue, const float cvalue);

#define CHECKSTRING(uvalue, cvalue) \
    if (LDJSONGetType(uvalue) != LDText || \
        LDJSONGetType(cvalue) != LDText) \
    { \
        return false; \
    }

#define CHECKNUMBER(uvalue, cvalue) \
    if (LDJSONGetType(uvalue) != LDNumber || \
        LDJSONGetType(cvalue) != LDNumber) \
    { \
        return false; \
    }

static bool
operatorInFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    if (LDJSONCompare(uvalue, cvalue)) {
        return true;
    }

    return false;
}

static bool
operatorStartsWithFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    size_t ulen, clen;

    CHECKSTRING(uvalue, cvalue);

    ulen = strlen(LDGetText(uvalue));
    clen = strlen(LDGetText(cvalue));

    if (clen > ulen) {
        return false;
    }

    return strncmp(LDGetText(uvalue), LDGetText(cvalue), clen) == 0;
}

static bool
operatorEndsWithFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    size_t ulen, clen;

    CHECKSTRING(uvalue, cvalue);

    ulen = strlen(LDGetText(uvalue));
    clen = strlen(LDGetText(cvalue));

    if (clen > ulen) {
        return false;
    }

    return strcmp(LDGetText(uvalue) + ulen - clen, LDGetText(cvalue)) == 0;
}

static bool
operatorMatchesFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    bool matches;
    pcre *context;
    const char *subject, *error, *regex;
    int errorOffset;

    CHECKSTRING(uvalue, cvalue);

    matches     = false;
    context     = NULL;
    subject     = NULL;
    error       = NULL;
    regex       = NULL;
    errorOffset = 0;

    LD_ASSERT(subject = LDGetText(uvalue));
    LD_ASSERT(regex = LDGetText(cvalue));

    context = pcre_compile(
        regex, PCRE_JAVASCRIPT_COMPAT, &error, &errorOffset, NULL);

    if (!context) {
        char msg[256];

        LD_ASSERT(snprintf(msg, sizeof(msg),
            "failed to compile regex '%s' got error '%s' with offset %d",
            regex, error, errorOffset) >= 0);

        LD_LOG(LD_LOG_ERROR, msg);

        return false;
    }

    matches = pcre_exec(
        context, NULL, subject, strlen(subject), 0, 0, NULL, 0) >= 0;

    pcre_free(context);

    return matches;
}

static bool
operatorContainsFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    CHECKSTRING(uvalue, cvalue);

    return strstr(LDGetText(uvalue), LDGetText(cvalue)) != NULL;
}

static bool
operatorLessThanFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) < LDGetNumber(cvalue);
}

static bool
operatorLessThanOrEqualFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) <= LDGetNumber(cvalue);
}

static bool
operatorGreaterThanFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) > LDGetNumber(cvalue);
}

static bool
operatorGreaterThanOrEqualFn(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    CHECKNUMBER(uvalue, cvalue);

    return LDGetNumber(uvalue) >= LDGetNumber(cvalue);
}

bool
LDi_parseTime(const struct LDJSON *const json, timestamp_t *result)
{
    LD_ASSERT(json);
    LD_ASSERT(result);

    if (LDJSONGetType(json) == LDNumber) {
        mpq_t num, sec, nsec, thousand;

        mpq_init(num);
        mpq_init(sec);
        mpq_init(nsec);
        mpq_init(thousand);

        mpq_set_d(num, LDGetNumber(json));
        mpq_set_ui(thousand, 1000, 1);
        mpq_div(num, num, thousand);

        result->sec = floor(mpq_get_d(num));

        mpq_set_ui(sec, result->sec, 1);
        mpq_sub(nsec, num, sec);
        mpq_mul(nsec, nsec, thousand);
        /* resolution hack */
        result->nsec = mpq_get_d(nsec);
        result->offset = 0;

        mpq_clear(num);
        mpq_clear(sec);
        mpq_clear(nsec);
        mpq_clear(thousand);

        return true;
    } else if (LDJSONGetType(json) == LDText) {
        const char *text;

        LD_ASSERT(text = LDGetText(json));

        if (timestamp_parse(text, strlen(text), result)) {
            LD_LOG(LD_LOG_ERROR, "failed to parse date uvalue");

            return false;
        }
        /* resolution hack */
        result->nsec /= 1000000;

        return true;
    }

    return false;
}

static bool
compareTime(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue, bool (*op) (double, double))
{
    timestamp_t ustamp;
    timestamp_t cstamp;

    if (LDi_parseTime(uvalue, &ustamp)) {
        if (LDi_parseTime(cvalue, &cstamp)) {
            return op(timestamp_compare(&ustamp, &cstamp), 0);
        }
    }

    return false;
}

static bool
fnLT(const double l, const double r)
{
    return l < r;
}

static bool
fnGT(const double l, const double r)
{
    return l > r;
}

static bool
operatorBefore(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    return compareTime(uvalue, cvalue, fnLT);
}

static bool
operatorAfter(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    return compareTime(uvalue, cvalue, fnGT);
}

static bool
compareSemVer(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue, int (*op)(semver_t, semver_t))
{
    bool result;
    semver_t usem, csem;

    CHECKSTRING(uvalue, cvalue);

    memset(&usem, 0, sizeof(usem));
    memset(&csem, 0, sizeof(csem));

    if (semver_parse(LDGetText(uvalue), &usem)) {
        LD_LOG(LD_LOG_ERROR, "failed to parse uvalue");

        return false;
    }

    if (semver_parse(LDGetText(cvalue), &csem)) {
        LD_LOG(LD_LOG_ERROR, "failed to parse cvalue");

        semver_free(&usem);

        return false;
    }

    result = op(usem, csem);

    semver_free(&usem);
    semver_free(&csem);

    return result;
}

static bool
operatorSemVerEqual(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    return compareSemVer(uvalue, cvalue, semver_eq);
}

static bool
operatorSemVerLessThan(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
{
    return compareSemVer(uvalue, cvalue, semver_lt);
}

static bool
operatorSemVerGreaterThan(const struct LDJSON *const uvalue,
    const struct LDJSON *const cvalue)
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
