#ifndef AB_UI_BIRD_ABILITIES_APK_TEST_H
#define AB_UI_BIRD_ABILITIES_APK_TEST_H

/* Resolve actual catalog metadata while selecting these bird-ability
 * scenes directly. This is a controlled module/integration test, not a captured
 * native playthrough or an unlock/progression claim for this level. */
static bool ability_test_episode_level(PreviewMenu *menu, size_t episode_id,
                                       const char *wanted, AbError *error) {
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    AbDataValue episode = result_at(episodes, episode_id);
    AbDataValue pages = menu_route_field(episode.table, "pages");
    for (size_t page = 1;; ++page) {
        AbDataValue record = result_at(pages.table, page);
        if (record.kind == AB_DATA_NIL)
            return ab_fail(error, "Missing actual bird-ability fixture level");
        AbDataValue levels = menu_route_field(record.table, "levels");
        for (size_t index = 1;; ++index) {
            AbDataValue level = result_at(levels.table, index);
            if (level.kind == AB_DATA_NIL)
                break;
            AbDataValue name = menu_route_field(level.table, "name");
            if (!menu_route_equal(name, menu_data(wanted)))
                continue;
            AbDataEntry entries[] = {
                {menu_data("episode"), menu_route_number((float)episode_id)},
                {menu_data("page"), menu_route_number((float)page)},
                {menu_data("level"), menu_route_number((float)index)},
                {menu_data("levelName"), name}};
            AbDataTable metadata = {entries, 4, 4, NULL};
            if (!ab_classic_level_request_resolve(episodes, &metadata,
                                                  &menu->level_request, error))
                return false;
            menu->level_request_ready = true;
            return menu_preview_draw(menu, 0, error) && menu->game;
        }
    }
}

static bool ability_test_level(PreviewMenu *menu, const char *wanted, AbError *error) {
    return ability_test_episode_level(menu, 1, wanted, error);
}

static bool ability_test_pause(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t body = SIZE_MAX;
    for (size_t index = 0; index < level.bodies->count; ++index) {
        size_t candidate = level.bodies->objects[index].physics.body;
        if (level.world->storage.bodies[candidate].motion.mass.type ==
            AB_BODY_DYNAMIC) {
            body = candidate;
            break;
        }
    }
    if (body == SIZE_MAX ||
        !ab_body_velocity_set(&level.world->storage.bodies[body].motion,
                              (AbVec2){7, 0}))
        return ab_fail(error, "Pause fixture has no moving dynamic body");
    AbVec2 position = level.world->storage.bodies[body].motion.mass.transform.position;
    size_t textures = ab_asset_textures_count(menu->textures);
    if (!game_session_escape(menu, error) || !game->paused || game->physics)
        return ab_fail(error, "Escape did not pause the active game");
    for (size_t frame = 0; frame < 30; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    AbVec2 frozen = level.world->storage.bodies[body].motion.mass.transform.position;
    if (position.x != frozen.x || position.y != frozen.y ||
        game->pause->page.extend_state != 1 || !game->paused)
        return ab_fail(error, "Pause page failed to extend over a frozen world");
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    const char *capture = getenv("AB_PAUSE_CAPTURE");
    if (okay && capture)
        okay = save(capture, &image, error);
    ab_image_free(&image);
    if (!okay || !menu_preview_resize(menu, (AbVec2){1280, 720}, error) ||
        !menu_preview_resize(menu, (AbVec2){1024, 768}, error) ||
        ab_asset_textures_count(menu->textures) != textures ||
        !game_session_escape(menu, error) || game->paused || !game->physics)
        return ab_fail(error, "Escape/resize did not retain a resumable game");
    for (size_t frame = 0; frame < 30; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    return true;
}

static bool ability_test_yellow(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_level(menu, "Level16", error) ||
        !ability_test_pause(menu, renderer, error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t object = 0;
    while (
        object < level.bodies->count &&
        !menu_route_equal(
            menu_route_field(&level.properties->objects[object].physical, "definition"),
            menu_data("YellowBird")))
        ++object;
    if (object == level.bodies->count)
        return ab_fail(error, "Selected APK scene has no actual yellow bird");
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbDataValue definition;
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     level.bodies->objects[object].name, &definition,
                                     error) ||
        definition.kind != AB_DATA_TABLE)
        return false;
    size_t body_index = level.bodies->objects[object].physics.body;
    if (!ab_world_set_body_type(level.world, body_index, AB_BODY_DYNAMIC, error))
        return false;
    AbBodyMotion *body = &level.world->storage.bodies[body_index].motion;
    if (!ab_body_velocity_set(body, (AbVec2){10, 20}) ||
        !ab_level_property_set(level.properties, object, menu_span("xVel"),
                               menu_route_number(3), error) ||
        !ab_level_property_set(level.properties, object, menu_span("yVel"),
                               menu_route_number(4), error))
        return false;
    float mass = menu_real(menu_route_field(physical, "mass").bits);
    float impulse_mass = mass * body->mass.inverse_mass;
    size_t puffs = ab_classic_trajectories_view(game->playfield.trajectories).points;
    uint32_t audio = ab_audio_mixer_view(menu->mixer).next_id;
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    game->flying = (AbDataValue){.kind = AB_DATA_TABLE, .table = physical};
    if (!ab_classic_bird_specialty_trigger_flying(runtime.specialty, 0, error))
        return false;
    AbDataValue sprite = menu_route_field(physical, "sprite");
    if (fabsf(body->mass.linear_velocity.x - (10 + 27.75f * impulse_mass)) > 0.0001f ||
        fabsf(body->mass.linear_velocity.y - (20 + 37 * impulse_mass)) > 0.0001f ||
        !menu_route_equal(sprite,
                          menu_route_field(definition.table, "spriteSpecial")) ||
        !menu_route_truthy(menu_route_field(physical, "specialtyTriggered")) ||
        menu_route_truthy(game->globals.specialty_available) ||
        !menu_route_equal(game->globals.bird_specialty, menu_data("BOOST")) ||
        ab_classic_trajectories_view(game->playfield.trajectories).points !=
            puffs + 1 ||
        ab_audio_mixer_view(menu->mixer).next_id != audio + 1)
        return ab_fail(error, "Yellow boost lost its canonical impulse/effects/state");
    /* Draw through the real game renderer without advancing the controlled
     * queued-bird fixture's enclosing input/controller state. */
    menu->context.draw_list->count = 0;
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    bool okay =
        game_service(game, &(AbGameTailCall){.operation = AB_GAME_TAIL_DRAW}, error) &&
        ab_metal_capture(renderer, menu->context.draw_list, (uint32_t)menu->screen.x,
                         (uint32_t)menu->screen.y, clear, 16 * 1024 * 1024, &image,
                         error);
    ab_image_free(&image);
    game_session_free(menu);
    menu->level_request_ready = false;
    if (okay)
        puts("Actual yellow bird: cached-velocity boost, native body impulse, "
             "decoded effects/audio, specialty sprite and Metal draw.");
    return okay;
}

typedef enum {
    ABILITY_MOUSE_BOOMERANG,
    ABILITY_MOUSE_BOMB,
    ABILITY_MOUSE_BOMB_FUSE,
    ABILITY_MOUSE_BLUES,
} AbilityMouseMode;

static bool ability_test_flying_mouse(PreviewMenu *menu, AbMetal *renderer,
                                      AbilityMouseMode mode, AbError *error) {
    bool blues = mode == ABILITY_MOUSE_BLUES;
    bool bomb = mode == ABILITY_MOUSE_BOMB || mode == ABILITY_MOUSE_BOMB_FUSE;
    bool automatic = mode == ABILITY_MOUSE_BOMB_FUSE;
    if (!ability_test_episode_level(menu, bomb || blues ? 1 : 3,
                                    blues  ? "Level8"
                                    : bomb ? "Level88"
                                           : "LevelP3_224",
                                    error))
        return false;
    PreviewGame *game = menu->game;
    size_t phase = 0, after_launch = 0;
    bool activated = false, saw_fuse = false;
    size_t initial_count = ab_classic_level_view(game->playfield.level).bodies->count;
    AbDataTable *flying = NULL;
    float outgoing = 0;
    int16_t pcm[512];
    for (size_t frame = 0; frame < 700; ++frame) {
        AbUiPointerReply reply;
        if (frame > 240 && game->shot.bird_ready && phase < 3 &&
            ab_classic_camera_view(game->playfield.camera).system.input_enabled) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase) {
                target.x -= 4;
                target.y += automatic ? -1.5f : 1.5f;
            }
            AbCameraViewport viewport;
            AbVec2 point;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu,
                                      menu_span(phase == 0   ? "LPRESS"
                                                : phase == 1 ? "LHOLD"
                                                             : "LRELEASE"),
                                      point, &reply, error))
                return false;
            ++phase;
        } else if (phase == 3) {
            ++after_launch;
            if (!automatic && after_launch == 15) {
                if (game->flying.kind != AB_DATA_TABLE)
                    return ab_fail(error, "Mouse launch lost the actual flying bird");
                flying = game->flying.table;
                outgoing = menu_real(menu_route_field(flying, "xVel").bits);
                if (!menu_preview_pointer(menu, menu_span("LPRESS"), (AbVec2){700, 250},
                                          &reply, error))
                    return false;
            } else if (!automatic && after_launch == 16 &&
                       !menu_preview_pointer(menu, menu_span("LRELEASE"),
                                             (AbVec2){700, 250}, &reply, error)) {
                return false;
            }
        }
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
        if (automatic) {
            if (!flying && phase == 3 && game->flying.kind == AB_DATA_TABLE)
                flying = game->flying.table;
            if (flying && menu_route_field(flying, "bombTimer").kind == AB_DATA_NUMBER)
                saw_fuse = true;
            activated = saw_fuse && game->flying.kind == AB_DATA_NIL &&
                        menu_real(menu_route_field(flying, "bombTimer").bits) < 0 &&
                        menu_route_truthy(menu_route_field(flying, "collision"));
        }
        if (!automatic && after_launch == 16) {
            if (blues) {
                AbClassicGameRuntimeView runtime =
                    ab_classic_game_runtime_view(game->game);
                activated = runtime.blues && runtime.blues->has_result &&
                            game->flying.kind == AB_DATA_TABLE;
                AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
                if (activated && level.bodies->count == initial_count + 3) {
                    for (size_t child = 0; child < 3; ++child) {
                        size_t object = runtime.blues->last_result.objects[child];
                        if (object != initial_count + child ||
                            level.removed_objects[object])
                            activated = false;
                    }
                } else
                    activated = false;
            } else
                activated =
                    flying && (bomb ? (game->flying.kind == AB_DATA_NIL &&
                                       menu_route_equal(game->globals.bird_specialty,
                                                        menu_data("BOMB")))
                                    : menu_route_truthy(
                                          menu_route_field(flying, "boomerangActive")));
            if (!activated || !(outgoing > 0))
                return ab_fail(error, "Actual mouse input did not activate the bird");
        }
        if ((!automatic && after_launch == 60) || (automatic && activated)) {
            float returned = menu_real(menu_route_field(flying, "xVel").bits);
            if (!activated || (!bomb && !blues && !(returned < 0)))
                return ab_fail(error, "Green force queue failed to reverse flight");
            AbImage image = {0};
            float clear[4];
            menu_background_clear(menu, clear);
            bool okay = ab_metal_capture(
                renderer, menu->context.draw_list, (uint32_t)menu->screen.x,
                (uint32_t)menu->screen.y, clear, 16 * 1024 * 1024, &image, error);
            const char *capture = getenv(blues       ? "AB_BLUES_CAPTURE"
                                         : automatic ? "AB_BLACK_FUSE_CAPTURE"
                                         : bomb      ? "AB_BLACK_CAPTURE"
                                                     : "AB_BOOMERANG_CAPTURE");
            if (okay && capture)
                okay = save(capture, &image, error);
            ab_image_free(&image);
            game_session_free(menu);
            menu->level_request_ready = false;
            return okay;
        }
    }
    return ab_fail(error, "Mouse fixture did not reach launch and specialty");
}

static bool ability_test_green(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_episode_level(menu, 3, "LevelP3_224", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t object = 0;
    while (
        object < level.bodies->count &&
        !menu_route_equal(
            menu_route_field(&level.properties->objects[object].physical, "definition"),
            menu_data("BoomerangBird")))
        ++object;
    if (object == level.bodies->count || !level.bodies->load_edges_present)
        return ab_fail(error, "Actual green-bird level/edges are absent");
    AbDataTable *physical = &level.properties->objects[object].physical;
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    AbClassicBoomerang *boomerang = runtime.boomerang;
    size_t body_index = level.bodies->objects[object].physics.body;
    if (!ab_world_set_body_type(level.world, body_index, AB_BODY_DYNAMIC, error))
        return false;
    AbBodyMotion *body = &level.world->storage.bodies[body_index].motion;
    game->flying = (AbDataValue){.kind = AB_DATA_TABLE, .table = physical};
    game->globals.specialty_available = game->enabled;
    const char *const names[] = {"shot", "xVel", "yVel", "angle", "angularVelocity"};
    AbDataValue values[] = {game->enabled, menu_route_number(10), menu_route_number(8),
                            menu_route_number(-2), (AbDataValue){0}};
    for (size_t index = 0; index < 5; ++index)
        if (!ab_level_property_set(level.properties, object, menu_span(names[index]),
                                   values[index], error))
            return false;
    if (!ab_force_queue_clear(boomerang->forces, error) ||
        !ab_classic_boomerang_update(boomerang, 0.1f, error))
        return false;
    float mass = menu_real(menu_route_field(physical, "mass").bits);
    if (boomerang->forces->count != 1 || boomerang->forces->entries[0].force.x != 0 ||
        boomerang->forces->entries[0].force.y != -2.8f * mass ||
        body->mass.angular_velocity != 1.5f)
        return ab_fail(error, "Green flight lost source queued lift/spin");
    if (!ab_level_properties_publish_launch(level.properties, object, (AbVec2){10, 8},
                                            error))
        return false;
    size_t puffs = ab_classic_trajectories_view(game->playfield.trajectories).points;
    uint32_t audio = ab_audio_mixer_view(menu->mixer).next_id;
    if (!ab_classic_bird_specialty_trigger_flying(runtime.specialty, 0, error))
        return false;
    AbDataValue sprite = menu_route_field(physical, "sprite");
    AbDataValue definition = {0};
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     level.bodies->objects[object].name, &definition,
                                     error) ||
        definition.kind != AB_DATA_TABLE)
        return false;
    if (!menu_route_truthy(menu_route_field(physical, "boomerangActive")) ||
        menu_route_truthy(game->globals.specialty_available) ||
        !menu_route_equal(game->globals.bird_specialty, menu_data("BOOMERANG")) ||
        !menu_route_equal(sprite,
                          menu_route_field(definition.table, "spriteSpecial")) ||
        ab_classic_trajectories_view(game->playfield.trajectories).points !=
            puffs + 1 ||
        ab_audio_mixer_view(menu->mixer).next_id != audio + 1)
        return ab_fail(error, "Green activation lost its source state/effects");
    float expected_x = ((10 * 0.05f) * mass) * -50;
    float expected_y = (((2 - fabsf(8.0f / 10)) * 0.05f) * mass) * -350;
    if (menu_real(menu_route_field(physical, "boomerangXForce").bits) != expected_x ||
        menu_real(menu_route_field(physical, "boomerangYForce").bits) != expected_y ||
        !ab_force_queue_clear(boomerang->forces, error) ||
        !ab_classic_boomerang_update(boomerang, 0.1f, error))
        return ab_fail(error, "Green turning force initialization diverged");
    if (boomerang->forces->count != 2 ||
        boomerang->forces->entries[0].force.x != expected_x ||
        boomerang->forces->entries[1].force.y != expected_y ||
        body->mass.angular_velocity != 3.5f)
        return ab_fail(error, "Green turn lost queued force order or angular speed");
    /* Source sound uses cached angle and the immutable load edges. */
    if (!ab_level_property_set(level.properties, object, menu_span("x"),
                               menu_route_number(level.bodies->load_edges.x - 50),
                               error) ||
        !ab_level_property_set(level.properties, object, menu_span("angle"),
                               menu_route_number(-1), error) ||
        !ab_classic_boomerang_update(boomerang, 2, error))
        return false;
    if (menu_real(boomerang->volume.bits) != 0.5f ||
        ab_audio_mixer_view(menu->mixer).next_id != audio + 2 ||
        body->mass.angular_velocity != 20)
        return ab_fail(error, "Green swish/attenuation/spin cap diverged");
    menu->context.draw_list->count = 0;
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    bool okay =
        game_service(game, &(AbGameTailCall){.operation = AB_GAME_TAIL_DRAW}, error) &&
        ab_metal_capture(renderer, menu->context.draw_list, (uint32_t)menu->screen.x,
                         (uint32_t)menu->screen.y, clear, 16 * 1024 * 1024, &image,
                         error);
    ab_image_free(&image);
    game_session_free(menu);
    menu->level_request_ready = false;
    if (okay)
        okay =
            ability_test_flying_mouse(menu, renderer, ABILITY_MOUSE_BOOMERANG, error);
    if (okay)
        puts("Actual green bird: flight lift, queued turn forces, spin limits, "
             "specialty art/audio/puff, edge-attenuated swish and Metal draw; "
             "real mouse launch/activation reverses the simulated flight.");
    return okay;
}

static bool ability_test_black(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    for (size_t timed = 0; timed < 2; ++timed) {
        if (!ability_test_level(menu, "Level88", error))
            return false;
        PreviewGame *game = menu->game;
        AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
        size_t object = 0;
        while (object < level.bodies->count &&
               !menu_route_equal(
                   menu_route_field(&level.properties->objects[object].physical,
                                    "definition"),
                   menu_data("BlackBird")))
            ++object;
        if (object == level.bodies->count || !level.goal_count ||
            level.joints->joint_count || level.joints->glue_count)
            return ab_fail(error, "Missing ordinary APK black-bird blast fixture");
        size_t pig = level.goal_order[0];
        const AbDataTable *target = &level.properties->objects[pig].physical;
        size_t target_body = level.bodies->objects[pig].physics.body;
        float old_x =
            level.world->storage.bodies[target_body].motion.mass.linear_velocity.x;
        AbVec2 position = {menu_real(menu_route_field(target, "x").bits) - 1,
                           menu_real(menu_route_field(target, "y").bits)};
        if (!ab_classic_shots_set_position(game->playfield.shots, object, position,
                                           error))
            return false;
        AbDataTable *physical = &level.properties->objects[object].physical;
        game->flying = (AbDataValue){.kind = AB_DATA_TABLE, .table = physical};
        game->globals.specialty_available = game->enabled;
        AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
        AbDataValue definition = {0};
        if (!ab_object_definition_lookup(&level.properties->lookup,
                                         level.bodies->objects[object].name,
                                         &definition, error) ||
            definition.kind != AB_DATA_TABLE)
            return false;
        if (!timed) {
            if (!ab_classic_bird_specialty_trigger_flying(runtime.specialty, 0, error))
                return false;
        } else {
            if (!ab_level_property_set(level.properties, object, menu_span("bombTimer"),
                                       menu_route_number(1), error))
                return false;
            bool alive = false;
            const float durations[] = {0, 0.25f, 0.5f, 0.25f, 0.25f};
            for (size_t phase = 0; phase < 5; ++phase) {
                if (!ab_classic_birds_update(runtime.birds, object, durations[phase],
                                             &alive, error))
                    return false;
                if (alive != (phase != 4))
                    return ab_fail(error, "Timed bomb lost strict timer boundaries");
                if (phase == 1 || phase == 2) {
                    AbDataValue expected = menu_route_field(
                        definition.table,
                        phase == 1 ? "spriteSpecial2" : "spriteSpecial3");
                    if (!menu_route_equal(menu_route_field(physical, "damageSprite"),
                                          expected) ||
                        !menu_route_equal(menu_route_field(physical, "sprite"),
                                          expected))
                        return ab_fail(error, "Timed bomb lost its fuse sprite stage");
                }
            }
        }
        uintptr_t dead = 0;
        bool present = false;
        AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
        if (!ab_string_map_get(world.dead_blocks, level.bodies->objects[pig].name,
                               &dead, &present, error))
            return false;
        if (!level.removed_objects[object] || !present || !dead ||
            !(menu_real(menu_route_field(target, "strength").bits) <= 0) ||
            game->shake.duration != 0.15f || game->shake.amount != 40 ||
            !(old_x <
              level.world->storage.bodies[target_body].motion.mass.linear_velocity.x) ||
            !ab_particles_count(ab_classic_particles_engine(game->particles), false))
            return ab_fail(error, "Black explosion lost damage/death/removal/shake");
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
        AbImage image = {0};
        float clear[4];
        menu_background_clear(menu, clear);
        bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                     (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                     clear, 16 * 1024 * 1024, &image, error);
        ab_image_free(&image);
        if (!okay)
            return false;
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    if (!ability_test_flying_mouse(menu, renderer, ABILITY_MOUSE_BOMB, error) ||
        !ability_test_flying_mouse(menu, renderer, ABILITY_MOUSE_BOMB_FUSE, error))
        return false;
    puts("Actual black bird: mouse, manual and collision-triggered timed detonation, "
         "fuse sprite stages, "
         "blast impulse/damage, pig death, parent removal and Metal effects.");
    return true;
}

/* Miss every bird through actual mouse input. Reach loss through the ordinary
 * timers/controller/ending owners, rather than injecting a losing event. */
static bool ability_test_failure(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_level(menu, "Level1", error)) {
        if (!error->message[0])
            ab_fail(error, "Cannot open the natural-loss fixture level");
        return false;
    }
    const char *stage = "missed-shot update";
    PreviewGame *game = menu->game;
    size_t phase = 0, fired = 0;
    int16_t pcm[512];
    for (size_t frame = 0; frame < 5000 && !game->results; ++frame) {
        if (!game->ended && frame > 240 && game->shot.bird_ready &&
            ab_classic_camera_view(game->playfield.camera).system.input_enabled) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase)
                target.x += 4;
            AbCameraViewport viewport;
            AbVec2 point;
            AbUiPointerReply reply;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu,
                                      menu_span(phase == 0   ? "LPRESS"
                                                : phase == 1 ? "LHOLD"
                                                             : "LRELEASE"),
                                      point, &reply, error))
                goto failed;
            if (++phase == 3) {
                phase = 0;
                ++fired;
            }
        }
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            goto failed;
    }
    if (!game->results || !game->results->failed || game->result_complete ||
        game->results->button_count != 2 || fired != 3)
        return ab_fail(error,
                       "Missed shots did not reach the actual failed card "
                       "(shots=%zu, ended=%d)",
                       fired, game->ended);
    for (size_t frame = 0; frame < 45; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            goto failed;
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768, clear,
                                 16 * 1024 * 1024, &image, error);
    const char *capture = getenv("AB_FAILURE_CAPTURE");
    if (okay && capture)
        okay = save(capture, &image, error);
    ab_image_free(&image);
    stage = "failed-card Retry";
    if (!okay || !result_test_click(menu, 1, error) || !menu->game ||
        menu->game->ended || menu->game->shot.birds_shot != 0 ||
        !game_session_escape(menu, error))
        return ab_fail(error, "Failed-card Retry did not create a fresh game");
    stage = "paused retry update";
    for (size_t frame = 0; frame < 20; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            goto failed;
    stage = "pause Menu click and return";
    AbVec2 point;
    AbUiPointerReply reply;
    if (!game_test_center(&menu->game->pause->page.controls[AB_UI_PAUSE_MENU], &point,
                          error) ||
        !menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error) ||
        !menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) ||
        !menu_preview_draw(menu, 1.0f / 60, error))
        goto failed;
    if (menu->game || !menu->level_active)
        return ab_fail(error, "Pause Menu did not return to the retained level grid");
    puts("Natural level loss: three missed mouse shots, failed Metal card, Retry, "
         "Escape pause/resume, frozen world and pause Menu navigation.");
    return true;
failed:
    if (!error->message[0])
        ab_fail(error, "Natural-loss fixture failed at %s", stage);
    return false;
}

static bool ability_test_white(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_level(menu, "Level26", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    AbBirdCollisionGrenades *grenades =
        ab_classic_world_runtime_view(game->world).grenades;
    size_t initial = level.bodies->count, phase = 0, flight = 0;
    size_t egg = SIZE_MAX;
    float before_y = 0;
    bool dropped = false, exploded = false;
    int16_t pcm[512];
    for (size_t frame = 0; frame < 1400; ++frame) {
        AbUiPointerReply reply;
        if (frame > 240 && game->shot.bird_ready && phase < 3 &&
            ab_classic_camera_view(game->playfield.camera).system.input_enabled) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase) {
                target.x -= 4;
                target.y += 1.5f;
            }
            AbCameraViewport viewport;
            AbVec2 point;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu,
                                      menu_span(phase == 0   ? "LPRESS"
                                                : phase == 1 ? "LHOLD"
                                                             : "LRELEASE"),
                                      point, &reply, error))
                return false;
            ++phase;
        } else if (phase == 3) {
            ++flight;
            if (!exploded && flight == 15) {
                if (game->flying.kind != AB_DATA_TABLE ||
                    !menu_route_equal(
                        menu_route_field(game->flying.table, "definition"),
                        menu_data("BasicBird2")))
                    return ab_fail(error, "White bird fixture did not launch Matilda");
                before_y = menu_real(menu_route_field(game->flying.table, "yVel").bits);
            }
            if (!exploded && (flight == 15 || flight == 16) &&
                !menu_preview_pointer(menu,
                                      menu_span(flight == 15 ? "LPRESS" : "LRELEASE"),
                                      (AbVec2){700, 250}, &reply, error))
                return false;
        }
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
        if (!dropped && phase == 3 && flight == 16) {
            egg = initial;
            AbDataTable *physical = &level.properties->objects[egg].physical;
            if (!runtime.eggs || level.bodies->count != initial + 1 ||
                grenades->count != 1 || game->flying.kind != AB_DATA_TABLE ||
                !menu_route_equal(menu_route_field(physical, "definition"),
                                  menu_data("EggGranade")) ||
                !menu_route_truthy(menu_route_field(physical, "isEggGrenade")) ||
                level.frame->objects[egg].collider != 3 || game->shot.birds_shot != 1 ||
                !(menu_real(menu_route_field(game->flying.table, "yVel").bits) <
                  before_y - 10))
                return ab_fail(error, "White bird lost its egg/rebound/source list");
            dropped = true;
            AbImage image = {0};
            float clear[4];
            menu_background_clear(menu, clear);
            bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768,
                                         clear, 16 * 1024 * 1024, &image, error);
            const char *capture = getenv("AB_WHITE_EGG_CAPTURE");
            if (okay && capture)
                okay = save(capture, &image, error);
            ab_image_free(&image);
            if (!okay)
                return false;
        }
        if (dropped && !exploded && level.removed_objects[egg]) {
            const AbDataValue *world =
                ab_data_get(&level.properties->world,
                            (AbDataValue){.kind = AB_DATA_STRING,
                                          .string = level.bodies->objects[egg].name});
            if (grenades->count || !world || world->kind != AB_DATA_NIL)
                return ab_fail(error,
                               "Exploded egg remained in its source projectile list");
            AbParticles *particles = ab_classic_particles_engine(game->particles);
            bool shells = false;
            for (size_t index = 0; index < ab_particles_count(particles, false);
                 ++index)
                shells |= menu_route_equal(
                    (AbDataValue){
                        .kind = AB_DATA_STRING,
                        .string = ab_particles_at(particles, false, index)->definition},
                    menu_data("eggShells"));
            if (!shells)
                return ab_fail(error,
                               "White bird egg did not emit its explosion shells");
            exploded = true;
            flight = 0;
        }
        if (exploded && flight == 180) {
            game_session_free(menu);
            menu->level_request_ready = false;
            puts("Actual white bird: mouse egg drop, parent rebound, projectile "
                 "collision, "
                 "shells/explosion/audio/removal and 180 subsequent Metal frames.");
            return true;
        }
    }
    return ab_fail(error,
                   "White bird fixture did not complete its actual egg collision");
}

static bool ability_test_repeated_blues(PreviewMenu *menu, AbMetal *renderer,
                                        AbError *error) {
    if (!ability_test_level(menu, "Level8", error))
        return false;
    PreviewGame *game = menu->game;
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t initial = level.bodies->count, splits = 0, phase = 0, flight = 0;
    size_t wanted = level.bird_count;
    size_t pan_step = 0;
    const char *previous = NULL;
    AbDataTable *first_other = NULL;
    AbDataValue first_names[2] = {0};
    int16_t pcm[512];
    for (size_t frame = 0; frame < 4800; ++frame) {
        AbUiPointerReply reply;
        bool camera_ready =
            ab_classic_camera_view(game->playfield.camera).system.input_enabled;
        bool sling_visible;
        if (!ab_classic_sling_is_visible(game->playfield.sling, &sling_visible, error))
            return false;
        if (pan_step ||
            (splits > 0 && splits < wanted && phase == 0 && !sling_visible &&
             game->shot.bird_ready && camera_ready && frame % 30 == 0)) {
            AbVec2 point = {pan_step ? 800 : 200, 150};
            if (!menu_preview_pointer(menu,
                                      menu_span(pan_step == 0   ? "LPRESS"
                                                : pan_step == 3 ? "LRELEASE"
                                                                : "LHOLD"),
                                      point, &reply, error))
                return false;
            pan_step = (pan_step + 1) % 4;
        } else if (splits < wanted && frame > 240 && game->shot.bird_ready &&
                   phase < 3 && camera_ready && sling_visible &&
                   game->shot.current_bird_name &&
                   (!previous || strcmp(previous, game->shot.current_bird_name))) {
            AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
            if (phase) {
                target.x -= 4;
                target.y += 3;
            }
            AbCameraViewport viewport;
            AbVec2 point;
            if (!ab_camera_system_query_viewport(
                    ab_classic_camera_system(game->playfield.camera), &viewport) ||
                !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS,
                                     AB_SPACE_SCREEN, target, &point) ||
                !menu_preview_pointer(menu,
                                      menu_span(phase == 0   ? "LPRESS"
                                                : phase == 1 ? "LHOLD"
                                                             : "LRELEASE"),
                                      point, &reply, error))
                return false;
            if (++phase == 3) {
                previous = game->shot.current_bird_name;
                flight = 0;
            }
        } else if (phase == 3) {
            ++flight;
            if ((flight == 15 || flight == 16) &&
                !menu_preview_pointer(menu,
                                      menu_span(flight == 15 ? "LPRESS" : "LRELEASE"),
                                      (AbVec2){700, 250}, &reply, error))
                return false;
        }
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
        if (phase == 3 && flight == 16) {
            ++splits;
            if (!runtime.blues->has_result ||
                level.bodies->count != initial + 3 * splits ||
                game->shot.birds_shot != splits)
                return ab_fail(
                    error,
                    "Repeated Blues mouse split %zu failed: bodies %zu/%zu, "
                    "shots %g, specialty %u, flying kind %u, parent %s",
                    splits, level.bodies->count, initial + 3 * splits,
                    game->shot.birds_shot,
                    (unsigned)menu_route_truthy(game->globals.specialty_available),
                    (unsigned)game->flying.kind, previous);
            if (splits == 1) {
                first_other = ab_classic_birds_view(runtime.birds).other_birds;
                first_names[0] = first_other->entries[0].value;
                first_names[1] = first_other->entries[1].value;
            } else if (ab_classic_birds_view(runtime.birds).other_birds ==
                           first_other ||
                       !menu_route_equal(first_other->entries[0].value,
                                         first_names[0]) ||
                       !menu_route_equal(first_other->entries[1].value,
                                         first_names[1])) {
                return ab_fail(error, "Second Blues split damaged retained aliases");
            }
        }
        if (phase == 3 && flight == 60 && splits < wanted)
            phase = 0;
        if (splits == wanted && flight == 240) {
            AbImage image = {0};
            float clear[4];
            menu_background_clear(menu, clear);
            bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768,
                                         clear, 16 * 1024 * 1024, &image, error);
            const char *capture = getenv("AB_BLUES_REPEAT_CAPTURE");
            if (okay && capture)
                okay = save(capture, &image, error);
            ab_image_free(&image);
            game_session_free(menu);
            menu->level_request_ready = false;
            if (okay)
                printf("Repeated Blues: %zu real queued mouse launches and splits, "
                       "%zu children, retained tables and 240 subsequent frames on "
                       "Metal.\n",
                       splits, 3 * splits);
            return okay;
        }
    }
    return ab_fail(error, "Repeated Blues reached %zu/%zu mouse splits", splits,
                   wanted);
}

#endif
