#ifndef AB_CLASSIC_BOOMERANG_H
#define AB_CLASSIC_BOOMERANG_H

#include "ab/classic_birds.h"
#include "ab/classic_sling.h"
#include "ab/classic_trajectories.h"
#include "ab/force_queue.h"
#include "ab/settings_statistics.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicBirds *birds;
    AbClassicSling *sling;
    AbClassicVisuals *visuals;
    AbClassicTrajectories *trajectories;
    AbAudioBank *audio;
    AbSettingsStatistics *settings;
    AbForceQueue *forces;
    const AbDataValue *flying, *specialty_available;
    float physics_scale;
    /* Source scratch globals, written only by the corresponding branch. */
    AbDataValue y_force_coefficient, volume;
} AbClassicBoomerang;

/* triggerSpecialty125163 instructions413..519. Read cached physical velocity
 * and creation mass, then publish ordinary/Facebook turning state, actual
 * spriteSpecial, sound and attached trajectory puff in source order. */
bool ab_classic_boomerang_trigger(AbClassicBoomerang *owner, size_t object,
                                  const AbDataTable *definition, AbError *error);

/* updateBoomerangBirds108793, after the ordinary camera update. Traverse live
 * canonical birds in their retained source order. Queue forces for the next
 * physics phase; native velocity setters do not rewrite cached Lua velocity.
 * Preserve the pre-activation lift/spin, both turning branches, angle-crossing
 * audio and creation-time level-edge attenuation. No allocation, RNG or time
 * stepping. The ordinary non-mutating owned bird registry is required; foreign
 * tables/identities reject. Earlier effects survive a late error. */
bool ab_classic_boomerang_update(AbClassicBoomerang *owner, float seconds,
                                 AbError *error);

#endif
