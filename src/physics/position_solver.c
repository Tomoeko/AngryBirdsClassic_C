#include "ab/physics_solver.h"
#include "ab/rotation.h"

#include "internal.h"
#include "position_internal.h"

static bool update_transform(AbPositionBody *body, AbVec2 local_center) {
    AbRotation rotation;
    if (!ab_rotation_from_angle(body->angle, &rotation)) {
        return false;
    }
    body->transform.sine = rotation.sine;
    body->transform.cosine = rotation.cosine;
    AbVec2 rotated = ab_physics_rotate(local_center, body->transform);
    body->transform.position =
        (AbVec2){body->center.x - rotated.x, body->center.y - rotated.y};
    return true;
}

bool ab_position_constraint_solve(AbPositionState *positions,
                                  const AbPositionConstraint *constraint,
                                  float *minimum_separation, float correction_rate) {
    AbPositionState first_position = positions[constraint->first_body];
    AbPositionState second_position = positions[constraint->second_body];
    AbPositionBody first = {.center = first_position.center,
                            .angle = first_position.angle,
                            .inverse_mass = constraint->first_inverse_mass,
                            .inverse_inertia = constraint->first_inverse_inertia};
    AbPositionBody second = {.center = second_position.center,
                             .angle = second_position.angle,
                             .inverse_mass = constraint->second_inverse_mass,
                             .inverse_inertia = constraint->second_inverse_inertia};
    for (size_t point = 0; point < constraint->geometry.point_count; ++point) {
        if (!update_transform(&first, constraint->first_local_center) ||
            !update_transform(&second, constraint->second_local_center)) {
            return false;
        }
        if (!ab_contact_position_point_rate(&constraint->geometry, point, &first,
                                            &second, minimum_separation,
                                            correction_rate)) {
            return false;
        }
    }
    positions[constraint->first_body] = (AbPositionState){first.center, first.angle};
    positions[constraint->second_body] = (AbPositionState){second.center, second.angle};
    return true;
}

AbPositionResult ab_contacts_solve_position(AbPositionState *positions,
                                            size_t body_count,
                                            const AbPositionConstraint *constraints,
                                            size_t constraint_count) {
    if (constraint_count && (!positions || !constraints)) {
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
    float minimum_separation = 0;
    for (size_t index = 0; index < constraint_count; ++index) {
        if (!ab_position_constraint_solve(positions, constraints + index,
                                          &minimum_separation, 0.2f)) {
            return AB_POSITION_INVALID;
        }
    }
    return minimum_separation >= -0.15f ? AB_POSITION_RESOLVED : AB_POSITION_UNRESOLVED;
}
