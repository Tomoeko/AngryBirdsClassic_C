#ifndef AB_OBJECT_FRICTION_H
#define AB_OBJECT_FRICTION_H

#include "ab/base.h"
#include "ab/physics_step.h"

typedef struct {
    float air_resistance;
    AbVec2 relative;
} AbObjectFriction;

/* GameLua's per-object force preparation before each physics step. The caller
 * resolves definition numbers (missing/nonnumeric values become zero). Nonzero
 * air resistance takes priority; otherwise the rotated relative coefficients
 * apply only when their velocity projection is negative. Forces apply at the
 * body's center and wake a dynamic body even when the air force is zero.
 * Other body types and inactive relative friction preserve all body state.
 * The native elapsed argument is unused. Used numbers/results must be finite;
 * failure preserves the body. No allocation, resource lookup or original code. */
bool ab_object_friction_apply(AbBodyMotion *body, const AbObjectFriction *settings,
                              AbError *error);

#endif
