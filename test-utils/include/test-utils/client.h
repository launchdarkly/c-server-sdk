#pragma once

#include <launchdarkly/api.h>

struct LDClient *
makeTestClient();

struct LDClient *
makeOfflineClient();
