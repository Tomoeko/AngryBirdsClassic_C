#ifndef AB_AUDIO_CATALOG_H
#define AB_AUDIO_CATALOG_H

#include "ab/lua_data.h"
#include "ab/zip.h"

typedef struct {
    AbLuaChunk chunk;
    AbLuaData data;
    const AbDataTable *groups;
    const AbDataTable *registrations;
    AbBuffer owned_bytes;
} AbAudioCatalog;

/* Read the pinned decoded scripts/audio.lua resource's independent group
 * constructor. Verify its size/hash, parse its structure, and recover root
 * instructions 13..348 as static data. Platform configuration branches and
 * function definitions do not run; this is not a Lua VM. Also recover the 340
 * independent constant property tables passed to createAudio in the loading
 * function, in call order, as registrations with numeric keys 1..340. Preserve
 * absent properties rather than inventing native defaults. Channel volume and
 * count-limit statements are outside this projection. The decoded input
 * stays borrowed until close. Chunk/table/layout allocations are owned and
 * bounded together by memory_limit; temporary constructor storage is at most 1348
 * bytes. Output starts empty and closes fully on failure. This prepares catalog
 * metadata only, without choosing sound format, loading files or playing audio. */
bool ab_audio_catalog_read(AbSpan decoded, size_t memory_limit, AbAudioCatalog *output,
                           AbError *error);
/* Decode the fixed audio resource directly from an already opened APK. Own the
 * decoded bytes together with the catalog until close; allocation_limit bounds
 * their retained total and each payload stage. Codec workspace is separately
 * bounded by the packed reader. Archive/key are borrowed only during open;
 * no key is stored. The caller verifies the archive/profile and supplies the
 * resource key through the existing first-party target-resource boundary. */
bool ab_audio_catalog_open(const AbZip *archive, AbSpan key, size_t allocation_limit,
                           AbAudioCatalog *output, AbError *error);
void ab_audio_catalog_close(AbAudioCatalog *catalog);

#endif
