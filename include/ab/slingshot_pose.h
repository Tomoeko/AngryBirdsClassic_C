#ifndef AB_SLINGSHOT_POSE_H
#define AB_SLINGSHOT_POSE_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    AbVec2 start_physics;
    AbVec2 band_physics;
    float band_angle;
    float distance;
    float bird_radius;
    float bird_width;
    float physics_to_world;
    bool bird_present;
    bool radius_present;
    bool box;
} AbSlingshotPoseInput;

typedef struct {
    AbVec2 position;
    AbVec2 stretch_position;
    float angle;
    float width;
} AbSlingshotPose;

/* SlingshotSystem.calculateRubberBand's draw pose. The position input is the
 * level-start vector, distinct from the sling-base vector. The native coordinate
 * transform multiplies physics positions by twenty, independently of the Lua
 * physicsToWorld scalar used for the bird's radius adjustment. A missing bird
 * skips that adjustment and all trig. A box or a missing radius selects half
 * the bird width; other birds use their present radius.
 * Width starts at 25; positive distance selects clamp(50 / distance, 10, 25).
 * Preserve Lua's promoted sine/cosine and binary32 operation order. All used
 * numbers/results must be finite; evaluated angles must have |angle| <= 1024.
 * No allocation or callbacks. Failure preserves output. */
bool ab_slingshot_render_pose(const AbSlingshotPoseInput *input,
                              AbSlingshotPose *output, AbError *error);

typedef struct {
    AbVec2 object_position;
    float radius;
    float back_pivot_y;
    float back_height;
    float physics_scale;
    bool object_present;
    bool radius_present;
} AbSlingshotStartInput;

typedef struct {
    AbVec2 level_start;
    AbVec2 base;
    AbVec2 band;
} AbSlingshotStartPose;

/* setPositionByStartObject: preserve all three vectors when the object is nil.
 * Otherwise use resolved back-sprite pivot/bounds and the object's physics
 * position. Missing radius selects positive zero. The level-start Y subtracts
 * (height - pivot_y) * physics_scale, then adds radius and binary32 0.2; the
 * base keeps the object position, and the band subtracts binary32 0.1 from
 * each level-start component. All used inputs/intermediates/results must be
 * finite. No callbacks, resources or allocation. Failure preserves output. */
bool ab_slingshot_start_pose(const AbSlingshotStartInput *input,
                             AbSlingshotStartPose *output, AbError *error);

#endif
