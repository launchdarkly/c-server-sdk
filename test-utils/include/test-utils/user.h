#pragma once

#include <launchdarkly/api.h>

class User {
public:
    User(std::string userKey) {
        user = LDUserNew(userKey.c_str());
        if(!user) {
            throw "User failed to initialize";
        }
    }

    ~User() {
        if(user) {
            LDUserFree(user);
        }
    }

    User& SetName(const char *name) {
        LDUserSetName(user, name);
        return *this;
    }

    User& SetCountry(const char *country) {
        LDUserSetCountry(user, country);
        return *this;
    }

    struct LDUser * RawPtr() {
        return user;
    }

private:
    struct LDUser *user;
};
