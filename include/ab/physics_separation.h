#ifndef AB_PHYSICS_SEPARATION_H
#define AB_PHYSICS_SEPARATION_H

#include "ab/physics_distance.h"
#include "ab/physics_sweep.h"

typedef enum {
    AB_SEPARATION_POINTS,
    AB_SEPARATION_FACE_FIRST,
    AB_SEPARATION_FACE_SECOND,
} AbSeparationKind;

typedef struct {
    const AbDistanceProxy *first;
    const AbDistanceProxy *second;
    AbSweep first_sweep;
    AbSweep second_sweep;
    AbSeparationKind kind;
    AbVec2 local_point;
    AbVec2 axis;
} AbSeparation;

/* Initialize a separating axis from a one/two-point distance cache. Proxy
 * structures and vertices stay borrowed and immutable; sweeps are copied.
 * Initialize output before use: the point case preserves inactive local_point.
 * Numeric distance/sweep contracts apply. Failed calls preserve outputs.
 */
bool ab_separation_initialize(AbSeparation *output, const AbDistanceCache *cache,
                              const AbDistanceProxy *first, const AbSweep *first_sweep,
                              const AbDistanceProxy *second,
                              const AbSweep *second_sweep, float fraction,
                              float *separation);
/* The face case returns -1 for its unused endpoint index. Evaluate ignores that
 * endpoint's supplied index. Outputs and borrowed inputs must be disjoint.
 */
bool ab_separation_find(const AbSeparation *state, float fraction, int32_t *first,
                        int32_t *second, float *separation);
bool ab_separation_evaluate(const AbSeparation *state, int32_t first, int32_t second,
                            float fraction, float *separation);

#endif
