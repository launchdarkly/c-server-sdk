#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include "persistent_store_collection.h"
#include "integrations/file_data.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class PersistentStoreCollectionFixture : public CommonFixture {
};

TEST_F(PersistentStoreCollectionFixture, ConvertsCollection) {
    struct LDStoreCollectionState *collections = NULL;
    unsigned int collectionCount = 0;

    LDJSON *newData = LDi_loadJSONFile("../tests/datafiles/persistent-store-init.json");

    LDi_makeCollections(newData, &collections, &collectionCount);

    EXPECT_EQ(2, collectionCount);
    EXPECT_EQ(2, collections[0].itemCount);

    EXPECT_EQ(3, collections[1].itemCount);

    EXPECT_EQ(std::string("flag1"), collections[0].items[0].key);
    EXPECT_EQ(1, collections[0].items[0].item.version);
    std::string flag1Str =
            R"({"key":"flag1","version":1,"on":true,"fallthrough":{"variation":2},"variations":["fall","off","on"]})";
    EXPECT_EQ(flag1Str, (char *) collections[0].items[0].item.buffer);
    EXPECT_EQ(flag1Str.size(), collections[0].items[0].item.bufferSize);

    EXPECT_EQ(std::string("flag2"), collections[0].items[1].key);
    EXPECT_EQ(2, collections[0].items[1].item.version);
    std::string flag2Str = R"({"key":"flag2","version":2,"on":true,"fallthrough":{"variation":0},"variations":["fall","off","on"]})";
    EXPECT_EQ(flag2Str, (char *) collections[0].items[1].item.buffer);
    EXPECT_EQ(flag2Str.size(), collections[0].items[1].item.bufferSize);


    EXPECT_EQ(std::string("seg1"), collections[1].items[0].key);
    EXPECT_EQ(3, collections[1].items[0].item.version);
    EXPECT_EQ(
            std::string(
            R"({"key":"seg1","version":3,"included":["user1"]})"),
            (char *) collections[1].items[0].item.buffer);

    EXPECT_EQ(std::string("seg2"), collections[1].items[1].key);
    EXPECT_EQ(4, collections[1].items[1].item.version);
    EXPECT_EQ(
            std::string(
            R"({"key":"seg2","version":4,"included":["user2"]})"),
            (char *) collections[1].items[1].item.buffer);

    EXPECT_EQ(std::string("seg3"), collections[1].items[2].key);
    EXPECT_EQ(5, collections[1].items[2].item.version);
    EXPECT_EQ(
            std::string(
            R"({"key":"seg3","version":5,"included":["user3"]})"),
            (char *) collections[1].items[2].item.buffer);

    LDJSONFree(newData);
    LDi_freeCollections(collections, collectionCount);
}
