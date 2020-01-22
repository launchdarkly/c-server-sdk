#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <launchdarkly/store/redis.h>

bool storeUpsertInternal(void *const contextRaw, const char *const kind,
    const struct LDStoreCollectionItem *const feature,
    const char *const featureKey, void (*const hook)());

#ifdef __cplusplus
}
#endif
