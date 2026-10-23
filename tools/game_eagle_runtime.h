#ifndef AB_GAME_EAGLE_RUNTIME_H
#define AB_GAME_EAGLE_RUNTIME_H

/* mightyEagle.lua and gamelogic launch88284, pinned 5.2.0 APK. These
 * application bindings use the same live owners as ordinary birds. */
static AbDataTable *game_eagle_physical(PreviewGame *game, size_t object) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    return object < level.bodies->count ? &level.properties->objects[object].physical
                                        : NULL;
}

static bool game_eagle_write(AbDataTable *table, const char *name, AbDataValue value,
                             AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = table}};
    return ab_object_property_set(&access, menu_span(name), value, error);
}

static bool game_eagle_number(AbDataTable *table, const char *name, float value,
                              AbError *error) {
    return isfinite(value) &&
           game_eagle_write(table, name, menu_route_number(value), error);
}

static float game_eagle_value(const AbDataTable *table, const char *name) {
    AbDataValue value = menu_route_field(table, name);
    return value.kind == AB_DATA_NUMBER ? menu_real(value.bits) : NAN;
}

static bool game_eagle_true(const AbDataTable *table, const char *name) {
    AbDataValue value = menu_route_field(table, name);
    return value.kind == AB_DATA_BOOLEAN && value.bits;
}

static bool game_eagle_sound(PreviewGame *game, const char *name, AbError *error) {
    uint32_t voice;
    return ab_audio_bank_play(game->menu->audio,
                              (AbAudioPlay){.name = game_text(name),
                                            .volume = menu_route_number(1),
                                            .loop = game->disabled},
                              &voice, error);
}

static void game_eagle_cancel_ending(PreviewGame *game) {
    game->finish.complete.ending_timer = 0;
    game->finish.failed_timer = 0;
    game->finish.complete.completing = false;
    game->will_end = game->disabled;
}

static bool game_eagle_dead(PreviewGame *game, size_t object, bool bait,
                            AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    if (!game_eagle_number(game_eagle_physical(game, object), "strength", 0, error) ||
        !ab_string_map_set(world.dead_blocks, level.bodies->objects[object].name,
                           (uintptr_t)(world.physical_values + object), true, error))
        return false;
    if (bait) {
        AbDataTable *birds =
            ab_classic_birds_view(ab_classic_game_runtime_view(game->game).birds).birds;
        AbObjectPropertyAccess access = {
            .physics_object = {.kind = AB_DATA_TABLE, .table = birds}};
        return ab_object_property_set(&access, level.bodies->objects[object].name,
                                      (AbDataValue){0}, error);
    }
    return true;
}

static bool game_eagle_goals(PreviewGame *game, bool remove, bool exit,
                             AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldCursor cursor = {0};
    bool present;
    size_t object;
    for (;;) {
        if (!ab_classic_level_world_next(game->playfield.level, &cursor, &object,
                                         &present, error))
            return false;
        if (!present)
            return true;
        AbDataTable *physical = game_eagle_physical(game, object);
        bool goal = menu_route_field(physical, "strength").kind != AB_DATA_NIL &&
                    menu_route_truthy(menu_route_field(physical, "levelGoal"));
        if (goal && remove) {
            if (!game_eagle_dead(game, object, false, error))
                return false;
        } else if (goal) {
            float mass = game_eagle_value(physical, "mass");
            AbVec2 point = {game_eagle_value(physical, "x"),
                            game_eagle_value(physical, "y")};
            size_t body = level.frame->objects[object].body;
            if (!isfinite(mass) ||
                !ab_body_impulse_apply(&level.world->storage.bodies[body].motion,
                                       (AbVec2){0, -mass * 15}, &point) ||
                !game_eagle_number(physical, "strength", exit ? .00001f : .0001f,
                                   error) ||
                !game_eagle_number(physical, "defence", 0, error) ||
                (!exit &&
                 !game_eagle_write(physical, "ignoreAllDamage", game->disabled, error)))
                return ab_fail(error, "Cannot apply Mighty Eagle goal impulse");
        } else if (!remove && !exit &&
                   menu_route_truthy(menu_route_field(physical, "isMightyBait")) &&
                   !game_eagle_dead(game, object, true, error))
            return false;
    }
}

static bool game_eagle_joints(PreviewGame *game, AbError *error) {
    AbGameTailJoints *joints = ab_classic_joint_updates_tail(game->joint_updates);
    /* Each event can mutate the live metadata projection; resume from a fresh
     * source registry after each real destroy boundary. */
    while (joints->count) {
        AbSpan name = joints->entries[0].name;
        if (!ab_classic_joint_updates_call(
                game->joint_updates,
                &(AbGameTailCall){.operation = AB_GAME_TAIL_DESTROY_JOINT,
                                  .joint_name = name},
                error) ||
            !ab_classic_joint_updates_refresh(game->joint_updates, error))
            return false;
    }
    return true;
}

static bool game_eagle_ground(PreviewGame *game, size_t object, AbError *error) {
    AbDataTable *physical = game_eagle_physical(game, object);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level.catalog, menu_span("MightyEagleBird"), NULL,
                                   &definition, error))
        return false;
    AbDataValue sprite = ab_object_property(&definition, menu_span("spriteCollision"));
    game->shake.amplitude = 100;
    game->eagle_timer.camera_shake = menu_route_number(100);
    if (sprite.kind != AB_DATA_STRING ||
        !game_eagle_write(physical, "hitGround", game->enabled, error) ||
        !ab_classic_visuals_set_sprite(game->playfield.visuals, object, sprite.string,
                                       error) ||
        !game_eagle_sound(game, "mighty_eagle_thump", error) ||
        !game_eagle_goals(game, false, false, error))
        return false;
    game->eagle_timer.destroy_joints = game->enabled;
    game->eagle_timer.timer = menu_route_number(4);
    return true;
}

static bool game_eagle_spawn(PreviewGame *game, AbVec2 target, AbError *error);

static bool game_eagle_object_update(void *context, void *receiver, float seconds,
                                     AbError *error) {
    PreviewEagle *eagle = context;
    PreviewGame *game = eagle->game;
    AbDataTable *physical = receiver;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    bool bait = game_eagle_true(physical, "isMightyBait");
    size_t object = bait ? eagle->bait : eagle->bird;
    if (physical != game_eagle_physical(game, object))
        return ab_fail(error, "Foreign Mighty Eagle update receiver");
    if (bait) {
        if (game->eagle_timer.timer.kind != AB_DATA_NIL &&
            menu_real(game->eagle_timer.timer.bits) < 6.2f &&
            game->eagle_timer.moving.kind == AB_DATA_BOOLEAN &&
            game->eagle_timer.moving.bits) {
            game->globals.specialty_available = game->disabled;
            game->eagle_timer.moving = game->disabled;
            if (!game_eagle_spawn(game,
                                  (AbVec2){game_eagle_value(physical, "x"),
                                           game_eagle_value(physical, "y")},
                                  error))
                return false;
        }
        if (!menu_route_truthy(menu_route_field(physical, "shot")) ||
            game_eagle_true(physical, "hasCollided"))
            return true;
        float angular =
            menu_route_field(physical, "angularVelocity").kind == AB_DATA_NIL
                ? 0
                : game_eagle_value(physical, "angularVelocity");
        angular = fminf(20, angular + seconds * 10);
        return game_eagle_number(physical, "angularVelocity", angular, error) &&
               ab_body_angular_velocity_set(
                   &level.world->storage.bodies[level.frame->objects[object].body]
                        .motion,
                   angular);
    }
    eagle->height = fmaxf(
        0, fminf(50, ab_classic_sling_start_pose(game->playfield.sling).level_start.y -
                         game_eagle_value(physical, "y")));
    float timer = game_eagle_value(physical, "particleTimer") + seconds;
    float limit = game_eagle_value(physical, "particleTimerLimit");
    float count = floorf(timer / limit);
    if (count > 0) {
        timer = (float)fmod((double)timer, (double)limit);
        if (!ab_classic_particles_legacy(game->particles,
                                         level.bodies->objects[object].name,
                                         game_text("mightyEagleParticles"),
                                         menu_route_number(count), false, false, error))
            return false;
    }
    if (!game_eagle_number(physical, "particleTimer", timer, error))
        return false;
    bool hit = game_eagle_true(physical, "hitGround");
    float x = game_eagle_value(physical, "initVelX");
    float y = game_eagle_value(physical, "initVelY");
    AbBodyMotion *body =
        &level.world->storage.bodies[level.frame->objects[object].body].motion;
    if (!ab_body_velocity_set(body, (AbVec2){x * 10000, (hit ? -y : y) * 10000}))
        return ab_fail(error, "Cannot set Mighty Eagle flight velocity");
    if (hit)
        return ab_body_angular_velocity_set(body, 4 * (float)3.141592653589793);
    if (!ab_classic_level_set_rotation(game->playfield.level, object,
                                       (float)atan2((double)y, (double)x), error))
        return false;
    AbDataValue height = menu_route_field(game->menu->level_request.episode,
                                          "mightyEagleCollisionHeight");
    return !menu_route_truthy(height) ||
           (height.kind == AB_DATA_NUMBER &&
            (menu_real(height.bits) >= game_eagle_value(physical, "y") ||
             game_eagle_ground(game, object, error)));
}

static bool game_eagle_register(PreviewGame *game, size_t object, bool bait,
                                AbError *error) {
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(world.dynamics);
    bool okay =
        bait ? ab_classic_shots_register_box(game->playfield.shots, object, error)
             : ab_classic_shots_register_circle(game->playfield.shots, object, error);
    if (!okay ||
        !ab_classic_camera_register_object(game->playfield.camera, object, error))
        return false;
    okay =
        bait ? ab_classic_object_removal_register_box(dynamics.physical, object, error)
             : ab_classic_object_removal_register_circle(dynamics.physical, object,
                                                         error);
    if (!okay || !ab_classic_birds_register_object(runtime.birds, object, error))
        return false;
    okay = bait ? ab_classic_world_runtime_register_box(game->world, object, error)
                : ab_classic_world_runtime_register_circle(game->world, object, error);
    if (!okay ||
        !ab_classic_controller_register_object(runtime.controller, object, error) ||
        !ab_classic_characters_register_objects(runtime.characters, error))
        return false;
    game->explosions.physical_count =
        ab_classic_world_runtime_view(game->world).physical_count;
    PreviewEagle *eagle = game->eagle;
    size_t slot = bait ? 0 : 1;
    eagle->functions[slot] = (AbObjectUpdateFunction){eagle, game_eagle_object_update};
    return ab_object_update_add(eagle->updates + slot, eagle->functions + slot, NULL,
                                error) &&
           ab_classic_object_hooks_register(
               ab_classic_flames_updates(
                   ab_classic_block_scripts_view(world.scripts).flames),
               object, eagle->updates + slot, error);
}

static bool game_eagle_add_bird(PreviewGame *game, size_t object, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *birds =
        ab_classic_birds_view(ab_classic_game_runtime_view(game->game).birds).birds;
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE, .table = birds}};
    return ab_object_property_set(
        &access, level.bodies->objects[object].name,
        (AbDataValue){.kind = AB_DATA_TABLE,
                      .table = game_eagle_physical(game, object)},
        error);
}

static bool game_eagle_spawn(PreviewGame *game, AbVec2 target, AbError *error) {
    PreviewEagle *eagle = game->eagle;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbObjectDefinition definition;
    if (!eagle || eagle->bird != SIZE_MAX ||
        !ab_object_definition_open(level.catalog, menu_span("MightyEagleBird"), NULL,
                                   &definition, error))
        return ab_fail(error, "Invalid Mighty Eagle construction");
    game_eagle_cancel_ending(game);
    game->eagle_timer.bait_x = game->eagle_timer.bait_y = (AbDataValue){0};
    AbGameCircleInput input = {
        .name = menu_span("MightyEagle_a"),
        /* The script retains fractional edges; setLevelLimits truncates only
         * the native boundary check's separate integer storage. */
        .position = {game->world_left_edge + 1,
                     target.y - ((target.x - game->world_left_edge + 1) / 2)},
        .controllable = true};
    const char *const numeric[] = {"radius", "density", "friction", "restitution",
                                   "z_order"};
    float *const values[] = {&input.radius, &input.density, &input.friction,
                             &input.restitution, &input.z_order};
    for (size_t index = 0; index < 5; ++index) {
        AbDataValue value = ab_object_property(&definition, menu_span(numeric[index]));
        if (value.kind != AB_DATA_NUMBER ||
            !isfinite(*values[index] = menu_real(value.bits)))
            return ab_fail(error, "Invalid Mighty Eagle definition %s", numeric[index]);
    }
    AbDataValue sprite = ab_object_property(&definition, menu_span("sprite"));
    AbSpriteAtlasBinding binding;
    if (sprite.kind != AB_DATA_STRING ||
        !game->menu->sprites.provider.sprite(game->menu->sprites.provider.context,
                                             sprite.string, &binding) ||
        !ab_classic_level_create_circle(game->playfield.level, input, &eagle->bird,
                                        error) ||
        !ab_classic_render_scene_create_object(game->playfield.scene,
                                               game->playfield.visuals, eagle->bird,
                                               sprite.string, input.z_order, error))
        return ab_fail(error, "Cannot create actual Mighty Eagle renderer/body");
    size_t object = eagle->bird;
    level = ab_classic_level_view(game->playfield.level);
    level.bodies->objects[object].definition_name = menu_span("MightyEagleBird");
    AbDataTable *physical = game_eagle_physical(game, object);
    const char *const copied[] = {"controllable",      "strength",
                                  "defence",           "materialName",
                                  "damageFactors",     "useLegacyCollisionPath",
                                  "collisionFunction", "exitedFunction"};
    if (!game_eagle_write(physical, "definition", game_text("MightyEagleBird"), error))
        return false;
    for (size_t index = 0; index < sizeof(copied) / sizeof(*copied); ++index)
        if (!game_eagle_write(physical, copied[index],
                              ab_object_property(&definition, menu_span(copied[index])),
                              error))
            return false;
    float x = target.x - input.position.x, y = target.y - input.position.y;
    float length = (float)sqrt((double)(x * x + y * y));
    const char *const names[] = {"targetX",   "targetY",       "initVelX",
                                 "initVelY",  "spritePivotX",  "spritePivotY",
                                 "xVel",      "yVel",          "animTimer",
                                 "jumpTimer", "particleTimer", "particleTimerLimit"};
    const float numbers[] = {target.x,
                             target.y,
                             length != 0 ? x / length : 0,
                             length != 0 ? y / length : 0,
                             binding.sprite.region->pivot_x,
                             binding.sprite.region->pivot_y,
                             0,
                             0,
                             9999,
                             9999,
                             0,
                             .025f};
    for (size_t index = 0; index < sizeof(names) / sizeof(*names); ++index)
        if (!game_eagle_number(physical, names[index], numbers[index], error))
            return false;
    if (!game_eagle_write(physical, "levelGoal", game->disabled, error) ||
        !game_eagle_write(physical, "damageSprite", sprite, error) ||
        !game_eagle_write(physical, "animOn", game->disabled, error) ||
        !game_eagle_write(physical, "isMightyEagle", game->enabled, error) ||
        !game_eagle_write(physical, "shot", game->enabled, error) ||
        !game_eagle_write(physical, "recordTrajectory", game->disabled, error) ||
        !game_eagle_register(game, object, false, error) ||
        !game_eagle_add_bird(game, object, error))
        return false;
    game->shot.bird_count++;
    game->flying = (AbDataValue){.kind = AB_DATA_TABLE, .table = physical};
    level.world->definition.maximum_translation = 5;
    level.world->definition.maximum_translation_squared = 25;
    AbParticleAdmission *admission =
        ab_particles_admission(ab_classic_particles_engine(game->particles));
    admission->hard_limit = 250;
    admission->soft_limit = 0;
    admission->soft_multiplier = .2f;
    AbDataValue collider = ab_object_property(&definition, menu_span("collider"));
    if (collider.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Invalid Mighty Eagle collider");
    level.bodies->objects[object].physics.collider = collider;
    level.frame->objects[object].collider = (uint32_t)menu_real(collider.bits);
    AbClassicDynamicsView dynamics =
        ab_classic_dynamics_view(ab_classic_world_runtime_view(game->world).dynamics);
    return ab_classic_scene_set_scale(
               ab_classic_render_scene_objects(game->playfield.scene), object,
               (AbVec2){3, 3}, error) &&
           ab_classic_simulation_recreate_circle(dynamics.simulation, object, error) &&
           ab_classic_birds_sync(ab_classic_game_runtime_view(game->game).birds, error);
}

static bool game_eagle_timer_call(void *context, AbMightyEagleTimerCall call,
                                  AbError *error) {
    PreviewGame *game = context;
    switch (call.operation) {
        case AB_MIGHTY_EAGLE_CREATE:
            return game_eagle_spawn(game, call.position, error);
        case AB_MIGHTY_EAGLE_YELL:
            return game_eagle_sound(game, "mighty_eagle_yell", error);
        case AB_MIGHTY_EAGLE_FLY:
            return game_eagle_sound(game, "mighty_eagle_fly", error);
        case AB_MIGHTY_EAGLE_REMOVE_GOALS:
            return game_eagle_goals(game, true, false, error);
        case AB_MIGHTY_EAGLE_DESTROY_JOINTS:
            return game_eagle_joints(game, error);
    }
    return ab_fail(error, "Unknown Mighty Eagle timer operation");
}

static bool game_eagle_timer(PreviewGame *game, float seconds, AbError *error) {
    game->eagle_timer.camera_shake = menu_route_number(game->shake.amplitude);
    if (!ab_mighty_eagle_timer_update(&game->eagle_timer,
                                      &game->globals.specialty_available, seconds,
                                      game_eagle_timer_call, game, error))
        return false;
    game->shake.amplitude = menu_real(game->eagle_timer.camera_shake.bits);
    return true;
}

static bool game_eagle_exit(void *context, size_t object, float seconds,
                            AbError *error) {
    (void)seconds;
    PreviewGame *game = context;
    AbDataTable *physical = game_eagle_physical(game, object);
    if (game_eagle_true(physical, "isMightyBait")) {
        game->eagle_timer.bait_x = menu_route_field(physical, "x");
        game->eagle_timer.bait_y = menu_route_field(physical, "y");
        if (game->eagle_timer.timer.kind == AB_DATA_NIL) {
            game->eagle_timer.timer = menu_route_number(8.7f);
            game->eagle_timer.moving = game->enabled;
        }
        return true;
    }
    if (menu_route_truthy(menu_route_field(physical, "hitGround")))
        return true;
    game->shake.amplitude = 100;
    game->eagle_timer.camera_shake = menu_route_number(100);
    if (!game_eagle_sound(game, "mighty_eagle_thump", error) ||
        !game_eagle_goals(game, false, true, error) || !game_eagle_joints(game, error))
        return false;
    game->eagle_timer.timer = menu_route_number(4);
    return true;
}

static bool game_eagle_collision(void *context, size_t object, AbSpan first,
                                 AbSpan second, AbError *error) {
    (void)first;
    PreviewGame *game = context;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t other = 0;
    while (other < level.bodies->count &&
           !menu_route_equal((AbDataValue){.kind = AB_DATA_STRING,
                                           .string = level.bodies->objects[other].name},
                             (AbDataValue){.kind = AB_DATA_STRING, .string = second}))
        ++other;
    if (other == level.bodies->count)
        return ab_fail(error, "Missing Mighty Eagle contact object");
    AbDataTable *physical = game_eagle_physical(game, object);
    AbDataTable *target = game_eagle_physical(game, other);
    if (game_eagle_true(physical, "isMightyBait")) {
        if (game_eagle_true(target, "isMightyEagle")) {
            /* The source references a global obj1 here. The ordinary Eagle
             * collision already removes bait; no invented global is substituted. */
            return ab_fail(error, "Reached original bait collision global obj1");
        }
        if (!game_eagle_true(physical, "collision") &&
            game->eagle_timer.timer.kind == AB_DATA_NIL) {
            game->eagle_timer.timer = menu_route_number(8.7f);
            game->eagle_timer.moving = game->enabled;
        }
        return true;
    }
    if (game_eagle_true(target, "isMightyBait"))
        return game_eagle_dead(game, other, true, error);
    AbDataValue ground_object;
    if (!ab_object_world_lookup(&level.properties->lookup, game_text("ground"),
                                &ground_object, error) ||
        ground_object.kind != AB_DATA_TABLE)
        return ab_fail(error, "Mighty Eagle collision lost the canonical ground");
    float ground = game_eagle_value(ground_object.table, "y");
    if (!isfinite(ground))
        return ab_fail(error, "Invalid source ground height");
    bool ground_name = second.size == 6 && !memcmp(second.data, "ground", 6);
    return ground - game_eagle_value(physical, "y") < 0 && !ground_name
               ? true
               : game_eagle_ground(game, object, error);
}

static bool game_eagle_result(PreviewGame *game, float score, AbError *error) {
    PreviewEagle *eagle = game->eagle;
    const AbDataValue *threshold =
        ab_data_get(game->menu->star_limits.data.globals, game->level_name);
    AbDataValue maximum = threshold && threshold->kind == AB_DATA_TABLE
                              ? menu_route_field(threshold->table, "eagleScore")
                              : (AbDataValue){0};
    if (!eagle || maximum.kind != AB_DATA_NUMBER || menu_real(maximum.bits) <= 0)
        return ab_fail(error, "Missing actual Eagle percentage threshold");
    eagle->percent = fminf(floorf((score / menu_real(maximum.bits)) * 100), 100);
    if (!isfinite(eagle->percent) || eagle->percent < 0)
        return ab_fail(error, "Invalid Eagle percentage arithmetic");
    const AbDataValue *record = ab_data_get(
        ab_settings_statistics_root(game->menu->highscores), game->level_name);
    AbDataValue prior = record && record->kind == AB_DATA_TABLE
                            ? menu_route_field(record->table, "eagleScore")
                            : (AbDataValue){0};
    if (prior.kind != AB_DATA_NIL && prior.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Invalid previous Eagle percentage");
    eagle->previous_percent = prior.kind == AB_DATA_NUMBER ? menu_real(prior.bits) : 0;
    game->previous_score = menu_route_number(eagle->previous_percent);
    AbSpan path[] = {game->level_name.string, menu_span("eagleScore")};
    if (eagle->previous_percent < eagle->percent) {
        if (!ab_settings_statistics_write(game->menu->highscores, path, 2,
                                          menu_route_number(eagle->percent), error))
            return false;
        eagle->result_fields[0][0] = (AbDataEntry){
            game_text("id"), menu_event(game->menu, "EID_NEW_EAGLE_HIGHSCORE")};
        eagle->result_fields[0][1] =
            (AbDataEntry){game_text("level"), game->level_name};
        eagle->result_fields[0][2] = (AbDataEntry){
            game_text("oldHighscore"), menu_route_number(eagle->previous_percent)};
        eagle->result_fields[0][3] =
            (AbDataEntry){game_text("newHighscore"), menu_route_number(eagle->percent)};
        eagle->result_events[0] = (AbDataTable){eagle->result_fields[0], 4, 4, NULL};
        if (!ab_event_manager_notify(game->menu->events, eagle->result_events, error))
            return false;
        if (eagle->percent >= 100) {
            eagle->result_fields[1][0] = (AbDataEntry){
                game_text("id"), menu_event(game->menu, "EID_EAGLE_FEATHER_GAINED")};
            eagle->result_fields[1][1] =
                (AbDataEntry){game_text("level"), game->level_name};
            eagle->result_fields[1][2] = (AbDataEntry){
                game_text("episode"), game->menu->level_request.episode_id};
            eagle->result_events[1] =
                (AbDataTable){eagle->result_fields[1], 3, 4, NULL};
            if (!ab_event_manager_notify(game->menu->events, eagle->result_events + 1,
                                         error))
                return false;
        }
    }
    const char *const extrema[] = {"eagleScoreMax", "eagleScoreMin"};
    for (size_t index = 0; index < 2; ++index) {
        AbDataValue value = menu_route_field(record->table, extrema[index]);
        /* Preserve the target's comparison for both fields, including its
         * similarly ordered Min comparison; these store raw points. */
        if (value.kind == AB_DATA_NIL ||
            (value.kind == AB_DATA_NUMBER && menu_real(value.bits) < score)) {
            path[1] = menu_span(extrema[index]);
            if (!ab_settings_statistics_write(game->menu->highscores, path, 2,
                                              menu_route_number(score), error))
                return false;
        }
    }
    eagle->result_fields[2][0] = (AbDataEntry){
        game_text("id"), menu_event(game->menu, "EID_LEVEL_PLAYED_WITH_EAGLE")};
    eagle->result_fields[2][1] = (AbDataEntry){game_text("level"), game->level_name};
    eagle->result_fields[2][2] = (AbDataEntry){
        game_text("firstTime"), {.kind = AB_DATA_BOOLEAN, .bits = game->first_clear}};
    eagle->result_fields[2][3] = (AbDataEntry){
        game_text("cooldownDisabled"),
        menu_route_field(game->menu->level_request.episode, "disable_eagle_cooldown")};
    eagle->result_events[2] = (AbDataTable){eagle->result_fields[2], 4, 4, NULL};
    if (!ab_event_manager_notify(game->menu->events, eagle->result_events + 2, error))
        return false;
    game->save_pending = true;
    return true;
}

#endif
