#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <hiredis/hiredis.h>

#include <launchdarkly/api.h>

struct LDRedisConfig;

LD_EXPORT(struct LDRedisConfig *) LDRedisConfigNew();

LD_EXPORT(bool) LDRedisConfigSetHost(struct LDRedisConfig *const config,
    const char *const host);

LD_EXPORT(bool) LDRedisConfigSetPort(struct LDRedisConfig *const config,
    const uint16_t port);

LD_EXPORT(bool) LDRedisConfigSetPrefix(struct LDRedisConfig *const config,
    const char *const prefix);

LD_EXPORT(bool) LDRedisConfigSetPoolSize(struct LDRedisConfig *const config,
    const unsigned int poolSize);

LD_EXPORT(void) LDRedisConfigFree(struct LDRedisConfig *const config);

LD_EXPORT(struct LDStore *) LDMakeRedisStore(
    struct LDRedisConfig *const config);

#ifdef __cplusplus
}
#endif
