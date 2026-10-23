#ifndef AB_CLASSIC_SUPERSEED_H
#define AB_CLASSIC_SUPERSEED_H

#include "ab/classic_birds.h"
#include "ab/classic_playfield.h"
#include "ab/classic_simulation.h"

typedef struct {
    AbClassicPlayfieldView playfield;
    AbClassicBirds *birds;
    AbClassicObjectRemoval *physical;
    AbClassicSimulation *simulation;
    void *context;
    bool (*clear_updates)(void *context, size_t object, AbError *error);
    bool (*bird_added)(void *context, size_t object, AbDataValue hook, AbError *error);
} AbClassicSuperSeed;

/* Powerup_SuperSeed736: the source half-second oscillating growth radius.
 * Update the raw timer's nil boundary exactly; use binary32 script arithmetic. */
bool ab_classic_superseed_radius(AbDataTable *physical, const AbDataTable *definition,
                                 float *output, AbError *error);

/* Actual removeObject/createCircle/createRenderer/clone/scale/body-type/
 * onBirdAdded/GameObject sequence. Reuse a logical index, rebuilding its native
 * body and every native body cache. Call between simulation passes. Physical
 * aliases retained outside the live world/birds projection are unsupported;
 * source bubble closures retain only the unchanged name. No frame allocation. */
bool ab_classic_superseed_resize(AbClassicSuperSeed *seed, size_t object,
                                 AbError *error);
/* Begin the current unshot bird's growth, with the source definition-specific
 * radius, density and velocity multipliers. Effects/audio belong to activation. */
bool ab_classic_superseed_apply(AbClassicSuperSeed *seed, size_t object,
                                AbError *error);
/* MuseumMode.updateBird's superseed_timer continuation. */
bool ab_classic_superseed_update(AbClassicSuperSeed *seed, size_t object, float seconds,
                                 bool *alive, AbError *error);

#endif
