#include "ab/classic_characters.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbClassicCharacters *owner;
    AbObjectDefinition definition;
    size_t index;
} CharacterEntry;

struct AbClassicCharacters {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbRandom48 *random;
    AbClassicCharacterHost host;
    AbCharacterAnimations animations;
    AbCharacterAnimationBindings *bindings;
    AbClassicCharacterSprites *sprites;
    CharacterEntry *entries;
    size_t bytes;
};

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool finite_number(AbDataValue value, float *output) {
    if (value.kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output);
}

static bool random_integer(void *context, int minimum, int maximum, int *output) {
    CharacterEntry *entry = context;
    float value;
    if (ab_script_random(entry->owner->random, 2, minimum, maximum, &value) !=
        AB_SCRIPT_RANDOM_OK) {
        return false;
    }
    *output = (int)value;
    return true;
}

static bool lose_condition(void *context, bool *output, AbError *error) {
    AbClassicCharacterHost host = ((CharacterEntry *)context)->owner->host;
    return host.lose_condition
               ? host.lose_condition(host.context, output, error)
               : ab_fail(error, "Missing classic character lose-condition host");
}

static bool distance(CharacterEntry *entry, AbClassicCharacterKind kind, float *output,
                     AbError *error) {
    AbClassicCharacterHost host = entry->owner->host;
    return host.camera_distance
               ? host.camera_distance(host.context, entry->index, kind, output, error)
               : ab_fail(error, "Missing classic character camera-distance host");
}

static bool goal_distance(void *context, float *output, AbError *error) {
    return distance(context, AB_CLASSIC_CHARACTER_GOAL, output, error);
}

static bool bird_distance(void *context, float *output, AbError *error) {
    return distance(context, AB_CLASSIC_CHARACTER_BIRD, output, error);
}

static bool sound(CharacterEntry *entry, AbClassicCharacterKind kind, float volume,
                  AbError *error) {
    AbClassicCharacterHost host = entry->owner->host;
    return host.play_sound
               ? host.play_sound(host.context, entry->index, kind, volume, error)
               : ab_fail(error, "Missing classic character sound host");
}

static bool goal_sound(void *context, float volume, AbError *error) {
    return sound(context, AB_CLASSIC_CHARACTER_GOAL, volume, error);
}

static bool bird_sound(void *context, float volume, AbError *error) {
    return sound(context, AB_CLASSIC_CHARACTER_BIRD, volume, error);
}

static bool sprite(CharacterEntry *entry, AbDataValue name, AbError *error) {
    if (name.kind != AB_DATA_STRING) {
        return ab_fail(error, "Unsupported classic character sprite value");
    }
    return ab_classic_visuals_set_sprite(entry->owner->visuals, entry->index,
                                         name.string, error);
}

static const AbDamageSpriteNames *damage_names(CharacterEntry *entry) {
    AbClassicLevelView level = ab_classic_level_view(entry->owner->level);
    const AbDataTable *physical = &level.properties->objects[entry->index].physical;
    AbDamageSpriteNames *names = &entry->owner->sprites[entry->index].damage;
    /* updateCharacterAnimations (137493) reads each face from the live object.
     * Collision callbacks have already replaced its damage/blink/smile names;
     * returning to a face must not restore the startup health state. */
    names->sprite = field(physical, "damageSprite");
    names->blink = field(physical, "blinkSprite");
    names->smile = field(physical, "smileSprite");
    return names;
}

static bool goal_sprite(void *context, AbGoalFace face, AbError *error) {
    CharacterEntry *entry = context;
    switch (face) {
        case AB_GOAL_FACE_SPECIAL:
            return sprite(entry, entry->owner->sprites[entry->index].special_expression,
                          error);
        case AB_GOAL_FACE_SMILE:
            return sprite(entry, damage_names(entry)->smile, error);
        case AB_GOAL_FACE_DAMAGE:
            return sprite(entry, damage_names(entry)->sprite, error);
        case AB_GOAL_FACE_BLINK:
            return sprite(entry, damage_names(entry)->blink, error);
    }
    return ab_fail(error, "Invalid classic goal face");
}

static bool bird_sprite(void *context, AbBirdFace face, AbError *error) {
    CharacterEntry *entry = context;
    switch (face) {
        case AB_BIRD_FACE_BASE:
            return sprite(entry, damage_names(entry)->sprite, error);
        case AB_BIRD_FACE_YELL:
            return sprite(entry, field(entry->definition.source, "spriteYell"), error);
        case AB_BIRD_FACE_BLINK:
            return sprite(entry, field(entry->definition.source, "spriteBlink"), error);
    }
    return ab_fail(error, "Invalid classic bird face");
}

static bool apply_jump(void *context, size_t object, const AbBirdJumpFrame *frame,
                       AbError *error) {
    AbClassicCharacters *characters = context;
    return ab_classic_level_apply_jump(characters->level, object, frame, error);
}

static bool update_hat(void *context, size_t object, AbError *error) {
    AbClassicCharacterHost host = ((AbClassicCharacters *)context)->host;
    return host.update_hat ? host.update_hat(host.context, object, error)
                           : ab_fail(error, "Missing classic character hat host");
}

static bool bind_entries(AbClassicCharacters *characters, AbClassicLevelView level,
                         AbError *error) {
    for (size_t index = 0; index < characters->animations.count; ++index) {
        CharacterEntry *entry = characters->entries + index;
        entry->owner = characters;
        entry->index = index;
        characters->sprites[index].damage = level.initial_sprites[index];
        const AbLevelObject *object = level.bodies->objects + index;
        if (!object->level_goal && !object->physics.controllable) {
            continue;
        }
        if (!ab_object_definition_open(level.catalog, object->definition_name, NULL,
                                       &entry->definition, error)) {
            return false;
        }
        AbCharacterAnimationBindings *binding = characters->bindings + index;
        if (object->level_goal) {
            AbDataValue ranges = field(entry->definition.source, "damageSprites");
            AbDataValue first =
                field(ranges.kind == AB_DATA_TABLE ? ranges.table : NULL, "damage1");
            *binding = (AbCharacterAnimationBindings){
                .goal = {.context = entry,
                         .random_integer = random_integer,
                         .lose_condition = lose_condition,
                         .set_sprite = goal_sprite,
                         .camera_distance = goal_distance,
                         .play_oink = goal_sound}};
            if (!finite_number(field(entry->definition.source, "strength"),
                               &binding->goal.base_strength) ||
                !finite_number(
                    field(first.kind == AB_DATA_TABLE ? first.table : NULL, "min"),
                    &binding->goal.damage1_min)) {
                return ab_fail(error, "Unsupported classic goal expression definition");
            }
        }
        if (object->physics.controllable) {
            binding->bird = (AbBirdAnimationControls){
                .context = entry,
                .random_integer = random_integer,
                .camera_distance = bird_distance,
                .play_yell = bird_sound,
                .set_sprite = bird_sprite,
                .yell_sprite_present =
                    field(entry->definition.source, "spriteYell").kind != AB_DATA_NIL,
                .blink_sprite_present =
                    field(entry->definition.source, "spriteBlink").kind != AB_DATA_NIL};
            binding->jump = (AbBirdJumpRandom){entry, random_integer};
        }
    }
    return true;
}

bool ab_classic_characters_open(AbClassicLevel *level, AbClassicVisuals *visuals,
                                AbRandom48 *random, AbClassicCharacterHost host,
                                size_t storage_bytes, AbClassicCharacters **output,
                                AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic character output");
    }
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    AbClassicVisualView visual = ab_classic_visuals_view(visuals);
    size_t count = view.bodies ? view.bodies->count : 0;
    size_t row_bytes = sizeof(AbCharacterAnimationBindings) +
                       sizeof(AbClassicCharacterSprites) + sizeof(CharacterEntry);
    if (!view.bodies || !view.frame || !view.catalog || !view.timers_initialized ||
        !view.sprites_initialized || view.frame->count != count || !random ||
        visual.level != level || visual.count != count ||
        (count && (!view.character_objects || !view.initial_sprites)) ||
        storage_bytes < sizeof(AbClassicCharacters) ||
        count > (storage_bytes - sizeof(AbClassicCharacters)) / row_bytes) {
        return ab_fail(error, "Invalid classic character resources or storage limit");
    }
    AbClassicCharacters *characters = calloc(1, sizeof(*characters));
    if (!characters) {
        return ab_fail(error, "Cannot allocate classic character owner");
    }
    characters->level = level;
    characters->visuals = visuals;
    characters->random = random;
    characters->host = host;
    characters->bytes = sizeof(*characters) + count * row_bytes;
    characters->animations =
        (AbCharacterAnimations){.objects = view.character_objects,
                                .count = count,
                                .goal_order = view.goal_order,
                                .goal_count = view.goal_count,
                                .bird_order = view.bird_order,
                                .bird_count = view.bird_count,
                                .current_bird = SIZE_MAX,
                                .removed_objects = view.removed_objects,
                                .context = characters,
                                .apply_jump = apply_jump,
                                .update_hat = update_hat};
    if (count) {
        characters->bindings = calloc(count, sizeof(*characters->bindings));
        characters->sprites = calloc(count, sizeof(*characters->sprites));
        characters->entries = calloc(count, sizeof(*characters->entries));
        if (!characters->bindings || !characters->sprites || !characters->entries) {
            ab_fail(error, "Cannot allocate classic character rows");
            goto release_characters;
        }
    }
    characters->animations.bindings = characters->bindings;
    if (!bind_entries(characters, view, error)) {
        goto release_characters;
    }
    *output = characters;
    return true;

release_characters:
    ab_classic_characters_close(characters);
    return false;
}

void ab_classic_characters_close(AbClassicCharacters *characters) {
    if (characters) {
        free(characters->entries);
        free(characters->sprites);
        free(characters->bindings);
        free(characters);
    }
}

size_t ab_classic_characters_bytes(const AbClassicCharacters *characters) {
    return characters ? characters->bytes : 0;
}

AbClassicCharacterView ab_classic_characters_view(AbClassicCharacters *characters) {
    return characters
               ? (AbClassicCharacterView){&characters->animations, characters->bindings,
                                          characters->sprites,
                                          characters->animations.count}
               : (AbClassicCharacterView){0};
}

bool ab_classic_characters_update(AbClassicCharacters *characters, float seconds,
                                  AbError *error) {
    return characters
               ? ab_character_animations_update(&characters->animations, seconds, error)
               : ab_fail(error, "Missing classic character owner");
}
