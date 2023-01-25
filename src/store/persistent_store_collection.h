#pragma once

#include "launchdarkly/api.h"

void
LDi_makeKindCollection(const char* kind, struct LDJSON *const items, struct LDStoreCollectionState* collection);

void
LDi_makeCollections(struct LDJSON *const sets, struct LDStoreCollectionState **collections, unsigned int *count);

void
LDi_freeCollections(struct LDStoreCollectionState *collections, unsigned int count);
