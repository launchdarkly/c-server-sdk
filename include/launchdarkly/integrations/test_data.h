/*!
 * @file integrations/test_data.h
 * @brief Public API for LDTestData
 */
#pragma once

#include <stdio.h>
#include <stdarg.h>

#include <launchdarkly/boolean.h>
#include <launchdarkly/export.h>
#include <launchdarkly/json.h>
#include <launchdarkly/data_source.h>

/**
 * @struct LDTestData
 * @brief An opaque TestData object
 * @details
 * A mechanism for providing dynamically updatable feature flag state in a simplified form to an SDK
 * client in test scenarios.
 *
 * Unlike {@link file_data.h FileData}, this mechanism does not use any external resources. It provides only
 * the data that the application has put into it using {@link LDTestDataUpdate}.
 *
 * @code{.c}
 * struct LDTestData *td = LDTestDataInit();
 *
 * {
 *    struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag-key-1");
 *    LDFlagBuilderVariationForAllUsersBoolean(flag, true));
 *    LDTestDataUpdate(td, flag);
 * }
 *
 * {
 *    struct LDClient *client;
 *    struct LDConfig *config = LDConfigNew("key"));
 *    LDConfigSetDataSource(config, LDTestDataCreateDataSource(td));
 *    if(!(client = LDClientInit(config, 10))) {
 *        LDConfigFree(config);
 *        return;
 *    }
 * }
 *
 * {
 *    struct LDFlagBuilder *flag = LDTestDataFlag(td, "flag-key-2");
 *    LDFlagBuilderVariationForUserBoolean("some-user-key", LDBooleanTrue);
 *    LDFlagBuilderFallthroughVariationBoolean(LDBooleanFalse));
 *    LDTestDataUpdate(td, flag);
 * }
 * @endcode
 *
 * The above example uses a simple boolean flag, but more complex configurations are possible using
 * the methods of the LDFlagBuilder that is returned by {@link LDTestDataFlag}. LDFlagBuilder
 * supports many of the ways a flag can be configured on the LaunchDarkly dashboard, but does not
 * currently support
 *  1. rule operators other than "in" and "not in", or
 *  2. percentage rollouts.
 *
 * If the same LDTestData instance is used to configure multiple LDClient instances,
 * any changes made to the data will propagate to all of the LDClients.
 *
 * @since 2.4.6
 * @see {@link test_data.h TestData}
 * @see {@link file_data.h FileData}
 */
struct LDTestData;

/**
 * @struct LDFlagBuilder
 * @brief A builder for feature flag configurations to be used with LDTestData.
 */
struct LDFlagBuilder;

/**
 * @struct LDFlagRuleBuilder
 * @brief A builder for feature flag rules to be used with LDTestData.
 *
 * In the LaunchDarkly model, a flag can have any number of rules, and a rule can have any number of
 * clauses. A clause is an individual test such as "name is 'X'". A rule matches a user if all of the
 * rule's clauses match the user.
 *
 * To start defining a rule, use one of the flag builder's matching methods such as
 * {@link LDFlagBuilderIfMatch}. This defines the first clause for the rule.
 * Optionally, you may add more clauses with the rule builder's methods such as
 * {@link LDFlagRuleBuilderAndMatch}. Finally, call {@link LDFlagRuleBuilderThenReturnBoolean} or
 * {@link LDFlagRuleBuilderThenReturn} to finish defining the rule.
 */
struct LDFlagRuleBuilder;

/**
 * @brief Creates a new instance of the LDTestData data source.
 * @return a new configurable test data source
 */
LD_EXPORT(struct LDTestData *)
LDTestDataInit(void);

/**
 * @brief Free all of the data associated with the test data source.
 * @param[in] testData May be `NULL`.
 * @return Void.
 */
LD_EXPORT(void)
LDTestDataFree(struct LDTestData *testData);

/**
 * @brief Creates or copies a LDFlagBuilder for building a test flag configuration.
 *
 * If this flag key has already been defined in this LDTestData instance, then the builder
 * starts with the same configuration that was last provided for this flag.
 *
 * Otherwise, it starts with a new default configuration in which the flag has @c true and
 * @c false variations, is @c true for all users when targeting is turned on and
 * @c false otherwise, and currently has targeting turned on. You can change any of those
 * properties, and provide more complex behavior, using the LDFlagBuilder functions.
 *
 * Once you have set the desired configuration, pass the builder to LDTestDataUpdate().
 *
 * @param[in] testData the LDTestData instance
 * @param[in] key the flag key
 * @return a flag configuration builder
 * @see LDTestDataUpdate
 */
LD_EXPORT(struct LDFlagBuilder *)
LDTestDataFlag(struct LDTestData * testData, const char *key);

/**
 * @brief Updates the test data with the specified flag configuration.
 *
 * This has the same effect as if a flag were added or modified on the LaunchDarkly dashboard.
 * It immediately propagates the flag change to any LDClient instance(s) that you have
 * already configured to use this LDTestData. If no LDClient has been started yet,
 * it simply adds this flag to the test data which will be provided to any LDClient that
 * you subsequently configure.
 *
 * Any subsequent changes to this LDFlagBuilder instance do not affect the test data,
 * unless you call @c LDTestDataUpdate again.
 *
 * @param[in] testData the LDTestData instance
 * @param[in] flagBuilder a flag configuration builder. Ownership of `flagBuilder` is transferred.
 * @return True on success, False on failure.
 * @see LDTestDataFlag
 */
LD_EXPORT(LDBoolean)
LDTestDataUpdate(struct LDTestData * testData, struct LDFlagBuilder *flagBuilder);

/**
 * @brief Create a LDDataSource instance to be used in a client config.
 * @param[in] testData the LDTestData instance
 * @return a data source to be used with LDConfigSetDataSource
 * @see LDConfigSetDataSource
 */
LD_EXPORT(struct LDDataSource *)
LDTestDataCreateDataSource(struct LDTestData *);

/**
 * @brief A shortcut for setting the flag to use the standard boolean configuration.
 *
 * This is the default for all new flags created with LDTestDataFlag(). The flag
 * will have two variations, @c true and @c false (in that order); it will return
 * @c false whenever targeting is off, and @c true when targeting is on if no other
 * settings specify otherwise.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagBuilderBooleanFlag(struct LDFlagBuilder *flagBuilder);

/**
 * @brief Sets targeting to be on or off for this flag.
 *
 * The effect of this depends on the rest of the flag configuration, just as it does on the
 * real LaunchDarkly dashboard. In the default configuration that you get from calling
 * LDTestDataFlag() with a new flag key, the flag will return @c false
 * whenever targeting is off, and @c true when targeting is on.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] on LDBooleanTrue if targeting should be on
 * @return void
 */
LD_EXPORT(void)
LDFlagBuilderOn(struct LDFlagBuilder *flagBuilder, LDBoolean on);

/**
 * @brief Specifies the index of the fallthrough variation.
 *
 * The fallthrough is the variation that is returned if targeting is on
 * and the user was not matched by a more specific target or rule.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] variationIndex the desired fallthrough variation: 0 for the first, 1 for the second, etc.
 * @return void
 */
LD_EXPORT(void)
LDFlagBuilderFallthroughVariation(struct LDFlagBuilder *flagBuilder, int variationIndex);

/**
  * @brief Specifies the fallthrough variation for a boolean flag.
  *
  * The fallthrough is the value that is returned if targeting is on
  * and the user was not matched by a more specific target or rule.
  *
  * If the flag was previously configured with other variations, this also changes it to a
  * boolean flag.
  *
  * @param[in] flagBuilder The flag configuration builder.
  * @param[in] value true if the flag should return true by default when targeting is on
  * @return True on success, False on failure.
  */
LD_EXPORT(LDBoolean)
LDFlagBuilderFallthroughVariationBoolean(struct LDFlagBuilder *flagBuilder, LDBoolean value);

/**
 * @brief Specifies the index of the off variation.
 *
 * This is the variation that is returned whenever targeting is off.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] variationIndex the desired fallthrough variation: 0 for the first, 1 for the second, etc.
 * @return void
 */
LD_EXPORT(void)
LDFlagBuilderOffVariation(struct LDFlagBuilder *flagBuilder, int variationIndex);

/**
 * @brief Specifies the off variation for a boolean flag.
 *
 * This is the variation that is returned whenever targeting is off.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] value true if the flag should return true by default when targeting is on
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagBuilderOffVariationBoolean(struct LDFlagBuilder *flagBuilder, LDBoolean value);


/**
 * @brief Sets the flag to always return the specified variation for all users.
 *
 * The variation is specified by number, out of whatever variation values have already been
 * defined. Targeting is switched on, and any existing targets or rules are removed. The fallthrough
 * variation is set to the specified value. The off variation is left unchanged.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] variationIndex the desired fallthrough variation: 0 for the first, 1 for the second, etc.
 * @return void
 */
LD_EXPORT(void)
LDFlagBuilderVariationForAllUsers(struct LDFlagBuilder *flagBuilder, int variationIndex);

/**
 * @brief Sets the flag to always return the specified boolean variation for all users.
 *
 * Targeting is switched on, any existing targets or rules are removed, and the flag's variations are
 * set to @c true and @c false. The fallthrough variation is set to the specified value. The off variation is
 * left unchanged.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] variation the desired true/false variation to be returned for all users
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagBuilderVariationForAllUsersBoolean(struct LDFlagBuilder *flagBuilder, LDBoolean value);

/**
 * @brief Sets the flag to always return the specified variation value for all users.
 *
 * The value may be of any JSON type, as defined by {@link json.h LDJSON}. This method changes the
 * flag to have only a single variation, which is this value, and to return the same
 * variation regardless of whether targeting is on or off. Any existing targets or rules
 * are removed.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] value the desired value to be returned for all users; Ownership of `value` is transferred.
 * @return void
 */
LD_EXPORT(void)
LDFlagBuilderValueForAllUsers(struct LDFlagBuilder *flagBuilder, struct LDJSON *value);

/**
 * @brief Sets the flag to return the specified variation for a specific user key when targeting
 * is on.
 *
 * This has no effect when targeting is turned off for the flag.
 *
 * The variation is specified by number, out of whatever variation values have already been
 * defined.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] userKey a user key
 * @param[in] variationIndex the desired variation to be returned for this user when targeting is on:
 *   0 for the first, 1 for the second, etc.
 * @return void
 */
LD_EXPORT(LDBoolean)
LDFlagBuilderVariationForUser(struct LDFlagBuilder *flagBuilder, const char *userKey, int variationIndex);

/**
 * @brief Sets the flag to return the specified boolean variation for a specific user key when
 * targeting is on.
 *
 * This has no effect when targeting is turned off for the flag.
 *
 * If the flag was not already a boolean flag, this also changes it to a boolean flag.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] userKey a user key
 * @param[in] variation the desired true/false variation to be returned for this user when
 *   targeting is on
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagBuilderVariationForUserBoolean(struct LDFlagBuilder *flagBuilder, const char *userKey, LDBoolean value);

/**
 * Changes the allowable variation values for the flag.
 *
 * The value may be of any JSON type, as defined by {@link json.h LDJSON}. For instance, a boolean flag
 * normally has an @c LDArray containing @c LDNewBoolean(LDBooleanTrue), @c LDNewBoolean(LDBooleanFalse);
 * a string-valued flag might have an @c LDArray containing @c LDNewText("red"), @c LDNewText("green"); etc.
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] variations the desired variations; Ownership of `variations` is transferred.
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagBuilderVariations(struct LDFlagBuilder *flagBuilder, struct LDJSON *variations);

/**
 * @brief Starts defining a flag rule, using the "is one of" operator.
 *
 * For example, this creates a rule that returns @c true if the name is "Patsy" or "Edina":
 *
 * @code{.c}
 * struct LDJSON *names;
 * struct LDFlagBuilder *flag;
 * struct LDFlagRuleBuilder *rule;
 *
 * flag = LDTestDataFlag(testData, "flag");
 * names = LDNewArray();
 * LDArrayPush(names, LDNewText("Patsy"));
 * LDArrayPush(names, LDNewText("Edina"));
 * rule = LDFlagBuilderIfMatch(flag, "name", names);
 * LDFlagRuleBuilderThenReturnBoolean(rule, LDBooleanTrue);
 * @endcode
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] attribute the user attribute to match against
 * @param[in] values values to compare to
 * @return a LDFlagRuleBuilder; call {@link LDFlagRuleBuilderThenReturn} or
 *   {@link LDFlagRuleBuilderThenReturnBoolean} to finish the rule, or add more tests with another
 *   method like {@link LDFlagRuleBuilderAndMatch}; Ownership is not transferred.
 */
LD_EXPORT(struct LDFlagRuleBuilder *)
LDFlagBuilderIfMatch(struct LDFlagBuilder *flagBuilder, const char *const attribute, struct LDJSON *values);

/**
 * @brief Starts defining a flag rule, using the "is not one of" operator.
 *
 * For example, this creates a rule that returns @c true if the name is neither "Saffron" nor "Bubble":
 *
 * @code{.c}
 * struct LDJSON *names;
 * struct LDFlagBuilder *flag;
 * struct LDFlagRuleBuilder *rule;
 *
 * flag = LDTestDataFlag(testData, "flag");
 * names = LDNewArray();
 * LDArrayPush(names, LDNewText("Saffron"));
 * LDArrayPush(names, LDNewText("Bubble"));
 * rule = LDFlagBuilderIfNotMatch(flag, "name", names);
 * LDFlagRuleBuilderThenReturnBoolean(rule, LDBooleanTrue);
 * @endcode
 *
 * @param[in] flagBuilder The flag configuration builder.
 * @param[in] attribute the user attribute to match against
 * @param[in] values values to compare to
 * @return a LDFlagRuleBuilder; call {@link LDFlagRuleBuilderThenReturn} or
 *   {@link LDFlagRuleBuilderThenReturnBoolean} to finish the rule, or add more tests with another
 *   method like {@link LDFlagRuleBuilderAndMatch}; Ownership is not transferred.
 */
LD_EXPORT(struct LDFlagRuleBuilder *)
LDFlagBuilderIfNotMatch(struct LDFlagBuilder *flagBuilder, const char *const attribute, struct LDJSON *values);

/**
 * @brief Adds another clause, using the "is one of" operator.
 *
 * For example, this creates a rule that returns @c true if the name is "Patsy" and the
 * country is "gb":
 *
 * @code{.c}
 * struct LDFlagBuilder *flag;
 * struct LDFlagRuleBuilder *rule;
 *
 * flag = LDTestDataFlag(testData, "flag");
 * rule = LDFlagBuilderIfMatch(flag, "name", LDNewText("Patsy"));
 * LDFlagRuleBuilderAndMatch(rule, "country", LDNewText("gb"));
 * LDFlagRuleBuilderThenReturnBoolean(rule, LDBooleanTrue);
 * @endcode
 *
 * @param[in] ruleBuilder the rule builder
 * @param[in] attribute the user attribute to match against
 * @param[in] values values to compare to
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagRuleBuilderAndMatch(struct LDFlagRuleBuilder *ruleBuilder, const char *const attribute, struct LDJSON *values);

/**
 * @brief Adds another clause, using the "is not one of" operator.
 *
 * For example, this creates a rule that returns @c true if the name is "Patsy" and the
 * country is not "gb":
 *
 * @code{.c}
 * struct LDFlagBuilder *flag;
 * struct LDFlagRuleBuilder *rule;
 *
 * flag = LDTestDataFlag(testData, "flag");
 * rule = LDFlagBuilderIfMatch(flag, "name", LDNewText("Patsy"));
 * LDFlagRuleBuilderAndNotMatch(rule, "country", LDNewText("gb"));
 * LDFlagRuleBuilderThenReturnBoolean(rule, LDBooleanTrue);
 * @endcode
 *
 * @param[in] ruleBuilder the rule builder
 * @param[in] attribute the user attribute to match against
 * @param[in] values values to compare to
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagRuleBuilderAndNotMatch(struct LDFlagRuleBuilder *ruleBuilder, const char *const attribute, struct LDJSON *values);

/**
 * @brief Finishes defining the rule, specifying the result as a variation index.
 *
 * @param[in] ruleBuilder the rule builder
 * @param[in] variationIndex the variation to return if the rule matches the user: 0 for the first, 1
 *   for the second, etc.
 * @return void
 */
LD_EXPORT(void)
LDFlagRuleBuilderThenReturn(struct LDFlagRuleBuilder *ruleBuilder, int variationIndex);

/**
 * @brief Finishes defining the rule, specifying the result value as a boolean.
 *
 * @param[in] ruleBuilder the rule builder
 * @param[in] variation the value to return if the rule matches the user
 * @return True on success, False on failure.
 */
LD_EXPORT(LDBoolean)
LDFlagRuleBuilderThenReturnBoolean(struct LDFlagRuleBuilder *ruleBuilder, LDBoolean value);
