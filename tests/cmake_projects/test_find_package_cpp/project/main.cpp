#include <launchdarkly/api.h>
#include <stdio.h>

// Testing that experimental headers are shipped in the public API
#include <launchdarkly/experimental/cpp/value.hpp>

using namespace launchdarkly;

int main() {
    struct LDConfig *config;
    struct LDClient *client;

    Value v = Value{true};

    config = LDConfigNew("random-key");
    client = LDClientInit(config, 100);
    printf("%d\n", LDClientIsInitialized(client));

    return 0;
}
