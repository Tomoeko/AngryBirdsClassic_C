#include "ab/block_removal_effects.h"

#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool named(AbDataValue value, const char *name) {
    size_t length = strlen(name);
    return value.kind == AB_DATA_STRING && value.string.size == length &&
           !memcmp(value.string.data, name, length);
}

static bool field(AbDataValue table, const char *name, AbDataValue *output,
                  AbError *error) {
    if (table.kind != AB_DATA_TABLE || !table.table ||
        table.table->count > table.table->capacity ||
        (table.table->count && !table.table->entries)) {
        return ab_fail(error, "Removal effects index a non-table value");
    }
    const AbDataValue *found = ab_data_get(
        table.table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    *output = found ? *found : (AbDataValue){0};
    return true;
}

static bool property(AbDataValue object, const char *name,
                     AbBlockRemovalEffectsHost *host, AbDataValue *output,
                     AbError *error) {
    if (!host->property) {
        return ab_fail(error, "Missing removal script property host");
    }
    return host->property(host->context, object, span(name), output, error);
}

static bool current_dead(AbSpan key, AbBlockRemovalEffectsHost *host,
                         AbDataValue *output, AbError *error) {
    if (!host->state || !host->state->dead_blocks) {
        return ab_fail(error, "Missing current deadBlocks during removal effects");
    }
    uintptr_t handle = 0;
    bool present = false;
    if (!ab_string_map_get(host->state->dead_blocks, key, &handle, &present, error)) {
        return false;
    }
    *output = present && handle ? *(const AbDataValue *)handle : (AbDataValue){0};
    return true;
}

static bool explode(AbSpan key, AbDataValue definition, AbBlockRemovalEffectsHost *host,
                    AbError *error) {
    AbDataValue specialty = {0};
    if (!field(definition, "specialty", &specialty, error)) {
        return false;
    }
    AbDataValue object = {0};
    if (!named(specialty, "BOMB")) {
        if (!current_dead(key, host, &object, error) ||
            !field(object, "specialty", &specialty, error)) {
            return false;
        }
        if (!named(specialty, "BOMB")) {
            return true;
        }
    }
    bool (*explosion)(void *, AbDataValue, AbDataValue, AbDataValue, AbError *) =
        host->explosion;
    AbDataValue sound = {0};
    if (!current_dead(key, host, &object, error) ||
        !field(object, "explosionSound", &sound, error)) {
        return false;
    }
    if (!truthy(sound)) {
        if (!host->audio_name) {
            return ab_fail(error, "Missing removal explosion audio-name host");
        }
        if (!host->audio_name(
                host->context,
                (AbDataValue){.kind = AB_DATA_STRING, .string = span("tnt_explodes")},
                &sound, error)) {
            return false;
        }
    }
    if (!explosion) {
        return ab_fail(error, "Missing removal explosion host");
    }
    return explosion(host->context, object, definition, sound, error);
}

static bool audio(AbDataValue object, AbDataValue sound,
                  AbBlockRemovalEffectsHost *host, AbError *error) {
    AbDataValue silent = {0};
    if (!field(object, "killSilently", &silent, error)) {
        return false;
    }
    if (truthy(silent) || sound.kind == AB_DATA_NIL) {
        return true;
    }
    bool (*play)(void *, AbAudioPlay, AbError *) = host->audio;
    if (!host->audio_name) {
        return ab_fail(error, "Missing removal destroyed-sound name host");
    }
    AbDataValue resolved = {0};
    if (!host->audio_name(host->context, sound, &resolved, error)) {
        return false;
    }
    if (!play) {
        return ab_fail(error, "Missing removal destroyed-sound audio host");
    }
    return play(host->context,
                (AbAudioPlay){
                    .name = resolved,
                    .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f333333)},
                    .loop = {.kind = AB_DATA_BOOLEAN},
                    .channel = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40400000)}},
                error);
}

bool ab_block_removal_effects(const AbBlockRemovalEntry *entry,
                              AbDataValue cached_definition,
                              AbBlockRemovalEffectsHost *host,
                              AbBlockRemovalEffects *output, AbError *error) {
    if (!entry || !host || !output || (entry->key.size && !entry->key.data)) {
        return ab_fail(error, "Invalid removal effects input");
    }
    const AbBlockRemovalEntry captured = *entry;
    AbDataValue unused_name = {0};
    if (!field(captured.object, "name", &unused_name, error)) {
        return false;
    }
    AbBlockRemovalEffects result = {0};
    if (!property(captured.object, "particles", host, &result.particles, error)) {
        return false;
    }
    if (!host->destroyed_sound) {
        return ab_fail(error, "Missing level destroyed-sound host");
    }
    AbDataValue sound = {0};
    if (!host->destroyed_sound(host->context, captured.object, cached_definition,
                               &sound, error) ||
        !property(captured.object, "particleAmount", host, &result.amount, error)) {
        return false;
    }
    if (!truthy(result.amount)) {
        result.amount =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x41400000)};
    }
    if (!host->random) {
        return ab_fail(error, "Missing removal random host");
    }
    AbDataValue random = {0};
    if (!host->random(host->context, 12, 20, &random, error)) {
        return false;
    }
    if (named(result.particles, "smokeBuff") || named(result.particles, "flameBuff")) {
        result.amount =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)};
    } else if (named(result.particles, "batBuff")) {
        result.amount = random;
    }
    if (!explode(captured.key, cached_definition, host, error) ||
        !audio(captured.object, sound, host, error)) {
        return false;
    }
    if (!host->update_destroyed) {
        return ab_fail(error, "Missing destroyed-block values host");
    }
    if (!host->update_destroyed(host->context, captured.object, error)) {
        return false;
    }
    *output = result;
    return true;
}
