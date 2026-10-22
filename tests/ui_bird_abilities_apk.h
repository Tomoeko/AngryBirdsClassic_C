#ifndef AB_UI_BIRD_ABILITIES_APK_TEST_H
#define AB_UI_BIRD_ABILITIES_APK_TEST_H

/* Resolve actual catalog metadata while selecting this fixture's yellow-bird
 * scene directly. This is a controlled module/integration test, not a captured
 * native playthrough or an unlock/progression claim for this level. */
static bool ability_test_level(PreviewMenu *menu, const char *wanted, AbError *error) {
    const AbDataTable *episodes = ab_episode_catalog_episodes(menu->catalog);
    AbDataValue episode = result_at(episodes, 1);
    AbDataValue pages = menu_route_field(episode.table, "pages");
    for (size_t page = 1;; ++page) {
        AbDataValue record = result_at(pages.table, page);
        if (record.kind == AB_DATA_NIL)
            return ab_fail(error, "Missing actual yellow-bird fixture level");
        AbDataValue levels = menu_route_field(record.table, "levels");
        for (size_t index = 1;; ++index) {
            AbDataValue level = result_at(levels.table, index);
            if (level.kind == AB_DATA_NIL)
                break;
            AbDataValue name = menu_route_field(level.table, "name");
            if (!menu_route_equal(name, menu_data(wanted)))
                continue;
            AbDataEntry entries[] = {
                {menu_data("episode"), menu_route_number(1)},
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

/* Miss every bird through actual mouse input. Reach loss through the ordinary
 * timers/controller/ending owners, rather than injecting a losing event. */
static bool ability_test_failure(PreviewMenu *menu, AbMetal *renderer, AbError *error) {
    if (!ability_test_level(menu, "Level1", error))
        return false;
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
                return false;
            if (++phase == 3) {
                phase = 0;
                ++fired;
            }
        }
        if (!menu_preview_draw(menu, 1.0f / 60, error) ||
            !ab_audio_mixer_mix(menu->mixer, 267, pcm, 512, error))
            return false;
    }
    if (!game->results || !game->results->failed || game->result_complete ||
        game->results->button_count != 2 || fired != 3)
        return ab_fail(error,
                       "Missed shots did not reach the actual failed card "
                       "(shots=%zu, ended=%d)",
                       fired, game->ended);
    for (size_t frame = 0; frame < 45; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    AbImage image = {0};
    float clear[4] = {0, 0, 0, 1};
    bool okay = ab_metal_capture(renderer, menu->context.draw_list, 1024, 768, clear,
                                 16 * 1024 * 1024, &image, error);
    const char *capture = getenv("AB_FAILURE_CAPTURE");
    if (okay && capture)
        okay = save(capture, &image, error);
    ab_image_free(&image);
    if (!okay || !result_test_click(menu, 1, error) || !menu->game ||
        menu->game->ended || menu->game->shot.birds_shot != 0 ||
        !game_session_escape(menu, error))
        return ab_fail(error, "Failed-card Retry did not create a fresh game");
    for (size_t frame = 0; frame < 20; ++frame)
        if (!menu_preview_draw(menu, 1.0f / 60, error))
            return false;
    AbVec2 point;
    AbUiPointerReply reply;
    if (!game_test_center(&menu->game->pause->page.controls[AB_UI_PAUSE_MENU], &point,
                          error) ||
        !menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error) ||
        !menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error) ||
        !menu_preview_draw(menu, 1.0f / 60, error))
        return false;
    if (menu->game || !menu->level_active)
        return ab_fail(error, "Pause Menu did not return to the retained level grid");
    puts("Natural level loss: three missed mouse shots, failed Metal card, Retry, "
         "Escape pause/resume, frozen world and pause Menu navigation.");
    return true;
}

#endif
