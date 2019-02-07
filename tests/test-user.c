#include "ldinternal.h"

#include <stdio.h>

static struct LDUser *
constructBasic()
{
    struct LDJSON *custom = NULL;
    struct LDUser *user = NULL;

    LD_ASSERT(user = LDUserNew("abc"));

    LDUserSetAnonymous(user, false);
    LD_ASSERT(LDUserSetIP(user, "127.0.0.1"));
    LD_ASSERT(LDUserSetFirstName(user, "Jane"));
    LD_ASSERT(LDUserSetLastName(user, "Doe"));
    LD_ASSERT(LDUserSetEmail(user, "janedoe@launchdarkly.com"));
    LD_ASSERT(LDUserSetName(user, "Jane"));
    LD_ASSERT(LDUserSetAvatar(user, "unknown101"));
    LD_ASSERT(LDUserSetSecondary(user, "unknown202"));

    LD_ASSERT(custom = LDNewObject());

    LDUserSetCustom(user, custom);

    LD_ASSERT(LDUserAddPrivateAttribute(user, "secret"));

    return user;
}

static void
constructNoSettings()
{
    struct LDUser *const user = LDUserNew("abc");
    LD_ASSERT(user);
    LDUserFree(user);
}

static void
constructAllSettings()
{
    struct LDUser *user = NULL;

    LD_ASSERT(user = constructBasic());

    LDUserFree(user);
}

static void
serializeEmpty()
{
    cJSON *json = NULL;
    struct LDUser *user = NULL;
    char *serialized = NULL;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(json = LDUserToJSON(NULL, user, false));
    LD_ASSERT(serialized = cJSON_PrintUnformatted(json));

    LD_ASSERT(strcmp(serialized, "{\"key\":\"abc\"}") == 0);

    LDUserFree(user);
    free(serialized);
    cJSON_Delete(json);
}

static void
serializeRedacted()
{
    cJSON *json = NULL;
    struct LDUser *user = NULL;
    char *serialized = NULL;
    struct LDJSON *custom = NULL, *child = NULL;

    LD_ASSERT(user = LDUserNew("123"));

    LD_ASSERT(custom = LDNewObject());
    LD_ASSERT(child = LDNewNumber(42));
    LD_ASSERT(LDObjectSetKey(custom, "secret", child));
    LD_ASSERT(child = LDNewNumber(52));
    LD_ASSERT(LDObjectSetKey(custom, "notsecret", child));

    LDUserSetCustom(user, custom);
    LD_ASSERT(LDUserAddPrivateAttribute(user, "secret"));

    LD_ASSERT(json = LDUserToJSON(NULL, user, true));
    LD_ASSERT(serialized = cJSON_PrintUnformatted(json));

    LD_ASSERT(strcmp(serialized, "{\"key\":\"123\",\"custom\":{\"notsecret\":52},\"privateAttrs\":[\"secret\"]}") == 0);

    LDUserFree(user);
    free(serialized);
    cJSON_Delete(json);
}

static void
serializeAll()
{
    cJSON *json = NULL;
    struct LDUser *user = NULL;
    char *serialized = NULL;

    LD_ASSERT(user = constructBasic());

    LD_ASSERT(json = LDUserToJSON(NULL, user, false));
    LD_ASSERT(serialized = cJSON_PrintUnformatted(json));

    LD_ASSERT(strcmp(serialized, "{\"key\":\"abc\",\"secondary\":\"unknown202\",\"ip\":\"127.0.0.1\",\"firstName\":\"Jane\",\"lastName\":\"Doe\",\"email\":\"janedoe@launchdarkly.com\",\"name\":\"Jane\",\"avatar\":\"unknown101\",\"custom\":{}}") == 0);

    LDUserFree(user);
    free(serialized);
    cJSON_Delete(json);
}

int
main()
{
    setbuf(stdout, NULL);

    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    LDi_log(LD_LOG_INFO, "constructNoSettings()"); constructNoSettings();
    LDi_log(LD_LOG_INFO, "constructAllSettings()"); constructAllSettings();
    LDi_log(LD_LOG_INFO, "serializeEmpty()"); serializeEmpty();
    LDi_log(LD_LOG_INFO, "serializeRedacted()"); serializeRedacted();
    LDi_log(LD_LOG_INFO, "serializeAll()"); serializeAll();

    return 0;
}
