#include "ab/level_effect.h"

#include <string.h>

float ab_level_effect_base_bird_volume(void) {
    return 1.0f;
}

bool ab_level_effect_base_sound(AbLevelEffectSound sound, AbDataValue definition,
                                AbDataValue *output, AbError *error) {
    const char *const names[] = {"collisionSound", "damageSound", "destroyedSound"};
    if (!output || (unsigned)sound >= sizeof(names) / sizeof(*names) ||
        definition.kind != AB_DATA_TABLE || !definition.table ||
        definition.table->count > definition.table->capacity ||
        (definition.table->count && !definition.table->entries)) {
        return ab_fail(error, "Invalid base level-effect sound definition");
    }
    const char *name = names[sound];
    AbDataValue key = {.kind = AB_DATA_STRING,
                       .string = {(const uint8_t *)name, strlen(name)}};
    const AbDataValue *found = ab_data_get(definition.table, key);
    *output = found ? *found : (AbDataValue){0};
    return true;
}
