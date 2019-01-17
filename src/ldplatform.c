#include "ldinternal.h"

bool
LDi_createthread(ld_thread_t *const thread, THREAD_RETURN (*const routine)(void *), void *const argument)
{
    #ifdef _WIN32
        ld_thread_t attempt = CreateThread(NULL, 0, routine, argument, 0, NULL);

        if (attempt == NULL) {
            return false;
        } else {
            *thread = attempt;

            return true;
        }
    #else
        return pthread_create(thread, NULL, routine, argument) == 0;
    #endif
}
