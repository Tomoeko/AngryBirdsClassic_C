#ifndef AB_UI_PHYSICS_REPLAY_APK_H
#define AB_UI_PHYSICS_REPLAY_APK_H

typedef struct {
    size_t phase, frames, frames_since_shot, ability_attempts;
    float previous_shots;
    AbVec2 pan_start, pan_target;
    bool released_shot;
} PhysicsReplayInput;

static bool physics_replay_input(PreviewMenu *menu, PhysicsReplayInput *input,
                                 AbError *error) {
    PreviewGame *game = menu->game;
    if (game->ended || game->paused || menu->reward)
        return true;
    if (++input->frames < 360)
        return true;
    if (input->released_shot) {
        if (!(game->shot.birds_shot > input->previous_shots))
            return ab_fail(error, "Physics replay mouse release did not launch a bird");
        input->released_shot = false;
        input->frames_since_shot = 0;
    }
    ++input->frames_since_shot;
    const char *event = NULL;
    AbVec2 point = {menu->screen.x * .65f, menu->screen.y * .45f};
    if (input->phase >= 5) {
        point = input->phase == 5 ? input->pan_start : input->pan_target;
        event = input->phase == 5 ? "LPRESS" : input->phase == 6 ? "LHOLD" : "LRELEASE";
        input->phase = input->phase == 7 ? 0 : input->phase + 1;
        AbUiPointerReply reply;
        return menu_preview_pointer(menu, menu_span(event), point, &reply, error);
    }
    if (!input->phase && game->shot.bird_ready)
        input->phase = 1;
    /* A ready queue can still be moving its camera/bird into place. Continue
     * only after the real press selected a dragged bird; release a missed press
     * and retry through ordinary input rather than manufacturing a launch. */
    if ((input->phase == 2 || input->phase == 3) && !game->shot.dragged) {
        input->phase = 0;
        event = "LRELEASE";
    } else if (input->phase >= 1 && input->phase <= 3) {
        AbVec2 target = ab_classic_sling_view(game->playfield.sling).band.rest;
        if (input->phase > 1) {
            target.x -= 4;
            target.y += (int)game->shot.birds_shot % 3 == 0 ? 1.5f : 3;
        }
        AbCameraViewport viewport;
        if (!ab_camera_system_query_viewport(
                ab_classic_camera_system(game->playfield.camera), &viewport) ||
            !ab_coordinate_point(&viewport.rendering, AB_SPACE_PHYSICS, AB_SPACE_SCREEN,
                                 target, &point))
            return ab_fail(error, "Physics replay lost its actual camera viewport");
        if (input->phase == 1 &&
            (point.x < 0 || point.y < 0 || point.x > menu->screen.x ||
             point.y > menu->screen.y)) {
            input->pan_start = (AbVec2){menu->screen.x * .5f, menu->screen.y * .5f};
            input->pan_target = (AbVec2){
                input->pan_start.x + fmaxf(-menu->screen.x * .35f,
                                           fminf(menu->screen.x * .35f,
                                                 menu->screen.x * .25f - point.x)),
                input->pan_start.y + fmaxf(-menu->screen.y * .25f,
                                           fminf(menu->screen.y * .25f,
                                                 menu->screen.y * .5f - point.y))};
            input->phase = 5;
            return true;
        }
        event = input->phase == 1 ? "LPRESS" : input->phase == 2 ? "LHOLD" : "LRELEASE";
        if (input->phase == 3) {
            input->previous_shots = game->shot.birds_shot;
            input->released_shot = true;
            input->phase = 0;
        } else
            ++input->phase;
    } else if (input->phase == 4) {
        event = "LRELEASE";
        input->phase = 0;
    } else if (game->shot.specialty_available && game->shot.flying &&
               input->frames_since_shot >= 12) {
        event = "LPRESS";
        input->phase = 4;
        ++input->ability_attempts;
    }
    AbUiPointerReply reply;
    return !event || menu_preview_pointer(menu, menu_span(event), point, &reply, error);
}

static bool physics_replay_capture(PreviewMenu *menu, AbMetal *renderer,
                                   AbError *error) {
    AbImage image = {0};
    float clear[4];
    menu_background_clear(menu, clear);
    bool okay = ab_metal_capture(renderer, menu->context.draw_list,
                                 (uint32_t)menu->screen.x, (uint32_t)menu->screen.y,
                                 clear, 16 * 1024 * 1024, &image, error);
    ab_image_free(&image);
    return okay;
}

/* Controlled visual inspection, separate from the ordinary input replays.
 * Move only the fixture's camera and draw at zero elapsed time. */
static bool physics_replay_focus(PreviewMenu *menu, size_t object, AbError *error) {
    PreviewGame *game = menu->game;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    if (object >= level.bodies->count)
        return ab_fail(error, "Visual fixture has no actual object identity");
    const AbDataTable *physical = &level.properties->objects[object].physical;
    float x, y;
    if (!game_shock_read(physical, "x", &x, error) ||
        !game_shock_read(physical, "y", &y, error))
        return false;
    AbCameraSystem *camera = ab_classic_camera_system(game->playfield.camera);
    AbCameraValues values = {.position = {x * 20, y * 20}, .present = 3};
    AbCameraMove move = {.duration_present = true};
    return ab_camera_system_cancel_sequence(camera, error) &&
           ab_camera_system_stop_movement(camera) &&
           ab_camera_system_goto_position(camera, &values, &move, error) &&
           menu_preview_draw(menu, 0, error);
}

static bool physics_replay_widgets(PreviewMenu *menu, AbMetal *renderer,
                                   AbError *error) {
    const char *const names[] = {"Level1",       "LevelP2_80",  "LevelP3_271",
                                 "Level925",     "Level745",    "BlackBird_1",
                                 "BlackBird_31", "BlackBird_22"};
    const size_t episodes[] = {1, 2, 3, 7, 8, 10, 10, 10};
    const char *filter = getenv("AB_TEST_PHYSICS_REPLAY_LEVEL");
    bool selected = false;
    for (size_t scene = 0; scene < sizeof(episodes) / sizeof(episodes[0]); ++scene) {
        if (filter && strcmp(filter, names[scene]))
            continue;
        selected = true;
        uint32_t seed =
            scene == 7 ? UINT32_C(0x520a0207) : UINT32_C(0x520a0000) + (uint32_t)scene;
        if (!ab_random48_seed(&menu->random, seed) ||
            !ab_engine_random_seed(&menu->engine_random, seed) ||
            !ability_test_episode_level(menu, episodes[scene], names[scene], error))
            return false;
        PhysicsReplayInput input = {0};
        PreviewGame *game = menu->game;
        AbClassicBirdView birds =
            ab_classic_birds_view(ab_classic_game_runtime_view(game->game).birds);
        if (birds.other_birds->capacity != 4 * (size_t)game->shot.bird_count + 4)
            return ab_fail(error, "Replay has an invalid other-bird identity budget");
        size_t notices = 0;
        AbEventListener listener = {menu_span("PhysicsReplay"), &notices,
                                    shockwave_event};
        if (!ab_event_manager_add(menu->events,
                                  menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                  &listener, error))
            return false;
        size_t frames = 0;
        for (; frames < 3600 && !game->ended; ++frames) {
            AbError reached = {0};
            if (!physics_replay_input(menu, &input, &reached) ||
                !game_eagle_test_frame(menu, &reached) ||
                (frames % 240 == 0 &&
                 !physics_replay_capture(menu, renderer, &reached)))
                return ab_fail(error,
                               "Physics replay %s seed %08x frame %zu shots %g: %s",
                               names[scene], seed, frames,
                               (double)game->shot.birds_shot, reached.message);
        }
        AbClassicSlingView sling = ab_classic_sling_view(game->playfield.sling);
        if (game->shot.birds_shot < fminf(2, game->shot.bird_count) && !game->ended)
            return ab_fail(
                error,
                "Physics replay %s stopped: shots=%g/%g ready=%u fired=%u "
                "timer=%g current=%s flying=%u fill=%u input_phase=%zu",
                names[scene], (double)game->shot.birds_shot,
                (double)game->shot.bird_count, game->shot.bird_ready,
                game->shot.bird_fired, (double)game->shot.next_bird_timer,
                game->shot.current_bird_name ? game->shot.current_bird_name : "nil",
                game->shot.flying != NULL, sling.fill_in_next_bird, input.phase);
        if (scene == 1 && (!input.ability_attempts || !notices))
            return ab_fail(error,
                           "Mighty Hoax replay did not execute a white-bird ability");
        printf(
            "Physics replay %s seed %08x: %zu frames, %.0f birds, %zu ability inputs, "
            "%zu specialty events, ended=%u.\n",
            names[scene], seed, frames, (double)game->shot.birds_shot,
            input.ability_attempts, notices, game->ended);
        if (!ab_event_manager_remove(menu->events,
                                     menu_event(menu, "EID_BIRD_SPECIALTY_TRIGGERED"),
                                     &listener, error))
            return false;
        game_session_free(menu);
        menu->level_request_ready = false;
    }
    puts("Deterministic gameplay physics replays cover repeated mouse launches, bird "
         "abilities, collision/removal/joint phases and periodic real Metal "
         "submissions.");
    return selected || ab_fail(error, "Physics replay filter matched no scene");
}

#endif
