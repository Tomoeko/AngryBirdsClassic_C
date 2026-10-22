#include "ab/bird_collision_notify.h"

#include <math.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool game_method(AbBirdCollisionScript *script,
                        const AbBlockCollisionNotice *notice, AbError *error) {
    if (!script) {
        return true;
    }
    if (!script->collision) {
        return ab_fail(error, "Missing game-object onBirdCollision method");
    }
    return script->collision(script->context, notice, error);
}

bool ab_bird_collision_notify_resolved(const AbBirdCollisionNotifyInput *input,
                                       const AbBirdCollisionNotifyHost *host,
                                       AbBirdCollisionNotification *output,
                                       AbError *error) {
    if (!input || !host || !host->object || !output ||
        (input->contact.first.size && !input->contact.first.data) ||
        (input->contact.second.size && !input->contact.second.data)) {
        return ab_fail(error, "Invalid bird collision notification input or host");
    }
    const AbBirdCollisionNotifyInput snapshot = *input;
    const AbBlockCollisionInput *contact = &snapshot.contact;
    AbBirdCollisionObject *objects[2] = {0};
    if (!host->object(host->context, contact->first, objects, error) ||
        !host->object(host->context, contact->second, objects + 1, error)) {
        return false;
    }
    if (!objects[0] || !objects[1]) {
        return ab_fail(error, "Missing bird collision world object");
    }
    if (truthy(objects[0]->projectile_feather) ||
        truthy(objects[1]->projectile_feather)) {
        *output = (AbBirdCollisionNotification){.objects = {objects[0], objects[1]},
                                                .projectile_skipped = true};
        return true;
    }
    if (!isfinite(contact->force) || !isfinite(contact->damage) ||
        !isfinite(contact->normal.x) || !isfinite(contact->normal.y) ||
        (snapshot.point_present &&
         (!isfinite(contact->point.x) || !isfinite(contact->point.y)))) {
        return ab_fail(error, "Invalid bird collision notification numbers");
    }
    AbBlockCollisionNotice notice = {.first = &objects[0]->object,
                                     .second = &objects[1]->object,
                                     .force = contact->force,
                                     .damage = contact->damage};
    if (host->level && !host->level(host->context, &notice, error)) {
        return false;
    }
    if (!host->definition) {
        return ab_fail(error, "Missing bird collision definition host");
    }
    AbBlockCollisionDefinition *definitions[2] = {0};
    if (!host->definition(host->context, contact->first, definitions, error) ||
        !host->definition(host->context, contact->second, definitions + 1, error)) {
        return false;
    }
    if (!definitions[0] || !definitions[1]) {
        return ab_fail(error, "Missing bird collision definition");
    }
    notice.normal_present = true;
    for (size_t side = 0; side < 2; ++side) {
        notice.first = &objects[side]->object;
        notice.second = &objects[1u - side]->object;
        notice.normal =
            side ? (AbVec2){-contact->normal.x, -contact->normal.y} : contact->normal;
        AbDataValue name = definitions[side]->on_collision;
        if (truthy(name)) {
            if (!host->named) {
                return ab_fail(error, "Missing required named bird collision host");
            }
            if (!host->named(host->context, name, &notice, error)) {
                return false;
            }
        }
        if (!host->scripts) {
            return ab_fail(error, "Missing bird block-event host");
        }
        if (!host->scripts(host->context, &notice, error)) {
            return false;
        }
    }
    notice.first = &objects[0]->object;
    notice.second = &objects[1]->object;
    notice.normal = contact->normal;
    notice.point = snapshot.point_present ? contact->point : (AbVec2){0};
    notice.point_present = snapshot.point_present;
    if (!truthy(objects[1]->object.sensor) &&
        !game_method(objects[0]->script, &notice, error)) {
        return false;
    }
    if (!truthy(objects[0]->object.sensor)) {
        if (truthy(objects[1]->controllable)) {
            notice.first = &objects[1]->object;
            notice.second = &objects[0]->object;
        }
        if (!game_method(objects[1]->script, &notice, error)) {
            return false;
        }
    }
    *output =
        (AbBirdCollisionNotification){.objects = {objects[0], objects[1]},
                                      .definitions = {definitions[0], definitions[1]}};
    return true;
}

bool ab_bird_collision_notify(const AbBirdCollisionNotifyInput *input,
                              const AbBirdCollisionNotifyHost *host,
                              bool *projectile_skipped, AbError *error) {
    if (!projectile_skipped) {
        return ab_fail(error, "Missing bird collision notification output");
    }
    AbBirdCollisionNotification result = {0};
    if (!ab_bird_collision_notify_resolved(input, host, &result, error)) {
        return false;
    }
    *projectile_skipped = result.projectile_skipped;
    return true;
}
