#include "ab/bird_collision.h"

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool continue_collision(const AbBirdCollisionNotifyInput *input,
                               const AbBirdCollisionNotification *notification,
                               AbBirdCollisionState state, AbBirdCollisionHost *host,
                               AbError *error) {
    AbBirdCollisionObject *first = notification->objects[0];
    AbBirdCollisionObject *second = notification->objects[1];
    const AbBlockCollisionInput *contact = &input->contact;
    bool related_hat = false;
    if (!ab_bird_collision_prelude(contact->first, first, second, contact->damage,
                                   state.prelude, host->prelude, &related_hat, error)) {
        return false;
    }
    if (related_hat) {
        return true;
    }
    AbBirdCollisionObject *flying = state.sprites->flying_bird;
    AbBirdCollisionMightyInput mighty = {.first = contact->first,
                                         .second = contact->second,
                                         .first_object = first,
                                         .second_object = second,
                                         .flying_present = flying != NULL,
                                         .flying_name = flying ? flying->object.name
                                                               : (AbSpan){0}};
    AbBirdCollisionMightyHost mighty_host = {host->notifications->context,
                                             host->notifications->object};
    if (!ab_bird_collision_mighty(&mighty, &mighty_host, error) ||
        !ab_bird_collision_sprites(first, second, state.sprites, host->sprites,
                                   error) ||
        !ab_bird_collision_specialty(contact, second, state.sprites, host->specialty,
                                     error)) {
        return false;
    }
    AbBirdCollisionProjectileInput projectile = {contact->first, contact->second, first,
                                                 second};
    AbBirdCollisionProjectileResult result = {0};
    if (!ab_bird_collision_projectiles(&projectile, state.grenades, host->projectiles,
                                       &result, error)) {
        return false;
    }
    if (result.return_early) {
        return true;
    }
    if (result.first_present && !truthy(second->object.sensor)) {
        if (!host->effect_definition) {
            return ab_fail(error, "Missing cached bird effect-definition view");
        }
        AbBirdCollisionEffectInput effects = {
            .first = contact->first,
            .second = contact->second,
            .first_present = true,
            .force = contact->force,
            .damage = contact->damage,
            .second_object = second,
            .first_definition = host->effect_definition(host->definition_context,
                                                        notification->definitions[0]),
            .second_definition = host->effect_definition(host->definition_context,
                                                         notification->definitions[1])};
        if (!ab_bird_collision_effects(&effects, host->effects, error)) {
            return false;
        }
    }
    return ab_bird_collision_damage_refresh(second, host->damage, error);
}

bool ab_bird_collision_callback(const AbBirdCollisionNotifyInput *input,
                                AbBirdCollisionState state, AbBirdCollisionHost *host,
                                AbError *error) {
    if (!input || !host) {
        return ab_fail(error, "Missing bird collision input or hosts");
    }
    const AbBirdCollisionNotifyInput captured = *input;
    AbBirdCollisionNotification notification = {0};
    if (!ab_bird_collision_notify_resolved(&captured, host->notifications,
                                           &notification, error)) {
        return false;
    }
    if (notification.projectile_skipped) {
        return true;
    }
    if (!state.prelude || !state.sprites) {
        return ab_fail(error, "Missing bird collision continuation state");
    }
    const AbDataValue *saved_binding = state.prelude->specialty_available_binding;
    state.prelude->specialty_available_binding =
        &state.sprites->bird_specialty_available;
    bool okay = continue_collision(&captured, &notification, state, host, error);
    state.prelude->specialty_available_binding = saved_binding;
    return okay;
}
