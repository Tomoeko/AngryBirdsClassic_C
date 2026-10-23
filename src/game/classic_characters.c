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
    size_t capacity, bytes;
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
                         size_t begin, AbError *error) {
    for (size_t index = begin; index < characters->animations.count; ++index) {
        CharacterEntry *entry = characters->entries + index;
        entry->owner = characters;
        entry->index = index;
        characters->sprites[index].damage = level.initial_sprites[index];
        const AbLevelObject *object = level.bodies->objects + index;
        if (!object->level_goal && !object->physics.controllable) {
            continue;
        }
        AbSpan definition_name = object->definition_name;
        if (!definition_name.size) {
            AbDataValue definition =
                field(&level.properties->objects[index].physical, "definition");
            if (definition.kind != AB_DATA_STRING)
                return ab_fail(error, "Dynamic character has no definition name");
            definition_name = definition.string;
        }
        if (!ab_object_definition_open(level.catalog, definition_name, NULL,
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

bool ab_classic_characters_open_capacity(AbClassicLevel *level,
                                         AbClassicVisuals *visuals, AbRandom48 *random,
                                         AbClassicCharacterHost host,
                                         size_t object_capacity, size_t storage_bytes,
                                         AbClassicCharacters **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic character output");
    }
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    AbClassicVisualView visual = ab_classic_visuals_view(visuals);
    size_t count = view.bodies ? view.bodies->count : 0;
    size_t capacity = object_capacity ? object_capacity : count;
    size_t row_bytes = sizeof(AbCharacterAnimationBindings) +
                       sizeof(AbClassicCharacterSprites) + sizeof(CharacterEntry);
    if (!view.bodies || !view.frame || !view.catalog || !view.timers_initialized ||
        !view.sprites_initialized || view.frame->count != count || !random ||
        visual.level != level || visual.count != count ||
        (count && (!view.character_objects || !view.initial_sprites)) ||
        storage_bytes < sizeof(AbClassicCharacters) || capacity < count ||
        capacity > view.bodies->capacity ||
        capacity > (storage_bytes - sizeof(AbClassicCharacters)) / row_bytes) {
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
    characters->capacity = capacity;
    characters->bytes = sizeof(*characters) + capacity * row_bytes;
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
    if (capacity) {
        characters->bindings = calloc(capacity, sizeof(*characters->bindings));
        characters->sprites = calloc(capacity, sizeof(*characters->sprites));
        characters->entries = calloc(capacity, sizeof(*characters->entries));
        if (!characters->bindings || !characters->sprites || !characters->entries) {
            ab_fail(error, "Cannot allocate classic character rows");
            goto release_characters;
        }
    }
    characters->animations.bindings = characters->bindings;
    if (!bind_entries(characters, view, 0, error)) {
        goto release_characters;
    }
    *output = characters;
    return true;

release_characters:
    ab_classic_characters_close(characters);
    return false;
}

bool ab_classic_characters_open(AbClassicLevel *level, AbClassicVisuals *visuals,
                                AbRandom48 *random, AbClassicCharacterHost host,
                                size_t storage_bytes, AbClassicCharacters **output,
                                AbError *error) {
    return ab_classic_characters_open_capacity(level, visuals, random, host, 0,
                                               storage_bytes, output, error);
}

bool ab_classic_characters_register_objects(AbClassicCharacters *characters,
                                            AbError *error) {
    if (!characters)
        return ab_fail(error, "Missing new character adapter owner");
    AbClassicLevelView level = ab_classic_level_view(characters->level);
    size_t first = characters->animations.count;
    if (level.world->failed || level.world->locked || level.bodies->count < first ||
        level.bodies->count > characters->capacity ||
        ab_classic_visuals_view(characters->visuals).count != level.bodies->count)
        return ab_fail(error, "Invalid new character adapter prefix");
    characters->animations.count = level.bodies->count;
    if (!bind_entries(characters, level, first, error))
        return false;
    for (size_t index = first; index < characters->animations.count; ++index) {
        const AbDataTable *physical = &level.properties->objects[index].physical;
        characters->sprites[index].damage =
            (AbDamageSpriteNames){.sprite = field(physical, "damageSprite"),
                                  .blink = field(physical, "blinkSprite"),
                                  .smile = field(physical, "smileSprite"),
                                  .frozen = field(physical, "frozenSprite")};
    }
    characters->animations.bird_count = level.bird_count;
    characters->animations.goal_count = level.goal_count;
    return true;
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

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

bool ab_classic_characters_special_expression(AbClassicCharacters *characters,
                                              size_t object, AbSpan key, float seconds,
                                              AbError *error) {
    if (!characters || object >= characters->animations.count || !isfinite(seconds))
        return ab_fail(error, "Invalid pig special-expression binding");
    AbClassicLevelView level = ab_classic_level_view(characters->level);
    if (level.removed_objects[object])
        return true;
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue sprites = field(physical, "extraSprites");
    if (!truthy(sprites))
        return true;
    if (sprites.kind != AB_DATA_TABLE)
        return ab_fail(error, "Pig extraSprites is not its source table");
    const AbDataValue *sprite = ab_data_get(
        sprites.table, (AbDataValue){.kind = AB_DATA_STRING, .string = key});
    if (!sprite || !truthy(*sprite) ||
        truthy(field(physical, "disableSpecialExpressions")))
        return true;
    if (!ab_level_property_set(level.properties, object,
                               (AbSpan){(const uint8_t *)"specialExpression", 17},
                               *sprite, error) ||
        !ab_level_property_set(level.properties, object,
                               (AbSpan){(const uint8_t *)"specialExpressionTimer", 22},
                               number(seconds), error))
        return false;
    characters->sprites[object].special_expression = *sprite;
    level.character_objects[object].goal.special_expression = true;
    level.character_objects[object].goal.special_timer = seconds;
    return true;
}

bool ab_classic_characters_update(AbClassicCharacters *characters, float seconds,
                                  AbError *error) {
    if (!characters)
        return ab_fail(error, "Missing classic character owner");
    if (!ab_character_animations_update(&characters->animations, seconds, error))
        return false;
    AbClassicLevelView level = ab_classic_level_view(characters->level);
    for (size_t rank = 0; rank < level.goal_count; ++rank) {
        size_t object = level.goal_order[rank];
        AbDataTable *physical = &level.properties->objects[object].physical;
        if (level.removed_objects[object] ||
            field(physical, "specialExpressionTimer").kind == AB_DATA_NIL)
            continue;
        AbGoalAnimation *goal = &level.character_objects[object].goal;
        if (!ab_level_property_set(
                level.properties, object,
                (AbSpan){(const uint8_t *)"specialExpressionTimer", 22},
                number(goal->special_timer), error) ||
            !ab_level_property_set(level.properties, object,
                                   (AbSpan){(const uint8_t *)"specialExpression", 17},
                                   goal->special_expression
                                       ? characters->sprites[object].special_expression
                                       : (AbDataValue){0},
                                   error))
            return false;
    }
    return true;
}
