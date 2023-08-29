#include "gtest/gtest.h"
#include "commonfixture.h"

#include "test-utils/user.h"

extern "C" {
#include <launchdarkly/api.h>
#include <launchdarkly/boolean.h>

#include "assertion.h"
#include "config.h"
#include "utility.h"
#include "client.h"
#include "store.h"
#include "cJSON.h"
#include "integrations/test_data.h"
}

class Client {
public:
    Client(std::function<void(struct LDConfig *config)> configurer) {
        client = NULL;
        LD_ASSERT(config = LDConfigNew("key"));
        configurer(config);
    }

    virtual ~Client() {
        if (client) {
            LDClientClose(client);
        }

        if (config) {
            LDConfigFree(config);
        }
    }

    void Start() {
        LD_ASSERT(client = LDClientInit(config, 10));
        config = NULL;//config is moved to the client
    }

    bool IsStarted() {
        return client != NULL;
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

    int IntVariation(User &user, std::string flag, int defaultValue) {
        struct LDDetails details;
        int result = LDIntVariation(client, user.RawPtr(), flag.c_str(), defaultValue, &details);
        if(LD_ERROR == details.reason) {
            LD_LOG(LD_LOG_ERROR, LDEvalErrorKindToString(details.extra.errorKind));
        }
        LDDetailsClear(&details);

        return result;
    }

private:
    struct LDClient *client;
    struct LDConfig *config;

};

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class TestDataFixture : public CommonFixture {
  protected:
    Client *client;
    struct LDTestData *td;

    virtual void SetUp() {
        CommonFixture::SetUp();
        td = LDTestDataInit();
        client = NewTestDataClient();
    }

    virtual void TearDown() {
        //Need to delete client before td since it calls dataSource->close
        if(client) {
            delete client;
        }

        if (td) {
            LDTestDataFree(td);
        }

        CommonFixture::TearDown();
    }

    struct LDJSON * NewStringArray(std::vector<std::string> strings) {
        struct LDJSON *res = LDNewArray();
        for (auto str : strings) {
            LDArrayPush(res, LDNewText(str.c_str()));
        }
        return res;
    }

    Client * NewTestDataClient() {
        return new Client([this](struct LDConfig *config) {
            LDConfigSetSendEvents(config, LDBooleanFalse);
            LDConfigSetDataSource(config, LDTestDataCreateDataSource(this->td));
        });
    }
};

TEST_F(TestDataFixture, TestFlagDefaults) {
    auto user = User("user");

    LDTestDataUpdate(td, LDTestDataFlag(td, "flag1"));

    client->Start();

    LDBoolean defaultResult = client->BoolVariation(user, "flag1", LDBooleanFalse);
    ASSERT_EQ(LDBooleanTrue, defaultResult);

    struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
    LDFlagBuilderOn(flag, LDBooleanFalse);
    LDTestDataUpdate(td, flag);

    LDBoolean offResult = client->BoolVariation(user, "flag1", LDBooleanTrue);
    ASSERT_EQ(LDBooleanFalse, offResult);
}


TEST_F(TestDataFixture, TestFlagTargeting) {
    {
        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
        struct LDJSON *variations = NewStringArray({"red", "green", "blue"});
        LD_ASSERT(LDFlagBuilderVariations(flag, variations));
        LDFlagBuilderFallthroughVariation(flag, 0);
        LDFlagBuilderVariationForUser(flag, "ben", 1);
        LDFlagBuilderVariationForUser(flag, "john", 1);
        LDFlagBuilderVariationForUser(flag, "greg", 2);
        LDTestDataUpdate(td, flag);
    }

    client->Start();

    {
        auto user = User("user");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("red", result);
    }

    {
        auto user = User("ben");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("green", result);
    }

    {
        auto user = User("john");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("green", result);
    }

    {
        auto user = User("greg");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("blue", result);
    }

    {
        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
        LDFlagBuilderOn(flag, LDBooleanFalse);
        LDTestDataUpdate(td, flag);
    }

    {
        auto user = User("user");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("green", result);
    }

    {
        auto user = User("greg");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("green", result);
    }
}

TEST_F(TestDataFixture, TestFlagRules) {
    {
        struct LDJSON *variations = NewStringArray({"red", "green", "blue"});
        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
        LDFlagBuilderVariations(flag, variations);
        LDFlagBuilderFallthroughVariation(flag, 0);
        struct LDFlagRuleBuilder *rule = LDFlagBuilderIfMatch(flag, "name", LDNewText("ben"));
        LDFlagRuleBuilderThenReturn(rule, 1);

        LDTestDataUpdate(td, flag);
    }

    client->Start();

    {
        auto user = User("user");
        user.SetName("user");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("red", result);
    }

    {
        auto user = User("ben");
        user.SetName("ben");
        auto result = client->StringVariation(user, "flag1", "nothing");
        ASSERT_EQ("green", result);
    }

    {
        struct LDJSON *variations = NewStringArray({"red", "green", "blue"});

        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag2");
        LDFlagBuilderVariations(flag, variations);
        LDFlagBuilderFallthroughVariation(flag, 0);

        struct LDFlagRuleBuilder *rule = LDFlagBuilderIfMatch(flag, "country", LDNewText("gb"));
        LDFlagRuleBuilderAndNotMatch(rule, "name", LDNewText("ben"));
        LDFlagRuleBuilderThenReturn(rule, 1);

        LDTestDataUpdate(td, flag);
    }

    {
        auto user = User("john");
        user.SetName("john").SetCountry("gb");
        auto result = client->StringVariation(user, "flag2", "nothing");
        ASSERT_EQ("green", result);
    }

    {
        auto user = User("greg");
        user.SetName("john").SetCountry("usa");
        auto result = client->StringVariation(user, "flag2", "nothing");
        ASSERT_EQ("red", result);
    }

    {
        auto user = User("ben");
        user.SetName("ben").SetCountry("gb");
        auto result = client->StringVariation(user, "flag2", "nothing");
        ASSERT_EQ("red", result);
    }
}

TEST_F(TestDataFixture, TestBooleanFlag) {
    struct LDFlagBuilder * flag;
    struct LDJSON *value;

    LD_ASSERT(flag = LDTestDataFlag(td, "flag1"));
    LD_ASSERT(value = LDNewText("green"));
    LD_ASSERT(LDFlagBuilderVariations(flag, value));
    ASSERT_EQ(LDBooleanFalse, LDi_isBooleanFlag(flag));
    LD_ASSERT(LDFlagBuilderBooleanFlag(flag));
    ASSERT_EQ(LDBooleanTrue, LDi_isBooleanFlag(flag));

    LDFlagBuilderFree(flag);
}

TEST_F(TestDataFixture, TestMultipleClients) {
    client->Start();

    LDTestDataUpdate(td, LDTestDataFlag(td, "flag1"));

    {
        auto user = User("ben");
        LDBoolean result = client->BoolVariation(user, "flag1", LDBooleanFalse);
        ASSERT_EQ(LDBooleanTrue, result);
    }


    auto client2 = std::unique_ptr<Client>(NewTestDataClient());
    client2->Start();

    {
        auto user = User("ben");
        LDBoolean result = client2->BoolVariation(user, "flag1", LDBooleanFalse);
        ASSERT_EQ(LDBooleanTrue, result);
    }

    {
        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
        LDFlagBuilderOn(flag, LDBooleanFalse);
        LDTestDataUpdate(td, flag);
    }

    //Both clients update
    {
        auto user = User("ben");
        LDBoolean result = client->BoolVariation(user, "flag1", LDBooleanFalse);
        ASSERT_EQ(LDBooleanFalse, result);
    }

    {
        auto user = User("ben");
        LDBoolean result = client2->BoolVariation(user, "flag1", LDBooleanFalse);
        ASSERT_EQ(LDBooleanFalse, result);
    }
}

TEST_F(TestDataFixture, TestValueForAllUsers) {
    client->Start();

    {
        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
        LDFlagBuilderValueForAllUsers(flag, LDNewNumber(42));
        LDTestDataUpdate(td, flag);
    }

    {
        auto user = User("ben");
        auto result = client->IntVariation(user, "flag1", 0);
        ASSERT_EQ(42, result);
    }

    {
        auto user = User("john");
        auto result = client->IntVariation(user, "flag1", 0);
        ASSERT_EQ(42, result);
    }
}

TEST_F(TestDataFixture, TestVariationForAllUsers) {
    client->Start();

    {
        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
        LDFlagBuilderVariations(flag, NewStringArray({"one", "two", "three"}));
        LDFlagBuilderVariationForAllUsers(flag, 2);
        LDTestDataUpdate(td, flag);
    }

    {
        auto user = User("ben");
        auto result = client->StringVariation(user, "flag1", "");
        ASSERT_EQ("three", result);
    }
    {
        auto user = User("john");
        auto result = client->StringVariation(user, "flag1", "");
        ASSERT_EQ("three", result);
    }

    {
        struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
        LDFlagBuilderVariationForAllUsersBoolean(flag, LDBooleanTrue);
        LDTestDataUpdate(td, flag);
    }

    {
        auto user = User("ben");
        auto result = client->BoolVariation(user, "flag1", LDBooleanFalse);
        ASSERT_EQ(LDBooleanTrue, result);
    }

    {
        auto user = User("john");
        auto result = client->BoolVariation(user, "flag1", LDBooleanFalse);
        ASSERT_EQ(LDBooleanTrue, result);
    }
}

TEST_F(TestDataFixture, TestFlagNULLUserKey) {
    struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag1");
    LDFlagBuilderVariationForUser(flag, NULL, 1);
    ASSERT_EQ(LDTestDataUpdate(td, flag), LDBooleanFalse);

    LDFlagBuilderFree(flag);
}
