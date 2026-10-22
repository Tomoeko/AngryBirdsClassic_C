#ifndef AB_CLASSIC_TRAJECTORIES_H
#define AB_CLASSIC_TRAJECTORIES_H

#include "ab/classic_render_scene.h"
#include "ab/museum_bird_behavior.h"
#include "ab/trajectory_follow.h"

typedef struct AbClassicTrajectories AbClassicTrajectories;

typedef struct {
    AbClassicLevel *level;
    AbClassicRenderScene *scene;
    size_t tracks, points, storage_bytes;
} AbClassicTrajectoryConfig;

typedef struct {
    size_t tracks, points, retained_points;
} AbClassicTrajectoryView;

/* Own ordinary follow tracks and the renderer-registry projection over a
 * borrowed actual ordered scene. Start with the original fresh empty module
 * collections. Scene must reserve the required extra-renderer slots separately.
 * Every created renderer stays scene-owned until its scene closes; local point
 * metadata and borrowed canonical physical tables stay stable until close.
 * Sprite names/custom lists remain borrowed through scene/track use.
 *
 * Frame calls allocate nothing. Point slots are never recycled, including after
 * clear/disposal, preserving retained scene objects and numeric-ID aliases.
 * The original numeric renderer key can alias a prior key: replace that registry
 * entry without disposing its old scene renderer. Disposal uses the actual
 * exposed float ID and native conversion, including a missing match or an
 * adjacent renderer. Distinct converted registry keys make ordinary fixed
 * Scene.remove calls commute; preserve scene survivor order. Arbitrarily rebound
 * disposal functions, mixed keys and external registry observers are outside
 * this closed binding. Special trail classes/registration remain separate.
 *
 * No implicit scene cleanup on close: run clear while healthy, or discard the
 * borrowing scene with the failed session. Serialize operations; providers may
 * not reenter/mutate these owners. A failure stops further work and retains
 * preceding track/sprite/ID/scene effects. Output starts NULL on failed open.
 */
bool ab_classic_trajectories_open(AbClassicTrajectoryConfig config,
                                  AbClassicTrajectories **output, AbError *error);
void ab_classic_trajectories_close(AbClassicTrajectories *trajectories);
size_t ab_classic_trajectories_bytes(const AbClassicTrajectories *trajectories);
AbClassicTrajectoryView
ab_classic_trajectories_view(const AbClassicTrajectories *trajectories);

bool ab_classic_trajectories_add(AbClassicTrajectories *trajectories,
                                 AbDataValue object, const AbSpan *sprites,
                                 size_t count, bool world_coordinates,
                                 bool velocity_angle, AbError *error);
bool ab_classic_trajectories_remove(AbClassicTrajectories *trajectories,
                                    AbDataValue object, AbError *error);
/* GameUpdate prefix: follow-list update/point creation. Elapsed time is unused
 * in the ordinary path. Schedule before the later object retirement/controller
 * tail, after native post-physics cached pose publication. */
bool ab_classic_trajectories_update(AbClassicTrajectories *trajectories,
                                    AbError *error);
/* removeUnattachedRenderers: nil slots in the supplied keep constructor are
 * absent, including interior holes. Retain exact physical identities only. */
bool ab_classic_trajectories_keep(AbClassicTrajectories *trajectories,
                                  const AbDataValue *objects, size_t count,
                                  AbError *error);
bool ab_classic_trajectories_clear(AbClassicTrajectories *trajectories, AbError *error);
/* Original addPuff, including deep-copied position, falsy sprite/layer defaults
 * BIRD_SPECIAL/3 and attachedObject membership. Plain canonical object/nil,
 * string-or-falsy sprites and finite numeric-or-falsy layers only. */
bool ab_classic_trajectories_puff(AbClassicTrajectories *trajectories, AbVec2 position,
                                  AbDataValue object, AbDataValue sprite,
                                  AbDataValue layer, AbError *error);

/* Concrete trajectory branches for the actual Museum/Blues behavior hosts.
 * The enclosing Museum service dispatches camera operations to the real camera;
 * this adapter rejects that unrelated operation. Blues GET_OBJECT uses the live
 * canonical world lookup and preserves missing/nil results. */
bool ab_classic_trajectories_museum(AbClassicTrajectories *trajectories,
                                    const AbMuseumBirdCall *call, AbError *error);
bool ab_classic_trajectories_blues(void *context, AbBluesTrajectoryCall *call,
                                   AbError *error);

#endif
