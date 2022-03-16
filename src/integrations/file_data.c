#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <launchdarkly/api.h>
#include <launchdarkly/integrations/file_data.h>
#include <launchdarkly/memory.h>
#include <launchdarkly/logging.h>

#include "assertion.h"
#include "file_data.h"
#include "store.h"
#include "data_source.h"
#include "json_internal_helpers.h"

struct FileDataContext {
    struct LDJSON *set;
};

static LDBoolean
start(void *const context, struct LDStore *const store)
{
    struct FileDataContext *fileDataContext = (struct FileDataContext *)context;
    return LDStoreInit(store, fileDataContext->set);
}

static void
close(void *const context)
{
    (void)(context);
}

static void
destructor(void *const context)
{
    LDFree(context);
}

static char *
readFile(const char * filename) {
    FILE *file;
    char *buffer;
    unsigned int allocatedBytes = 512; /* only support upto 4gb files */
    unsigned int bytesRead = 0;

    if (!(buffer = (char *)LDAlloc(allocatedBytes))) {
        return NULL;
    }

    file = fopen(filename, "r");
    if(!file) {
        LDFree(buffer);
        return NULL;
    }

    while(1) {
        char c;

        if(bytesRead >= allocatedBytes) {
            char *tmpBuffer;
            char buf[128];

            allocatedBytes = allocatedBytes * 2;
            snprintf(buf, 128, "reallocating to %d bytes\n", allocatedBytes);
            LD_LOG(LD_LOG_INFO, buf);
            if (!(tmpBuffer = (char *)LDRealloc(buffer, allocatedBytes))) {
                fclose(file);
                LDFree(buffer);
                return NULL;
            }
            buffer = tmpBuffer;
        }

        c = fgetc(file);
        if (c == EOF) {
            buffer[bytesRead] = 0;
            break;
        }

        buffer[bytesRead] = c;
        bytesRead++;
    }

    fclose(file);

    return buffer;
}

struct LDJSON *
LDi_loadJSONFile(const char * filename) {
    struct LDJSON * json = NULL;
    char *buffer = readFile(filename);

    if(buffer) {
        json = LDJSONDeserialize(buffer);
        LDFree(buffer);
    }

    return json;
}

static struct LDJSON *
expandSimpleFlags(const struct LDJSON *json) {
    struct LDJSON* iter = NULL;
    struct LDJSON* flags = NULL;

    LD_ASSERT(LDJSONGetType(json) == LDObject);

    if(!(flags = LDNewObject())) {
        return NULL;
    }

    for (iter = LDGetIter(json); iter; iter = LDIterNext(iter)) {
        struct LDJSON *flag;

        if(!(flag = LDNewObject())) {
            goto fail;
        }

        { /* build variations */
            struct LDJSON *variations;
            struct LDJSON *variation;
            if(!(variations = LDNewArray())) {
                LDJSONFree(flag);
                goto fail;
            }
            if(!(variation = LDJSONDuplicate(iter))) {
                LDJSONFree(variations);
                LDJSONFree(flag);
                goto fail;
            }
            LDArrayPush(variations, variation);
            LDObjectSetKey(flag, "variations", variations);
        }

        { /* build fallthrough */
            struct LDJSON *fallthrough;
            if(!(fallthrough = LDNewObject())) {
                LDJSONFree(flag);
                goto fail;
            }

            if(!(LDObjectSetNumber(fallthrough, "variation", 0))) {
                LDJSONFree(fallthrough);
                LDJSONFree(flag);
                goto fail;
            }

            LDObjectSetKey(flag, "fallthrough", fallthrough);
        }

        if(!(LDObjectSetString(flag, "key", LDIterKey(iter)))) {
            LDJSONFree(flag);
            goto fail;
        }
        if(!(LDObjectSetBool(flag, "on", LDBooleanTrue))) {
            LDJSONFree(flag);
            goto fail;
        }
        if(!(LDObjectSetNumber(flag, "version", 1))) {
            LDJSONFree(flag);
            goto fail;
        }
        if(!(LDObjectSetString(flag, "salt", "salt"))) {
            LDJSONFree(flag);
            goto fail;
        }

        LDObjectSetKey(flags, LDIterKey(iter), flag);
    }

    return flags;

fail:
    LDJSONFree(flags);
    return NULL;
}

struct LDDataSource *
LDFileDataInit(int fileCount, const char **filenames)
{
    struct LDDataSource *dataSource;
    struct FileDataContext *context;

    struct LDJSON *set;
    struct LDJSON *flags;
    struct LDJSON *segments;

    int fileIndex;

    if (!(dataSource = (struct LDDataSource *)LDAlloc(sizeof(struct LDDataSource)))) {
        return NULL;
    }

    memset(dataSource, 0, sizeof(struct LDDataSource));

    if (!(context = (struct FileDataContext *)LDAlloc(sizeof(struct FileDataContext)))) {
        LDFree(dataSource);
        return NULL;
    }

    memset(context, 0, sizeof(struct FileDataContext));

    if(!(flags = LDNewObject())) {
        LDFree(context);
        LDFree(dataSource);
        return NULL;
    }

    if(!(segments = LDNewObject())) {
        LDJSONFree(flags);
        LDFree(context);
        LDFree(dataSource);
        return NULL;
    }

    for(fileIndex = 0; fileIndex < fileCount; fileIndex++) {
        struct LDJSON *json;
        const char *filename = filenames[fileIndex];

        if(!filename) {
            continue;
        }

        if (!(json = LDi_loadJSONFile(filename))) {
            char buf[128];
            snprintf(buf, 128, "Error opening file: %s\n", filename);
            LD_LOG(LD_LOG_TRACE, buf);
            continue;
        }

        if(LDJSONGetType(json) != LDObject) {
            char buf[128];
            snprintf(buf, 128, "No object found in file: %s\n", filename);
            LD_LOG(LD_LOG_TRACE, buf);
            LDJSONFree(json);
            continue;
        }

        { /* Handle flags key */
            struct LDJSON *tempFlags = NULL;

            if((tempFlags = LDObjectDetachKey(json, "flags"))) {
                struct LDJSON *temp;
                struct LDJSON *iter;

                for (iter = LDGetIter(tempFlags); iter; iter = LDIterNext(iter)) {
                    if(!LDObjectSetNumber(iter, "version", 1)) {
                        LDJSONFree(json);
                        goto fail;
                    }
                    if(!LDObjectSetString(iter, "salt", "salt")) {
                        LDJSONFree(json);
                        goto fail;
                    }
                }

                if(!(temp = LDNewObject())) {
                    LDJSONFree(json);
                    goto fail;
                }

                if(!LDObjectMerge(temp, tempFlags)) {
                    LDJSONFree(json);
                    LDJSONFree(temp);
                    LDJSONFree(tempFlags);
                    goto fail;
                }
                LDJSONFree(tempFlags);

                if(!LDObjectMerge(temp, flags)) {
                    LDJSONFree(json);
                    LDJSONFree(temp);
                    goto fail;
                }
                LDJSONFree(flags);

                flags = temp;
            }
        }

        { /* Handle flagValues key */
            struct LDJSON *tempFlagValues;
            struct LDJSON *expandedFlagValues;
            if((tempFlagValues = LDObjectLookup(json, "flagValues"))) {
                struct LDJSON *temp;

                if(!(expandedFlagValues = expandSimpleFlags(tempFlagValues))) {
                    LDJSONFree(json);
                    goto fail;
                }

                if(!(temp = LDNewObject())) {
                    LDJSONFree(json);
                    goto fail;
                }

                if(!LDObjectMerge(temp, expandedFlagValues)) {
                    LDJSONFree(expandedFlagValues);
                    LDJSONFree(json);
                    goto fail;
                }
                LDJSONFree(expandedFlagValues);

                if(!LDObjectMerge(temp, flags)) {
                    LDJSONFree(json);
                    LDJSONFree(temp);
                    goto fail;
                }
                LDJSONFree(flags);

                flags = temp;
            }
        }

        { /* Handle segments key */
            struct LDJSON *tempSegments;
            if((tempSegments = LDObjectDetachKey(json, "segments"))) {
                struct LDJSON *temp;

                if(!(temp = LDNewObject())) {
                    LDJSONFree(json);
                    goto fail;
                }

                if(!LDObjectMerge(temp, tempSegments)) {
                    LDJSONFree(json);
                    LDJSONFree(temp);
                    LDJSONFree(tempSegments);
                    goto fail;
                }
                LDJSONFree(tempSegments);

                if(!LDObjectMerge(temp, segments)) {
                    LDJSONFree(json);
                    LDJSONFree(temp);
                    goto fail;
                }
                LDJSONFree(segments);

                segments = temp;
            }
        }

        LDJSONFree(json);
    }

    if(!(set = LDNewObject())) {
        goto fail;
    }
    if(!LDObjectSetKey(set, "features", flags)) {
        LDJSONFree(set);
        goto fail;
    }
    if(!LDObjectSetKey(set, "segments", segments)) {
        LDJSONFree(set);
        goto fail;
    }
    context->set = set;

    dataSource->context = (void *)context;
    dataSource->init = start;
    dataSource->close = close;
    dataSource->destructor = destructor;

    return dataSource;

fail:
    LDJSONFree(segments);
    LDJSONFree(flags);
    LDFree(context);
    LDFree(dataSource);
    return NULL;
}
