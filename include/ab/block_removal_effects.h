#ifndef AB_BLOCK_REMOVAL_EFFECTS_H
#define AB_BLOCK_REMOVAL_EFFECTS_H

#include "ab/audio_play.h"
#include "ab/block_removal_cursor.h"

typedef struct {
    AbDataValue particles, amount;
} AbBlockRemovalEffects;

typedef struct {
    void *context;
    AbBlockRemovalState *state;
    /* Each call resolves the cached object's current script/get binding. */
    bool (*property)(void *context, AbDataValue object, AbSpan key, AbDataValue *output,
                     AbError *error);
    bool (*destroyed_sound)(void *context, AbDataValue object, AbDataValue definition,
                            AbDataValue *output, AbError *error);
    bool (*random)(void *context, float lower, float upper, AbDataValue *output,
                   AbError *error);
    bool (*explosion)(void *context, AbDataValue current_dead_object,
                      AbDataValue definition, AbDataValue sound, AbError *error);
    bool (*audio_name)(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error);
    bool (*audio)(void *context, AbAudioPlay request, AbError *error);
    bool (*update_destroyed)(void *context, AbDataValue cached_object, AbError *error);
} AbBlockRemovalEffectsHost;

/* Selected ordinary removeBlocks instructions 160..230, after scoring. Cache
 * particles, destroyed sound and truthy particleAmount/default twelve. Always
 * request random(12,20); smokeBuff/flameBuff force one and batBuff uses that
 * random result, including nonnumeric values. No particles are emitted here.
 *
 * A BOMB merged definition or current deadBlocks[key] selects an explosion.
 * Read the current entry's specialty only when the definition is not BOMB.
 * Capture the current object and explosion host before an optional
 * tnt_explodes audio-name query. Truthy explosionSound bypasses the query. Then
 * read the cached object's current killSilently: falsy permits any non-nil
 * cached destroyed sound, with .7 volume, false loop and channel three. Cache
 * audio before name resolution. Always update destroyed-block values afterward.
 *
 * Borrow stable plain tables/map wrappers as in the removal cursor. Bindings
 * and fields may change during callbacks; retain cached values and ignore script
 * returns. Status denotes host failure. Output commits only on success; earlier
 * mutations survive failures. No native removal, notification, particles,
 * spawning, allocation or Lua. Required hosts fail only when reached.
 */
bool ab_block_removal_effects(const AbBlockRemovalEntry *entry,
                              AbDataValue cached_definition,
                              AbBlockRemovalEffectsHost *host,
                              AbBlockRemovalEffects *output, AbError *error);

#endif
