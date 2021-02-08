#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <launchdarkly/api.h>

struct LDRedisConfig;

LD_EXPORT(struct LDRedisConfig *) LDRedisConfigNew();

LD_EXPORT(LDBoolean)
LDRedisConfigSetHost(
    struct LDRedisConfig *const config, const char *const host);

LD_EXPORT(LDBoolean)
LDRedisConfigSetPort(
    struct LDRedisConfig *const config, const unsigned short port);

LD_EXPORT(LDBoolean)
LDRedisConfigSetPrefix(
    struct LDRedisConfig *const config, const char *const prefix);

LD_EXPORT(LDBoolean)
LDRedisConfigSetPoolSize(
    struct LDRedisConfig *const config, const unsigned int poolSize);

LD_EXPORT(void) LDRedisConfigFree(struct LDRedisConfig *const config);

LD_EXPORT(struct LDStoreInterface *)
LDStoreInterfaceRedisNew(struct LDRedisConfig *const config);

#ifdef __cplusplus
}
#endif
