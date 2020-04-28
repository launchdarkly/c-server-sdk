#pragma once

#include <stdbool.h>

#include <launchdarkly/api.h>

struct LDClient *makeTestClient();

struct LDClient *makeOfflineClient();
