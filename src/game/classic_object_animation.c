#include "ab/classic_object_animation.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric object animation field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite object animation field");
}

static bool write(AbLevelProperties *properties, size_t object, const char *name,
                  AbDataValue value, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE,
                           .table = &properties->objects[object].physical}};
    return ab_object_property_set(&access, span(name), value, error);
}

static bool number(AbLevelProperties *properties, size_t object, const char *name,
                   float value, AbError *error) {
    AbDataValue stored = {.kind = AB_DATA_NUMBER};
    memcpy(&stored.bits, &value, sizeof(value));
    return (isfinite(value) ||
            ab_fail(error, "Object animation exceeded finite range")) &&
           write(properties, object, name, stored, error);
}

static bool frame_name(float frame, char output[40], AbError *error) {
    if (!isfinite(frame) || frame < 1 || frame > 16777215 || frame != floorf(frame))
        return ab_fail(error, "Unsupported object animation frame index");
    int length = snprintf(output, 40, "damageSprites%u", (unsigned)frame);
    return (length > 0 && length < 40) ||
           ab_fail(error, "Object animation frame name exceeds bounds");
}

bool ab_classic_object_animation_update(AbClassicLevel *owner,
                                        AbClassicVisuals *visuals,
                                        AbStringOrderWorkspace *workspace,
                                        size_t object, float seconds, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner);
    if (!level.properties || !visuals || object >= level.properties->count ||
        !isfinite(seconds))
        return ab_fail(error, "Invalid concrete object animation owner");
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue animation = field(physical, "animation");
    if (!truthy(animation) || truthy(field(physical, "animationFinished")) ||
        truthy(field(physical, "animationDisabled")))
        return true;
    if (animation.kind != AB_DATA_TABLE || !animation.table)
        return ab_fail(error, "Object animation is not a retained frame table");
    float duration;
    AbDataValue raw_duration = field(animation.table, "timePerFrame");
    if (!truthy(raw_duration)) {
        duration = 0.5f;
    } else if (!numeric(raw_duration, &duration, error)) {
        return false;
    }
    AbDataValue raw_timer = field(physical, "animationTimer");
    float timer;
    if (raw_timer.kind == AB_DATA_NIL) {
        timer = duration;
        if (!number(level.properties, object, "animationTimer", timer, error) ||
            !number(level.properties, object, "currentAnimationFrame", 1, error))
            return false;
    } else if (!numeric(raw_timer, &timer, error)) {
        return false;
    }
    timer -= seconds;
    if (!number(level.properties, object, "animationTimer", timer, error))
        return false;
    if (timer > 0)
        return true;
    timer += duration;
    if (!number(level.properties, object, "animationTimer", timer, error))
        return false;
    float frame;
    char name[40];
    if (!numeric(field(physical, "currentAnimationFrame"), &frame, error) ||
        !frame_name(frame + 1, name, error))
        return false;
    if (field(animation.table, name).kind == AB_DATA_NIL) {
        if (truthy(field(animation.table, "looping"))) {
            frame = 1;
            if (!number(level.properties, object, "currentAnimationFrame", frame,
                        error))
                return false;
        } else if (!write(level.properties, object, "animationFinished",
                          (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error)) {
            return false;
        }
    } else {
        frame += 1;
        if (!number(level.properties, object, "currentAnimationFrame", frame, error))
            return false;
    }
    if (!frame_name(frame, name, error) ||
        !write(level.properties, object, "damageSprites", field(animation.table, name),
               error))
        return false;
    AbDamageSpriteNames damage;
    return ab_classic_visuals_update_damage(visuals, object, workspace, &damage,
                                            error) &&
           write(level.properties, object, "damageSprite", damage.sprite, error);
}
