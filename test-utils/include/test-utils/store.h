#pragma once

#include <launchdarkly/api.h>

#include <store.h>

void runSharedStoreTests(struct LDStore *(*const prepareEmptyStore)());
