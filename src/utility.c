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

bool
LDi_randomHex(char *const buffer, const size_t bufferSize)
{
    const char *const alphabet = "0123456789ABCDEF";

    for (size_t i = 0; i < bufferSize; i++) {
        unsigned int rng = 0;
        if (LDi_random(&rng)) {
            buffer[i] = alphabet[rng % 16];
        }
        else {
            return false;
        }
    }

    return true;
}

bool
LDi_UUIDv4(char *const buffer)
{
    if (!LDi_randomHex(buffer, LD_UUID_SIZE)) {
        return false;
    }

    buffer[8]  = '-';
    buffer[13] = '-';
    buffer[18] = '-';
    buffer[23] = '-';

    return true;
}
