#ifndef AB_GAME_PUFFERS_H
#define AB_GAME_PUFFERS_H

#include "game_puffer_common.h"

static bool game_puffer_trigger(PreviewPufferObject *row, AbError *error);

static bool game_puffer_delay(void *context, void *receiver, float seconds,
                              AbError *error) {
    PreviewPufferObject *row = context;
    PreviewGame *game = row->owner->game;
    float timer;
    if (receiver != game_puffer_physical(row) ||
        !game_puffer_read(receiver, "puffTimer", &timer, error) ||
        !game_puffer_number(row, "puffTimer", timer - seconds, error))
        return false;
    return timer - seconds > 0 ||
           ab_classic_bird_specialty_trigger(
               ab_classic_game_runtime_view(game->game).specialty,
               (AbDataValue){.kind = AB_DATA_TABLE, .table = receiver}, seconds, error);
}

static bool game_puffer_inflated(void *context, void *receiver, float seconds,
                                 AbError *error) {
    PreviewPufferObject *row = context;
    float timer;
    if (receiver != game_puffer_physical(row) || !game_puffer_strength(row, error) ||
        !game_puffer_read(receiver, "puffTimer", &timer, error) ||
        !game_puffer_number(row, "puffTimer", timer - seconds, error))
        return false;
    if (timer - seconds > 0)
        return true;
    AbDataValue definition;
    float death, direction;
    return game_puffer_write(row, "puffTimer", (AbDataValue){0}, error) &&
           game_puffer_remove(row, GAME_PUFFER_INFLATED, error) &&
           game_puffer_definition(row, &definition, true, error) &&
           game_puffer_read(definition.table, "deathTime", &death, error) &&
           game_puffer_number(row, "deathTimer", death, error) &&
           game_puffer_read(definition.table, "directionChangeTime", &direction,
                            error) &&
           game_puffer_number(row, "directionChangeTimer", direction, error) &&
           game_puffer_add(row, GAME_PUFFER_DEFLATING, error) &&
           game_puffer_audio(row, menu_route_field(definition.table, "removeSound"),
                             error);
}

static bool game_puffer_direction(PreviewPufferObject *row,
                                  const AbDataTable *definition, AbError *error) {
    PreviewGame *game = row->owner->game;
    float x, y, magnitude, horizontal, vertical, interval;
    if (!game_puffer_read(game_puffer_physical(row), "xVel", &x, error) ||
        !game_puffer_read(definition, "directionChangeMagnitude", &magnitude, error) ||
        magnitude < 0 || magnitude >= 0x1p31f || floorf(magnitude) != magnitude)
        return ab_fail(error, "Unsupported puffer direction interval");
    int32_t high = (int32_t)magnitude;
    if (ab_script_random(&game->menu->random, 2, -high, high, &horizontal) !=
            AB_SCRIPT_RANDOM_OK ||
        !game_puffer_number(row, "xVel", x + horizontal, error) ||
        !game_puffer_read(game_puffer_physical(row), "yVel", &y, error) ||
        ab_script_random(&game->menu->random, 2, -high, high, &vertical) !=
            AB_SCRIPT_RANDOM_OK ||
        !game_puffer_number(row, "yVel", y + vertical, error))
        return false;
    x += horizontal;
    y += vertical;
    float angle;
    if (!ab_script_angle_from_components(-x, y, &angle))
        return ab_fail(error, "Nonfinite actual puffer direction components");
    angle -= 0x1.921fb6p+0f;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t body = level.frame->objects[row->object].body;
    return ab_classic_level_set_rotation(game->playfield.level, row->object, angle,
                                         error) &&
           (ab_body_velocity_set(&level.world->storage.bodies[body].motion,
                                 (AbVec2){x, y}) ||
            ab_fail(error, "Cannot publish actual puffer deflation velocity")) &&
           game_puffer_read(definition, "directionChangeTime", &interval, error) &&
           game_puffer_number(row, "directionChangeTimer", interval, error);
}

static bool game_puffer_retire(PreviewPufferObject *row, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicBirds *birds = ab_classic_game_runtime_view(game->game).birds;
    AbDataTable *registry = ab_classic_birds_view(birds).birds;
    AbDataTable *physical = game_puffer_physical(row);
    if (!game_puffer_write(row, "deflated", game->enabled, error) || !registry)
        return ab_fail(error, "Missing actual puffer bird membership");
    for (size_t index = 0; index < registry->count; ++index) {
        AbDataValue value = registry->entries[index].value;
        if (value.kind == AB_DATA_TABLE && value.table == physical)
            return ab_classic_birds_remove(birds, row->object, game->absent, error);
    }
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    if (row->object >= world.physical_count)
        return ab_fail(error, "Missing canonical puffer death-map identity");
    return ab_string_map_set(world.dead_blocks, level.bodies->objects[row->object].name,
                             (uintptr_t)(world.physical_values + row->object), true,
                             error);
}

static bool game_puffer_deflating(void *context, void *receiver, float seconds,
                                  AbError *error) {
    PreviewPufferObject *row = context;
    AbDataValue definition;
    float direction, death, total, radius;
    if (receiver != game_puffer_physical(row) ||
        !game_puffer_definition(row, &definition, true, error) ||
        !game_puffer_strength(row, error) ||
        !game_puffer_read(receiver, "directionChangeTimer", &direction, error) ||
        !game_puffer_number(row, "directionChangeTimer", direction - seconds, error) ||
        (direction - seconds <= 0 &&
         !game_puffer_direction(row, definition.table, error)) ||
        !game_puffer_read(receiver, "deathTimer", &death, error) ||
        !game_puffer_number(row, "deathTimer", death - seconds, error))
        return false;
    if (death - seconds <= 0)
        return game_puffer_retire(row, error);
    AbDataValue puffed = menu_route_field(definition.table, "puffedValues");
    if (puffed.kind != AB_DATA_TABLE ||
        !game_puffer_read(definition.table, "deathTime", &total, error) || total == 0 ||
        !game_puffer_read(puffed.table, "radius", &radius, error))
        return ab_fail(error, "Invalid actual puffer deflation definition");
    float ratio = (death - seconds) / total;
    PreviewGame *game = row->owner->game;
    return game_puffer_resize(row, radius * ratio, error) &&
           ab_classic_scene_set_scale(
               ab_classic_render_scene_objects(game->playfield.scene), row->object,
               (AbVec2){ratio, ratio}, error);
}

static bool game_puffer_trigger(PreviewPufferObject *row, AbError *error) {
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_puffer_physical(row);
    if (menu_route_truthy(menu_route_field(physical, "isPuffing")))
        return true;
    AbDataValue definition;
    bool delayed;
    if (!game_puffer_definition(row, &definition, true, error) ||
        !game_puffer_has(row, GAME_PUFFER_DELAY, &delayed, error) ||
        (delayed && !game_puffer_remove(row, GAME_PUFFER_DELAY, error)))
        return false;
    float time, friction, restitution, density, radius, current, original;
    AbDataValue puffed = menu_route_field(definition.table, "puffedValues");
    if (puffed.kind != AB_DATA_TABLE ||
        !game_puffer_read(definition.table, "puffTime", &time, error) ||
        !game_puffer_number(row, "puffTimer", time, error) ||
        !game_puffer_read(puffed.table, "friction", &friction, error) ||
        !game_puffer_read(puffed.table, "restitution", &restitution, error) ||
        !game_puffer_read(puffed.table, "density", &density, error) || friction < 0 ||
        restitution < 0 || density < 0)
        return ab_fail(error, "Invalid actual inflated puffer material");
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbObjectBody *body = &level.bodies->objects[row->object].physics;
    if (!ab_world_has_fixture(level.world, body->fixture))
        return ab_fail(error, "Inflation lost its actual circle fixture");
    AbWorldFixture *fixture = level.world->storage.fixtures + body->fixture;
    /* GameLua313d20/313d44/313d68 change native material fields, leaving the
     * Lua table's creation-time friction/restitution/density values untouched. */
    fixture->friction = friction;
    fixture->restitution = restitution;
    fixture->density = density;
    AbDataValue sprite = menu_route_field(definition.table, "spriteSpecial");
    if (sprite.kind != AB_DATA_STRING ||
        !game_puffer_write(row, "damageFactors",
                           menu_route_field(puffed.table, "damageFactors"), error) ||
        !game_puffer_write(row, "sprite", sprite, error) ||
        !ab_classic_visuals_set_sprite(game->playfield.visuals, row->object,
                                       sprite.string, error) ||
        !game_puffer_read(puffed.table, "radius", &radius, error) ||
        !game_puffer_read(physical, "radius", &current, error) ||
        !game_puffer_read(definition.table, "radius", &original, error) ||
        original == 0 || !game_puffer_resize(row, radius * (current / original), error))
        return false;
    AbDataValue explosion;
    return game_puffer_definition(row, &explosion, false, error) &&
           ab_classic_explosions_make(&game->explosions, row->object, explosion,
                                      game->absent, game->absent, error) &&
           game_puffer_add(row, GAME_PUFFER_INFLATED, error) &&
           game_puffer_audio(row, menu_route_field(definition.table, "specialSound"),
                             error) &&
           game_puffer_write(row, "isPuffing", game->enabled, error);
}

static bool game_puffer_collision(void *context, const size_t *flying,
                                  const AbBirdCollisionNamedInput *contact,
                                  AbError *error) {
    (void)contact;
    PreviewGame *game = context;
    if (!game->puffers || !flying || *flying >= game->puffers->capacity)
        return ab_fail(error, "Missing actual flying puffer bird");
    PreviewPufferObject *row = game->puffers->objects + *flying;
    if (menu_route_truthy(
            menu_route_field(game_puffer_physical(row), "specialtyTriggered")))
        return true;
    AbDataValue definition;
    float delay;
    return game_puffer_definition(row, &definition, true, error) &&
           game_puffer_read(definition.table, "automaticPuffDelay", &delay, error) &&
           game_puffer_number(row, "puffTimer", delay, error) &&
           game_puffer_add(row, GAME_PUFFER_DELAY, error);
}

static bool game_specialty_hook(void *context, AbDataValue name, size_t object,
                                AbError *error) {
    PreviewGame *game = context;
    if (!menu_route_equal(name, game_text("triggerPufferSpecialty")))
        return game_shock_hook(context, name, object, error);
    return game->puffers && object < game->puffers->capacity
               ? game_puffer_trigger(game->puffers->objects + object, error)
               : ab_fail(error, "Missing actual puffer level owner");
}

static void game_puffer_close(PreviewGame *game) {
    if (game->puffers) {
        free(game->puffers->objects);
        free(game->puffers);
        game->puffers = NULL;
    }
}

static bool game_puffer_open(PreviewGame *game, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    bool needed = false;
    for (size_t index = 0; index < level.bodies->count; ++index) {
        if (!level.bodies->objects[index].definition_name.size)
            continue;
        AbDataValue definition;
        if (!ab_object_definition_lookup(&level.properties->lookup,
                                         level.bodies->objects[index].name, &definition,
                                         error) ||
            definition.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing initial puffer definition");
        needed |=
            menu_route_equal(menu_route_field(definition.table, "onTriggerSpecialty"),
                             game_text("triggerPufferSpecialty")) ||
            menu_route_equal(menu_route_field(definition.table, "onRemoved"),
                             game_text("blackBirdOrangeVialRemoved"));
    }
    if (!needed)
        return true;
    size_t capacity = ab_classic_level_runtime_view(game->level).object_capacity;
    if (capacity > (4 * 1024 * 1024) / sizeof(PreviewPufferObject))
        return ab_fail(error, "Puffer callback storage exceeds retained byte limit");
    PreviewPuffers *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate puffer owner");
    game->puffers = owner;
    owner->game = game;
    owner->capacity = capacity;
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    if (!owner->objects)
        return ab_fail(error, "Cannot allocate puffer callback rows");
    for (size_t index = 0; index < capacity; ++index) {
        PreviewPufferObject *row = owner->objects + index;
        row->owner = owner;
        row->object = index;
        row->callbacks[GAME_PUFFER_DELAY] =
            (AbObjectUpdateFunction){row, game_puffer_delay};
        row->callbacks[GAME_PUFFER_INFLATED] =
            (AbObjectUpdateFunction){row, game_puffer_inflated};
        row->callbacks[GAME_PUFFER_DEFLATING] =
            (AbObjectUpdateFunction){row, game_puffer_deflating};
    }
    return true;
}

#endif
