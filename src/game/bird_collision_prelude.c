#include "ab/bird_collision_prelude.h"
#include "ab/ui_value_text.h"

#include <math.h>
#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool same_name(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            memcmp(first.data, second.data, first.size) == 0));
}

static bool related(AbBirdCollisionObject *first, AbBirdCollisionObject *second,
                    const AbBirdCollisionPreludeHost *host, bool *output,
                    AbError *error) {
    AbBirdCollisionHat *hat = NULL;
    if (!host->hat(host->context, first->object.name, &hat, error)) {
        return false;
    }
    *output = hat && same_name(hat->object_name, second->object.name);
    return true;
}

static bool reset_hat(AbBirdCollisionObject *first,
                      const AbBirdCollisionPreludeHost *host, AbError *error) {
    if (!truthy(first->shot)) {
        return true;
    }
    AbBirdCollisionHat *hat = NULL;
    if (!host->hat(host->context, first->object.name, &hat, error)) {
        return false;
    }
    if (!hat) {
        return true;
    }
    if (!host->object) {
        return ab_fail(error, "Missing bird collision hat-object host");
    }
    AbBirdCollisionObject *object = NULL;
    if (!host->object(host->context, hat->object_name, &object, error)) {
        return false;
    }
    if (!object) {
        return true;
    }
    object->defence = (AbDataValue){.kind = AB_DATA_NUMBER};
    object->strength = (AbDataValue){.kind = AB_DATA_NUMBER};
    return true;
}

static bool add_score(AbSpan incoming_first, AbBirdCollisionObject *first, float damage,
                      AbBirdCollisionPreludeState *state,
                      const AbBirdCollisionPreludeHost *host, AbError *error) {
    if (!host->score) {
        return ab_fail(error, "Missing bird collision damage-score host");
    }
    float increment;
    if (!host->score(host->context, damage, &increment, error)) {
        return false;
    }
    if (!isfinite(increment) || !isfinite(first->position.x) ||
        !isfinite(first->position.y) || !isfinite(state->floating_score_scaling)) {
        return ab_fail(error, "Invalid bird collision floating-score numbers");
    }
    uint32_t bits;
    memcpy(&bits, &increment, sizeof(bits));
    uint8_t storage[32];
    AbBirdCollisionFloatingScore request = {.position = first->position,
                                            .score = increment,
                                            .lifetime = 0.6f,
                                            .text_present = true};
    if (!ab_ui_value_text(
            (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = bits},
            storage, sizeof(storage), &request.text)) {
        return ab_fail(error, "Cannot format bird collision score");
    }
    float scale = 0.25f + increment / 3000.0f;
    request.max_scale = state->floating_score_scaling * scale;
    if (!isfinite(request.max_scale) || !host->floating) {
        return ab_fail(error, "Invalid bird collision floating-score request");
    }
    if (!host->floating(host->context, &request, error)) {
        return false;
    }
    if (!host->score_entry) {
        return ab_fail(error, "Missing bird collision score-entry host");
    }
    AbBirdCollisionScoreEntry *entry = NULL;
    if (!host->score_entry(host->context, incoming_first, &entry, error)) {
        return false;
    }
    if (!entry) {
        return ab_fail(error, "Missing bird collision score-entry storage");
    }
    if (!entry->present) {
        entry->present = true;
        entry->score = 0.0f;
    }
    if (!isfinite(entry->score)) {
        return ab_fail(error, "Invalid bird collision score total");
    }
    entry->score = entry->score + increment;
    if (!isfinite(entry->score) || state->count >= state->capacity ||
        !state->collision_scores) {
        return ab_fail(error, "Bird collision score append exceeds finite storage");
    }
    state->collision_scores[state->count++] = increment;
    return true;
}

bool ab_bird_collision_prelude(AbSpan incoming_first, AbBirdCollisionObject *first,
                               AbBirdCollisionObject *second, float damage,
                               AbBirdCollisionPreludeState *state,
                               const AbBirdCollisionPreludeHost *host,
                               bool *related_hat, AbError *error) {
    if (!first || !second || !state || !host || !host->hat || !related_hat) {
        return ab_fail(error, "Invalid bird collision prelude arguments");
    }
    bool skip;
    if (!related(first, second, host, &skip, error) ||
        (!skip && !related(second, first, host, &skip, error))) {
        return false;
    }
    if (skip) {
        *related_hat = true;
        return true;
    }
    if (!reset_hat(first, host, error)) {
        return false;
    }
    if (!isfinite(damage)) {
        return ab_fail(error, "Invalid bird collision damage");
    }
    if (damage > 0.0f &&
        !add_score(incoming_first, first, damage, state, host, error)) {
        return false;
    }
    if (truthy(state->delta_based_slingshot) &&
        ((truthy(first->shot) && !truthy(first->has_collided)) ||
         (truthy(second->shot) && !truthy(second->has_collided))) &&
        truthy(state->specialty_available_binding ? *state->specialty_available_binding
                                                  : state->bird_specialty_available)) {
        if (!host->cooldown) {
            return ab_fail(error, "Missing bird collision cooldown host");
        }
        if (!host->cooldown(host->context, error)) {
            return false;
        }
    }
    if (host->achievement && !host->achievement(host->context, first, second, error)) {
        return false;
    }
    *related_hat = false;
    return true;
}
