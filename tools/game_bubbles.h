#ifndef AB_GAME_BUBBLES_H
#define AB_GAME_BUBBLES_H

#include "game_bubble_common.h"
#include "game_bubble_draw.h"

static bool game_bubble_update(void *context, void *receiver, float seconds,
                               AbError *error) {
    PreviewBubbleObject *row = context;
    PreviewGame *game = row->owner->game;
    AbDataTable *physical = game_bubble_physical(row);
    if (receiver != physical)
        return ab_fail(error, "Rebound actual bubble update receiver");
    if (!menu_route_truthy(menu_route_field(physical, "inBubble")))
        return true;
    float timer, next;
    bool cleanup;
    if (!game_bubble_read(physical, "bubbleAntiGravityTimer", &timer, error) ||
        !ab_bird_bubble_timer(timer, seconds, game->speed, &next, &cleanup, error))
        return false;
    if (cleanup) {
        return game_bubble_write(row, "bubbleAntiGravityTimer", game->absent, error) &&
               game_bubble_write(row, "bubbleAntiGravityFloat", game->absent, error) &&
               game_bubble_write(row, "inBubble", game->absent, error) &&
               game_bubble_remove_update(row, error) &&
               game_bubble_particles(row, "bubblePop", error) &&
               (!menu_route_truthy(menu_route_field(physical, "controllable")) ||
                ab_classic_birds_remove(ab_classic_game_runtime_view(game->game).birds,
                                        row->object, game->absent, error));
    }
    return game_bubble_write(row, "bubbleAntiGravityTimer", menu_route_number(next),
                             error) &&
           game_bubble_apply_float(game, row->object, error);
}

static bool game_bubble_select_sprite(PreviewBubbleObject *row, AbError *error) {
    float random;
    if (ab_script_random(&row->owner->game->menu->random, 2, 1, 3, &random) !=
        AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Invalid actual bubble random state");
    if (random < 1 || random >= 4 || floorf(random) != random)
        return ab_fail(error, "Actual bubble sprite index is outside its array");
    return game_bubble_write(row, "bubbleSprite",
                             game_text(game_bubble_sprites[(size_t)random - 1]), error);
}

static bool game_bubble_capture(PreviewBubbleObject *row, float origin_x,
                                AbError *error) {
    PreviewGame *game = row->owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *physical = game_bubble_physical(row);
    float x, density;
    if (!game_bubble_write(row, "inBubble", game->enabled, error) ||
        !game_bubble_add_update(row, error) || !game_bubble_select_sprite(row, error) ||
        !game_bubble_write(row, "bubbleSpriteScale", menu_route_number(1), error) ||
        !game_bubble_write(row, "bubbleAntiGravityTimer", menu_route_number(2),
                           error) ||
        !game_bubble_read(physical, "x", &x, error) ||
        !game_bubble_write(row, "bubbleAntiGravityFloat",
                           menu_route_number((x - origin_x) * 1.3f), error) ||
        !game_bubble_read(physical, "density", &density, error))
        return false;
    AbVec2 velocity;
    return (!(density < .1f) || game_bubble_velocity(row, true, &velocity, error)) &&
           ab_classic_joints_break(
               level.joints, level.bodies->objects[row->object].name, 100,
               ab_classic_joint_updates_defer, game->joint_updates, error);
}

static bool game_bubble_trigger(PreviewBubbleObject *bird, float radius,
                                AbError *error) {
    PreviewBubbles *owner = bird->owner;
    PreviewGame *game = owner->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *physical = game_bubble_physical(bird);
    AbVec2 origin;
    if (!game_bubble_read(physical, "x", &origin.x, error) ||
        !game_bubble_read(physical, "y", &origin.y, error))
        return false;
    AbClassicWorldCursor cursor = {0};
    for (;;) {
        size_t object;
        bool present;
        if (!ab_classic_level_world_next(game->playfield.level, &cursor, &object,
                                         &present, error))
            return false;
        if (!present)
            break;
        if (object >= owner->capacity)
            return ab_fail(error, "Bubble world identity exceeds reserved rows");
        AbDataValue definition;
        AbDataTable *target = &level.properties->objects[object].physical;
        if (!ab_object_definition_lookup(&level.properties->lookup,
                                         level.bodies->objects[object].name,
                                         &definition, error) ||
            definition.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing actual bubble candidate definition");
        AbDataValue material = menu_route_field(definition.table, "materialName");
        AbBirdBubbleCandidate candidate = {
            .radius = radius,
            .excluded_material =
                menu_route_equal(material, game_text("immovable")) ||
                menu_route_equal(material, game_text("staticGround")) ||
                menu_route_equal(material, game_text("bubble")),
            .controllable =
                menu_route_truthy(menu_route_field(definition.table, "controllable")),
            .shot = menu_route_truthy(menu_route_field(target, "shot")),
            .puffer = menu_route_equal(menu_route_field(definition.table, "specialty"),
                                       game_text("PUFFER")),
            .flying =
                game->flying.kind == AB_DATA_TABLE && game->flying.table == target,
            .same = object == bird->object};
        /* Noneligible records do not read their potentially absent position. */
        bool inside;
        if (!ab_bird_bubble_candidate(candidate, &inside, error))
            return false;
        if (!inside)
            continue;
        if (!game_bubble_read(target, "x", &candidate.relative.x, error) ||
            !game_bubble_read(target, "y", &candidate.relative.y, error))
            return false;
        candidate.relative.x -= origin.x;
        candidate.relative.y -= origin.y;
        if (!ab_bird_bubble_candidate(candidate, &inside, error) ||
            (inside && !game_bubble_capture(owner->objects + object, origin.x, error)))
            return false;
    }
    size_t body = level.frame->objects[bird->object].body;
    return ab_extra_draws_set(game->extra_draws, menu_span("ingame_bubbles"),
                              &owner->draw, error) &&
           (ab_body_velocity_set(&level.world->storage.bodies[body].motion,
                                 (AbVec2){0}) ||
            ab_fail(error, "Cannot stop actual pink bird")) &&
           game_bubble_add_update(bird, error) &&
           game_bubble_write(bird, "inBubble", game->enabled, error) &&
           game_bubble_write(bird, "bubbleAntiGravityTimer", menu_route_number(2),
                             error) &&
           game_bubble_select_sprite(bird, error);
}

static bool game_bubble_branch(void *context, size_t object, AbError *error) {
    PreviewGame *game = context;
    if (!game->bubbles || object >= game->bubbles->capacity)
        return ab_fail(error, "Missing actual pink bird owner");
    return game_bubble_write(game->bubbles->objects + object, "pinkTimer",
                             menu_route_number(game->bubbles->charge_duration), error);
}

static bool game_bubble_timed(PreviewGame *game, size_t object, float seconds,
                              bool *alive, AbError *error) {
    if (!game->bubbles || object >= game->bubbles->capacity)
        return ab_fail(error, "Missing actual pink timer owner");
    PreviewBubbleObject *row = game->bubbles->objects + object;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *physical = game_bubble_physical(row);
    float timer;
    if (!game_bubble_read(physical, "pinkTimer", &timer, error))
        return false;
    timer -= seconds;
    if (!isfinite(timer) ||
        !game_bubble_write(row, "pinkTimer", menu_route_number(timer), error))
        return false;
    if (timer < game->bubbles->charge_duration) {
        if (!menu_route_truthy(menu_route_field(physical, "sonicBoom"))) {
            size_t body = level.frame->objects[object].body;
            AbDataValue definition, sound;
            uint32_t voice;
            if (!game_bubble_write(row, "sonicBoom", game->enabled, error) ||
                !ab_body_velocity_set(&level.world->storage.bodies[body].motion,
                                      (AbVec2){0}) ||
                !game_bubble_particles(row, "sonicBoom", error) ||
                !ab_object_definition_lookup(&level.properties->lookup,
                                             level.bodies->objects[object].name,
                                             &definition, error) ||
                definition.kind != AB_DATA_TABLE ||
                !ab_audio_bank_name_value(
                    game->menu->audio,
                    menu_route_field(definition.table, "specialSound"), &sound,
                    error) ||
                !ab_audio_bank_play(game->menu->audio,
                                    (AbAudioPlay){.name = sound,
                                                  .volume = menu_route_number(1),
                                                  .loop = game->disabled},
                                    &voice, error))
                return false;
        }
        float mass;
        if (!game_bubble_read(physical, "mass", &mass, error) ||
            !game_bubble_queue(
                row, (AbVec2){0, -level.world->definition.gravity.y * mass + 1}, error))
            return false;
    }
    if (timer < 0) {
        AbDataValue definition;
        float radius;
        if (!game_bubble_write(row, "pinkTimer", game->absent, error))
            return false;
        game->globals.specialty_available = game->disabled;
        if (!game_bubble_particles(row, "bubblePopBig", error) ||
            !ab_object_definition_lookup(&level.properties->lookup,
                                         level.bodies->objects[object].name,
                                         &definition, error) ||
            definition.kind != AB_DATA_TABLE ||
            !game_bubble_read(definition.table, "specialtyRadius", &radius, error) ||
            !game_bubble_trigger(row, radius, error))
            return false;
    }
    *alive = true;
    return true;
}

static void game_bubble_close(PreviewGame *game) {
    if (game->bubbles) {
        free(game->bubbles->objects);
        free(game->bubbles);
        game->bubbles = NULL;
    }
}

static bool game_bubble_open(PreviewGame *game, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    bool needed = false;
    for (size_t index = 0; index < level.bird_count; ++index) {
        AbDataValue definition;
        if (!ab_object_definition_lookup(
                &level.properties->lookup,
                level.bodies->objects[level.bird_order[index]].name, &definition,
                error) ||
            definition.kind != AB_DATA_TABLE)
            return ab_fail(error, "Missing initial bubble bird definition");
        needed |= menu_route_equal(menu_route_field(definition.table, "specialty"),
                                   game_text("BUBBLES"));
    }
    if (!needed)
        return true;
    size_t capacity = ab_classic_level_runtime_view(game->level).object_capacity;
    if (capacity > (4 * 1024 * 1024) / sizeof(PreviewBubbleObject))
        return ab_fail(error, "Bubble callback rows exceed retained byte limit");
    PreviewBubbles *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate bubble effect owner");
    game->bubbles = owner;
    owner->game = game;
    owner->capacity = capacity;
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    const AbDataTable *sonic =
        ab_classic_particles_definition(game->particles, menu_span("sonicBoom"));
    if (!owner->objects ||
        !game_bubble_read(sonic, "lifeTime", &owner->charge_duration, error))
        return ab_fail(error, "Missing actual bubble charge definition/storage");
    for (size_t index = 0; index < 3; ++index)
        if (!ab_lua_asset_sprite_bind(&game->menu->sprites.provider,
                                      menu_span(game_bubble_sprites[index]),
                                      owner->sprites + index, error) ||
            !owner->sprites[index].region)
            return ab_fail(error, "Missing actual bubble texture");
    owner->draw = (AbExtraDrawFunction){.context = owner, .draw = game_bubble_draw};
    for (size_t index = 0; index < capacity; ++index) {
        PreviewBubbleObject *row = owner->objects + index;
        row->owner = owner;
        row->object = index;
        row->update = (AbObjectUpdateFunction){row, game_bubble_update};
    }
    return true;
}

#endif
