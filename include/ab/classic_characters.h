#ifndef AB_CLASSIC_CHARACTERS_H
#define AB_CLASSIC_CHARACTERS_H

#include "ab/classic_visuals.h"
#include "ab/character_animation.h"
#include "ab/damage_sprite.h"
#include "ab/random.h"

typedef struct AbClassicCharacters AbClassicCharacters;

typedef enum {
    AB_CLASSIC_CHARACTER_GOAL,
    AB_CLASSIC_CHARACTER_BIRD,
} AbClassicCharacterKind;

typedef struct {
    void *context;
    bool (*lose_condition)(void *context, bool *output, AbError *error);
    bool (*camera_distance)(void *context, size_t object, AbClassicCharacterKind kind,
                            float *output, AbError *error);
    bool (*play_sound)(void *context, size_t object, AbClassicCharacterKind kind,
                       float volume, AbError *error);
    bool (*update_hat)(void *context, size_t object, AbError *error);
} AbClassicCharacterHost;

typedef struct {
    AbDamageSpriteNames damage;
    AbDataValue special_expression;
} AbClassicCharacterSprites;

typedef struct {
    AbCharacterAnimations *animations;
    AbCharacterAnimationBindings *bindings;
    AbClassicCharacterSprites *sprites;
    size_t count;
} AbClassicCharacterView;

/* Own per-object bindings for the recovered character controller. Borrow the
 * level's initialized timers, canonical character state and initial goal/bird
 * orders. Seed damage-name views from its prepared sprite snapshot;
 * read bird face names and goal strength/damage1 limits from raw definitions.
 * Damage/blink/smile actions read the live physical damage-name fields, refresh
 * the inspection view and copy names through the canonical visual owner. Jump events
 * update real bodies and render histories through the owned level. All random
 * draws use the caller's shared generator. No RNG/effects run during open.
 *
 * Missing host operations fail only when reached. Distance, loss, sound choice/
 * playback and hats remain explicit boundaries; do not replace them with no-ops
 * in a gameplay runtime. Scene submission is separate. Level, visuals, catalog
 * names, host context and RNG must outlive this owner; close it first. Fixed
 * membership only, without insertion/removal or complete frame scheduling.
 * storage_bytes bounds all owned allocations. Output is NULL on failure.
 */
bool ab_classic_characters_open(AbClassicLevel *level, AbClassicVisuals *visuals,
                                AbRandom48 *random, AbClassicCharacterHost host,
                                size_t storage_bytes, AbClassicCharacters **output,
                                AbError *error);
void ab_classic_characters_close(AbClassicCharacters *characters);
size_t ab_classic_characters_bytes(const AbClassicCharacters *characters);

/* Borrow stable views. Update numeric state, special names, controller
 * gates/current bird and control flags at their recovered phases. Damage names
 * observe the last face action; publish changes to the canonical physical table.
 * Supplied name bytes remain borrowed until replaced. Preserve callback/context
 * pointers and membership/order storage. Initial names remain catalog-backed. Goal
 * smiling and eagle-bait controls start false; their live assignments are caller state.
 */
AbClassicCharacterView ab_classic_characters_view(AbClassicCharacters *characters);

/* Run the recovered expression, oink and bird passes. No allocation, implicit
 * clock advance, physics stepping, scene draw or timer reseeding. Earlier state,
 * RNG, sprite and body changes remain after a later failure. */
bool ab_classic_characters_update(AbClassicCharacters *characters, float seconds,
                                  AbError *error);

#endif
