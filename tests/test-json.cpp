#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <string.h>

#include <launchdarkly/api.h>

#include "utility.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class JSONFixture : public CommonFixture {
};

TEST_F(JSONFixture, Null) {
    struct LDJSON *json = LDNewNull();

    ASSERT_TRUE(json);

    ASSERT_EQ(LDJSONGetType(json), LDNull);

    LDJSONFree(json);
}

TEST_F(JSONFixture, Bool) {
    const LDBoolean value = LDBooleanTrue;

    struct LDJSON *json = LDNewBool(value);

    ASSERT_TRUE(json);

    ASSERT_EQ(LDJSONGetType(json), LDBool);

    ASSERT_EQ(LDGetBool(json), value);

    LDJSONFree(json);
}

TEST_F(JSONFixture, Number) {
    const double value = 3.33;

    struct LDJSON *json = LDNewNumber(value);

    ASSERT_TRUE(json);

    ASSERT_EQ(LDJSONGetType(json), LDNumber);

    ASSERT_EQ(LDGetNumber(json), value);

    LDJSONFree(json);
}

TEST_F(JSONFixture, Text) {
    const char *const value = "hello world!";

    struct LDJSON *json = LDNewText(value);

    ASSERT_TRUE(json);

    ASSERT_EQ(LDJSONGetType(json), LDText);

    ASSERT_STREQ(LDGetText(json), value);

    LDJSONFree(json);
}

TEST_F(JSONFixture, Array) {
    struct LDJSON *json, *tmp, *iter;

    ASSERT_TRUE(json = LDNewArray());

    ASSERT_TRUE(tmp = LDNewBool(LDBooleanTrue));
    ASSERT_TRUE(LDArrayPush(json, tmp));

    ASSERT_TRUE(tmp = LDNewBool(LDBooleanFalse));
    ASSERT_TRUE(LDArrayPush(json, tmp));

    ASSERT_TRUE(iter = LDGetIter(json));
    ASSERT_TRUE(LDGetBool(iter));

    ASSERT_TRUE(iter = LDIterNext(iter));
    ASSERT_FALSE(LDGetBool(iter));

    ASSERT_EQ(LDCollectionGetSize(json), 2);

    LDJSONFree(json);
}

TEST_F(JSONFixture, Object) {
    struct LDJSON *json, *tmp, *iter;

    ASSERT_TRUE(json = LDNewObject());

    ASSERT_TRUE(tmp = LDNewBool(LDBooleanTrue));
    ASSERT_TRUE(LDObjectSetKey(json, "a", tmp));

    ASSERT_TRUE(tmp = LDNewBool(LDBooleanFalse));
    ASSERT_TRUE(LDObjectSetKey(json, "b", tmp));

    ASSERT_TRUE(iter = LDGetIter(json));
    ASSERT_STREQ(LDIterKey(iter), "a");
    ASSERT_TRUE(LDGetBool(iter) == LDBooleanTrue);

    ASSERT_TRUE(iter = LDIterNext(iter));
    ASSERT_STREQ(LDIterKey(iter), "b");
    ASSERT_TRUE(LDGetBool(iter) == LDBooleanFalse);

    ASSERT_TRUE(tmp = LDObjectLookup(json, "b"));
    ASSERT_FALSE(LDGetBool(tmp));

    ASSERT_TRUE(tmp = LDObjectLookup(json, "a"));
    ASSERT_TRUE(LDGetBool(tmp));

    LDJSONFree(json);
}

TEST_F(JSONFixture, Merge) {
    struct LDJSON *left, *right;

    ASSERT_TRUE(left = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(left, "a", LDNewNumber(1)));

    ASSERT_TRUE(right = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(right, "b", LDNewNumber(2)));
    ASSERT_TRUE(LDObjectSetKey(right, "c", LDNewNumber(3)));

    ASSERT_TRUE(LDObjectMerge(left, right));

    ASSERT_EQ(LDGetNumber(LDObjectLookup(left, "a")), 1);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(left, "b")), 2);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(left, "c")), 3);

    LDJSONFree(left);
    LDJSONFree(right);
}

TEST_F(JSONFixture, Append) {
    struct LDJSON *iter, *left, *right;

    ASSERT_TRUE(left = LDNewArray());
    ASSERT_TRUE(LDArrayPush(left, LDNewNumber(1)));

    ASSERT_TRUE(right = LDNewArray());
    ASSERT_TRUE(LDArrayPush(right, LDNewNumber(2)));
    ASSERT_TRUE(LDArrayPush(right, LDNewNumber(3)));

    ASSERT_TRUE(LDArrayAppend(left, right));

    ASSERT_EQ(LDGetNumber(iter = LDGetIter(left)), 1);
    ASSERT_EQ(LDGetNumber(iter = LDIterNext(iter)), 2);
    ASSERT_EQ(LDGetNumber(LDIterNext(iter)), 3);

    LDJSONFree(left);
    LDJSONFree(right);
}
