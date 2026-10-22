#ifndef AB_CLASSIC_BLUES_CHILD_H
#define AB_CLASSIC_BLUES_CHILD_H

#include "ab/classic_playfield.h"

typedef enum {
    AB_BLUES_CHILD_CIRCLE = 1,
    AB_BLUES_CHILD_RENDERER,
    AB_BLUES_CHILD_SPRITE,
    AB_BLUES_CHILD_ROTATION,
    AB_BLUES_CHILD_VELOCITY,
    AB_BLUES_CHILD_COLLIDER,
    AB_BLUES_CHILD_GAME_OBJECT,
    AB_BLUES_CHILD_SCALE,
} AbClassicBluesChildStage;

typedef struct {
    AbClassicPlayfieldView playfield;
    const AbDataValue *birds;
    /* Caller-owned stable name storage. Keep every used byte alive until the
     * level, registries, events and retained aliases have all released it. */
    AbBuffer names;
    /* Optional read-only diagnostic trace immediately before actual source
     * calls. Copy arguments now; do not retain this array, mutate or reenter. */
    void *trace_context;
    void (*trace)(void *context, AbClassicBluesChildStage stage,
                  const AbDataValue *arguments, size_t count);
} AbClassicBluesChildConfig;

typedef struct {
    AbClassicBluesChildConfig config;
    size_t used;
    bool initialized, calling, failed;
} AbClassicBluesChild;

/* Concrete triggerSpecialty125163 child129468, with its captured parent supplied
 * as a canonical physical object index. This is distinct from createBirdClone.
 * Bind the actual playfield and live birds table. No effects, defaults or RNG at
 * initialize. Child creation uses actual circle, renderer, transform, velocity,
 * collider, birds insertion and source-specific GameObject/behavior owners.
 *
 * Name concatenation, offset and velocity arithmetic retain source order. Read
 * damageSprite anew at both rendering boundaries; preserve raw copied values
 * and shared damageFactors identity. The child receives only the non-cluster
 * Blues trajectory descriptor. No onBirdAdded event occurs in this source path.
 * Numeric native-boundary inputs must be finite; names/sprites are strings and
 * colliders are supported integral types. No Lua coercion/metatables or foreign
 * world identities. A reached zero/nonfinite visual scale retains the existing
 * renderer's unsupported boundary rather than silently skipping the call.
 *
 * Serialized calls allocate no storage beyond bounded world growth. Source
 * callbacks may query/update live fields, but may not reenter or release owners.
 * Failure retains earlier name/world/renderer/script/behavior effects and stops
 * this binding. Outputs commit only on success. The caller must register new
 * identities with native object/vector and simulation/contact/removal/character/
 * camera adapters before their next pass. The enclosing full specialty branch
 * owns puff/tutorial/audio/hat/teleportation/flying/otherBirds changes separately.
 */
bool ab_classic_blues_child_initialize(AbClassicBluesChildConfig config,
                                       AbClassicBluesChild *output, AbError *error);
bool ab_classic_blues_child_create(AbClassicBluesChild *owner, size_t parent,
                                   AbSpan suffix, AbVec2 offset, size_t *object,
                                   AbSpan *name, AbError *error);

#endif
