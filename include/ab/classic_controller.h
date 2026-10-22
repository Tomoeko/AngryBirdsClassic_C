#ifndef AB_CLASSIC_CONTROLLER_H
#define AB_CLASSIC_CONTROLLER_H

#include "ab/classic_birds.h"
#include "ab/classic_characters.h"
#include "ab/classic_removals.h"
#include "ab/classic_scores.h"
#include "ab/classic_sling.h"
#include "ab/block_script.h"
#include "ab/game_update_tail.h"

typedef struct AbClassicController AbClassicController;
typedef struct AbClassicGameInput AbClassicGameInput;

typedef struct {
    void *context;
    bool (*update)(void *context, float seconds, float time, AbError *error);
} AbClassicControllerEffect;

typedef struct {
    AbClassicControllerEffect **items;
    size_t capacity;
} AbClassicControllerEffects;

typedef enum {
    AB_CLASSIC_CONTROLLER_EFFECT_UNRESOLVED,
    AB_CLASSIC_CONTROLLER_EFFECT_BASE,
    AB_CLASSIC_CONTROLLER_EFFECT_CUSTOM,
} AbClassicControllerEffectKind;

typedef struct {
    AbClassicLevel *level;
    AbClassicShots *shots;
    AbClassicSling *sling;
    AbClassicBirds *birds;
    AbClassicBehaviors *behaviors;
    AbClassicCamera *camera;
    AbClassicCharacters *characters;
    AbClassicRenderScene *scene;
    AbClassicRemovals *removals;
    AbFloatingScores *floating;
    AbClassicScores *scores;
    AbBlockScriptManager *blocks;
    AbBlockScriptUpdate *block_update;
    AbClassicControllerEffects *level_effects;
    AbClassicControllerEffectKind effect_kind;
    AbGameTailState *state;
    float *score;
    const bool *physics_enabled, *level_completed;
    size_t wobble_steps, storage_bytes;
    /* Stable identity rows reserved at open; zero selects the initial count. */
    size_t object_capacity;
    void *context;
    bool (*boundary_warning)(void *context, AbSpan name, AbError *error);
    bool (*mighty_exit)(void *context, size_t object, float seconds, AbError *error);
    /* Reached active animation/rocket, custom object effects, boomerang,
     * mighty/red effects, joint calls and final draw boundary. Mandatory draw
     * always reaches this host. It must execute the real selected service;
     * an absent callback rejects. object->identity is the fixed level index. */
    AbGameTailHost service;
    /* When bound, share source cursor/skip housekeeping with the input owner. */
    AbClassicGameInput *input;
} AbClassicControllerConfig;

/* Bind the recovered GameUpdate tail to actual owned gameplay services. The
 * source world order resumes after each retained key in the current string map;
 * each of the two source loops starts separately. All source
 * owners, globals, registries and callback contexts stay borrowed until close.
 * Constructor has no game/RNG effects. Calls allocate nothing.
 *
 * The effect kind is an explicit resolved class selection: base updateObject
 * is the empty original method (prototype 782). The separate level-effects
 * registry is a required dense list of bound methods, terminated by NULL;
 * methods may append/replace later entries within stable capacity. Empty is a
 * real caller-owned registry, never inferred from a missing service.
 *
 * Owned creation/removal supports registered canonical identities. Foreign
 * world/script table replacement and foreign keys are unsupported.
 * Object scripts delegate update(seconds,time) through the real behavior map.
 * Optional hooks can borrow objects returned below; preserve their storage.
 * No input prelude, native simulation/particle scheduling, bootstrap or GPU
 * implementation is supplied here. A tail call is not a complete game frame.
 */
bool ab_classic_controller_open(AbClassicControllerConfig config,
                                AbClassicController **output, AbError *error);
void ab_classic_controller_close(AbClassicController *controller);
size_t ab_classic_controller_bytes(const AbClassicController *controller);
/* Borrow the configured input binding; NULL denotes a tail-only caller. */
AbClassicGameInput *ab_classic_controller_input(const AbClassicController *controller);
AbGameTailObject *ab_classic_controller_object(AbClassicController *controller,
                                               size_t object);
/* Register the next owned level identity within reserved stable storage before
 * the source iterator reaches it. This changes metadata only, without source
 * insertion, timer initialization, RNG, sprite preparation or launch queues. */
bool ab_classic_controller_register_object(AbClassicController *controller,
                                           size_t object, AbError *error);

/* Run after the actual physics/post-property and GameUpdate input phases.
 * Use the original unscaled seconds/time and already resolved camera input.
 * Camera specialty availability is refreshed from shared shot state at its
 * actual update boundary, after retirement can have cleared it.
 * Shared shot state supplies birdFired/current bird; root score writes also
 * update its score view before later services. Refresh live world/script gates
 * and optional canonical shared numeric globals at their source boundaries.
 * With bound globals, the function's time argument never overwrites global time;
 * bootstrap/root clock scheduling remains the caller's responsibility.
 * Partial effects survive failure; the owner then
 * rejects further updates so a damaged session cannot silently resume.
 */
bool ab_classic_controller_update(AbClassicController *controller, float seconds,
                                  float time, const AbCameraSystemInput *camera,
                                  AbError *error);

#endif
