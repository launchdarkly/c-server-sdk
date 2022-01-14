#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <string.h>

#include <launchdarkly/api.h>

#include "utility.h"

#include "json_internal_helpers.h"

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

TEST_F(JSONFixture, ObjectSetString) {
    struct LDJSON* object;

    ASSERT_TRUE(object = LDNewObject());

    ASSERT_TRUE(LDObjectSetString(object, "key1", "value1"));
    ASSERT_STREQ("value1", LDGetText(LDObjectLookup(object, "key1")));

    ASSERT_TRUE(LDObjectSetString(object, "key1", "value2"));
    ASSERT_STREQ("value2", LDGetText(LDObjectLookup(object, "key1")));

    LDJSONFree(object);
}

TEST_F(JSONFixture, ObjectSetBool) {
    struct LDJSON* object;

    ASSERT_TRUE(object = LDNewObject());

    ASSERT_TRUE(LDObjectSetBool(object, "key1", LDBooleanTrue));
    ASSERT_TRUE(LDGetBool(LDObjectLookup(object, "key1")));

    ASSERT_TRUE(LDObjectSetBool(object, "key1", LDBooleanFalse));
    ASSERT_FALSE(LDGetBool(LDObjectLookup(object, "key1")));

    LDJSONFree(object);
}

TEST_F(JSONFixture, ObjectSetNumber) {
    struct LDJSON* object;

    ASSERT_TRUE(object = LDNewObject());

    ASSERT_TRUE(LDObjectSetNumber(object, "key1", 10));
    ASSERT_EQ(10, LDGetNumber(LDObjectLookup(object, "key1")));

    ASSERT_TRUE(LDObjectSetNumber(object, "key1", 20));
    ASSERT_EQ(20, LDGetNumber(LDObjectLookup(object, "key1")));

    LDJSONFree(object);
}

TEST_F(JSONFixture, ObjectNewChild) {
    struct LDJSON* object;
    struct LDJSON* child1;

    ASSERT_TRUE(object = LDNewObject());

    ASSERT_TRUE(child1 = LDObjectNewChild(object, "child1"));
    ASSERT_TRUE(LDObjectSetString(child1, "child2", "value"));

    ASSERT_STREQ("value", LDGetText(LDObjectLookup(LDObjectLookup(object, "child1"), "child2")));

    LDJSONFree(object);
}


TEST_F(JSONFixture, ObjectSetReference) {
    struct LDJSON* object;
    struct LDJSON* ref;

    ASSERT_TRUE(object = LDNewObject());
    ASSERT_TRUE(ref = LDNewText("ref"));

    ASSERT_TRUE(LDObjectSetReference(object, "ref", ref));
    ASSERT_STREQ("ref", LDGetText(LDObjectLookup(object, "ref")));

    LDJSONFree(object);

    // The idea is that LDJSONFree should not have freed ref; therefore we should be able
    // to access ref as usual. This isn't the most reliable testing technique, since free could
    // just as well leave the memory intact.

    ASSERT_STREQ("ref", LDGetText(ref));

    LDJSONFree(ref);
}

TEST_F(JSONFixture, ObjectSetDefensiveChecks) {
    struct LDJSON* object;
    struct LDJSON* notObject;

    ASSERT_TRUE(object = LDNewObject());
    ASSERT_TRUE(notObject = LDNewText("not"));;

    ASSERT_FALSE(LDObjectSetString(nullptr, "key", "value"));
    ASSERT_FALSE(LDObjectSetString(object, nullptr, "value"));
    ASSERT_FALSE(LDObjectSetString(object, "key", nullptr));
    ASSERT_FALSE(LDObjectSetString(notObject, "key", "value"));

    ASSERT_FALSE(LDObjectSetNumber(nullptr, "key", 1));
    ASSERT_FALSE(LDObjectSetNumber(object, nullptr, 1));
    ASSERT_FALSE(LDObjectSetNumber(notObject, "key", 1));

    ASSERT_FALSE(LDObjectSetBool(nullptr, "key", LDBooleanTrue));
    ASSERT_FALSE(LDObjectSetBool(object, nullptr, LDBooleanTrue));
    ASSERT_FALSE(LDObjectSetBool(notObject, "key", LDBooleanTrue));

    ASSERT_FALSE(LDObjectNewChild(nullptr, "key"));
    ASSERT_FALSE(LDObjectNewChild(object, nullptr));
    ASSERT_FALSE(LDObjectNewChild(notObject, "key"));

    ASSERT_FALSE(LDObjectSetReference(nullptr, "key", notObject));
    ASSERT_FALSE(LDObjectSetReference(object, nullptr, notObject));
    ASSERT_FALSE(LDObjectSetReference(object, "key", nullptr));
    ASSERT_FALSE(LDObjectSetReference(notObject, "key", notObject));

    LDJSONFree(object);
    LDJSONFree(notObject);
}
