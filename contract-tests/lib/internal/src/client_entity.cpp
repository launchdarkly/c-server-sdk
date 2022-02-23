#include "service/client_entity.hpp"

namespace ld {

ClientEntity::ClientEntity(struct LDClient *client) : m_client{client} {}

ClientEntity::~ClientEntity() { LDClientClose(m_client); }

JsonOrError ClientEntity::do_command(const CommandParams &params) {
    switch (params.command) {
        case Command::EvaluateFlag:
            if (!params.evaluate) {
                return make_client_error("Evaluate params should be set");
            }
            return this->evaluate(*params.evaluate);

        case Command::EvaluateAllFlags:
            if (!params.evaluateAll) {
                return make_client_error("EvaluateAll params should be set");
            }
            return this->evaluateAll(*params.evaluateAll);

        case Command::IdentifyEvent:
            if (!params.identifyEvent) {
                return make_client_error("IdentifyEvent params should be set");
            }
            if (!this->identify(*params.identifyEvent)) {
                return make_server_error("Failed to generate identify event");
            }
            return nlohmann::json{};

        case Command::CustomEvent:
            if (!params.customEvent) {
                return std::string{"CustomEvent params should be set"};
            }
            if (!this->customEvent(*params.customEvent)) {
                return make_server_error("Failed to generate custom event");
            }
            return nlohmann::json{};

        case Command::AliasEvent:
            if (!params.aliasEvent) {
                return make_client_error("AliasEvent params should be set");
            }
            if (!this->aliasEvent(*params.aliasEvent)) {
                return make_server_error("Failed to generate alias event");
            }
            return nlohmann::json{};

        case Command::FlushEvents:
            if (!this->flush()) {
                return make_server_error("Failed to flush events");
            }
            return nlohmann::json{};

        default:
            return make_server_error("Command not supported");
    }
}

JsonOrError ClientEntity::evaluate(const EvaluateFlagParams &params) {
    UserPtr user = make_user(params.user);
    if (!user) {
        return make_server_error("Unable to construct user");
    }

    struct LDDetails details{};
    LDDetailsInit(&details);

    const char* key = params.flagKey.c_str();

    auto defaultVal = params.defaultValue;

    EvaluateFlagResponse result;

    switch (params.valueType) {
        case ValueType::Bool:
            result.value = LDBoolVariation(
                    m_client,
                    user.get(),
                    key,
                    defaultVal.get<LDBoolean>(),
                    &details
            ) != 0;
            break;
        case ValueType::Int:
            result.value = LDIntVariation(
                    m_client,
                    user.get(),
                    key,
                    defaultVal.get<int>(),
                    &details
            );
            break;
        case ValueType::Double:
            result.value = LDDoubleVariation(
                    m_client,
                    user.get(),
                    key,
                    defaultVal.get<double>(),
                    &details
            );
            break;
        case ValueType::String: {
            char *evaluation = LDStringVariation(
                    m_client,
                    user.get(),
                    key,
                    defaultVal.get<std::string>().c_str(),
                    &details
            );
            result.value = std::string{evaluation};
            LDFree(evaluation);
            break;
        }
        case ValueType::Any:
        case ValueType::Unspecified: {
            struct LDJSON* fallback = LDJSONDeserialize(defaultVal.dump().c_str());
            if (!fallback) {
                return make_server_error("JSON appears to be invalid");
            }
            struct LDJSON* evaluation = LDJSONVariation(
                    m_client,
                    user.get(),
                    key,
                    fallback,
                    &details
            );
            LDJSONFree(fallback);

            char *evaluationString = LDJSONSerialize(evaluation);
            LDJSONFree(evaluation);

            if (!evaluationString) {
                return make_server_error("Failed to serialize JSON");
            }
            result.value = nlohmann::json::parse(evaluationString);
            LDFree(evaluationString);
            break;
        }
        default:
            LDDetailsClear(&details);
            return make_client_error("Unrecognized variation type");

    }

    if (params.detail) {
        result.variationIndex = details.hasVariation ? std::optional(details.variationIndex) : std::nullopt;
        std::variant<nlohmann::json, Error> reason = extract_reason(&details);
        LDDetailsClear(&details);
        if (std::holds_alternative<nlohmann::json>(reason)) {
            result.reason = std::get<nlohmann::json>(reason);
        } else {
            return std::get<Error>(reason);
        }
    }

    return result;
}

JsonOrError ClientEntity::evaluateAll(const EvaluateAllFlagParams &params) {
    auto user = make_user(params.user);
    if (!user) {
        return make_server_error("Unable to construct user");
    }

    uint32_t options = LD_ALLFLAGS_DEFAULT;
    if (params.detailsOnlyForTrackedFlags.value_or(false)) {
        options |= LD_DETAILS_ONLY_FOR_TRACKED_FLAGS;
    }
    if (params.clientSideOnly.value_or(false)) {
        options |= LD_CLIENT_SIDE_ONLY;
    }
    if (params.withReasons.value_or(false)) {
        options |= LD_INCLUDE_REASON;
    }

    struct LDAllFlagsState *state = LDAllFlagsState(m_client, user.get(), options);
    if (!state) {
        return make_server_error("LDAllFlagsState invocation failed");
    }

    char *jsonStr = LDAllFlagsStateSerializeJSON(state);
    LDAllFlagsStateFree(state);

    EvaluateAllFlagsResponse rsp {
        .state = nlohmann::json::parse(jsonStr)
    };

    LDFree(jsonStr);
    return rsp;
}


bool ClientEntity::identify(const IdentifyEventParams &params) {
    auto user = make_user(params.user);
    if (!user) {
        return false;
    }
    return static_cast<bool>(LDClientIdentify(m_client, user.get()));
}

bool ClientEntity::customEvent(const CustomEventParams &params) {
    auto user = make_user(params.user);
    if (!user) {
        return false;
    }

    struct LDJSON* json = params.data ?
          LDJSONDeserialize(params.data->dump().c_str()) :
          LDNewNull();

    if (params.metricValue) {
        return static_cast<bool>(LDClientTrackMetric(
                m_client,
                params.eventKey.c_str(),
                user.get(),
                json,
                *params.metricValue
        ));
    }
    if (params.data) {
        return static_cast<bool>(LDClientTrack(
                m_client,
                params.eventKey.c_str(),
                user.get(),
                json
        ));
    }

    return false;
}

bool ClientEntity::aliasEvent(const AliasEventParams &params) {
    auto user = make_user(params.user);
    if (!user) {
        return false;
    }
    auto previousUser = make_user(params.previousUser);
    if (!previousUser) {
        return false;
    }
    return static_cast<bool>(LDClientAlias(m_client, user.get(), previousUser.get()));
}

bool ClientEntity::flush() {
    return static_cast<bool>(LDClientFlush(m_client));
}


std::unique_ptr<ClientEntity> ClientEntity::from(const CreateInstanceParams &params, std::chrono::milliseconds defaultStartWaitTime) {
    struct LDConfig *config = make_config(params.configuration);
    if (!config) {
        return nullptr;
    }

    auto startWaitTime = std::chrono::milliseconds(
            params.configuration.startWaitTimeMs.value_or(defaultStartWaitTime.count())
    );

    struct LDClient *client = LDClientInit(config, startWaitTime.count());

    if (client == nullptr) {
        LDConfigFree(config);
        return nullptr;
    }

    return std::make_unique<ClientEntity>(client);
}

std::unique_ptr<struct LDUser, std::function<void(struct LDUser*)>> make_user(const User &obj) {
    struct LDUser* user = LDUserNew(obj.key.c_str());
    if (!user) {
        return nullptr;
    }
    if (obj.anonymous) {
        LDUserSetAnonymous(user, static_cast<LDBoolean>(*obj.anonymous));
    }
    if (obj.ip) {
        LDUserSetIP(user, obj.ip->c_str());
    }
    if (obj.firstName) {
        LDUserSetFirstName(user, obj.firstName->c_str());
    }
    if (obj.lastName) {
        LDUserSetLastName(user, obj.lastName->c_str());
    }
    if (obj.email) {
        LDUserSetEmail(user, obj.email->c_str());
    }
    if (obj.name) {
        LDUserSetName(user, obj.name->c_str());
    }
    if (obj.avatar) {
        LDUserSetAvatar(user, obj.avatar->c_str());
    }
    if (obj.country) {
        LDUserSetCountry(user, obj.country->c_str());
    }
    if (obj.secondary) {
        LDUserSetSecondary(user, obj.secondary->c_str());
    }
    if (obj.custom) {
        struct LDJSON* json = LDJSONDeserialize(obj.custom->dump().c_str());
        LDUserSetCustom(user, json); // takes ownership of json
    }

    return {user, [](struct LDUser* user){
        LDUserFree(user);
    }};
}

struct LDConfig * make_config(const SDKConfigParams &in) {

    struct LDConfig *out = LDConfigNew(in.credential.c_str());
    if (!out) {
        return nullptr;
    }

    if (in.streaming && in.streaming->baseUri) {
        LDConfigSetStreamURI(out, in.streaming->baseUri->c_str());
    }

    if (!in.events) {
        LDConfigSetSendEvents(out, LDBooleanFalse);
    } else {
        const SDKConfigEventParams& events = *in.events;

        if (events.baseUri) {
            LDConfigSetEventsURI(out, events.baseUri->c_str());
        }

        if (events.allAttributesPrivate) {
            LDConfigSetAllAttributesPrivate(out, static_cast<LDBoolean>(*events.allAttributesPrivate));
        }

        for (const std::string& attr : events.globalPrivateAttributes) {
            LDConfigAddPrivateAttribute(out, attr.c_str());
        }

        if (events.inlineUsers) {
            LDConfigInlineUsersInEvents(out, static_cast<LDBoolean>(*events.inlineUsers));
        }

        if (events.capacity) {
            LDConfigSetEventsCapacity(out, *events.capacity);
        }

        if (events.flushIntervalMs) {
            LDConfigSetUserKeysFlushInterval(out, *events.flushIntervalMs);
        }
    }
    return out;
}


std::variant<nlohmann::json, Error> extract_reason(const LDDetails *details) {
    struct LDJSON* jsonObj = LDReasonToJSON(details);
    if (!jsonObj) {
        return make_server_error("Unable to map evaluation reason to JSON representation");
    }
    char* jsonStr = LDJSONSerialize(jsonObj);
    if (!jsonStr) {
        LDJSONFree(jsonObj);
        return make_server_error("Unable to serialize evaluation reason JSON");
    }
    nlohmann::json j = nlohmann::json::parse(jsonStr);
    LDFree(jsonStr);
    return j;
}

} // namespace ld
