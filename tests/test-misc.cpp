#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>

#include "utility.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class MiscFixture : public CommonFixture {
};

TEST_F(MiscFixture, GenerateUUIDv4) {
    char buffer[LD_UUID_SIZE];
    /* This is mainly called as something for Valgrind to analyze */
    ASSERT_TRUE(LDi_UUIDv4(buffer));
}
