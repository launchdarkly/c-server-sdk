#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>
#include <launchdarkly/integrations/file_data.h>

#include "assertion.h"
#include "config.h"
#include "utility.h"
#include "client.h"
#include "store.h"
#include "integrations/file_data.h"
}

class User {
public:
    User(std::string userKey) {
        user = LDUserNew(userKey.c_str());
    }

    ~User() {
        if(user) {
            LDUserFree(user);
        }
    }

    struct LDUser * RawPtr() {
        return user;
    }

private:
    struct LDUser *user;
};

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class FileDataFixture : public CommonFixture {
  protected:
    struct LDConfig *config;
    struct LDClient *client;

    FileDataFixture() : config(NULL), client(NULL) {
    }

    virtual ~FileDataFixture() {
    }

    virtual void SetUp() {
        CommonFixture::SetUp();
        LD_ASSERT(config = LDConfigNew("key"));
        LDConfigSetSendEvents(config, LDBooleanFalse);
    }

    virtual void TearDown() {
        if(client) {
            LDClientClose(client);
            client = NULL;
            config = NULL;
        } else if(config) {
            LDConfigFree(config);
            config = NULL;
        }

        CommonFixture::TearDown();
    }

    void InitializeClientWithFiles(std::vector<const char *> filenames) {
        LDConfigSetDataSource(config, LDFileDataInit(filenames.size(), filenames.data()));
        LD_ASSERT(client = LDClientInit(config, 10));
    }

    std::string StringVariation(User &user, std::string flag, std::string defaultValue) {
        struct LDDetails details;
        char *result = LDStringVariation(client, user.RawPtr(), flag.c_str(), defaultValue.c_str(), &details);
        std::string resultString = std::string(result);
        LDFree(result);

        if(LD_ERROR == details.reason) {
            LD_LOG(LD_LOG_ERROR, LDEvalErrorKindToString(details.extra.errorKind));
        }
        LDDetailsClear(&details);

        return resultString;
    }

    bool BoolVariation(User &user, std::string flag, bool defaultValue) {
        struct LDDetails details;
        bool result = LDBoolVariation(client, user.RawPtr(), flag.c_str(), defaultValue, &details);
        if(LD_ERROR == details.reason) {
            LD_LOG(LD_LOG_ERROR, LDEvalErrorKindToString(details.extra.errorKind));
        }
        LDDetailsClear(&details);

        return result;
    }
};

TEST_F(FileDataFixture, LoadJsonFile) {
    struct LDJSON *const json = LDi_loadJSONFile("../tests/datafiles/simple.json");
    ASSERT_EQ(LDObject, LDJSONGetType(json));
    ASSERT_STREQ("value", LDGetText(LDObjectLookup(json, "key")));
    LDJSONFree(json);
}

TEST_F(FileDataFixture, LoadMalformedJSONFile) {
    struct LDJSON *const json = LDi_loadJSONFile("../tests/datafiles/malformed.json");
    ASSERT_EQ(NULL, json);
}

TEST_F(FileDataFixture, LoadNoDataJSONFile) {
    struct LDJSON *const json = LDi_loadJSONFile("../tests/datafiles/no-data.json");
    ASSERT_EQ(NULL, json);
}

TEST_F(FileDataFixture, FileDataWithAllProperties) {
    InitializeClientWithFiles({"../tests/datafiles/all-properties.json"});

    auto user = User("user");

    auto result = StringVariation(user, "flag1", "nothing");
    ASSERT_EQ("on", result);

    auto result2 = StringVariation(user, "flag2", "nothing");
    ASSERT_EQ("value2", result2);
}

TEST_F(FileDataFixture, BadDataIsIgnored) {
    InitializeClientWithFiles(
            { "../tests/datafiles/all-properties.json"
            , "../tests/datafiles/malformed.json"
            , "../tests/datafiles/no-data.json"
            });

    auto user = User("user");
    auto result = StringVariation(user, "flag1", "nothing");

    ASSERT_EQ("on", result);
}

TEST_F(FileDataFixture, FileDataWithFlagOnly) {
    InitializeClientWithFiles({"../tests/datafiles/flag-only.json"});

    auto user = User("user");

    auto result = StringVariation(user, "flag1", "nothing");
    ASSERT_EQ("on", result);
}

TEST_F(FileDataFixture, FileDataWithDuplicateKeys) {
    InitializeClientWithFiles({"../tests/datafiles/flag-only.json", "../tests/datafiles/flag-with-duplicate-key.json"});

    auto user = User("user");

    auto result = StringVariation(user, "flag1", "nothing");
    ASSERT_EQ("on", result);

    auto result2 = BoolVariation(user, "another", false);
    ASSERT_TRUE(result2);
}

TEST_F(FileDataFixture, SegmentFileData) {
    InitializeClientWithFiles({"../tests/datafiles/segment-only.json", "../tests/datafiles/flag-with-segment-rule.json"});

    auto user1 = User("user1");
    auto user2 = User("user2");

    auto user1result = StringVariation(user1, "flag", "nothing");
    ASSERT_EQ("green", user1result);

    auto user2result = StringVariation(user2, "flag", "nothing");
    ASSERT_EQ("red", user2result);
}

TEST_F(FileDataFixture, InitWithNoFilesDoesntFail) {
    InitializeClientWithFiles({});

    auto user1 = User("user1");
    auto user1result = StringVariation(user1, "flag", "nothing");
    ASSERT_EQ("nothing", user1result);
}
