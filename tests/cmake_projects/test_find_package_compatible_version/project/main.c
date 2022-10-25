#include <launchdarkly/api.h>
#include <stdio.h>

int main() {
    struct LDConfig *config;
    struct LDClient *client;

    config = LDConfigNew("random-key");
    client = LDClientInit(config, 100);
    printf("%d\n", LDClientIsInitialized(client));

    return 0;
}
