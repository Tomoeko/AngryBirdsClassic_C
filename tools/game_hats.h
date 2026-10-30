#ifndef AB_GAME_HATS_H
#define AB_GAME_HATS_H

#include "ab/script_math.h"

static bool game_birthday(const PreviewGame *game) {
    return menu_route_equal(game->menu->level_request.episode_id,
                            menu_route_number(7)) &&
           menu_route_equal(game->menu->level_request.page_number,
                            menu_route_number(2));
}

static bool game_hat_index(PreviewGame *game, AbSpan name, size_t *output,
                           AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    for (size_t object = 0; object < level.bodies->count; ++object) {
        AbSpan candidate = level.bodies->objects[object].name;
        if (candidate.size == name.size &&
            (!name.size || !memcmp(candidate.data, name.data, name.size))) {
            *output = object;
            return true;
        }
    }
    ab_fail(error, "Hat service lost its actual object name");
    return false;
}

static bool game_hat_object(void *context, AbSpan name, const AbDataTable **output,
                            AbError *error) {
    PreviewGame *game = context;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    const AbDataValue *value =
        ab_data_get(&level.properties->world,
                    (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    if (value && value->kind != AB_DATA_NIL && value->kind != AB_DATA_TABLE)
        return ab_fail(error, "Hat lookup has an invalid actual world value");
    *output = value && value->kind == AB_DATA_TABLE ? value->table : NULL;
    return true;
}

static bool game_hat_type(PreviewGame *game, AbSpan name, AbBodyType type,
                          AbError *error) {
    size_t object;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    return game_hat_index(game, name, &object, error) &&
           ab_world_set_body_type(level.world, level.frame->objects[object].body, type,
                                  error);
}

static bool game_hat_static(void *context, AbSpan name, AbError *error) {
    return game_hat_type(context, name, AB_BODY_STATIC, error);
}

static bool game_hat_dynamic(void *context, AbSpan name, AbError *error) {
    return game_hat_type(context, name, AB_BODY_DYNAMIC, error);
}

static bool game_hat_defence(void *context, AbSpan name, float defence,
                             AbError *error) {
    PreviewGame *game = context;
    size_t object;
    return game_hat_index(game, name, &object, error) &&
           game_eagle_number(game_eagle_physical(game, object), "defence", defence,
                             error);
}

static bool game_hat_damping(void *context, AbSpan name, float damping,
                             AbError *error) {
    PreviewGame *game = context;
    size_t object;
    if (!game_hat_index(game, name, &object, error))
        return false;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    level.world->storage.bodies[level.frame->objects[object].body]
        .motion.linear_damping = damping;
    return true;
}

static bool game_hat_impulse(void *context, AbSpan name, AbVec2 impulse, AbVec2 point,
                             AbError *error) {
    PreviewGame *game = context;
    size_t object;
    if (!game_hat_index(game, name, &object, error))
        return false;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    return ab_body_impulse_apply(
               &level.world->storage.bodies[level.frame->objects[object].body].motion,
               impulse, &point) ||
           ab_fail(error, "Cannot apply actual hat release impulse");
}

static bool game_hat_defer(void *context, AbSpan joint, AbError *error) {
    return ab_classic_joint_updates_defer(((PreviewGame *)context)->joint_updates,
                                          joint, error);
}

static bool game_hat_print(void *context, AbError *error) {
    (void)context;
    return fputs("releasing hat\n", stderr) >= 0 ||
           ab_fail(error, "Cannot write actual hat release diagnostic");
}

static bool game_hat_lookup(void *context, AbSpan bird, AbBirdHat **output,
                            AbError *error) {
    PreviewGame *game = context;
    uintptr_t value = 0;
    bool present;
    if (!ab_string_map_get(ab_classic_world_runtime_view(game->world).hats, bird,
                           &value, &present, error))
        return false;
    *output = present ? (AbBirdHat *)value : NULL;
    return !present || value || ab_fail(error, "Invalid retained hat record");
}

static bool game_created_register(PreviewGame *game, size_t object, bool box,
                                  AbError *error) {
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(world.dynamics);
    if (!(box ? ab_classic_shots_register_box(game->playfield.shots, object, error)
              : ab_classic_shots_register_circle(game->playfield.shots, object,
                                                 error)) ||
        !ab_classic_camera_register_object(game->playfield.camera, object, error) ||
        !(box ? ab_classic_object_removal_register_box(dynamics.physical, object, error)
              : ab_classic_object_removal_register_circle(dynamics.physical, object,
                                                          error)) ||
        !ab_classic_birds_register_object(runtime.birds, object, error) ||
        !(box ? ab_classic_world_runtime_register_box(game->world, object, error)
              : ab_classic_world_runtime_register_circle(game->world, object, error)) ||
        !ab_classic_controller_register_object(runtime.controller, object, error) ||
        !ab_classic_characters_register_objects(runtime.characters, error))
        return false;
    game->explosions.physical_count =
        ab_classic_world_runtime_view(game->world).physical_count;
    return game_factory_created(game, object, error);
}

static bool game_hat_create(void *context, AbSpan definition_name, AbSpan name,
                            AbVec2 position, float density, AbError *error) {
    PreviewGame *game = context;
    PreviewHat *row = game->hats.records + game->hats.creating;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level.catalog, definition_name, NULL, &definition,
                                   error) ||
        !ab_classic_level_create_defined_box(game->playfield.level, name,
                                             definition_name, position, density,
                                             &row->object, error))
        return false;
    AbDataValue sprite = ab_object_property(&definition, menu_span("sprite"));
    return sprite.kind == AB_DATA_STRING && definition.z_order.kind == AB_DATA_NUMBER &&
           ab_classic_render_scene_create_object(
               game->playfield.scene, game->playfield.visuals, row->object,
               sprite.string, menu_real(definition.z_order.bits), error) &&
           ab_classic_behaviors_create_object(
               game->playfield.behaviors, row->object,
               ab_object_property(&definition, menu_span("behaviors")),
               level.catalog_data, error) &&
           game_created_register(game, row->object, true, error);
}

static bool game_hat_joint(void *context, AbSpan name, AbSpan bird, AbSpan hat,
                           AbError *error) {
    PreviewGame *game = context;
    PreviewHat *row = game->hats.records + game->hats.creating;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    const char *const keys[] = {
        "name", "type", "coordType", "end1", "end2",
        "x1",   "y1",   "x2",        "y2",   "collideConnected"};
    const AbDataValue values[] = {{.kind = AB_DATA_STRING, .string = name},
                                  menu_route_number(2),
                                  menu_route_number(2),
                                  {.kind = AB_DATA_STRING, .string = bird},
                                  {.kind = AB_DATA_STRING, .string = hat},
                                  menu_route_number(0),
                                  menu_route_number(0),
                                  menu_route_number(0),
                                  menu_route_number(0),
                                  {.kind = AB_DATA_BOOLEAN}};
    for (size_t index = 0; index < 10; ++index)
        row->joint_fields[index] = (AbDataEntry){game_text(keys[index]), values[index]};
    row->joint = (AbDataTable){row->joint_fields, 10, 12, NULL};
    AbClassicJointSource source = {.table = &row->joint};
    const size_t objects[] = {row->bird, row->object};
    for (size_t index = 0; index < 2; ++index) {
        size_t body = level.frame->objects[objects[index]].body;
        AbBodyMassState *state = &level.world->storage.bodies[body].motion.mass;
        source.body_indices[index] = body;
        source.bodies[index] = (AbJointBodyFrame){state->transform, state->angle};
    }
    AbClassicJoint definition;
    size_t engine;
    return ab_classic_joint_read(&source, &definition, error) &&
           ab_classic_joints_create(level.joints, &definition, &engine, error) &&
           ab_classic_joint_updates_register(game->joint_updates, name, error);
}

static bool game_hat_publish(void *context, AbSpan bird, AbBirdHat *record,
                             AbError *error) {
    PreviewGame *game = context;
    size_t object;
    if (!game_hat_index(game, bird, &object, error) ||
        !ab_string_map_set(ab_classic_world_runtime_view(game->world).hats, bird,
                           (uintptr_t)record, true, error))
        return false;
    ab_classic_level_view(game->playfield.level).character_objects[object].has_hat =
        true;
    return true;
}

static AbBirdHatHost game_hat_host(PreviewGame *game) {
    return (AbBirdHatHost){game,
                           game_hat_create,
                           game_hat_static,
                           game_hat_defence,
                           game_hat_joint,
                           game_hat_publish,
                           game_hat_print,
                           game_hat_lookup,
                           game_hat_defer,
                           game_hat_damping,
                           game_hat_dynamic,
                           game_hat_object,
                           game_hat_impulse};
}

static bool game_hat_release(void *context, AbSpan bird, AbError *error) {
    return ab_bird_hat_release(bird, game_hat_host(context), error);
}

static bool game_hat_input(void *context, AbDataValue name, AbError *error) {
    return name.kind == AB_DATA_STRING
               ? game_hat_release(context, name.string, error)
               : ab_fail(error, "Hat release has no actual bird name");
}

static bool game_hat_move(PreviewGame *game, size_t bird, float bounce, bool hold,
                          AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbBirdHat *hat;
    if (!game_hat_lookup(game, level.bodies->objects[bird].name, &hat, error))
        return false;
    if (!hat)
        return true;
    size_t object;
    if (!game_hat_index(game, hat->collision.object_name, &object, error))
        return false;
    if (level.removed_objects[object])
        return true;
    const AbDataTable *physical = game_eagle_physical(game, bird);
    AbVec2 point = {game_eagle_value(physical, "x") + hat->offset.x,
                    game_eagle_value(physical, "y") + hat->offset.y - bounce};
    if (!ab_classic_level_set_position(game->playfield.level, object, point, error))
        return false;
    AbBodyMotion *body =
        &level.world->storage.bodies[level.frame->objects[object].body].motion;
    return !hold || (ab_body_velocity_set(body, (AbVec2){0}) &&
                     ab_body_angular_velocity_set(body, 0));
}

static bool game_hat_sling(void *context, size_t bird, AbClassicSlingHatPhase phase,
                           float time, AbError *error) {
    PreviewGame *game = context;
    if (phase == AB_SLING_HAT_ACTIVATE) {
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        AbBirdHat *hat;
        return game_hat_lookup(game, level.bodies->objects[bird].name, &hat, error) &&
               (!hat || game_hat_dynamic(game, hat->collision.object_name, error));
    }
    float bounce = 0;
    if (phase == AB_SLING_HAT_TRANSFER &&
        !ab_script_sine(fminf(time, 1) * 0x1.921fb6p+1f, &bounce))
        return ab_fail(error, "Invalid source hat transfer angle");
    return game_hat_move(game, bird, bounce, phase == AB_SLING_HAT_HOLD, error);
}

static bool game_hat_jump(void *context, size_t bird, AbError *error) {
    PreviewGame *game = context;
    AbCharacterAnimationObject *object =
        ab_classic_level_view(game->playfield.level).character_objects + bird;
    float sine;
    return ab_script_sine(object->jump.phase, &sine)
               ? game_hat_move(game, bird, sine * object->jump.height, false, error)
               : ab_fail(error, "Invalid source hat jump angle");
}

static bool game_hats_open(PreviewGame *game, AbError *error) {
    if (!game_birthday(game))
        return true;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    game->hats.capacity = level.bird_count;
    game->hats.records = calloc(level.bird_count, sizeof(*game->hats.records));
    if (level.bird_count && !game->hats.records)
        return ab_fail(error, "Cannot allocate birthday hat records");
    AbDataValue hats = menu_route_field(level.catalog, "hats");
    if (hats.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing original birthday hat definitions");
    AbClassicWorldCursor cursor = {0};
    for (;;) {
        size_t bird;
        bool present;
        if (!ab_classic_level_world_next(game->playfield.level, &cursor, &bird,
                                         &present, error))
            return false;
        if (!present)
            break;
        AbDataTable *physical = game_eagle_physical(game, bird);
        if (!menu_route_truthy(menu_route_field(physical, "controllable")))
            continue;
        if (game->hats.count == game->hats.capacity)
            return ab_fail(error,
                           "Birthday hat collection exceeds authored bird count");
        PreviewHat *row = game->hats.records + game->hats.count++;
        row->bird = bird;
        float value;
        uint8_t text[32];
        AbSpan suffix;
        if (ab_script_random(&game->menu->random, 2, 1, 4, &value) !=
                AB_SCRIPT_RANDOM_OK ||
            !ab_ui_value_text(menu_number(value), text, sizeof(text), &suffix))
            return ab_fail(error, "Cannot select actual birthday hat");
        int size = snprintf(row->selected, sizeof(row->selected), "PartyHat_%.*s",
                            (int)suffix.size, suffix.data);
        if (size < 0 || (size_t)size >= sizeof(row->selected))
            return ab_fail(error, "Birthday hat name exceeds stable storage");
        if (menu_route_equal(menu_route_field(physical, "sprite"),
                             game_text("BIRD_BLUE")))
            memcpy(row->selected, "PartyHat_blue", sizeof("PartyHat_blue"));
    }
    for (size_t index = 0; index < game->hats.count; ++index) {
        PreviewHat *row = game->hats.records + index;
        game->hats.creating = index;
        bool created;
        if (!ab_bird_hat_create(hats.table, game_eagle_physical(game, row->bird),
                                menu_span(row->selected), &game->hats.counter,
                                &row->source, &created, game_hat_host(game), error))
            return false;
    }
    return ab_classic_sling_bind_hats(
        game->playfield.sling, (AbClassicSlingHatHost){game, game_hat_sling}, error);
}

#endif
