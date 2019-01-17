#include <stdio.h>
#include <stdlib.h>

#include "ldapi.h"

int
main()
{
    struct LDConfig *const config = LDConfigNew("SDK_KEY");

    struct LDClient *const client = LDClientInit(config, 0);

    if (client) {
        printf("LDClientInit Success\n");

        LDClientClose(client);
    } else {
        printf("LDClientInit Failed\n");
    }

    return 0;
}
