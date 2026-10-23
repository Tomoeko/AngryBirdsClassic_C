#ifndef AB_CLASSIC_FLAMES_H
#define AB_CLASSIC_FLAMES_H

#include "ab/classic_render_scene.h"
#include "ab/classic_object_hooks.h"
#include "ab/extra_draw.h"
#include "ab/flame_data.h"
#include "ab/game_update_tail.h"

typedef struct AbClassicFlames AbClassicFlames;
typedef struct AbClassicController AbClassicController;

typedef struct {
    AbClassicLevel *level;
    AbClassicRenderScene *scene;
    const AbSpriteProvider *sprites;
    AbRandom48 *random;
    AbExtraDraws *extra_draws;
    float physics_scale;
    size_t storage_bytes;
} AbClassicFlameConfig;

typedef struct {
    const AbFlameState *state;
    const AbFlameDefinition *definition;
    AbSpan draw_key;
    float z;
    bool applied;
} AbClassicFlameView;

/* Own initial FlameEffect records, source field projections, cached frame
 * resources and object-update registrations. Borrow the live level, scene,
 * shared script RNG and extra-draw registry through their last dispatch.
 * Initial canonical names and immutable component definitions only; arbitrary
 * callback/definition replacement and dynamic flame construction are separate.
 * Open preloads metadata but performs no RNG/update/draw registration. Apply
 * publishes flameEffect, registers the shared update, runs the original zero
 * update and registers its z draw. No frame allocation or resource decoding.
 * Close during session teardown after all frame users have drained; it performs
 * no callbacks and leaves physical property views expired. */
bool ab_classic_flames_open(AbClassicFlameConfig config, AbClassicFlames **output,
                            AbError *error);
void ab_classic_flames_close(AbClassicFlames *flames);
size_t ab_classic_flames_bytes(const AbClassicFlames *flames);
bool ab_classic_flames_apply(AbClassicFlames *flames, size_t object,
                             const AbDataTable *definition, AbError *error);
/* Bind the constructor's shared native string-map hook registry to stable
 * controller identities. Later source registrations use the same owner. */
bool ab_classic_flames_bind(AbClassicFlames *flames, AbClassicController *controller,
                            AbGameTailHooks **output, AbError *error);
AbClassicObjectHooks *ab_classic_flames_updates(AbClassicFlames *flames);
bool ab_classic_flames_view(const AbClassicFlames *flames, size_t object,
                            AbClassicFlameView *output);

#endif
