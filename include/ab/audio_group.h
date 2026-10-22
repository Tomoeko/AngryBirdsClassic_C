#ifndef AB_AUDIO_GROUP_H
#define AB_AUDIO_GROUP_H

#include "ab/lua_data.h"
#include "ab/random.h"

/* getAudioName for recovered scalar keys and dense group tables. A missing/nil
 * group returns requested unchanged without using random. A present group must
 * have exactly the non-nil numeric keys 1..count, count <= 2^24; sparse length,
 * metatables and non-table groups are outside this contract. Draw one original
 * script random integer over [1,count], then return the selected typed value.
 * A rare native maximum+1 result selects nil. An empty group consumes a draw
 * before returning an error; other invalid storage/data preserves RNG/output.
 * Catalog/selected values remain borrowed and stable during the call. Output
 * and random must be disjoint from that data and one another. No allocation,
 * sound decoding, playback, seed choice or implicit global state is used. */
bool ab_audio_group_name(const AbDataTable *groups, AbDataValue requested,
                         AbRandom48 *random, AbDataValue *output, AbError *error);

#endif
