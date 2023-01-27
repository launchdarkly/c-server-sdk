
#include <string.h>
#include "launchdarkly/api.h"

#include "assertion.h"
#include "store_utilities.h"
#include "store.h"

static const char *const LD_SS_FEATURES   = "features";
static const char *const LD_SS_SEGMENTS   = "segments";

const char *
featureKindToString(const enum FeatureKind kind)
{
    switch (kind) {
        case LD_FLAG:
            return LD_SS_FEATURES;
        case LD_SEGMENT:
            return LD_SS_SEGMENTS;
        default:
            LD_LOG(LD_LOG_FATAL, "invalid feature kind");

            LD_ASSERT(LDBooleanFalse);
    }
}

LDBoolean
stringToFeatureKind(const char* kind, enum FeatureKind *out_kind) {

    if(strcmp(kind, LD_SS_FEATURES) == 0) {
        *out_kind = LD_FLAG;
        return LDBooleanTrue;
    }
    if(strcmp(kind, LD_SS_SEGMENTS) == 0) {
        *out_kind = LD_SEGMENT;
        return LDBooleanTrue;
    }

    return LDBooleanFalse;
}

const char *
LDi_getDataKey(const struct LDJSON *const feature)
{
    struct LDJSON *tmp;
    LD_ASSERT(feature);

    tmp = LDObjectLookup(feature, "key");
    LD_ASSERT(tmp);

    return LDGetText(tmp);
}

unsigned int
LDi_getDataVersion(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if (!(tmp = LDObjectLookup(feature, "version"))) {
        LD_LOG(LD_LOG_ERROR, "feature missing version");

        return 0;
    }

    if (LDJSONGetType(tmp) != LDNumber) {
        LD_LOG(LD_LOG_ERROR, "feature version is not a number");

        return 0;
    }

    return LDGetNumber(tmp);
}

LDBoolean
LDi_validateData(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if (LDJSONGetType(feature) != LDObject) {
        LD_LOG(LD_LOG_ERROR, "feature is not an object");

        return LDBooleanFalse;
    }

    if (!(tmp = LDObjectLookup(feature, "version"))) {
        LD_LOG(LD_LOG_ERROR, "feature missing version");

        return LDBooleanFalse;
    }

    if (LDJSONGetType(tmp) != LDNumber) {
        LD_LOG(LD_LOG_ERROR, "feature version is not a number");

        return LDBooleanFalse;
    }

    if (!(tmp = LDObjectLookup(feature, "key"))) {
        LD_LOG(LD_LOG_ERROR, "feature missing key");

        return LDBooleanFalse;
    }

    if (LDJSONGetType(tmp) != LDText) {
        LD_LOG(LD_LOG_ERROR, "feature key is not a string");

        return LDBooleanFalse;
    }

    if ((tmp = LDObjectLookup(feature, "deleted"))) {
        if (LDJSONGetType(tmp) != LDBool) {
            LD_LOG(LD_LOG_ERROR, "feature deleted field is not boolean");

            return LDBooleanFalse;
        }
    }

    return LDBooleanTrue;
}

LDBoolean
LDi_isDataDeleted(const struct LDJSON *const feature)
{
    const struct LDJSON *tmp;

    LD_ASSERT(feature);

    if ((tmp = LDObjectLookup(feature, "deleted"))) {
        if (LDJSONGetType(tmp) != LDBool) {
            LD_LOG(LD_LOG_ERROR, "feature deletion status is not boolean");

            return LDBooleanTrue;
        }

        return LDGetBool(tmp);
    }

    return LDBooleanFalse;
}

struct LDJSON *
LDi_makeDeletedData(const char *const key, const unsigned int version)
{
    struct LDJSON *placeholder, *tmp;

    if (!(placeholder = LDNewObject())) {
        return NULL;
    }

    if (!(tmp = LDNewText(key))) {
        LDFree(placeholder);

        return NULL;
    }

    if (!LDObjectSetKey(placeholder, "key", tmp)) {
        LDFree(placeholder);
        LDFree(tmp);

        return NULL;
    }

    if (!(tmp = LDNewNumber(version))) {
        LDFree(placeholder);

        return NULL;
    }

    if (!LDObjectSetKey(placeholder, "version", tmp)) {
        LDFree(placeholder);
        LDFree(tmp);

        return NULL;
    }

    if (!(tmp = LDNewBool(LDBooleanTrue))) {
        LDFree(placeholder);

        return NULL;
    }

    if (!LDObjectSetKey(placeholder, "deleted", tmp)) {
        LDFree(placeholder);
        LDFree(tmp);

        return NULL;
    }

    return placeholder;
}
