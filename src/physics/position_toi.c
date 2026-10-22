#include "position_internal.h"

AbPositionResult ab_contacts_solve_toi_position(AbPositionState *positions,
                                                size_t body_count,
                                                const AbPositionConstraint *constraints,
                                                size_t constraint_count,
                                                size_t first_toi, size_t second_toi) {
    if (!constraint_count) {
        return AB_POSITION_RESOLVED;
    }
    if (!positions || !constraints || first_toi >= body_count ||
        second_toi >= body_count) {
        return AB_POSITION_INVALID;
    }
    for (size_t index = 0; index < constraint_count; ++index) {
        const AbPositionConstraint *constraint = constraints + index;
        if (constraint->first_body >= body_count ||
            constraint->second_body >= body_count ||
            constraint->geometry.point_count > 2 ||
            (unsigned)constraint->geometry.kind > AB_CONTACT_FACE_SECOND) {
            return AB_POSITION_INVALID;
        }
    }
    float minimum = 0;
    for (size_t index = 0; index < constraint_count; ++index) {
        AbPositionConstraint constraint = constraints[index];
        if (constraint.first_body != first_toi && constraint.first_body != second_toi) {
            constraint.first_inverse_mass = 0;
            constraint.first_inverse_inertia = 0;
        }
        if (constraint.second_body != first_toi &&
            constraint.second_body != second_toi) {
            constraint.second_inverse_mass = 0;
            constraint.second_inverse_inertia = 0;
        }
        if (!ab_position_constraint_solve(positions, &constraint, &minimum, 0.75f)) {
            return AB_POSITION_INVALID;
        }
    }
    return minimum >= -0.075f ? AB_POSITION_RESOLVED : AB_POSITION_UNRESOLVED;
}
