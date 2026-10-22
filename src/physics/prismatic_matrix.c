#include "prismatic_internal.h"

AbSymmetric3 ab_prismatic_constraint_matrix(const AbPrismaticState *state) {
    float first_axis = state->inverse_inertia[0] * state->axis_arms[0];
    float second_axis = state->inverse_inertia[1] * state->axis_arms[1];
    float masses = state->inverse_mass[0] + state->inverse_mass[1];
    float zz = masses;
    zz += first_axis * state->axis_arms[0];
    zz += second_axis * state->axis_arms[1];
    float first_perp = state->inverse_inertia[0] * state->perpendicular_arms[0];
    float second_perp = state->inverse_inertia[1] * state->perpendicular_arms[1];
    float xx = masses;
    xx += first_perp * state->perpendicular_arms[0];
    xx += second_perp * state->perpendicular_arms[1];
    float xz = second_perp * state->axis_arms[1];
    xz += first_perp * state->axis_arms[0];
    float yy = state->inverse_inertia[0] + state->inverse_inertia[1];
    if (yy == 0) {
        yy = 1;
    }
    return (AbSymmetric3){xx, first_perp + second_perp, xz,
                          yy, first_axis + second_axis, zz};
}
