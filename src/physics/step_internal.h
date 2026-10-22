#ifndef AB_PHYSICS_STEP_INTERNAL_H
#define AB_PHYSICS_STEP_INTERNAL_H

#include "ab/physics_step.h"

bool ab_step_controls_valid(const AbBodyMotion *bodies, size_t count,
                            const AbFreeIslandStep *step);
void ab_step_apply_forces(AbBodyMotion *body, const AbFreeIslandStep *step);
void ab_step_move_body(AbBodyMotion *body, const AbFreeIslandStep *step);
bool ab_step_sync_body(AbBodyMotion *body);
void ab_step_sleep(AbBodyMotion *bodies, size_t count, const AbFreeIslandStep *step,
                   bool position_solved);

#endif
