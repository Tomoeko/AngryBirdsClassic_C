#ifndef AB_TRAJECTORY_FOLLOW_H
#define AB_TRAJECTORY_FOLLOW_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbDataTable *object;
    const AbSpan *sprites;
    size_t sprite_count, latest_sprite;
    AbVec2 last_position;
    bool world_coordinates, velocity_angle;
} AbTrajectoryTrack;

typedef struct {
    AbDataTable *object;
    AbSpan sprite;
    AbVec2 position;
    float angle;
    int layer;
} AbTrajectoryPoint;

typedef struct {
    AbTrajectoryTrack *tracks;
    size_t count, capacity;
    void *context;
    bool (*emit)(void *context, const AbTrajectoryPoint *point, AbError *error);
} AbTrajectoryFollow;

/* Ordinary TrajectoryRenderer follow-list operations, with plain object tables
 * and finite numeric x/y. Source tables, custom sprite strings and caller-owned
 * track storage stay borrowed and stable. NULL sprites selects the three exact
 * TRAIL_WHITE names; custom lists are dense, nonempty and bounded to 2^24 entries.
 * Flags are resolved native truthiness. Add always seeds last_position using
 * physics-to-world conversion, including when world_coordinates is true.
 * Duplicate objects are allowed. No allocation, renderer or special trail.
 * Invalid add inputs preserve the list. */
bool ab_trajectory_follow_add(AbTrajectoryFollow *follow, AbDataTable *object,
                              const AbSpan *sprites, size_t sprite_count,
                              bool world_coordinates, bool velocity_angle,
                              AbError *error);

/* Scan the entire list and remove the last matching object. The original nil
 * removal index removes the final list entry when the requested object is absent.
 * NULL is an absent object token. Empty lists remain empty. No renderer disposal.
 */
bool ab_trajectory_follow_remove(AbTrajectoryFollow *follow, AbDataTable *object,
                                 AbError *error);

/* Read live positions in dense track order. Emit at most one point per track
 * when distance strictly exceeds the original sqrt(1000) spacing. Advance the
 * sprite cycle before calling emit; assign last_position only after it succeeds.
 * Velocity-angle uses the target's script atan2 result for the displacement.
 * Plain finite positions/differences/results only; unsupported numeric state
 * rejects when reached. Prior track/host effects survive later failure.
 *
 * The host owns actual LuaAssetRenderer-equivalent creation, position/angle
 * publication and attached-object/render-registry lifetimes. Special renderer
 * update/disposal and ordinary renderer cleanup remain separate. Callbacks may
 * change later object fields; they must not resize/rebind tracks, edit track
 * state, release borrowed data or reenter. Elapsed time is unused by this pass.
 */
bool ab_trajectory_follow_update(AbTrajectoryFollow *follow, AbError *error);

#endif
