#pragma once
#include "launchdarkly/api.h"
#include "internal_store.h"

/*
 * Implementation of an in-memory feature store.
 */

struct LDInternalStoreInterface *
LDStoreMemoryNew(void);
