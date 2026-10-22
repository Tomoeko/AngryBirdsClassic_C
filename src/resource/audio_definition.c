#include "ab/audio_definition.h"

#include <string.h>

static AbDataValue property(const AbDataTable *properties, const char *name) {
    const AbDataValue *value = ab_data_get(
        properties, (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool valid_string(AbDataValue value) {
    return value.kind == AB_DATA_STRING && (!value.string.size || value.string.data);
}

bool ab_audio_definition_read(const AbDataTable *properties, AbDataValue existing,
                              AbSpan prefix, bool use_mp3, uint8_t *path_storage,
                              size_t capacity, AbAudioDefinition *output, bool *created,
                              AbError *error) {
    if (!output || !created) {
        return ab_fail(error, "Missing audio definition output");
    }
    if (existing.kind != AB_DATA_NIL) {
        *created = false;
        return true;
    }
    if (!properties || (properties->count && !properties->entries) ||
        (prefix.size && !prefix.data)) {
        return ab_fail(error, "Missing audio definition properties or path prefix");
    }
    AbDataValue file = property(properties, "fileName");
    AbDataValue name = property(properties, "clipName");
    AbDataValue extension = property(properties, "extension");
    if (!valid_string(file) || !valid_string(name) ||
        (truthy(extension) && !valid_string(extension))) {
        return ab_fail(error, "Audio paths/names require constant string properties");
    }
    AbSpan suffix =
        truthy(extension) ? extension.string : (AbSpan){(const uint8_t *)"wav", 3};
    if (use_mp3 && extension.kind == AB_DATA_NIL) {
        suffix = (AbSpan){(const uint8_t *)"mp3", 3};
    }
    if (!path_storage || prefix.size > capacity ||
        file.string.size > capacity - prefix.size) {
        return ab_fail(error, "Audio resource path exceeds output capacity");
    }
    size_t size = prefix.size + file.string.size;
    if (size == capacity || suffix.size > capacity - size - 1) {
        return ab_fail(error, "Audio resource path exceeds output capacity");
    }
    size += 1 + suffix.size;
    AbAudioDefinition definition = {
        .path = {path_storage, size},
        .stream = property(properties, "stream"),
        .name = name,
        .channel = property(properties, "channel"),
        .volume = property(properties, "volume"),
        .can_be_overwritten = property(properties, "canBeOverWritten"),
        .loop = property(properties, "loop"),
    };
    if (!truthy(definition.stream)) {
        definition.stream = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
    if (prefix.size) {
        memcpy(path_storage, prefix.data, prefix.size);
    }
    if (file.string.size) {
        memcpy(path_storage + prefix.size, file.string.data, file.string.size);
    }
    path_storage[prefix.size + file.string.size] = '.';
    if (suffix.size) {
        memcpy(path_storage + size - suffix.size, suffix.data, suffix.size);
    }
    *output = definition;
    *created = true;
    return true;
}
