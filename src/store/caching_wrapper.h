#pragma once
#include "launchdarkly/api.h"
#include "internal_store.h"

/*
 * The caching store wrapper provides an implementation of LDStoreInterface that provides caching.
 * It is constructed with a reference to a non-caching persistent store implementation such as the redis
 * store integration.
 */

struct LDInternalStoreInterface *
LDStoreCachingWrapperNew(struct LDStoreInterface *persistentStore, unsigned int cacheMilliseconds);
