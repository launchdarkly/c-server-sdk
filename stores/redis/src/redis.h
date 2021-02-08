#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <hiredis/hiredis.h>

#include <launchdarkly/boolean.h>
#include <launchdarkly/store/redis.h>

LDBoolean
storeUpsertInternal(
    void *const                               contextRaw,
    const char *const                         kind,
    const struct LDStoreCollectionItem *const feature,
    const char *const                         featureKey,
    void (*const hook)());

#ifdef __cplusplus
}
#endif
