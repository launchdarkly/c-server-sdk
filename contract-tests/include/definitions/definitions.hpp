#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <unordered_map>

namespace nlohmann {

template <class T>
void to_json(nlohmann::json& j, const std::optional<T>& v)
{
    if (v.has_value())
        j = *v;
    else
        j = nullptr;
}

template <class T>
void from_json(const nlohmann::json& j, std::optional<T>& v)
{
    if (j.is_null())
        v = std::nullopt;
    else
        v = j.get<T>();
}
} // namespace nlohmann

namespace ld {

struct SDKConfigStreamingParams {
    std::optional<std::string> baseUri;
    std::optional<uint32_t> initialRetryDelayMs;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SDKConfigStreamingParams, baseUri, initialRetryDelayMs);

struct SDKConfigEventParams {
    std::optional<std::string> baseUri;
    std::optional<uint32_t> capacity;
    std::optional<bool> enableDiagnostics;
    std::optional<bool> allAttributesPrivate;
    std::vector<std::string> globalPrivateAttributes;
    std::optional<uint32_t> flushIntervalMs;
    std::optional<bool> inlineUsers;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SDKConfigEventParams, baseUri, capacity, enableDiagnostics,
                                                allAttributesPrivate, globalPrivateAttributes, flushIntervalMs,
                                                inlineUsers);
struct SDKConfigServiceEndpointsParams {
    std::optional<std::string> streaming;
    std::optional<std::string> polling;
    std::optional<std::string> events;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SDKConfigServiceEndpointsParams, streaming, polling, events);


struct SDKConfigParams {
    std::string credential;
    std::optional<uint32_t> startWaitTimeMs;
    std::optional<bool> initCanFail;
    std::optional<SDKConfigStreamingParams> streaming;
    std::optional<SDKConfigEventParams> events;
    std::optional<SDKConfigServiceEndpointsParams> serviceEndpoints;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(SDKConfigParams, credential, startWaitTimeMs, initCanFail,
                                                streaming, events, serviceEndpoints);

struct CreateInstanceParams {
    SDKConfigParams configuration;
    std::string tag;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CreateInstanceParams, configuration, tag);

enum class ValueType {
    Bool = 1,
    Int,
    Double,
    String,
    Any,
    Unspecified
};
NLOHMANN_JSON_SERIALIZE_ENUM(ValueType, {
    { ValueType::Bool, "bool" },
    { ValueType::Int, "int" },
    { ValueType::Double, "double" },
    { ValueType::String, "string" },
    { ValueType::Any, "any" },
    { ValueType::Unspecified, "" }
})

struct User {
    std::string key;
    std::optional<bool> anonymous;
    std::optional<std::string> ip;
    std::optional<std::string> firstName;
    std::optional<std::string> lastName;
    std::optional<std::string> email;
    std::optional<std::string> name;
    std::optional<std::string> avatar;
    std::optional<std::string> country;
    std::optional<std::string> secondary;
    std::optional<nlohmann::json> custom;
    std::optional<std::vector<std::string>> privateAttributeNames;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(User, key, anonymous, ip, firstName, lastName, email, name, avatar,
                                                country, secondary, custom, privateAttributeNames);

struct EvaluateFlagParams {
    std::string flagKey;
    User user;
    ValueType valueType;
    nlohmann::json defaultValue;
    bool detail;
    EvaluateFlagParams();
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvaluateFlagParams, flagKey, user, valueType, defaultValue, detail);

struct EvaluateFlagResponse {
    nlohmann::json value;
    std::optional<uint32_t> variationIndex;
    std::optional<nlohmann::json> reason;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvaluateFlagResponse, value, variationIndex, reason);


struct EvaluateAllFlagParams {
    User user;
    std::optional<bool> withReasons;
    std::optional<bool> clientSideOnly;
    std::optional<bool> detailsOnlyForTrackedFlags;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvaluateAllFlagParams, user, withReasons, clientSideOnly,
                                                detailsOnlyForTrackedFlags);
struct EvaluateAllFlagsResponse {
    nlohmann::json state;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvaluateAllFlagsResponse, state);

struct CustomEventParams {
    std::string eventKey;
    User user;
    std::optional<nlohmann::json> data;
    std::optional<bool> omitNullData;
    std::optional<double> metricValue;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CustomEventParams, eventKey, user, data, omitNullData, metricValue);

struct IdentifyEventParams {
    User user;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(IdentifyEventParams, user);

struct AliasEventParams {
    User user;
    User previousUser;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AliasEventParams, user, previousUser);

enum class Command {
    Unknown = -1,
    EvaluateFlag,
    EvaluateAllFlags,
    IdentifyEvent,
    CustomEvent,
    AliasEvent,
    FlushEvents
};
NLOHMANN_JSON_SERIALIZE_ENUM(Command, {
    { Command::Unknown, nullptr},
    { Command::EvaluateFlag, "evaluate" },
    { Command::EvaluateAllFlags, "evaluateAll" },
    { Command::IdentifyEvent, "identifyEvent" },
    { Command::CustomEvent, "customEvent" },
    { Command::AliasEvent, "aliasEvent" },
    { Command::FlushEvents, "flushEvents" }
})

struct CommandParams {
    Command command;
    std::optional<EvaluateFlagParams> evaluate;
    std::optional<EvaluateAllFlagParams> evaluateAll;
    std::optional<CustomEventParams> customEvent;
    std::optional<IdentifyEventParams> identifyEvent;
    std::optional<AliasEventParams> aliasEvent;
    CommandParams();
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CommandParams, command, evaluate, evaluateAll, customEvent,
                                                identifyEvent, aliasEvent);
} // namespace ld
