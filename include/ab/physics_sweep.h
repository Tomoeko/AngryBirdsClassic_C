#ifndef AB_PHYSICS_SWEEP_H
#define AB_PHYSICS_SWEEP_H

#include "ab/physics_geometry.h"

typedef struct {
    AbVec2 local_center;
    AbVec2 previous_center;
    AbVec2 center;
    float previous_angle;
    float angle;
    float alpha;
} AbSweep;

/* Interpolate the center and angle at a finite fraction, then shift to the
 * transform origin. Alpha is independent of this interpolation. Finite inputs
 * and the rotation module's supported interpolated angle are required. Failure
 * preserves output. Normalize changes only angles; advance changes the previous
 * endpoint and alpha when moving forward within [0,1]. Both preserve the sweep
 * on invalid inputs or nonfinite arithmetic. No allocation occurs.
 */
bool ab_sweep_transform(const AbSweep *sweep, float fraction, AbRigidTransform *output);
bool ab_sweep_normalize(AbSweep *sweep);
bool ab_sweep_advance(AbSweep *sweep, float alpha);

#endif
