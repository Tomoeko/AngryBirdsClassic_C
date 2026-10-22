#include "ab/bird_collision_effects.h"

#include <math.h>
#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool named(AbDataValue value, const char *name) {
    return value.kind == AB_DATA_STRING && value.string.data &&
           value.string.size == strlen(name) &&
           !memcmp(value.string.data, name, value.string.size);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Bird contact effects require numeric metadata");
    }
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite bird contact metadata");
}

static float arm_result(float value) {
    if (isnan(value)) {
        uint32_t word = UINT32_C(0x7fc00000);
        memcpy(&value, &word, sizeof(value));
    }
    return value;
}

static bool play(const AbBirdCollisionEffectHost *host, AbDataValue sound, float volume,
                 AbError *error) {
    if (sound.kind == AB_DATA_NIL) {
        return true;
    }
    if (!host->audio_name) {
        return ab_fail(error, "Missing bird collision audio-name host");
    }
    bool (*audio)(void *, AbAudioPlay, AbError *) = host->audio;
    AbDataValue name = {0};
    if (!host->audio_name(host->context, sound, &name, error)) {
        return false;
    }
    if (!audio) {
        return ab_fail(error, "Missing bird collision audio host");
    }
    uint32_t word;
    memcpy(&word, &volume, sizeof(word));
    return audio(host->context,
                 (AbAudioPlay){
                     .name = name,
                     .volume = {.kind = AB_DATA_NUMBER, .bits = word},
                     .loop = {.kind = AB_DATA_BOOLEAN},
                     .channel = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)}},
                 error);
}

static bool collision_particles(AbBirdCollisionEffectBird *bird, AbSpan first,
                                const AbBirdCollisionEffectHost *host, AbError *error) {
    if (!bird || !bird->script) {
        return true;
    }
    if (!bird->script->get_particles) {
        return ab_fail(error, "Missing bird particle property method");
    }
    AbDataValue result = {0};
    if (!bird->script->get_particles(bird->script->context, &result, error)) {
        return false;
    }
    if (result.kind != AB_DATA_TABLE || !result.table ||
        result.table->count > result.table->capacity ||
        (result.table->count && !result.table->entries)) {
        return ab_fail(error, "Invalid bird particles property");
    }
    const AbDataValue *found = ab_data_get(
        result.table, (AbDataValue){.kind = AB_DATA_STRING,
                                    .string = {(const uint8_t *)"collision", 9}});
    AbDataValue descriptor = found ? *found : (AbDataValue){0};
    if (!truthy(descriptor)) {
        return true;
    }
    if (!host->particles) {
        return ab_fail(error, "Missing bird collision particle host");
    }
    return host->particles(host->context, first, descriptor, error);
}

bool ab_bird_collision_effects(const AbBirdCollisionEffectInput *input,
                               const AbBirdCollisionEffectHost *host, AbError *error) {
    if (!input || !input->second_object) {
        return ab_fail(error, "Missing bird contact effect input");
    }
    if (truthy(input->second_object->object.sensor) || !input->first_present) {
        return true;
    }
    const AbBirdCollisionEffectInput captured = *input;
    if (!host || !host->bird || !captured.first_definition ||
        !captured.second_definition || (captured.first.size && !captured.first.data) ||
        (captured.second.size && !captured.second.data) || !isfinite(captured.force) ||
        !isfinite(captured.damage)) {
        return ab_fail(error, "Invalid bird contact effect state or host");
    }
    AbBirdCollisionEffectBird *bird = NULL;
    if (!host->bird(host->context, captured.first, &bird, error)) {
        return false;
    }
    float momentum = 0;
    AbDataValue material = captured.second_definition->material_name;
    if ((named(material, "immovable") || named(material, "staticGround")) && bird &&
        bird->x_velocity.kind != AB_DATA_NIL && bird->y_velocity.kind != AB_DATA_NIL) {
        float x = 0, y = 0, mass = 0;
        if (!numeric(bird->x_velocity, &x, error) ||
            !numeric(bird->y_velocity, &y, error)) {
            return false;
        }
        float squared = x * x + y * y;
        float speed = (float)sqrt((double)squared);
        if (!numeric(bird->mass, &mass, error)) {
            return false;
        }
        momentum = arm_result(arm_result(speed * mass) / 10.0f);
    }
    if (!(host->threshold < captured.force) && !(host->threshold < momentum)) {
        return true;
    }
    AbDataValue target_sound = captured.second_definition->collision_sound;
    if (target_sound.kind != AB_DATA_NIL && 10.0f < captured.damage &&
        truthy(captured.second_definition->damage_sound)) {
        target_sound = captured.second_definition->damage_sound;
    }
    if (!play(host, target_sound, 0x1.666666p-1f, error) ||
        !collision_particles(bird, captured.first, host, error)) {
        return false;
    }
    float volume = 0x1.666666p-1f;
    if (bird) {
        float mass = 0;
        if (!numeric(bird->mass, &mass, error)) {
            return false;
        }
        float denominator = arm_result(60.0f * mass) / 10.0f;
        if (1.0f < captured.damage) {
            denominator = arm_result(denominator * captured.damage);
        }
        float maximum = captured.force;
        if (maximum < momentum) {
            maximum = momentum;
        }
        volume = arm_result(maximum / denominator);
        if (volume < 0x1.99999ap-3f) {
            volume = 0x1.99999ap-3f;
        }
        if (1.0f < volume) {
            volume = 1.0f;
        }
    }
    return play(host, captured.first_definition->collision_sound, volume, error);
}
