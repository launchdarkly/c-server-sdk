#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>

#include "lru.h"
#include "utility.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class LRUFixture : public CommonFixture {
};

TEST_F(LRUFixture, InsertExisting) {
    struct LDLRU *lru;

    ASSERT_TRUE(lru = LDLRUInit(10));

    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "abc"));
    ASSERT_EQ(LDLRUSTATUS_EXISTED, LDLRUInsert(lru, "abc"));

    LDLRUFree(lru);
}

TEST_F(LRUFixture, MaxCapacity) {
    struct LDLRU *lru;

    ASSERT_TRUE(lru = LDLRUInit(2));

    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "123"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "456"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "789"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "123"));
    ASSERT_EQ(LDLRUSTATUS_EXISTED, LDLRUInsert(lru, "789"));

    LDLRUFree(lru);
}

TEST_F(LRUFixture, AccessBumpsPosition) {
    struct LDLRU *lru;

    ASSERT_TRUE(lru = LDLRUInit(3));

    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "123"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "456"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "789"));
    ASSERT_EQ(LDLRUSTATUS_EXISTED, LDLRUInsert(lru, "123"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "ABC"));
    ASSERT_EQ(LDLRUSTATUS_EXISTED, LDLRUInsert(lru, "123"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "456"));

    LDLRUFree(lru);
}

TEST_F(LRUFixture, ZeroCapacityAlwaysNew) {
    struct LDLRU *lru;

    ASSERT_TRUE(lru = LDLRUInit(0));

    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "123"));
    ASSERT_EQ(LDLRUSTATUS_NEW, LDLRUInsert(lru, "123"));

    LDLRUFree(lru);
}
