#include "ab/bubble_contact.h"

#include <math.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

bool ab_bubble_contact_begin(AbBubbleContactObject *collider,
                             AbBubbleContactObject *bubble, AbBubbleContactHost host,
                             AbError *error) {
    if (!collider || !bubble || collider == bubble) {
        return ab_fail(error, "Invalid bubble contact objects");
    }
    bool already =
        collider->in_bubble.kind == AB_DATA_BOOLEAN && collider->in_bubble.bits != 0;
    if (!already) {
        bubble->strength = (AbDataValue){.kind = AB_DATA_NUMBER};
        bubble->kill_silently = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        bubble->dead = true;
    }
    if (!truthy(collider->shot)) {
        return true;
    }
    AbVec2 velocity = {collider->body_velocity.x * 0.1f,
                       collider->body_velocity.y * 0.1f};
    if (!isfinite(velocity.x) || !isfinite(velocity.y)) {
        return ab_fail(error, "Invalid bubble contact body velocity");
    }
    collider->pending = (AbDeferredVelocity){velocity, true};
    collider->in_bubble = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    collider->anti_gravity_timer =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40a00000)};
    collider->sprite = (AbDataValue){
        .kind = AB_DATA_STRING, .string = {(const uint8_t *)"BUBBLE_PARTICLE_6", 17}};
    collider->sprite_scale =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f000000)};
    if (!already) {
        if (!host.trapped) {
            return ab_fail(error, "Missing bubble trapped callback");
        }
        if (!host.trapped(host.context, collider, error)) {
            return false;
        }
    }
    AbWorldManifold geometry = {0};
    if (!host.geometry) {
        return ab_fail(error, "Missing bubble contact geometry lookup");
    }
    if (!host.geometry(host.context, &geometry, error)) {
        return false;
    }
    if ((collider->name.size && !collider->name.data) ||
        (bubble->name.size && !bubble->name.data) || !isfinite(geometry.normal.x) ||
        !isfinite(geometry.normal.y) || !isfinite(geometry.points[0].x) ||
        !isfinite(geometry.points[0].y) || !host.collision) {
        return ab_fail(error, "Invalid bubble collision geometry, names or callback");
    }
    AbBubbleCollisionCall call = {.first = collider->name,
                                  .second = bubble->name,
                                  .normal = {-geometry.normal.x, -geometry.normal.y},
                                  .point = geometry.points[0]};
    return host.collision(host.context, &call, error);
}
