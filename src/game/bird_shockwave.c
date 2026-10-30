#include "ab/bird_shockwave.h"

#include "ab/score.h"
#include "ab/script_math.h"

#include <math.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool number(const AbDataTable *table, const char *name, bool optional,
                   float *output, AbError *error) {
    AbDataValue value = field(table, name);
    if (optional && !truthy(value)) {
        *output = 0;
        return true;
    }
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Missing numeric shockwave field: %s", name);
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite shockwave field: %s", name);
}

bool ab_bird_shockwave_begin(const AbDataTable *table, AbBirdShockwave *output,
                             AbError *error) {
    if (!table || !output)
        return ab_fail(error, "Missing shockwave definition/output");
    AbBirdShockwave wave = {0};
    AbBirdShockwaveDefinition *definition = &wave.definition;
    const char *const names[] = {"duration", "delay",         "radius",
                                 "force",    "minimumForce",  "forceDropOffPoint",
                                 "damage",   "minimumDamage", "initialRadius"};
    float *const values[] = {&definition->duration,      &definition->delay,
                             &definition->radius,        &definition->force,
                             &definition->minimum_force, &definition->force_dropoff,
                             &definition->damage,        &definition->minimum_damage,
                             &definition->initial_radius};
    for (size_t index = 0; index < sizeof(names) / sizeof(*names); ++index)
        if (!number(table, names[index], index >= 7, values[index], error))
            return false;
    if (definition->duration <= 0 || definition->delay < 0 || definition->radius < 0 ||
        definition->initial_radius < 0 || definition->force_dropoff >= 1)
        return ab_fail(error, "Unsupported shockwave numeric domain");
    definition->impulse = truthy(field(table, "forceIsImpulse"));
    definition->damage_from_definition =
        truthy(field(table, "damageBasedOnMaxStrength"));
    wave.time = definition->duration;
    wave.delay_timer = definition->delay;
    wave.delay_present = true;
    *output = wave;
    return true;
}

bool ab_bird_shockwave_advance(AbBirdShockwave *wave, float seconds,
                               AbBirdShockwavePhase *output, AbError *error) {
    if (!wave || !output || !isfinite(seconds) || seconds < 0 ||
        !isfinite(wave->time) || !isfinite(wave->delay_timer) ||
        wave->definition.duration <= 0)
        return ab_fail(error, "Invalid shockwave phase input");
    AbBirdShockwave next = *wave;
    AbBirdShockwavePhase phase = {0};
    const AbBirdShockwaveDefinition *definition = &wave->definition;
    float ratio;
    if (next.delay_present) {
        phase.delaying = true;
        next.delay_timer -= seconds;
        ratio = fmaxf(0, (definition->duration + next.delay_timer) /
                             (definition->duration + definition->delay));
        if (next.delay_timer <= 0)
            next.delay_present = false;
    } else {
        phase.speed_written = true;
        phase.physics_speed = 0.6f;
        ratio = fmaxf(0, next.time / (definition->duration + definition->delay));
        phase.time_ratio = fmaxf(0, next.time / definition->duration);
        float angle = (float)3.141592653589793 * phase.time_ratio;
        angle *= 0.5f;
        if (!ab_script_cosine(angle, &next.progress))
            return ab_fail(error, "Shockwave progress exceeds the source math domain");
        next.current_radius =
            definition->initial_radius +
            next.progress * (definition->radius - definition->initial_radius);
        phase.shake_present = true;
        phase.camera_shake = 25 * (1 - next.progress);
        if (1 - phase.time_ratio < definition->force_dropoff) {
            next.force = definition->force;
        } else {
            float fraction = 1 - phase.time_ratio / (1 - definition->force_dropoff);
            next.force = definition->force +
                         fraction * (definition->minimum_force - definition->force);
        }
        next.time -= seconds;
        if (next.time <= 0) {
            phase.finished = true;
            phase.physics_speed = 1;
            phase.shake_present = false;
        }
    }
    float overlay;
    if (!ab_script_sine((float)3.141592653589793 * ratio, &overlay))
        return ab_fail(error, "Shockwave overlay exceeds the source math domain");
    phase.overlay_alpha = overlay * 0.7f;
    if (!isfinite(next.time) || !isfinite(next.delay_timer) ||
        !isfinite(next.current_radius) || !isfinite(next.force))
        return ab_fail(error, "Nonfinite shockwave phase result");
    *wave = next;
    *output = phase;
    return true;
}

bool ab_bird_shockwave_impact(const AbBirdShockwave *wave,
                              const AbBirdShockwaveObject *object,
                              AbBirdShockwaveImpact *output, AbError *error) {
    if (!wave || !object || !output)
        return ab_fail(error, "Missing shockwave object/output");
    AbBirdShockwaveImpact impact = {0};
    if (object->bird && !object->shot) {
        *output = impact;
        return true;
    }
    float squared = object->relative.x * object->relative.x +
                    object->relative.y * object->relative.y;
    float limit = wave->current_radius * wave->current_radius;
    if (!isfinite(squared) || !isfinite(limit) || !isfinite(wave->force))
        return ab_fail(error, "Nonfinite shockwave object geometry/force");
    if (!(squared < limit)) {
        *output = impact;
        return true;
    }
    impact.inside = impact.break_joints = true;
    impact.mark_damaged = !object->bird && !object->same_wave;
    if (impact.mark_damaged && object->live) {
        float strength = wave->definition.damage_from_definition &&
                                 object->definition_strength_present
                             ? object->definition_strength
                             : object->strength;
        float fraction = wave->definition.minimum_damage +
                         (1 - wave->progress) * (wave->definition.damage -
                                                 wave->definition.minimum_damage);
        float damage = fraction * strength - object->definition_defence;
        if (!isfinite(damage))
            return ab_fail(error, "Nonfinite shockwave damage");
        if (damage > 0) {
            impact.inflict = true;
            impact.damage = damage;
            if (!ab_damage_score(damage, &impact.score))
                return ab_fail(error, "Shockwave score exceeds finite range");
        }
    }
    if (wave->force >= 1) {
        impact.impulse = wave->definition.impulse && impact.mark_damaged;
        impact.force = !wave->definition.impulse;
        if (impact.impulse || impact.force)
            impact.vector = (AbVec2){object->relative.x * wave->force,
                                     object->relative.y * wave->force};
    }
    if (!isfinite(impact.vector.x) || !isfinite(impact.vector.y))
        return ab_fail(error, "Shockwave force exceeds finite range");
    *output = impact;
    return true;
}
