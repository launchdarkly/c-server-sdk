#include <string.h>
#include <stdlib.h>

#include <launchdarkly/api.h>

#include "misc.h"

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

double
LDi_normalize(const double n, const double nmin, const double nmax,
    const double omin, const double omax)
{
    return (n - nmin) * (omax - omin) / (nmax - nmin) + omin;
}
