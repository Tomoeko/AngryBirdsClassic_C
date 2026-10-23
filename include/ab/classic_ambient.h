#ifndef AB_CLASSIC_AMBIENT_H
#define AB_CLASSIC_AMBIENT_H

#include "ab/ambient_animation.h"
#include "ab/classic_visuals.h"
#include "ab/classic_particles.h"

typedef struct AbClassicAmbient AbClassicAmbient;

typedef enum {
    AB_CLASSIC_AMBIENT_ANIMATIONS,
    AB_CLASSIC_AMBIENT_PARTICLES,
} AbClassicAmbientKind;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbRandom48 *random;
    AbClassicParticles *particles;
    AbClassicAmbientKind kind;
    size_t applications_per_object, storage_bytes;
} AbClassicAmbientConfig;

typedef struct {
    const AbAmbientAnimationState *state;
    const AbAmbientAnimationVariant *variants;
    const AbAmbientParticle *particles;
    size_t count;
    bool present;
} AbClassicAmbientView;

/* Own one ambient class's identity-keyed map and copied mutable descriptors.
 * The eight animation and five particle catalog components each have one
 * descriptor, whose pairs order is unambiguous. Other entry orders reject.
 * Reserve initial objects and the caller's bounded number of repeated apply
 * calls at open; no allocation during apply/update/dispose. The original dense
 * frame arrays and all source names stay borrowed from the level/catalog.
 * Borrow the shared script RNG and actual visual/particle owner when active.
 * Native pointer-key table ordering is retained; native allocation addresses
 * and whole-frame random ordering are not established by this owner. */
bool ab_classic_ambient_open(AbClassicAmbientConfig config, AbClassicAmbient **output,
                             AbError *error);
void ab_classic_ambient_close(AbClassicAmbient *ambient);
size_t ab_classic_ambient_bytes(const AbClassicAmbient *ambient);
bool ab_classic_ambient_apply(AbClassicAmbient *ambient, size_t object,
                              const AbDataTable *definition, AbError *error);
/* Call after the source physics-enabled gate. Test the current physical name
 * against objects.world and erase absent objects. Animations start/advance and
 * restore through setSprite; particle descriptors advance and emit through the
 * actual shared particle engine. Late failures retain earlier effects. */
bool ab_classic_ambient_update(AbClassicAmbient *ambient, float seconds,
                               AbError *error);
bool ab_classic_ambient_dispose(AbClassicAmbient *ambient, AbError *error);
bool ab_classic_ambient_view(const AbClassicAmbient *ambient, size_t object,
                             AbClassicAmbientView *output);

#endif
