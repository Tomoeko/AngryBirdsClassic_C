#include "ab/physics_contact_state.h"

#include <math.h>

bool ab_contact_state_initialize(AbContactState *contact, float first_friction,
                                 float second_friction, float first_restitution,
                                 float second_restitution) {
    if (!contact || !isfinite(first_friction) || first_friction < 0 ||
        !isfinite(second_friction) || second_friction < 0 ||
        !isfinite(first_restitution) || !isfinite(second_restitution)) {
        return false;
    }
    float product = first_friction * second_friction;
    if (!isfinite(product)) {
        return false;
    }
    contact->flags = AB_CONTACT_ENABLED;
    contact->manifold.point_count = 0;
    contact->friction = sqrtf(product);
    contact->restitution =
        first_restitution > second_restitution ? first_restitution : second_restitution;
    contact->tangent_speed = 0;
    return true;
}

static void retain_impulses(AbManifold *current, const AbManifold *previous) {
    for (size_t point = 0; point < current->point_count; ++point) {
        AbContactPoint *output = current->points + point;
        output->normal_impulse = 0;
        output->tangent_impulse = 0;
        for (size_t old = 0; old < previous->point_count; ++old) {
            if (previous->points[old].feature_id == output->feature_id) {
                output->normal_impulse = previous->points[old].normal_impulse;
                output->tangent_impulse = previous->points[old].tangent_impulse;
                break;
            }
        }
    }
}

static void wake_body(AbBodyMotion *body) {
    if (!body->awake) {
        body->awake = true;
        body->sleep_time = 0;
    }
}

bool ab_contact_state_update(AbContactState *contact, AbBodyMotion *first,
                             AbBodyMotion *second, bool sensor,
                             const AbContactEvaluator *evaluator,
                             const AbContactListener *listener) {
    if (!contact || !first || !second || contact->manifold.point_count > 2 ||
        !evaluator || (sensor ? !evaluator->overlap : !evaluator->manifold)) {
        return false;
    }
    AbManifold previous = contact->manifold;
    bool was_touching = (contact->flags & AB_CONTACT_TOUCHING) != 0;
    bool touching;
    contact->flags |= AB_CONTACT_ENABLED;
    if (sensor) {
        touching = evaluator->overlap(evaluator->context);
        contact->manifold.point_count = 0;
    } else {
        evaluator->manifold(evaluator->context, &contact->manifold);
        if (contact->manifold.point_count > 2) {
            return false;
        }
        touching = contact->manifold.point_count != 0;
        retain_impulses(&contact->manifold, &previous);
        if (touching != was_touching) {
            wake_body(first);
            wake_body(second);
        }
    }
    if (touching) {
        contact->flags |= AB_CONTACT_TOUCHING;
    } else {
        contact->flags &= ~(uint32_t)AB_CONTACT_TOUCHING;
    }
    if (!listener) {
        return true;
    }
    if (touching && !was_touching && listener->begin) {
        listener->begin(listener->context, contact);
    }
    if (!touching && was_touching && listener->end) {
        listener->end(listener->context, contact);
    }
    if (!sensor && touching && listener->pre_solve) {
        listener->pre_solve(listener->context, contact, &previous);
    }
    return true;
}
