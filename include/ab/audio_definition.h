#ifndef AB_AUDIO_DEFINITION_H
#define AB_AUDIO_DEFINITION_H

#include "ab/lua_data.h"

typedef struct {
    AbSpan path;
    AbDataValue stream;
    AbDataValue name;
    AbDataValue channel;
    AbDataValue volume;
    AbDataValue can_be_overwritten;
    AbDataValue loop;
} AbAudioDefinition;

/* Recover createAudio's resource path/stream request and stored sound metadata
 * for constant property tables. Any non-nil existing sound, including false,
 * skips creation and leaves output/storage unchanged; set created to false.
 * Otherwise require string fileName/clipName and string-or-falsy extension.
 * A missing extension chooses mp3 when use_mp3 is true; explicit false chooses
 * wav. A truthy stream value is preserved; falsy becomes boolean false. Metadata
 * remains typed, with missing fields nil. No defaults are invented for channel,
 * volume, overwrite or loop. Numeric-to-string coercion and metatables reject.
 *
 * prefix and string fields stay borrowed. The path is concatenated into caller
 * storage without a NUL terminator; output.path borrows those bytes. All inputs
 * and output storage must be disjoint. No allocation, registry mutation,
 * decoding or playback occurs. Validate before writes; failure preserves
 * output, created and path storage. Resource-call mutation is outside this
 * pure metadata boundary. The caller supplies the recovered audioPath prefix
 * and Android format policy, independently of the desktop host platform. */
bool ab_audio_definition_read(const AbDataTable *properties, AbDataValue existing,
                              AbSpan prefix, bool use_mp3, uint8_t *path_storage,
                              size_t capacity, AbAudioDefinition *output, bool *created,
                              AbError *error);

#endif
