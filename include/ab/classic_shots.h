#ifndef AB_CLASSIC_SHOTS_H
#define AB_CLASSIC_SHOTS_H

#include "ab/bird_shot.h"
#include "ab/classic_behaviors.h"
#include "ab/classic_camera.h"
#include "ab/classic_visuals.h"
#include "ab/classic_game_globals.h"
#include "ab/slingshot_launch.h"

typedef struct AbClassicShots AbClassicShots;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicBehaviors *behaviors;
    AbClassicCamera *camera;
    AbDataValue *flying_object;
    AbShotHost service;
    void *context;
    size_t storage_bytes;
    AbClassicGameGlobals *globals;
    size_t object_capacity;
    /* Optional canonical controller, initialized by the actual enclosing
     * bootstrap before level construction. Borrow at a stable address;
     * NULL retains internally owned zero state for isolated component callers. */
    AbShotState *state;
} AbClassicShotConfig;

/* Own stable shot metadata and NUL-terminated copies of registered object names and
 * definition hook/sprite/audio identifiers. Borrow the level, visuals,
 * optional behaviors/camera and shared flying physical-object identity until close.
 * Initialize mass from creation-time script mass, independent of a queued
 * body's current engine mass. An external controller is never copied or reset;
 * otherwise the owned state starts zero. Its actual initialization, selection
 * and timing are caller work. Open performs no services, RNG or world writes.
 * Later transitions allocate no adapter storage;
 * the borrowed world's bounded broadphase growth rules still apply.
 *
 * Only the borrowed immutable catalog definitions are supported. A later name
 * lookup may select another prepared definition; dynamic catalog/string types,
 * general insertion and object reuse reject. Ground/noncontrollable rows do not acquire
 * shot definitions. All supplied contexts/borrowed values remain live. Clear
 * external controller references to this owner's birds/names before close.
 * Output starts NULL and failure releases partial storage. */
bool ab_classic_shots_open(AbClassicShotConfig config, AbClassicShots **output,
                           AbError *error);
void ab_classic_shots_close(AbClassicShots *shots);
size_t ab_classic_shots_bytes(const AbClassicShots *shots);
AbShotState *ab_classic_shots_state(AbClassicShots *shots);
AbClassicLevel *ab_classic_shots_level(const AbClassicShots *shots);
/* Optional canonical numeric globals. When bound, callbacks change these
 * fields rather than the float view. Refresh surrounds original services and
 * publication precedes the all-birds event. Bootstrap must supply finite numeric
 * time/score/lastScoreTime/oldScore/extraTutorialTimer before a transition.
 * Specialty availability shares the canonical typed value. No implicit clock tick. */
AbClassicGameGlobals *ab_classic_shots_globals(const AbClassicShots *shots);
bool ab_classic_shots_refresh_globals(AbClassicShots *shots, AbError *error);
AbShotBird *ab_classic_shots_bird(AbClassicShots *shots, size_t object);
/* The canonical live Lua birds table after the owned bird registry binds it.
 * NULL retains the initial-only queue for isolated phase callers. */
const AbDataTable *ab_classic_shots_bird_table(AbClassicShots *shots);

/* Register the next canonical direct circle after its source field/script
 * construction. Reserve object_capacity and string bytes at open; zero capacity
 * retains the initial count. Copy its current name/sprite identifiers and raw
 * shot/collision flags, preserve creation mass and actual canonical script
 * presence, and resolve an authored controllable definition when supplied.
 * Native bare circles may have no definition/script. No world/raw field write,
 * queue insertion, launch, RNG or callback; initial launch queue/count remains
 * unchanged. Existing bird/string pointers stay stable. A storage/type error
 * retains private scratch and requires discard. Do not call during launch. */
bool ab_classic_shots_register_circle(AbClassicShots *shots, size_t object,
                                      AbError *error);
/* Reserve lookup/pose metadata for a defined box without adding a launch bird. */
bool ab_classic_shots_register_box(AbClassicShots *shots, size_t object,
                                   AbError *error);

/* Canonical position boundary for shot services: update the owned level's body,
 * pose history and physical table, then this shot view. Callback changes to other
 * shot metadata use the exposed stable bird/state views. Do not bypass this
 * boundary when a shot callback changes the canonical position. */
bool ab_classic_shots_set_position(AbClassicShots *shots, size_t object,
                                   AbVec2 position, AbError *error);
/* At the recovered property-publication phase, copy canonical cached positions
 * into shot views. Do not call during launch or from a service. */
bool ab_classic_shots_sync_positions(AbClassicShots *shots, AbError *error);

/* Execute the recovered transition against real body/visual/behavior storage.
 * Position, velocity, impulse, definition, sprite and script-launch operations
 * are implemented here. Publish start impulse, flying identity, flags and final
 * launch velocity at their original boundaries. Statistics, notifications,
 * level/definition hooks, particles and audio require the supplied real service
 * when reached; missing services fail without substituting an effect.
 * Services may mutate the exposed state/birds and input vectors under bird_shot's
 * contract. Never reenter or replace/free storage. Earlier effects survive a
 * failure; stop the session on error. This is an adapter, not an outer frame,
 * bird selector, resource loader or complete playable session. */
bool ab_classic_shots_launch(AbClassicShots *shots, AbVec2 *position, AbVec2 *impulse,
                             AbError *error);

/* Compose launchBird with the owned launchBirdWithImpulse adapter. The shot
 * state's dragged pointer is authoritative; query callbacks change selection
 * through that state. sling->dragged mirrors it after each boundary, including
 * partial failure. Resolve definition force from the live owned name registry;
 * numeric zero is present and nil/false select the live sling default. The
 * supplied query implements remaining sling geometry/power/statistics services.
 * WITH_IMPULSE always executes this owner's real transition. Missing queries
 * fail when reached. No input selection, cancel, outer frame or allocation. */
bool ab_classic_shots_launch_sling(AbClassicShots *shots, AbSlingLaunchState *sling,
                                   AbVec2 *direction, AbSlingLaunchHost query,
                                   void *context, AbError *error);

#endif
