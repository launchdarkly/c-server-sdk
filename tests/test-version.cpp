#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class VersionFixture : public CommonFixture {
};

TEST_F(VersionFixture, VersionNonEmpty) {
    ASSERT_TRUE(LDVersion());
    ASSERT_STREQ(LD_SDK_VERSION, LDVersion());
}
