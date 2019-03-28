#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"

bool
LDSetString(char **const target, const char *const value)
{
    if (value) {
        char *tmp;

        if ((tmp = LDStrDup(value))) {
            LDFree(*target);

            *target = tmp;

            return true;
        } else {
            return false;
        }
    } else {
        LDFree(*target);

        *target = NULL;

        return true;
    }
}
