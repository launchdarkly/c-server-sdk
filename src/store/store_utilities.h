#pragma once

#include "launchdarkly/api.h"
#include "store.h"

const char *
featureKindToString(const enum FeatureKind kind);

LDBoolean
stringToFeatureKind(const char* kind, enum FeatureKind *out_kind);

/* The LDJSON must contain a "key" field of type text. */
const char *
LDi_getDataKey(const struct LDJSON *const feature);

/* Key the "version" field from the LDJSON. If it is not present, or not a number, then return 0. */
unsigned int
LDi_getDataVersion(const struct LDJSON *const feature);

/* Verify that the LDJSON object is a valid flag or segment. */
LDBoolean
LDi_validateData(const struct LDJSON *const feature);

/* Check if the LDJSON is a deleted flag or segment. */
LDBoolean
LDi_isDataDeleted(const struct LDJSON *const feature);

/* Make an LDJSON object representing a deleted flag or segment. */
struct LDJSON *
LDi_makeDeletedData(const char *const key, const unsigned int version);
