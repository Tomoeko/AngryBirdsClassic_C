#include "ab/object_properties.h"

#include "ab/rotation.h"

#include <math.h>

bool ab_object_properties_plan(const AbBodyMotion *body, const AbGameFrameObject *frame,
                               bool previous_awake, int32_t minimum_x,
                               int32_t maximum_x, AbObjectPropertyPlan *output,
                               AbError *error) {
    if (!body || !frame || !output) {
        return ab_fail(error, "Missing object-property binding or output");
    }
    AbObjectPropertyPlan plan = {0};
    if (!body->awake && !previous_awake) {
        *output = plan;
        return true;
    }
    const AbBodyMassState *mass = &body->mass;
    AbVec2 position = frame->rendered.position;
    if (!isfinite(position.x) || !isfinite(position.y) || !isfinite(mass->mass) ||
        !isfinite(mass->linear_velocity.x) || !isfinite(mass->linear_velocity.y) ||
        !ab_angle_from_components(mass->transform.sine, mass->transform.cosine,
                                  &plan.values.pose.angle)) {
        return ab_fail(error, "Unsupported object-property numeric boundary");
    }
    plan.values.pose.position = position;
    plan.values.velocity = mass->linear_velocity;
    plan.values.sleeping = !body->awake;
    plan.values.outside_boundaries =
        mass->mass > 0 && (position.y > 20 || position.x < (float)minimum_x ||
                           position.x > (float)maximum_x);
    plan.publish = true;
    *output = plan;
    return true;
}
