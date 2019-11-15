#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <launchdarkly/store/redis.h>

bool storeUpsertInternal(void *const contextRaw, const char *const kind,
    struct LDJSON *const feature, void (*const hook)());

#ifdef __cplusplus
}
#endif
