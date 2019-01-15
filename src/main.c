#include <stdio.h>

#include "ldapi.h"

int
main()
{
    struct LDConfig *const config = LDConfigNew("");
    printf("ld\n");
    LDConfigFree(config);
}
