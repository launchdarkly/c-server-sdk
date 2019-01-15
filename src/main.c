#include <stdio.h>

#include "ldapi.h"

int
main()
{
    struct LDConfig *const config = LDConfigNew("SDK_KEY");

    struct LDClient *const client = LDClientInit(config, 0);

    printf("ld\n");

    LDClientClose(client);
}
