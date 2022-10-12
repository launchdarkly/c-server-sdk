#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include "ldvalue.h"
#include <launchdarkly/memory.h>
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class LDValueFixture : public CommonFixture {
};

// Requires valgrind leak checker
TEST_F(LDValueFixture, ObjectIsFreed) {
    struct LDObject *obj = LDObject_New();
    LDObject_Free(obj);
}

// Requires valgrind leak checker
TEST_F(LDValueFixture, ArrayIsFreed) {
    struct LDArray *array = LDArray_New();
    LDArray_Free(array);
}

// Requires valgrind leak checker
TEST_F(LDValueFixture, ObjectToValueIsFreed) {
    LDValue_Free(LDValue_Object(LDObject_New()));
}

// Requires valgrind leak checker
TEST_F(LDValueFixture, ArrayToValueIsFreed) {
    LDValue_Free(LDValue_Array(LDArray_New()));
}

// Requires valgrind leak checker
TEST_F(LDValueFixture, PrimitivesAreFreed) {
    struct LDValue *boolVal = LDValue_True();
    struct LDValue *constantStringVal = LDValue_ConstantString("hello");
    struct LDValue *ownedStringVal = LDValue_OwnedString("goodbye");
    struct LDValue *numVal = LDValue_Number(12);
    struct LDValue *nullVal = LDValue_Null();

    LDValue_Free(boolVal);
    LDValue_Free(constantStringVal);
    LDValue_Free(ownedStringVal);
    LDValue_Free(numVal);
    LDValue_Free(nullVal);
}

// Requires valgrind leak checker
TEST_F(LDValueFixture, ObjectOwnedKeyIsFreed) {
    struct LDObject *obj = LDObject_New();
    LDObject_AddOwnedKey(obj, "key", LDValue_True());

    struct LDValue *objVal = LDObject_Build(obj);
    LDValue_Free(objVal);

    LDObject_Free(obj);
}

TEST_F(LDValueFixture, NullPointerHasInvalidType) {
    ASSERT_EQ(LDValue_Type(nullptr), LDValueType_Unrecognized);
}

TEST_F(LDValueFixture, PrimitivesHaveCorrectType) {
    struct LDValue *boolVal = LDValue_True();
    struct LDValue *constantStringVal = LDValue_ConstantString("hello");
    struct LDValue *ownedStringVal = LDValue_OwnedString("goodbye");
    struct LDValue *numVal = LDValue_Number(12);
    struct LDValue *nullVal = LDValue_Null();

    ASSERT_EQ(LDValue_Type(boolVal), LDValueType_Bool);
    ASSERT_EQ(LDValue_Type(constantStringVal), LDValueType_String);
    ASSERT_EQ(LDValue_Type(ownedStringVal), LDValueType_String);
    ASSERT_EQ(LDValue_Type(numVal), LDValueType_Number);
    ASSERT_EQ(LDValue_Type(nullVal), LDValueType_Null);

    LDValue_Free(boolVal);
    LDValue_Free(constantStringVal);
    LDValue_Free(ownedStringVal);
    LDValue_Free(numVal);
    LDValue_Free(nullVal);
}

TEST_F(LDValueFixture, CloneHasCorrectValue) {
    struct LDValue *boolVal = LDValue_True();
    struct LDValue *constantStringVal = LDValue_ConstantString("hello");
    struct LDValue *ownedStringVal = LDValue_OwnedString("goodbye");
    struct LDValue *numVal = LDValue_Number(12);
    struct LDValue *nullVal = LDValue_Null();

    struct LDValue *boolClone = LDValue_Clone(boolVal);
    struct LDValue *constantStringClone = LDValue_Clone(constantStringVal);
    struct LDValue *ownedStringClone = LDValue_Clone(ownedStringVal);
    struct LDValue *numClone = LDValue_Clone(numVal);
    struct LDValue *nullClone = LDValue_Clone(nullVal);

    ASSERT_TRUE(LDValue_Equal(boolVal, boolClone));
    ASSERT_TRUE(LDValue_Equal(constantStringVal, constantStringClone));
    ASSERT_TRUE(LDValue_Equal(ownedStringVal, ownedStringClone));
    ASSERT_TRUE(LDValue_Equal(numVal, numClone));
    ASSERT_TRUE(LDValue_Equal(nullVal, nullClone));

    LDValue_Free(boolVal);
    LDValue_Free(boolClone);
    LDValue_Free(constantStringVal);
    LDValue_Free(constantStringClone);
    LDValue_Free(ownedStringVal);
    LDValue_Free(ownedStringClone);
    LDValue_Free(numVal);
    LDValue_Free(numClone);
    LDValue_Free(nullVal);
    LDValue_Free(nullClone);
}

TEST_F(LDValueFixture, ArrayHasCorrectType) {
    struct LDValue *value = LDValue_Array(LDArray_New());
    ASSERT_EQ(LDValue_Type(value), LDValueType_Array);
    ASSERT_NE(LDValue_Type(value), LDValueType_Object);

    struct LDValue *valueClone = LDValue_Clone(value);
    ASSERT_TRUE(LDValue_Equal(value, valueClone));

    LDValue_Free(value);
    LDValue_Free(valueClone);
}

TEST_F(LDValueFixture, ObjectHasCorrectType) {
    struct LDValue *value = LDValue_Object(LDObject_New());
    ASSERT_EQ(LDValue_Type(value), LDValueType_Object);
    ASSERT_NE(LDValue_Type(value), LDValueType_Array);

    struct LDValue *valueClone = LDValue_Clone(value);
    ASSERT_TRUE(LDValue_Equal(value, valueClone));

    LDValue_Free(value);
    LDValue_Free(valueClone);
}

TEST_F(LDValueFixture, StringEquality) {
    struct LDValue *a = LDValue_ConstantString("value");
    struct LDValue *b = LDValue_OwnedString("value");

    ASSERT_STREQ(LDValue_GetString(a), LDValue_GetString(b));

    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, CountOfPrimitivesIsZero) {

    struct LDValue *boolVal = LDValue_True();
    struct LDValue *numVal = LDValue_Number(3);
    struct LDValue *constStrVal = LDValue_ConstantString("hello");
    struct LDValue *ownedStrVal = LDValue_OwnedString("hello");
    struct LDValue *nullVal = LDValue_Null();

    ASSERT_EQ(LDValue_Count(boolVal), 0);
    ASSERT_EQ(LDValue_Count(numVal), 0);
    ASSERT_EQ(LDValue_Count(constStrVal), 0);
    ASSERT_EQ(LDValue_Count(ownedStrVal), 0);
    ASSERT_EQ(LDValue_Count(nullVal), 0);

    LDValue_Free(boolVal);
    LDValue_Free(numVal);
    LDValue_Free(constStrVal);
    LDValue_Free(ownedStrVal);
    LDValue_Free(nullVal);
}


TEST_F(LDValueFixture, ObjectIsDisplayed) {

    struct LDObject *obj = LDObject_New();
    LDObject_AddConstantKey(obj, "bool", LDValue_True());
    LDObject_AddConstantKey(obj, "string", LDValue_ConstantString("hello"));

    struct LDArray *array = LDArray_New();
    LDArray_Add(array, LDValue_ConstantString("foo"));
    LDArray_Add(array, LDValue_ConstantString("bar"));

    LDObject_AddConstantKey(obj, "array", LDValue_Array(array));

    struct LDValue *value = LDValue_Object(obj);

    char *json = LDValue_SerializeJSON(value);

    ASSERT_STREQ("{\"bool\":true,\"string\":\"hello\",\"array\":[\"foo\",\"bar\"]}", json);

    LDFree(json);

    LDValue_Free(value);
}

TEST_F(LDValueFixture, ObjectIsBuiltManyTimes) {
    struct LDObject *obj = LDObject_New();
    LDObject_AddConstantKey(obj, "key", LDValue_ConstantString("value"));

    for (size_t i = 0; i < 10; i++) {
        LDValue *value = LDObject_Build(obj);
        char *json = LDValue_SerializeJSON(value);

        ASSERT_STREQ("{\"key\":\"value\"}", json);

        LDFree(json);
        LDValue_Free(value);
    }

    LDObject_Free(obj);
}

TEST_F(LDValueFixture, ObjectCanAddValueAfterBuild) {
    struct LDObject *obj = LDObject_New();
    LDObject_AddConstantKey(obj, "key1", LDValue_ConstantString("value1"));

    struct LDValue *obj1 = LDObject_Build(obj);
    ASSERT_EQ(LDValue_Count(obj1), 1);
    LDValue_Free(obj1);

    LDObject_AddConstantKey(obj, "key2", LDValue_ConstantString("value2"));
    struct LDValue *obj2 = LDObject_Build(obj);
    ASSERT_EQ(LDValue_Count(obj2), 2);
    LDValue_Free(obj2);

    LDObject_Free(obj);
}

TEST_F(LDValueFixture, ArrayIsBuiltManyTimes) {
    struct LDArray *array = LDArray_New();
    LDArray_Add(array, LDValue_ConstantString("value"));

    for (size_t i = 0; i < 10; i++) {
        LDValue *value = LDArray_Build(array);
        char *json = LDValue_SerializeJSON(value);

        ASSERT_STREQ("[\"value\"]", json);

        LDFree(json);
        LDValue_Free(value);
    }

    LDArray_Free(array);
}


TEST_F(LDValueFixture, ArrayCanAddValueAfterBuild) {
    struct LDArray *array = LDArray_New();
    LDArray_Add(array, LDValue_ConstantString("value1"));

    struct LDValue *array1 = LDArray_Build(array);
    ASSERT_EQ(LDValue_Count(array1), 1);
    LDValue_Free(array1);

    LDArray_Add(array, LDValue_ConstantString("value2"));
    struct LDValue *array2 = LDArray_Build(array);
    ASSERT_EQ(LDValue_Count(array2), 2);
    LDValue_Free(array2);

    LDArray_Free(array);
}

TEST_F(LDValueFixture, DisplayUser) {
    struct LDObject *attrs = LDObject_New();
    LDObject_AddConstantKey(attrs, "key", LDValue_ConstantString("foo"));
    LDObject_AddConstantKey(attrs, "name", LDValue_ConstantString("bar"));

    struct LDObject *custom = LDObject_New();

    struct LDArray *list = LDArray_New();
    LDArray_Add(list, LDValue_ConstantString("a"));
    LDArray_Add(list, LDValue_ConstantString("b"));
    LDArray_Add(list, LDValue_True());

    LDObject_AddConstantKey(custom, "things", LDValue_Array(list));

    LDObject_AddConstantKey(attrs, "custom", LDValue_Object(custom));

    struct LDValue *user = LDValue_Object(attrs);

    char *json = LDValue_SerializeFormattedJSON(user);

    ASSERT_STREQ("{\n\t\"key\":\t\"foo\",\n\t\"name\":\t\"bar\",\n\t\"custom\":\t{\n\t\t\"things\":\t[\"a\", \"b\", true]\n\t}\n}", json);
    LDFree(json);
    LDValue_Free(user);

}


TEST_F(LDValueFixture, IterateObject) {
    struct LDObject *obj = LDObject_New();

    LDObject_AddConstantKey(obj, "key1", LDValue_ConstantString("value1"));
    LDObject_AddConstantKey(obj, "key2", LDValue_ConstantString("value2"));

    struct LDValue *value = LDValue_Object(obj);
    ASSERT_EQ(2, LDValue_Count(value));

    struct LDIter *iter;

    ASSERT_TRUE(iter = LDValue_GetIter(value));
    ASSERT_STREQ(LDIter_Key(iter), "key1");
    ASSERT_STREQ(LDValue_GetString(LDIter_Val(iter)), "value1");

    ASSERT_TRUE(iter = LDIter_Next(iter));
    ASSERT_STREQ(LDIter_Key(iter), "key2");
    ASSERT_STREQ(LDValue_GetString(LDIter_Val(iter)), "value2");

    ASSERT_FALSE(LDIter_Next(iter));

    LDValue_Free(value);
}

TEST_F(LDValueFixture, IterateArray) {
    struct LDArray *array = LDArray_New();

    LDArray_Add(array, LDValue_ConstantString("value1"));
    LDArray_Add(array,  LDValue_ConstantString("value2"));

    struct LDValue *value = LDValue_Array(array);
    ASSERT_EQ(2, LDValue_Count(value));

    struct LDIter *iter;

    ASSERT_TRUE(iter = LDValue_GetIter(value));
    ASSERT_STREQ(LDValue_GetString(LDIter_Val(iter)), "value1");

    ASSERT_TRUE(iter = LDIter_Next(iter));
    ASSERT_STREQ(LDValue_GetString(LDIter_Val(iter)), "value2");

    ASSERT_FALSE(LDIter_Next(iter));

    LDValue_Free(value);
}

TEST_F(LDValueFixture, IteratePrimitive) {
    struct LDValue *boolVal = LDValue_True();

    ASSERT_FALSE(LDValue_GetIter(boolVal));
}

TEST_F(LDValueFixture, GetBool) {
    struct LDValue *trueVal = LDValue_True();
    ASSERT_TRUE(LDValue_GetBool(trueVal));

    struct LDValue *falseVal = LDValue_False();
    ASSERT_FALSE(LDValue_GetBool(falseVal));

    LDValue_Free(trueVal);
    LDValue_Free(falseVal);
}

TEST_F(LDValueFixture, GetNumber) {
    struct LDValue *value = LDValue_Number(12);
    ASSERT_EQ(12, LDValue_GetNumber(value));
    LDValue_Free(value);
}

TEST_F(LDValueFixture, GetString) {
    struct LDValue *value = LDValue_OwnedString("hello");
    ASSERT_STREQ("hello", LDValue_GetString(value));
    LDValue_Free(value);
}

TEST_F(LDValueFixture, IsNull) {
    struct LDValue *value = LDValue_Null();
    ASSERT_EQ(LDValue_Type(value), LDValueType_Null);
    LDValue_Free(value);
}

TEST_F(LDValueFixture, ParseBool) {
    struct LDValue *falseVal = LDValue_ParseJSON("false");
    ASSERT_FALSE(LDValue_GetBool(falseVal));

    struct LDValue *trueVal = LDValue_ParseJSON("true");
    ASSERT_TRUE(LDValue_GetBool(trueVal));

    LDValue_Free(falseVal);
    LDValue_Free(trueVal);
}

TEST_F(LDValueFixture, ParseNumber) {
    struct LDValue *numVal = LDValue_ParseJSON("12.34567");
    ASSERT_EQ(12.34567, LDValue_GetNumber(numVal));
    LDValue_Free(numVal);
}

TEST_F(LDValueFixture, ParseNull) {
    struct LDValue *nullVal = LDValue_ParseJSON("null");
    ASSERT_EQ(LDValue_Type(nullVal), LDValueType_Null);
    LDValue_Free(nullVal);
}

TEST_F(LDValueFixture, ParseString) {
    struct LDValue *stringVal = LDValue_ParseJSON("\"hello world\"");
    ASSERT_STREQ("hello world", LDValue_GetString(stringVal));
    LDValue_Free(stringVal);
}

TEST_F(LDValueFixture, ParseArray) {
    struct LDArray *array = LDArray_New();
    LDArray_Add(array, LDValue_True());
    LDArray_Add(array, LDValue_ConstantString("hello"));
    LDArray_Add(array, LDValue_Number(3));
    struct LDValue *a = LDValue_Array(array);

    struct LDValue *b = LDValue_ParseJSON("[true, \"hello\", 3]");

    ASSERT_TRUE(LDValue_Equal(a, b));

    LDValue_Free(a);
    LDValue_Free(b);
}



TEST_F(LDValueFixture, IterateParsedArray) {
    struct LDValue *b = LDValue_ParseJSON("[true, \"hello\", 3]");

    struct LDIter *iter = LDValue_GetIter(b);

    ASSERT_EQ(LDValue_GetBool(LDIter_Val(iter)), LDBooleanTrue);

    iter = LDIter_Next(iter);
    ASSERT_STREQ(LDValue_GetString(LDIter_Val(iter)), "hello");

    iter = LDIter_Next(iter);
    ASSERT_EQ(LDValue_GetNumber(LDIter_Val(iter)), 3);

    ASSERT_FALSE(LDIter_Next(iter));

    LDValue_Free(b);
}

TEST_F(LDValueFixture, ParseObject) {
    struct LDObject *obj = LDObject_New();
    LDObject_AddConstantKey(obj, "bool", LDValue_True());
    LDObject_AddConstantKey(obj, "number", LDValue_Number(12.34));
    LDObject_AddConstantKey(obj, "string", LDValue_ConstantString("hello"));
    struct LDValue *a = LDValue_Object(obj);

    struct LDValue *b = LDValue_ParseJSON(
        R"({"bool": true, "number": 12.34, "string": "hello"})");

    ASSERT_TRUE(LDValue_Equal(a, b));

    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, ParseObjectDuplicateKeysHasCorrectCount) {
    struct LDValue *a = LDValue_ParseJSON(
        R"({"a": true, "a": 12.34})");

    ASSERT_EQ(LDValue_Count(a), 2);

    for (auto iter = LDValue_GetIter(a); iter; iter = LDIter_Next(iter)) {
        ASSERT_STREQ(LDIter_Key(iter), "a");
    }

    LDValue_Free(a);
}

TEST_F(LDValueFixture, ParseObjectDuplicateKeysNotEqual) {
    struct LDValue *a = LDValue_ParseJSON(
        R"({"a": true, "a": 12.34})");

    struct LDValue *b = LDValue_ParseJSON(
        R"({"a": true, "a": 12.34})");

    ASSERT_EQ(LDValue_Count(a), LDValue_Count(b));
    ASSERT_FALSE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, IterateParsedObject) {
    struct LDValue *b = LDValue_ParseJSON(
        R"({"bool": true, "number": 12.34, "string": "hello"})");

    struct LDIter *iter = LDValue_GetIter(b);

    ASSERT_STREQ(LDIter_Key(iter), "bool");
    ASSERT_EQ(LDValue_GetBool(LDIter_Val(iter)), LDBooleanTrue);

    iter = LDIter_Next(iter);
    ASSERT_STREQ(LDIter_Key(iter), "number");
    ASSERT_EQ(LDValue_GetNumber(LDIter_Val(iter)), 12.34);

    iter = LDIter_Next(iter);
    ASSERT_STREQ(LDIter_Key(iter), "string");
    ASSERT_STREQ(LDValue_GetString(LDIter_Val(iter)), "hello");

    ASSERT_FALSE(LDIter_Next(iter));

    LDValue_Free(b);
}

TEST_F(LDValueFixture, IterateParsedArrayWithForLoop) {
    struct LDValue *array = LDValue_ParseJSON("[true, true, true, true]");
    for (auto i = LDValue_GetIter(array); i; i = LDIter_Next(i)) {
        ASSERT_EQ(LDValue_Type(LDIter_Val(i)), LDValueType_Bool);
        ASSERT_TRUE(LDValue_GetBool(LDIter_Val(i)));
    }
    LDValue_Free(array);
}


TEST_F(LDValueFixture, IterateParsedObjectWithForLoop) {
    struct LDValue *obj =
        LDValue_ParseJSON(R"({"a": true, "b": true, "c": true})");
    for (auto i = LDValue_GetIter(obj); i; i = LDIter_Next(i)) {
        ASSERT_EQ(LDValue_Type(LDIter_Val(i)), LDValueType_Bool);
        ASSERT_TRUE(LDIter_Key(i));
    }
    LDValue_Free(obj);
}

TEST_F(LDValueFixture, EqualArraysWithElements) {
    struct LDValue *a = LDValue_ParseJSON(R"(["a", "b", "c"])");
    struct LDValue *b = LDValue_ParseJSON(R"(["a", "b", "c"])");
    ASSERT_TRUE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, EqualArraysWithoutElements) {
    struct LDValue *a = LDValue_ParseJSON("[]");
    struct LDValue *b = LDValue_ParseJSON("[]");
    ASSERT_TRUE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}


TEST_F(LDValueFixture, UnequalArrayElement) {
    struct LDValue *a = LDValue_ParseJSON(R"(["a"])");
    struct LDValue *b = LDValue_ParseJSON(R"(["b"])");
    ASSERT_FALSE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, UnequalArrayOrder) {
    struct LDValue *a = LDValue_ParseJSON(R"(["a", "b"])");
    struct LDValue *b = LDValue_ParseJSON(R"(["b", "a])");
    ASSERT_FALSE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, EqualObjectWithElements) {
    struct LDValue *a = LDValue_ParseJSON(R"({"a" : true, "b" : false})");
    struct LDValue *b = LDValue_ParseJSON(R"({"a" : true, "b" : false})");
    ASSERT_TRUE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, EqualObjectWithElementsOutOfOrder) {
    struct LDValue *a = LDValue_ParseJSON(R"({"a" : true, "b" : false})");
    struct LDValue *b = LDValue_ParseJSON(R"({"b" : false, "a" : true})");
    ASSERT_TRUE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}

TEST_F(LDValueFixture, EqualObjectWithoutElements) {
    struct LDValue *a = LDValue_ParseJSON(R"({})");
    struct LDValue *b = LDValue_ParseJSON(R"({})");
    ASSERT_TRUE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}


TEST_F(LDValueFixture, UnequalObjectKeys) {
    struct LDValue *a = LDValue_ParseJSON(R"({"a" : true})");
    struct LDValue *b = LDValue_ParseJSON(R"({"b" : true})");
    ASSERT_FALSE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}


TEST_F(LDValueFixture, UnequalObjectValues) {
    struct LDValue *a = LDValue_ParseJSON(R"({"a" : true})");
    struct LDValue *b = LDValue_ParseJSON(R"({"a" : false})");
    ASSERT_FALSE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}


TEST_F(LDValueFixture, UnequalObjectSize) {
    struct LDValue *a = LDValue_ParseJSON(R"({"a" : true})");
    struct LDValue *b = LDValue_ParseJSON(R"({"a" : true, "b" : true})");
    ASSERT_FALSE(LDValue_Equal(a, b));
    LDValue_Free(a);
    LDValue_Free(b);
}


