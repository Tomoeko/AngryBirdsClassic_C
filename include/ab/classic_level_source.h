#ifndef AB_CLASSIC_LEVEL_SOURCE_H
#define AB_CLASSIC_LEVEL_SOURCE_H

#include "ab/target_data.h"

typedef struct AbClassicLevelSource AbClassicLevelSource;

typedef struct {
    const AbLuaData *catalog;
    AbSpan key;
} AbClassicLevelSourceView;

/* Verify the pinned APK once, resolve its resource key and own decoded blocks
 * and constructor metadata across level transitions. Borrow immutable archive
 * bytes until close. Levels may borrow this owner; close them first. Resource
 * storage has its own allocation bound. No world, RNG, GPU or mutable object
 * state is cached. Output starts NULL and failed opens release all resources. */
bool ab_classic_level_source_open(const AbZip *archive, size_t allocation_limit,
                                  AbClassicLevelSource **output, AbError *error);
void ab_classic_level_source_close(AbClassicLevelSource *source);
size_t ab_classic_level_source_bytes(const AbClassicLevelSource *source);
/* Borrow source data only for the exact still-open archive identity. The key
 * view is private runtime data, never a log or generated artifact. */
bool ab_classic_level_source_view(const AbClassicLevelSource *source,
                                  const AbZip *archive, AbClassicLevelSourceView *view,
                                  AbError *error);

#endif
