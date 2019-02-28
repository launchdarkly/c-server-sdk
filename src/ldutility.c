#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"

bool
LDSetString(char **const target, const char *const value)
{
    if (value) {
        char *const tmp = LDStrDup(value);

        if (tmp) {
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
