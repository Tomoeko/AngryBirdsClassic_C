#include "ab/bird_remove.h"
#include "ab/object_property_access.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool table_valid(AbDataValue value) {
    return value.kind == AB_DATA_TABLE && value.table &&
           value.table->count <= value.table->capacity &&
           (!value.table->count || value.table->entries);
}

static bool field(AbDataValue table, AbDataValue key, AbDataValue *output,
                  AbError *error) {
    if (!table_valid(table)) {
        return ab_fail(error, "Bird removal indexes a non-table");
    }
    const AbDataValue *found = ab_data_get(table.table, key);
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool assign(AbDataValue table, AbDataValue name, AbDataValue value,
                   AbError *error) {
    if (name.kind != AB_DATA_STRING || (name.string.size && !name.string.data)) {
        return ab_fail(error, "Bird removal needs a string table key");
    }
    AbObjectPropertyAccess access = {.physics_object = table};
    return ab_object_property_set(&access, name.string, value, error);
}

static bool object_name(AbDataValue object, AbDataValue *output, AbError *error) {
    return field(object, text("name"), output, error);
}

static bool remove_hat(AbDataValue object, AbBirdRemoveState *state, AbError *error) {
    AbDataValue name = {0}, entry = {0}, hat = {0}, world = {0}, physical = {0};
    if (!object_name(object, &name, error)) {
        return false;
    }
    if (name.kind == AB_DATA_NIL) {
        return true;
    }
    if (!field(state->hats, name, &entry, error)) {
        return false;
    }
    if (entry.kind == AB_DATA_NIL) {
        return true;
    }
    if (!field(entry, text("hat"), &hat, error)) {
        return false;
    }
    if (!truthy(hat)) {
        return true;
    }
    if (!field(state->objects, text("world"), &world, error) ||
        !field(world, hat, &physical, error)) {
        return false;
    }
    if (!truthy(physical)) {
        return true;
    }
    AbDataValue zero = {.kind = AB_DATA_NUMBER};
    return assign(physical, text("defence"), zero, error) &&
           assign(physical, text("strength"), zero, error);
}

static bool removal_particles(AbDataValue object, AbDataValue name,
                              AbBirdRemoveHost *host, AbError *error) {
    AbDataValue script = {0}, particles = {0}, descriptor = {0};
    if (!field(object, text("gameObjectScript"), &script, error)) {
        return false;
    }
    if (!truthy(script)) {
        return true;
    }
    if (!table_valid(script) || !host || !host->particles) {
        return ab_fail(error, "Missing bird-removal script particle method");
    }
    if (!host->particles(host->context, script, &particles, error)) {
        return false;
    }
    if (!truthy(particles)) {
        return true;
    }
    if (!field(particles, text("removal"), &descriptor, error)) {
        return false;
    }
    if (!truthy(descriptor)) {
        return true;
    }
    if (!host->emit) {
        return ab_fail(error, "Missing bird-removal particle emitter");
    }
    return host->emit(host->context, name, descriptor, false, false, error);
}

static bool play_removal_sound(AbDataValue object, AbDataValue name,
                               AbDataValue play_sound, AbBirdRemoveState *state,
                               AbBirdRemoveHost *host, AbError *error) {
    AbDataValue bubble = {0}, sound = text("bird_destroyed");
    if (!field(object, text("inBubble"), &bubble, error)) {
        return false;
    }
    if (truthy(bubble)) {
        AbDataValue definition = {0}, selected = {0};
        if (!host || !host->definition) {
            return ab_fail(error, "Missing bubble bird-removal definition host");
        }
        if (!host->definition(host->context, name, &definition, error) ||
            !field(definition, text("destroyedInsideBubbleSound"), &selected, error)) {
            return false;
        }
        if (truthy(selected)) {
            sound = selected;
        }
    }
    if (!truthy(play_sound) || !truthy(state->level_effect)) {
        return true;
    }
    if (!table_valid(state->level_effect) || !host || !host->volume) {
        return ab_fail(error, "Missing bird-removal volume method");
    }
    AbDataValue volume = {0};
    if (!host->volume(host->context, state->level_effect, object, &volume, error)) {
        return false;
    }
    bool (*audio)(void *, AbAudioPlay, AbError *) = host->audio;
    void *audio_context = host->context;
    AbDataValue resolved = {0};
    if (!host->audio_name) {
        return ab_fail(error, "Missing bird-removal audio-name host");
    }
    if (!host->audio_name(host->context, sound, &resolved, error)) {
        return false;
    }
    if (!audio) {
        return ab_fail(error, "Missing cached bird-removal audio host");
    }
    AbAudioPlay request = {
        .name = resolved, .volume = volume, .loop = {.kind = AB_DATA_BOOLEAN}};
    return audio(audio_context, request, error);
}

static bool same_name(AbDataValue first, AbDataValue second) {
    return first.kind == AB_DATA_STRING && second.kind == AB_DATA_STRING &&
           first.string.size == second.string.size &&
           (!first.string.size ||
            !memcmp(first.string.data, second.string.data, first.string.size));
}

bool ab_bird_remove(AbDataValue object, AbDataValue play_sound,
                    AbBirdRemoveState *state, AbBirdRemoveHost *host, AbError *error) {
    if (!state || !table_valid(object)) {
        return ab_fail(error, "Invalid bird-removal object or state");
    }
    if (play_sound.kind == AB_DATA_NIL) {
        play_sound = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    }
    AbDataValue script = {0}, name = {0}, definition = {0}, hook = {0};
    if (!field(object, text("gameObjectScript"), &script, error)) {
        return false;
    }
    if (truthy(script)) {
        if (!table_valid(script) || !host || !host->destroy) {
            return ab_fail(error, "Missing bird-removal script destruction method");
        }
        if (!host->destroy(host->context, script, error)) {
            return false;
        }
    }
    if (!host || !host->definition) {
        return ab_fail(error, "Missing bird-removal definition host");
    }
    if (!object_name(object, &name, error) ||
        !host->definition(host->context, name, &definition, error) ||
        !field(definition, text("onBirdRemoved"), &hook, error)) {
        return false;
    }
    if (truthy(hook)) {
        if (!host->hook) {
            return ab_fail(error, "Missing named onBirdRemoved hook");
        }
        if (!host->hook(host->context, hook, object, error)) {
            return false;
        }
    }
    if (!remove_hat(object, state, error) || !object_name(object, &name, error) ||
        name.kind != AB_DATA_STRING || (name.string.size && !name.string.data)) {
        return ab_fail(error, "Bird removal cannot capture the current name");
    }
    if (!removal_particles(object, name, host, error)) {
        return false;
    }
    if (!table_valid(state->camera) || !host->camera) {
        return ab_fail(error, "Missing bird-removal camera method");
    }
    if (!host->camera(host->context, state->camera, object, error)) {
        return false;
    }
    if (same_name(state->current_bird_name, name)) {
        state->current_bird_name = (AbDataValue){0};
    }
    if (state->flying_bird.kind == AB_DATA_TABLE &&
        state->flying_bird.table == object.table) {
        state->flying_bird = (AbDataValue){0};
        state->specialty_available = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
    if (!play_removal_sound(object, name, play_sound, state, host, error)) {
        return false;
    }
    if (!host->remove) {
        return ab_fail(error, "Missing physical bird-removal host");
    }
    if (!host->remove(host->context, name, error)) {
        return false;
    }
    AbDataValue world = {0}, dynamic = {0};
    return field(state->objects, text("world"), &world, error) &&
           assign(world, name, (AbDataValue){0}, error) &&
           field(state->objects, text("dynamic"), &dynamic, error) &&
           assign(dynamic, name, (AbDataValue){0}, error) &&
           assign(state->birds, name, (AbDataValue){0}, error) &&
           assign(state->other_birds, name, (AbDataValue){0}, error);
}
