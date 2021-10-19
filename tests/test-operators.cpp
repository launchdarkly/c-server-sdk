#include "gtest/gtest.h"
#include "commonfixture.h"
#include <algorithm>

extern "C" {
#include <launchdarkly/api.h>
    
#include "operators.h"
#include "utility.h"
}

static const char *const dateStr1 = "2017-12-06T00:00:00.000-07:00";
static const char *const dateStr2 = "2017-12-06T00:01:01.000-07:00";
static const unsigned int dateMs1 = 10000000;
static const unsigned int dateMs2 = 10000001;
static const char *const invalidDate = "ThisIsABadDate";

struct ld_deleter {
    void operator()(struct LDJSON *const json) {
        LDJSONFree(json);
    }
};

#pragma pack(1)
struct OperatorTestParams {
    std::shared_ptr<LDJSON> op;
    std::shared_ptr<LDJSON> uvalue;
    std::shared_ptr<LDJSON> cvalue;

    LDBoolean expect;

    OperatorTestParams(LDJSON *op,
                       LDJSON *uvalue,
                       LDJSON *cvalue,
                       LDBoolean expect) {
        this->op = std::shared_ptr<LDJSON>(op, ld_deleter());
        this->uvalue = std::shared_ptr<LDJSON>(uvalue, ld_deleter());;
        this->cvalue = std::shared_ptr<LDJSON>(cvalue, ld_deleter());;
        this->expect = expect;
    }
};

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class OperatorsFixture : public CommonFixture,
                         public ::testing::WithParamInterface<OperatorTestParams> {
public:
    // Allows for the test to have human readable names with the parameters.
    struct ParamToString {
        template<class ParamType>
        std::string operator()(const testing::TestParamInfo<ParamType> &info) const {
            char *const uvalueString = LDJSONSerialize(info.param.uvalue.get());
            char *const cvalueString = LDJSONSerialize(info.param.cvalue.get());
            char *const opString = LDJSONSerialize(info.param.op.get());

            std::string charsToRemove = "\"._: *+()|%-";
            std::string combined = "";

            combined += std::to_string(info.index);
            combined += "i";
            combined += uvalueString;
            combined += opString;
            combined += cvalueString;
            combined += info.param.expect ? "isTrue" : "isFalse";

            for (char charToRemove: charsToRemove) {
                combined.erase(std::remove(combined.begin(), combined.end(), charToRemove), combined.end());
            }

            LDFree(uvalueString);
            LDFree(cvalueString);
            LDFree(opString);

            return combined;
        }
    };
};


TEST_P(OperatorsFixture, VerifyOperation) {
    const OperatorTestParams &item = GetParam();
    OpFn opfn;

    ASSERT_TRUE(opfn = LDi_lookupOperation(LDGetText(reinterpret_cast<const LDJSON *const>(item.op.get()))));

    ASSERT_TRUE(opfn(item.uvalue.get(), item.cvalue.get()) == item.expect);
}


INSTANTIATE_TEST_SUITE_P(
        Operators,
        OperatorsFixture,
        ::testing::Values(
                /* number operators */
                OperatorTestParams{LDNewText("in"), LDNewNumber(99), LDNewNumber(99), LDBooleanTrue},
                OperatorTestParams{LDNewText("in"), LDNewNumber(99.0001), LDNewNumber(99.0001), LDBooleanTrue},
                OperatorTestParams{LDNewText("lessThan"), LDNewNumber(1), LDNewNumber(1.99999), LDBooleanTrue},
                OperatorTestParams{LDNewText("lessThan"), LDNewNumber(1.99999), LDNewNumber(1), LDBooleanFalse},
                OperatorTestParams{LDNewText("lessThan"), LDNewNumber(1), LDNewNumber(2), LDBooleanTrue},
                OperatorTestParams{LDNewText("lessThanOrEqual"), LDNewNumber(1), LDNewNumber(1), LDBooleanTrue},
                OperatorTestParams{LDNewText("greaterThan"), LDNewNumber(2), LDNewNumber(1.99999), LDBooleanTrue},
                OperatorTestParams{LDNewText(
                        "greaterThan"), LDNewNumber(1.99999), LDNewNumber(2), LDBooleanFalse},
                OperatorTestParams{LDNewText("greaterThan"), LDNewNumber(2), LDNewNumber(1), LDBooleanTrue},
                OperatorTestParams{LDNewText(
                        "greaterThanOrEqual"), LDNewNumber(1), LDNewNumber(1), LDBooleanTrue},

                /* string operators */
                OperatorTestParams{LDNewText("in"), LDNewText("x"), LDNewText("x"), LDBooleanTrue},
                OperatorTestParams{LDNewText("in"), LDNewText("x"), LDNewText("xyz"), LDBooleanFalse},
                OperatorTestParams{LDNewText("startsWith"), LDNewText("xyz"), LDNewText("x"), LDBooleanTrue},
                OperatorTestParams{LDNewText("startsWith"), LDNewText("x"), LDNewText("xyz"), LDBooleanFalse},
                OperatorTestParams{LDNewText("endsWith"), LDNewText("xyz"), LDNewText("z"), LDBooleanTrue},
                OperatorTestParams{LDNewText("endsWith"), LDNewText("z"), LDNewText("xyz"), LDBooleanFalse},
                OperatorTestParams{LDNewText("contains"), LDNewText("xyz"), LDNewText("y"), LDBooleanTrue},
                OperatorTestParams{LDNewText("contains"), LDNewText("y"), LDNewText("yz"), LDBooleanFalse},

                /* mixed strings and numbers */
                OperatorTestParams{LDNewText("in"), LDNewText("99"), LDNewNumber(99), LDBooleanFalse},
                OperatorTestParams{LDNewText("in"), LDNewNumber(99), LDNewText("99"), LDBooleanFalse},
                OperatorTestParams{LDNewText("contains"), LDNewText("99"), LDNewNumber(99), LDBooleanFalse},
                OperatorTestParams{LDNewText("startsWith"), LDNewText("99"), LDNewNumber(99), LDBooleanFalse},
                OperatorTestParams{LDNewText("endsWith"), LDNewText("99"), LDNewNumber(99), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "lessThanOrEqual"), LDNewText("99"), LDNewNumber(99), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "lessThanOrEqual"), LDNewNumber(99), LDNewText("99"), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "greaterThanOrEqual"), LDNewText("99"), LDNewNumber(99), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "greaterThanOrEqual"), LDNewNumber(99), LDNewText("99"), LDBooleanFalse},

                /* date operators */
                OperatorTestParams{LDNewText("before"), LDNewText(dateStr1), LDNewText(dateStr2), LDBooleanTrue},
                OperatorTestParams{LDNewText(
                        "before"), LDNewNumber(dateMs1), LDNewNumber(dateMs2), LDBooleanTrue},
                OperatorTestParams{LDNewText("before"), LDNewText(dateStr2), LDNewText(dateStr1), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "before"), LDNewNumber(dateMs2), LDNewNumber(dateMs1), LDBooleanFalse},
                OperatorTestParams{LDNewText("before"), LDNewText(dateStr1), LDNewText(dateStr1), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "before"), LDNewNumber(dateMs1), LDNewNumber(dateMs1), LDBooleanFalse},
                OperatorTestParams{LDNewText("before"), LDNewText(""), LDNewText(dateStr1), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "before"), LDNewText(dateStr1), LDNewText(invalidDate), LDBooleanFalse},
                OperatorTestParams{LDNewText("after"), LDNewText(dateStr2), LDNewText(dateStr1), LDBooleanTrue},
                OperatorTestParams{LDNewText("after"), LDNewNumber(dateMs2), LDNewNumber(dateMs1), LDBooleanTrue},
                OperatorTestParams{LDNewText("after"), LDNewText(dateStr1), LDNewText(dateStr2), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "after"), LDNewNumber(dateMs1), LDNewNumber(dateMs2), LDBooleanFalse},
                OperatorTestParams{LDNewText("after"), LDNewText(dateStr1), LDNewText(dateStr1), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "after"), LDNewNumber(dateMs1), LDNewNumber(dateMs1), LDBooleanFalse},
                OperatorTestParams{LDNewText("after"), LDNewText(""), LDNewText(dateStr1), LDBooleanFalse},
                OperatorTestParams{LDNewText(
                        "after"), LDNewText(dateStr1), LDNewText(invalidDate), LDBooleanFalse},

                /* regex */
                OperatorTestParams{LDNewText("matches"), LDNewText("hello world"), LDNewText("hello.*rld"),
                                   LDBooleanTrue},
                OperatorTestParams{LDNewText("matches"), LDNewText("hello world"), LDNewText("hello.*orl"),
                                   LDBooleanTrue},
                OperatorTestParams{LDNewText("matches"), LDNewText("hello world"), LDNewText("l+"), LDBooleanTrue},
                OperatorTestParams{LDNewText("matches"), LDNewText("hello world"), LDNewText("(world|planet)"),
                                   LDBooleanTrue},
                OperatorTestParams{LDNewText("matches"), LDNewText("hello world"), LDNewText("aloha"), LDBooleanFalse},
                OperatorTestParams{LDNewText("matches"), LDNewText("hello world"), LDNewText("***bad rg"),
                                   LDBooleanFalse},

                /* semver operators */
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("2.0.0"), LDNewText("2.0.0"), LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("2.0"), LDNewText("2.0.0"), LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("2-rc1"), LDNewText("2.0.0-rc1"), LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("2+build2"), LDNewText("2.0.0+build2"),
                                   LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("2.0.0"), LDNewText("2.0.1"), LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerLessThan"), LDNewText("2.0.0"), LDNewText("2.0.1"), LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerLessThan"), LDNewText("2.0"), LDNewText("2.0.1"), LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerLessThan"), LDNewText("2.0.1"), LDNewText("2.0.0"), LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerLessThan"), LDNewText("2.0.1"), LDNewText("2.0"), LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerLessThan"), LDNewText("2.0.1"), LDNewText("xbad%ver"),
                                   LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerLessThan"), LDNewText("2.0.0-rc"), LDNewText("2.0.0-rc.beta"),
                                   LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerGreaterThan"), LDNewText("2.0.1"), LDNewText("2.0"), LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerGreaterThan"), LDNewText("2.0.1"), LDNewText("2.0"), LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerGreaterThan"), LDNewText("2.0.0"), LDNewText("2.0.1"),
                                   LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerGreaterThan"), LDNewText("2.0"), LDNewText("2.0.1"),
                                   LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerGreaterThan"), LDNewText("2.0.1"), LDNewText("xbad%ver"),
                                   LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerGreaterThan"), LDNewText("2.0.0-rc.1"), LDNewText("2.0.0-rc.0"),
                                   LDBooleanTrue},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("02.0.0"), LDNewText("2.0.0"), LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("v2.0.0"), LDNewText("2.0.0"), LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("2.01.0"), LDNewText("2.1.0"), LDBooleanFalse},
                OperatorTestParams{LDNewText("semVerEqual"), LDNewText("2.0.01"), LDNewText("2.0.1"), LDBooleanFalse}


        ), OperatorsFixture::ParamToString());

TEST_F(OperatorsFixture, ParseDateZero) {
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t texpected, ttimestamp;

    ASSERT_TRUE(jexpected = LDNewNumber(0.0));
    ASSERT_TRUE(LDi_parseTime(jexpected, &texpected));

    ASSERT_TRUE(jtimestamp = LDNewText("1970-01-01T00:00:00Z"));
    ASSERT_TRUE(LDi_parseTime(jtimestamp, &ttimestamp));

    ASSERT_TRUE(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

TEST_F(OperatorsFixture, ParseUTCTimestamp) {
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t texpected, ttimestamp;

    ASSERT_TRUE(jexpected = LDNewNumber(1460847451684));
    ASSERT_TRUE(LDi_parseTime(jexpected, &texpected));

    ASSERT_TRUE(jtimestamp = LDNewText("2016-04-16T22:57:31.684Z"));
    ASSERT_TRUE(LDi_parseTime(jtimestamp, &ttimestamp));

    ASSERT_TRUE(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

TEST_F(OperatorsFixture, ParseTimezone) {
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t texpected, ttimestamp;

    ASSERT_TRUE(jexpected = LDNewNumber(1460851752759));
    ASSERT_TRUE(LDi_parseTime(jexpected, &texpected));

    ASSERT_TRUE(jtimestamp = LDNewText("2016-04-16T17:09:12.759-07:00"));
    ASSERT_TRUE(LDi_parseTime(jtimestamp, &ttimestamp));

    ASSERT_TRUE(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

TEST_F(OperatorsFixture, ParseTimezoneNoMillis) {
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t texpected, ttimestamp;

    ASSERT_TRUE(jexpected = LDNewNumber(1460851752000));
    ASSERT_TRUE(LDi_parseTime(jexpected, &texpected));

    ASSERT_TRUE(jtimestamp = LDNewText("2016-04-16T17:09:12-07:00"));
    ASSERT_TRUE(LDi_parseTime(jtimestamp, &ttimestamp));

    ASSERT_TRUE(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

TEST_F(OperatorsFixture, TimeCompareSimilar) {
    struct LDJSON *jtimestamp1, *jtimestamp2;
    timestamp_t ttimestamp2, ttimestamp1;

    ASSERT_TRUE(jtimestamp1 = LDNewNumber(1000));
    ASSERT_TRUE(LDi_parseTime(jtimestamp1, &ttimestamp1));

    ASSERT_TRUE(jtimestamp2 = LDNewNumber(1001));
    ASSERT_TRUE(LDi_parseTime(jtimestamp2, &ttimestamp2));

    ASSERT_TRUE(timestamp_compare(&ttimestamp1, &ttimestamp2) == -1);

    LDJSONFree(jtimestamp1);
    LDJSONFree(jtimestamp2);
}
