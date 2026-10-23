#include "ab/classic_eggs.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbBirdCollisionObject value;
    float timer;
} EggRecord;

struct AbClassicEggs {
    AbClassicEggConfig config;
    EggRecord *records;
    AbBirdCollisionObject **items;
    uint8_t *names;
    size_t used, capacity, name_used, name_bytes, bytes;
    bool calling, failed;
};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(AbSpan value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = value};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(span(name)));
    return value ? *value : (AbDataValue){0};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    memcpy(output, &value.bits, sizeof(*output));
    return (value.kind == AB_DATA_NUMBER && isfinite(*output)) ||
           ab_fail(error, "Egg drop requires a finite numeric source property");
}

static bool string(AbDataValue value, AbSpan *output, AbError *error) {
    if (value.kind != AB_DATA_STRING || !value.string.data || !value.string.size)
        return ab_fail(error, "Egg drop requires its actual source identifier");
    *output = value.string;
    return true;
}

static bool write(AbDataTable *table, const char *name, AbDataValue value,
                  AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, span(name), value, error);
}

static bool begin(AbClassicEggs *eggs, AbError *error) {
    AbClassicLevelView level =
        ab_classic_level_view(eggs ? eggs->config.playfield.level : NULL);
    if (!eggs || eggs->calling || eggs->failed || !level.world || level.world->locked ||
        level.world->failed)
        return ab_fail(error, "Invalid, failed or reentrant white bird egg owner");
    eggs->calling = true;
    return true;
}

static bool finish(AbClassicEggs *eggs, bool okay) {
    eggs->calling = false;
    eggs->failed = !okay;
    return okay;
}

bool ab_classic_eggs_open(AbClassicEggConfig config, AbClassicEggs **output,
                          AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned white bird egg output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.playfield.level);
    if (!level.bodies || !level.properties || !config.birds || !config.physical ||
        !config.particles || !config.explosions || !config.audio || !config.settings ||
        !config.sprites || !config.sprites->sprite || !config.created ||
        !config.grenades || !config.grenades->present || config.grenades->count ||
        config.grenades->items || config.grenades->capacity ||
        !isfinite(config.default_force) || config.storage_bytes < sizeof(AbClassicEggs))
        return ab_fail(error, "Missing actual white bird egg owners/storage");
    size_t name_bytes = 0;
    for (size_t index = 0; index < level.bird_count; ++index) {
        size_t object = level.bird_order[index];
        if (object >= level.bodies->count ||
            level.bodies->objects[object].name.size >= SIZE_MAX - name_bytes)
            return ab_fail(error, "Invalid white bird egg name budget");
        name_bytes += level.bodies->objects[object].name.size + 1;
    }
    size_t row = sizeof(EggRecord) + sizeof(AbBirdCollisionObject *);
    size_t remaining = config.storage_bytes - sizeof(AbClassicEggs);
    if (name_bytes > remaining || level.bird_count > (remaining - name_bytes) / row)
        return ab_fail(error, "White bird egg storage budget exhausted");
    AbClassicEggs *eggs = calloc(1, sizeof(*eggs));
    if (!eggs)
        return ab_fail(error, "Cannot allocate white bird egg owner");
    eggs->config = config;
    eggs->capacity = level.bird_count;
    eggs->name_bytes = name_bytes;
    eggs->bytes = sizeof(*eggs) + name_bytes + level.bird_count * row;
    if (eggs->capacity) {
        eggs->records = calloc(eggs->capacity, sizeof(*eggs->records));
        eggs->items = calloc(eggs->capacity, sizeof(*eggs->items));
        eggs->names = malloc(name_bytes);
        if (!eggs->records || !eggs->items || !eggs->names) {
            ab_classic_eggs_close(eggs);
            return ab_fail(error, "Cannot allocate stable white bird egg records");
        }
    }
    config.grenades->items = eggs->items;
    config.grenades->capacity = eggs->capacity;
    *output = eggs;
    return true;
}

void ab_classic_eggs_close(AbClassicEggs *eggs) {
    if (!eggs)
        return;
    if (eggs->config.grenades->items == eggs->items)
        *eggs->config.grenades = (AbBirdCollisionGrenades){.present = true};
    free(eggs->names);
    free(eggs->items);
    free(eggs->records);
    free(eggs);
}

size_t ab_classic_eggs_bytes(const AbClassicEggs *eggs) {
    return eggs ? eggs->bytes : 0;
}

static bool create(AbClassicEggs *eggs, size_t parent_index,
                   const AbDataTable *definition, AbError *error) {
    AbClassicEggConfig *config = &eggs->config;
    AbClassicPlayfieldView *play = &config->playfield;
    AbClassicLevelView level = ab_classic_level_view(play->level);
    if (parent_index >= level.bodies->count || level.removed_objects[parent_index] ||
        eggs->used == eggs->capacity)
        return ab_fail(error, "Invalid or exhausted white bird egg drop");
    AbDataTable *parent = &level.properties->objects[parent_index].physical;
    AbSpan prefix = {0}, special = {0}, projectile = {0};
    if (!ab_settings_statistics_extra_tutorial(config->settings,
                                               field(definition, "sprite"), error) ||
        !string(field(parent, "name"), &prefix, error) ||
        !string(field(definition, "spriteSpecial"), &special, error) ||
        !string(field(definition, "projectile"), &projectile, error))
        return false;
    if (prefix.size >= eggs->name_bytes - eggs->name_used)
        return ab_fail(error, "White bird egg name storage exhausted");
    uint8_t *bytes = eggs->names + eggs->name_used;
    memcpy(bytes, prefix.data, prefix.size);
    bytes[prefix.size] = 'a';
    AbSpan name = {bytes, prefix.size + 1};
    eggs->name_used += name.size;
    if (!write(parent, "sprite", text(special), error) ||
        !ab_classic_visuals_set_sprite(play->visuals, parent_index, special, error))
        return false;
    const AbDataValue *blocks = ab_data_get(level.catalog, text(span("blocks")));
    const AbDataValue *raw = blocks && blocks->kind == AB_DATA_TABLE
                                 ? ab_data_get(blocks->table, text(projectile))
                                 : NULL;
    if (!raw || raw->kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual egg projectile definition");
    const AbDataTable *egg_definition = raw->table;
    AbSpan sprite = {0};
    AbGameCircleInput circle = {.name = name, .controllable = true};
    float layer;
    if (!string(field(egg_definition, "sprite"), &sprite, error) ||
        !numeric(field(parent, "x"), &circle.position.x, error) ||
        !numeric(field(parent, "y"), &circle.position.y, error) ||
        !numeric(field(parent, "radius"), &circle.radius, error) ||
        !numeric(field(parent, "density"), &circle.density, error) ||
        !numeric(field(parent, "friction"), &circle.friction, error) ||
        !numeric(field(parent, "restitution"), &circle.restitution, error) ||
        !numeric(field(parent, "z_order"), &layer, error))
        return false;
    circle.position.y += circle.radius * 2;
    circle.z_order = layer;
    size_t object;
    if (!ab_classic_level_create_circle(play->level, circle, &object, error) ||
        !ab_classic_render_scene_create_object(play->scene, play->visuals, object,
                                               sprite, layer, error))
        return false;
    AbDataTable *physical = &level.properties->objects[object].physical;
    const char *const inherited[] = {"strength", "defence", "materialName",
                                     "useLegacyCollisionPath"};
    if (!write(physical, "definition", text(projectile), error) ||
        !write(physical, "controllable",
               (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error))
        return false;
    for (size_t index = 0; index < sizeof(inherited) / sizeof(*inherited); ++index)
        if (!write(physical, inherited[index], field(parent, inherited[index]), error))
            return false;
    AbSpriteAtlasBinding binding = {0};
    if (!config->sprites->sprite(config->sprites->context, sprite, &binding) ||
        !binding.sprite.region)
        return ab_fail(error, "Missing actual dropped-egg pivot resource");
    if (!write(physical, "damageFactors", field(egg_definition, "damageFactors"),
               error) ||
        !write(physical, "levelGoal", (AbDataValue){.kind = AB_DATA_BOOLEAN}, error) ||
        !write(physical, "spritePivotX", number(binding.sprite.region->pivot_x),
               error) ||
        !write(physical, "spritePivotY", number(binding.sprite.region->pivot_y),
               error) ||
        !write(physical, "damageSprite", text(sprite), error) ||
        !write(physical, "xVel", number(0), error) ||
        !write(physical, "yVel", number(100), error) ||
        !write(physical, "isEggGrenade",
               (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error) ||
        !ab_classic_visuals_set_sprite(play->visuals, object, sprite, error))
        return false;
    float angle;
    if (!numeric(field(parent, "angle"), &angle, error) ||
        !ab_classic_level_set_rotation(play->level, object, angle, error) ||
        !ab_body_velocity_set(
            &level.world->storage.bodies[level.bodies->objects[object].physics.body]
                 .motion,
            (AbVec2){0, 100}))
        return ab_fail(error, "Invalid dropped-egg body transform/velocity");
    EggRecord *record = eggs->records + eggs->used++;
    record->value.object.name = name;
    record->timer = 5;
    config->grenades->items[config->grenades->count++] = &record->value;
    AbDataValue sound;
    uint32_t handle;
    if (!ab_audio_bank_name_value(config->audio, field(definition, "specialSound"),
                                  &sound, error) ||
        !ab_audio_bank_play(config->audio,
                            (AbAudioPlay){.name = sound,
                                          .volume = number(1),
                                          .loop = {.kind = AB_DATA_BOOLEAN}},
                            &handle, error))
        return false;
    AbLevelObject *child = level.bodies->objects + object;
    child->physics.collider = number(3);
    level.frame->objects[object].collider = 3;
    if (!ab_classic_behaviors_create_object(play->behaviors, object,
                                            field(egg_definition, "behaviors"),
                                            level.catalog_data, error) ||
        !ab_classic_shots_register_circle(play->shots, object, error) ||
        !ab_classic_camera_register_object(play->camera, object, error) ||
        !ab_classic_object_removal_register_circle(config->physical, object, error) ||
        !ab_classic_birds_register_object(config->birds, object, error) ||
        !config->created(config->context, object, error))
        return false;
    float mass, x, y;
    if (!numeric(field(parent, "mass"), &mass, error) ||
        !numeric(field(parent, "x"), &x, error) ||
        !numeric(field(parent, "y"), &y, error))
        return false;
    AbVec2 impulse = {(-0.04f * config->default_force) * mass,
                      (0.08f * config->default_force) * mass};
    AbVec2 point = {x - 0.5f, y};
    if (!ab_body_impulse_apply(
            &level.world->storage
                 .bodies[level.bodies->objects[parent_index].physics.body]
                 .motion,
            impulse, &point))
        return ab_fail(error, "Invalid white bird rebound impulse");
    return ab_classic_trajectories_puff(
        play->trajectories, (AbVec2){x * 20, y * 20},
        (AbDataValue){.kind = AB_DATA_TABLE, .table = parent}, (AbDataValue){0},
        (AbDataValue){0}, error);
}

bool ab_classic_eggs_trigger(AbClassicEggs *eggs, size_t parent,
                             const AbDataTable *definition, AbError *error) {
    return begin(eggs, error) && finish(eggs, create(eggs, parent, definition, error));
}

static bool update(AbClassicEggs *eggs, AbError *error) {
    AbClassicEggConfig *config = &eggs->config;
    AbClassicLevelView level = ab_classic_level_view(config->playfield.level);
    AbBirdCollisionGrenades *list = config->grenades;
    for (size_t remaining = list->count; remaining; --remaining) {
        AbBirdCollisionObject *record = list->items[remaining - 1];
        if (record->explode.kind != AB_DATA_BOOLEAN || !record->explode.bits)
            continue;
        AbSpan name = record->object.name;
        size_t object = 0;
        while (object < level.bodies->count &&
               (level.bodies->objects[object].name.size != name.size ||
                memcmp(level.bodies->objects[object].name.data, name.data, name.size)))
            ++object;
        AbDataValue definition, sound;
        if (object == level.bodies->count ||
            !ab_object_definition_lookup(&level.properties->lookup, name, &definition,
                                         error) ||
            definition.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing exploding egg world/definition identity");
        AbDataValue particles = field(definition.table, "particles");
        if (!ab_classic_particles_legacy(config->particles, name, particles, number(6),
                                         true, false, error) ||
            !ab_audio_bank_name_value(config->audio, text(span("special_explosion")),
                                      &sound, error) ||
            !ab_classic_explosions_make(config->explosions, object, definition, sound,
                                        (AbDataValue){0}, error) ||
            !ab_classic_particles_legacy(config->particles, name, particles, number(5),
                                         true, false, error) ||
            !ab_classic_object_removal_call(config->physical, name, error))
            return false;
        AbObjectPropertyAccess world = {
            .physics_object = {.kind = AB_DATA_TABLE,
                               .table = &level.properties->world}};
        if (!ab_object_property_set(&world, name, (AbDataValue){0}, error))
            return false;
        --list->count;
        memmove(list->items + remaining - 1, list->items + remaining,
                (list->count - (remaining - 1)) * sizeof(*list->items));
    }
    return true;
}

bool ab_classic_eggs_update(AbClassicEggs *eggs, AbError *error) {
    return begin(eggs, error) && finish(eggs, update(eggs, error));
}
