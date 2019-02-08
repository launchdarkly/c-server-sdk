#include <string.h>
#include <stdlib.h>

#include "ldinternal.h"

bool
LDSetString(char **const target, const char *const value)
{
    if (value) {
        char *const tmp = strdup(value);

        if (tmp) {
            free(*target); *target = tmp; return true;
        } else {
            return false;
        }
    } else {
        free(*target); *target = NULL; return true;
    }
}
