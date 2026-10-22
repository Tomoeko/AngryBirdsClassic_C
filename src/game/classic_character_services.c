#include "ab/classic_character_services.h"
#include "ab/burn_manager.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool property(AbClassicCharacterServices *services, size_t object,
                     const char *name, AbDataValue *output, AbError *error) {
    return ab_level_property_get(services->level.properties, object, span(name), output,
                                 error);
}

static bool number(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Unsupported character service numeric property");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output)
               ? true
               : ab_fail(error, "Nonfinite character service numeric property");
}

static bool lose(void *context, bool *output, AbError *error) {
    AbClassicCharacterServices *services = context;
    if (!services || !services->initialized || !output)
        return ab_fail(error, "Invalid character loss query");
    AbClassicCharacterServiceConfig *config = &services->config;
    *output = false;
    if (truthy(*config->prevent_ending))
        return true;
    if (!truthy(*config->level_timeout)) {
        AbDataValue wait = *config->do_not_wait_for_moving_objects;
        if (truthy(*config->moving_objects) &&
            (wait.kind != AB_DATA_BOOLEAN || !wait.bits))
            return true;
        AbClassicBirdView birds = ab_classic_birds_view(config->birds);
        if (!birds.birds)
            return ab_fail(error, "Missing actual character loss bird table");
        if (birds.birds->count > birds.birds->capacity ||
            (birds.birds->count && !birds.birds->entries))
            return ab_fail(error, "Invalid character loss bird table storage");
        for (size_t order = 0; order < birds.birds->count; ++order) {
            const AbDataEntry *entry = birds.birds->entries + order;
            if (entry->value.kind == AB_DATA_NIL)
                continue;
            size_t object = SIZE_MAX;
            for (size_t index = 0; index < services->level.bird_count; ++index) {
                size_t candidate = services->level.bird_order[index];
                AbSpan name = services->level.bodies->objects[candidate].name;
                if (entry->key.kind == AB_DATA_STRING &&
                    entry->key.string.size == name.size &&
                    (!name.size ||
                     !memcmp(entry->key.string.data, name.data, name.size))) {
                    object = candidate;
                    break;
                }
            }
            if (object == SIZE_MAX || services->level.removed_objects[object] ||
                entry->value.kind != AB_DATA_TABLE ||
                entry->value.table !=
                    &services->level.properties->objects[object].physical)
                return ab_fail(error, "Unsupported character loss bird membership");
            AbDataValue controllable;
            if (!property(services, object, "controllable", &controllable, error))
                return false;
            if (truthy(controllable))
                return true;
        }
    }
    bool burning = false;
    if (!ab_burn_manager_is_burning(NULL, &burning, error))
        return false;
    *output = !burning;
    return true;
}

bool ab_classic_character_services_lose(AbClassicCharacterServices *services,
                                        bool *output, AbError *error) {
    return lose(services, output, error);
}

bool ab_classic_character_services_win(AbClassicCharacterServices *services,
                                       bool *output, AbError *error) {
    if (!services || !services->initialized || !output)
        return ab_fail(error, "Invalid concrete level-complete query");
    AbClassicCharacterServiceConfig *config = &services->config;
    *output = false;
    if (truthy(*config->prevent_ending))
        return true;
    if (!config->eagle_launched)
        return ab_fail(error, "Missing completion eagle state");
    if (config->eagle_launched->kind == AB_DATA_BOOLEAN &&
        config->eagle_launched->bits) {
        if (!config->eagle_timer)
            return ab_fail(error, "Missing completion eagle timer binding");
        if (config->eagle_timer->kind == AB_DATA_NIL)
            return true;
        float timer;
        if (!number(*config->eagle_timer, &timer, error))
            return false;
        if (timer > 0)
            return true;
    }
    if (!config->goals)
        return ab_fail(error, "Missing actual completion goals");
    bool cleared;
    if (!ab_classic_goals_check(config->goals, &cleared, error))
        return false;
    if (!cleared)
        return true;
    if (!config->do_not_timeout)
        return ab_fail(error, "Missing actual completion timeout flag");
    AbDataValue wait = *config->do_not_wait_for_moving_objects;
    *output = truthy(*config->do_not_timeout) || truthy(*config->level_timeout) ||
              !truthy(*config->moving_objects) ||
              (wait.kind == AB_DATA_BOOLEAN && wait.bits);
    return true;
}

static bool distance(void *context, size_t object, AbClassicCharacterKind kind,
                     float *output, AbError *error) {
    AbClassicCharacterServices *services = context;
    if (!services || !services->initialized || !output ||
        object >= services->level.bodies->count ||
        services->level.removed_objects[object])
        return ab_fail(error, "Invalid live character distance binding");
    AbVec2 position = {0};
    if (kind == AB_CLASSIC_CHARACTER_BIRD) {
        position = ab_classic_sling_view(services->config.sling).band.rest;
    } else if (kind == AB_CLASSIC_CHARACTER_GOAL) {
        AbDataValue x, y;
        if (!property(services, object, "x", &x, error) ||
            !property(services, object, "y", &y, error) ||
            !number(x, &position.x, error) || !number(y, &position.y, error))
            return false;
    } else {
        return ab_fail(error, "Unknown character distance kind");
    }
    AbClassicCameraView camera = ab_classic_camera_view(services->config.camera);
    position.x *= 20;
    position.y *= 20;
    float x = camera.system.screen_position.x - position.x;
    float y = camera.system.screen_position.y - position.y;
    float xx = x * x;
    float yy = y * y;
    float result = (float)sqrt((double)(xx + yy));
    if (!camera.system.mover || !isfinite(result))
        return ab_fail(error, "Unsupported character camera distance");
    *output = result;
    return true;
}

static bool sound(void *context, size_t object, AbClassicCharacterKind kind,
                  float volume, AbError *error) {
    AbClassicCharacterServices *services = context;
    if (!services || !services->initialized || object >= services->level.bodies->count)
        return ab_fail(error, "Invalid character sound binding");
    const AbDataValue *request;
    if (kind == AB_CLASSIC_CHARACTER_GOAL)
        request = services->config.pig_sound;
    else if (kind == AB_CLASSIC_CHARACTER_BIRD)
        request = services->config.bird_sound;
    else
        return ab_fail(error, "Unknown character sound kind");
    AbDataValue selected, gain = {.kind = AB_DATA_NUMBER};
    memcpy(&gain.bits, &volume, sizeof(gain.bits));
    uint32_t handle;
    return ab_audio_bank_name_value(services->config.audio, *request, &selected,
                                    error) &&
           ab_audio_bank_play(services->config.audio,
                              (AbAudioPlay){.name = selected,
                                            .volume = gain,
                                            .loop = {.kind = AB_DATA_BOOLEAN},
                                            .channel = {.kind = AB_DATA_NUMBER}},
                              &handle, error);
}

static bool hat(void *context, size_t object, AbError *error) {
    AbClassicCharacterServices *services = context;
    if (!services || !services->initialized || object >= services->level.bodies->count)
        return ab_fail(error, "Invalid character hat binding");
    return services->config.update_hat
               ? services->config.update_hat(services->config.context, object, error)
               : ab_fail(error, "Missing actual character hat update");
}

bool ab_classic_character_services_initialize(AbClassicCharacterServiceConfig config,
                                              AbClassicCharacterServices *output,
                                              AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!output || !level.properties || !level.bodies || !config.camera ||
        !ab_classic_camera_system(config.camera) || !config.sling || !config.birds ||
        !ab_classic_birds_view(config.birds).birds || !config.audio ||
        !config.prevent_ending || !config.level_timeout || !config.moving_objects ||
        !config.do_not_wait_for_moving_objects || !config.bird_sound ||
        !config.pig_sound)
        return ab_fail(error, "Invalid actual character service bindings");
    *output = (AbClassicCharacterServices){config, level, true};
    return true;
}

AbClassicCharacterHost
ab_classic_character_services_host(AbClassicCharacterServices *services) {
    return services && services->initialized
               ? (AbClassicCharacterHost){services, lose, distance, sound, hat}
               : (AbClassicCharacterHost){0};
}
