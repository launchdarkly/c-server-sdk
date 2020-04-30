#include <stdio.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "user.h"
#include "utility.h"

static struct LDUser *
constructBasic()
{
    struct LDJSON *custom;
    struct LDUser *user;

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
    struct LDUser *user;

    LD_ASSERT(user = constructBasic());

    LDUserFree(user);
}

static void
serializeEmpty()
{
    struct LDJSON *json;
    struct LDUser *user;
    char *serialized;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(json = LDUserToJSON(NULL, user, false));
    LD_ASSERT(serialized = LDJSONSerialize(json));

    LD_ASSERT(strcmp(serialized, "{\"key\":\"abc\"}") == 0);

    LDUserFree(user);
    LDFree(serialized);
    LDJSONFree(json);
}

static void
serializeRedacted()
{
    struct LDJSON *json;
    struct LDUser *user;
    char *serialized;
    struct LDJSON *custom, *child;

    LD_ASSERT(user = LDUserNew("123"));

    LD_ASSERT(custom = LDNewObject());
    LD_ASSERT(child = LDNewNumber(42));
    LD_ASSERT(LDObjectSetKey(custom, "secret", child));
    LD_ASSERT(child = LDNewNumber(52));
    LD_ASSERT(LDObjectSetKey(custom, "notsecret", child));

    LDUserSetCustom(user, custom);
    LD_ASSERT(LDUserAddPrivateAttribute(user, "secret"));

    LD_ASSERT(json = LDUserToJSON(NULL, user, true));
    LD_ASSERT(serialized = LDJSONSerialize(json));

    LD_ASSERT(strcmp(serialized, "{\"key\":\"123\",\"custom\":{\"notsecret\":52},\"privateAttrs\":[\"secret\"]}") == 0);

    LDUserFree(user);
    LDFree(serialized);
    LDJSONFree(json);
}

static void
serializeAll()
{
    struct LDJSON *json;
    struct LDUser *user;
    char *serialized;

    LD_ASSERT(user = constructBasic());

    LD_ASSERT(json = LDUserToJSON(NULL, user, false));
    LD_ASSERT(serialized = LDJSONSerialize(json));

    LD_ASSERT(strcmp(serialized, "{\"key\":\"abc\",\"secondary\":\"unknown202\",\"ip\":\"127.0.0.1\",\"firstName\":\"Jane\",\"lastName\":\"Doe\",\"email\":\"janedoe@launchdarkly.com\",\"name\":\"Jane\",\"avatar\":\"unknown101\",\"custom\":{}}") == 0);

    LDUserFree(user);
    LDFree(serialized);
    LDJSONFree(json);
}

int
main()
{
    setbuf(stdout, NULL);

    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    constructNoSettings();
    constructAllSettings();
    serializeEmpty();
    serializeRedacted();
    serializeAll();

    return 0;
}
