#ifndef AB_TEST_CLASSIC_GAME_UPDATE_H
#define AB_TEST_CLASSIC_GAME_UPDATE_H

#include "ab/classic_game_update.h"
#include "ab/classic_frame_particles.h"
#include "ab/classic_game_draw.h"
#include "ab/process_manager.h"
#include "ab/classic_loader.h"
#include "ab/native_events.h"
#include "ab/classic_audio_ramp.h"
#include "ab/url_requests.h"
#include "ab/classic_anger.h"
#include "ab/classic_game_mode_bindings.h"
#include "root_query_menu.h"
#include "classic_balloons.h"
#include "classic_game_menu_input.h"

typedef struct {
    AbClassicGameUpdate *owner;
    unsigned *phase;
    float timer;
    bool fail_passive;
} LifecycleUpdateOrder;

static bool lifecycle_update_registry(void *context, float seconds, float time,
                                      AbError *error) {
    LifecycleUpdateOrder *test = context;
    (void)error;
    CHECK(seconds == 0 && time == 4 && !*test->phase &&
          !ab_classic_game_update(test->owner, (AbClassicGameTouchInput){0}, 0,
                                  (AbDataValue){.kind = AB_DATA_NUMBER},
                                  (AbCameraSystemInput){0}, NULL));
    *test->phase = 1;
    return true;
}

static bool lifecycle_update_passive(void *context, float seconds, AbError *error) {
    LifecycleUpdateOrder *test = context;
    CHECK(seconds == 0 && *test->phase == 1);
    test->timer = 0.5f;
    *test->phase = 2;
    return !test->fail_passive ||
           ab_fail(error, "Intentional composed passive failure");
}

static bool lifecycle_update_extra(void *context, float seconds, float time,
                                   AbError *error) {
    LifecycleUpdateOrder *test = context;
    (void)error;
    CHECK(seconds == 0 && time == 4 && *test->phase == 2 && test->timer == 0.5f);
    *test->phase = 3;
    return true;
}

/* Controlled zero-time GameUpdate over actual owners; this probes composition
 * and cutoff boundaries without claiming selected bootstrap or native frames. */
static bool lifecycle_game_update(
    ShotTest *shot, AbClassicShots *shots, AbClassicCamera *camera,
    AbClassicController *controller, LifecycleInputBinding *binding,
    const AbClassicGamePrelude *existing_prelude,
    const AbClassicCharacterServices *conditions, AbClassicScores *scores,
    AbFloatingScores *floating, AbParticles *particles, bool *physics,
    AbClassicLevelFinishState *finish_state, unsigned *phase, AbStringMap *dead,
    AbClassicFrameParticles *frame, AbClassicGameDraw *draw, AbError *error) {
    AbClassicGameUpdate game = {0};
    AbClassicAnger *anger = NULL;
    AbClassicAngerState anger_state = {0};
    AbClassicAngerSettings *anger_settings = NULL;
    CHECK(ab_classic_anger_open(
              (AbClassicAngerConfig){
                  .state = &anger_state,
                  .settings = &anger_settings,
                  .clock = {NULL, ab_session_clock_platform},
                  .events = shot->events,
                  .load_level_late = {.kind = AB_DATA_STRING,
                                      .string = span("EID_LOAD_LEVEL_LATE")},
                  .level_increased = {.kind = AB_DATA_STRING,
                                      .string = span("EID_ANGER_LEVEL_INCREASED")},
                  .records = 4,
                  .iteration_limit = 16,
                  .storage_bytes = 16384},
              &anger, error) &&
          ab_classic_anger_update(anger, error) && !anger_settings &&
          anger_state.initialized.bits == 1 &&
          anger_state.level.bits == UINT32_C(0x40000000) &&
          !anger_state.level_timer.active.bits &&
          !anger_state.cooldown_timer.active.bits);
    AbUrlRequests *urls = NULL;
    CHECK(ab_url_requests_open(8, 16, 65536, (AbUrlRequestHost){0}, &urls, error) &&
          ab_url_requests_update(urls, 0, error));
    /* Original initialized Level1 has both ramp volumes/lengths nil. Bind the
     * real shared mixer at root61785 before loader/menu, without gain writes. */
    AbClassicAudioRampState ramp_state = {0};
    AbClassicAudioRamp ramp = {0};
    CHECK(ab_classic_audio_ramp_initialize(&ramp_state, shot->mixer, &ramp, error) &&
          ab_classic_audio_ramp_update(&ramp, 0, error));
    AbNativeEvents *native_events = NULL;
    AbNativeAnimation native_animation = {0};
    size_t native_system_count = 0;
    bool updating_native_systems = false;
    CHECK(ab_native_events_open(4, 4096, &native_events, error) &&
          ab_native_animation_initialize(native_events, &native_system_count,
                                         &updating_native_systems, &native_animation,
                                         error));
    AbClassicLoaderState loader_state = {.loading = {.kind = AB_DATA_BOOLEAN},
                                         .frame = {.kind = AB_DATA_NUMBER}};
    AbDataValue loader_ids[] = {
        {.kind = AB_DATA_STRING, .string = span("EID_DO_LOADING")},
        {.kind = AB_DATA_STRING, .string = span("EID_SHOW_LOADING_PAGE")},
        {.kind = AB_DATA_STRING, .string = span("EID_HIDE_LOADING_PAGE")}};
    AbClassicLoader *loader = NULL;
    CHECK(ab_classic_loader_open(
              (AbClassicLoaderConfig){.state = &loader_state,
                                      .script_time = &shot->globals.time,
                                      .do_loading = loader_ids,
                                      .show_loading = loader_ids + 1,
                                      .hide_loading = loader_ids + 2,
                                      .skip_input = &binding->state.skip_input,
                                      .events = shot->events,
                                      .storage_bytes = 4096},
              &loader, error) &&
          ab_classic_loader_update(loader, error) &&
          loader_state.frame.bits == bits(1));
    AbClassicGameInputConfig original_input = binding->input.config;
    LifecycleGameMenuInput menu = {0};
    CHECK(lifecycle_menu_open(&binding->input, (AbClassicMenuInputStorage){0}, &menu,
                              error));
    LifecycleUpdateOrder order = {&game, phase, 0, false};
    AbClassicGamePrelude prelude = {0};
    AbClassicGamePreludeConfig prelude_config = existing_prelude->config;
    prelude_config.cursor = &binding->state.cursor;
    prelude_config.cursor_physics = &binding->state.cursor_physics;
    prelude_config.cheats_enabled = &binding->state.cheats_enabled;
    prelude_config.touch_count = &binding->state.touch_count;
    CHECK(ab_classic_game_prelude_initialize(prelude_config, &prelude, error));
    AbClassicGameUpdates *updates = NULL;
    AbClassicBalloons *balloons = NULL;
    AbClassicGameUpdateFunction function = {&order, lifecycle_update_registry};
    CHECK(ab_classic_game_updates_open(4, 32768, &updates, error) &&
          ab_classic_game_updates_clear(updates, error) &&
          ab_classic_balloons_open((AbClassicBalloonConfig){.level = shot->owner,
                                                            .updates = updates,
                                                            .dead_blocks = dead,
                                                            .nodes = 32,
                                                            .storage_bytes = 65536},
                                   &balloons, error));
    AbStringMapLayout balloon_layout = {0};
    const AbClassicGameUpdateFunction *registered = NULL;
    CHECK(
        ab_string_map_layout(ab_classic_balloons_members(balloons), &balloon_layout) &&
        !balloon_layout.count &&
        ab_classic_game_updates_lookup(updates, span("removeBalloonsTooHigh"),
                                       &registered, error) &&
        registered && registered->context == balloons &&
        registered->update(registered->context, NAN, NAN, error) &&
        ab_classic_game_updates_add(updates, span("LifecycleUpdateOrder"), &function,
                                    error));
    AbClassicGameStartState start_state = {.level_start_timer = 10};
    AbDataValue enabled = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbDataValue disabled = {.kind = AB_DATA_BOOLEAN}, absent = {0};
    AbDataValue start_sound = {.kind = AB_DATA_STRING,
                               .string = span("level_start_military")};
    AbDataValue menu_song = {.kind = AB_DATA_STRING, .string = span("title_theme")};
    AbClassicGameStart start = {0};
    CHECK(ab_classic_game_start_initialize(
        (AbClassicGameStartConfig){.sling = binding->input.config.sling,
                                   .audio = shot->audio,
                                   .updates = updates,
                                   .state = &start_state,
                                   .release_build = &enabled,
                                   .cheats_enabled = &binding->state.cheats_enabled,
                                   .started_from_editor = &disabled,
                                   .level_start_sound = &start_sound,
                                   .main_menu_song = &menu_song,
                                   .current_music = &absent,
                                   .keys = &binding->empty,
                                   .physics_enabled = physics},
        &start, error));
    AbClassicLevelFinish *finish = NULL;
    AbClassicLevelEnding *ending = NULL;
    AbClassicGameProgress *progress = NULL;
    AbDataValue clear_sound = {.kind = AB_DATA_STRING,
                               .string = span("level_clear_military")};
    AbDataValue failed_sound = {.kind = AB_DATA_STRING,
                                .string = span("level_failed_piglets")};
    AbClassicCharacterServices live = *conditions;
    AbClassicLevelEndingState ending_state = {0};
    live.config.prevent_ending = &enabled;
    live.config.do_not_wait_for_moving_objects = &ending_state.do_not_wait;
    AbClassicBirdBonusScores bonus = {0};
    CHECK(ab_classic_level_finish_open(
              (AbClassicLevelFinishConfig){.shots = shots,
                                           .sling = binding->input.config.sling,
                                           .camera = camera,
                                           .audio = shot->audio,
                                           .events = shot->events,
                                           .state = finish_state,
                                           .clear_sound = &clear_sound,
                                           .failed_sound = &failed_sound,
                                           .notifications = 1,
                                           .storage_bytes = 32768},
              &finish, error) &&
          ab_classic_level_ending_open(
              (AbClassicLevelEndingConfig){.shots = shots,
                                           .sling = binding->input.config.sling,
                                           .scores = scores,
                                           .floating = floating,
                                           .audio = shot->audio,
                                           .events = shot->events,
                                           .conditions = &live,
                                           .finish = finish_state,
                                           .state = &ending_state,
                                           .bonus = &bonus,
                                           .eagle_launched =
                                               conditions->config.eagle_launched,
                                           .bird_sound = conditions->config.bird_sound,
                                           .notifications = 1,
                                           .storage_bytes = 32768},
              &ending, error));
    AbDataValue triggered = {0};
    CHECK(ab_classic_game_progress_open(
        (AbClassicGameProgressConfig){.finish = finish,
                                      .ending = ending,
                                      .finish_state = finish_state,
                                      .conditions = &live,
                                      .events = shot->events,
                                      .cheats_enabled = &binding->state.cheats_enabled,
                                      .key_c = &absent,
                                      .key_gaming_a = &absent,
                                      .quad_click = &absent,
                                      .level_will_end_triggered = &triggered,
                                      .notifications = 1,
                                      .storage_bytes = 32768},
        &progress, error));
    AbClassicPowerupUpdate passive = {.context = &order,
                                      .update = lifecycle_update_passive};
    AbClassicPowerupUpdate *passive_items[1] = {&passive};
    AbClassicPowerupUpdates active = {0}, passive_updates = {passive_items, 1};
    float speed = 1;
    AbClassicGamePowerups powerups = {0};
    CHECK(ab_classic_game_powerups_initialize(
        (AbClassicGamePowerupConfig){.sling = binding->input.config.sling,
                                     .active = &active,
                                     .passive = &passive_updates,
                                     .enabled = &enabled,
                                     .cheats_enabled = &binding->state.cheats_enabled,
                                     .released_keys = &binding->empty,
                                     .physics_speed_factor = &speed,
                                     .skip_input = &binding->state.skip_input,
                                     .game_timer = &order.timer},
        &powerups, error));
    AbClassicGamePowerups *original_draw_powerups = draw->config.powerups;
    draw->config.powerups = &powerups;
    AbProcessManager *processes = NULL;
    AbDataValue process_finished = {.kind = AB_DATA_STRING,
                                    .string = span("EID_PROCESS_FINISHED")};
    CHECK(ab_process_manager_open(
        (AbProcessManagerConfig){.events = shot->events,
                                 .finished_event = &process_finished,
                                 .actions = 1,
                                 .generations = 1,
                                 .storage_bytes = 4096},
        NULL, &processes, error));
    float particle_timer = 0;
    AbClassicGameLevelParticles level_particles = {0};
    CHECK(ab_classic_game_level_particles_initialize(
        (AbClassicGameLevelParticleConfig){.particles = particles,
                                           .level_particles = &absent,
                                           .enabled = &disabled,
                                           .physics_enabled = physics,
                                           .timer = &particle_timer,
                                           .context = &order,
                                           .extra_update = lifecycle_update_extra},
        &level_particles, error));
    CHECK(ab_classic_game_update_initialize(
        (AbClassicGameUpdateConfig){&prelude, &start, progress, &powerups,
                                    &level_particles, &binding->input, controller},
        &game, error));
    /* Controlled base-Frame gameHud/pausePage tree for actual pause queries;
     * this is not the target's complete GameHud constructor. Share this actual
     * MenuManager with the menu/input fixture instead of injecting isIngame. */
    QueryMenu query_menu;
    const char *pause_names[] = {"pausePage"};
    CHECK(query_menu_prepare_named((ReferenceRootQuery){.root = 5, .children = 1},
                                   pause_names, &query_menu, error));
    query_menu.frames[1].fields.visible = query_ui_flag(1);
    query_menu.frames[1].fields.pause_page_show = query_ui_flag(1);
    ab_ui_menu_manager_close(menu.manager);
    menu.manager = query_menu.manager;
    AbClassicGameTouchInput mode_touches = {0};
    AbCameraSystemInput mode_camera = {0};
    uint32_t width = 480, height = 320;
    AbClassicGameModeBindings mode_bindings;
    CHECK(ab_classic_game_mode_bindings_initialize(
        (AbClassicGameModeBindingConfig){.game = &game,
                                         .draw = draw,
                                         .touches = &mode_touches,
                                         .camera = &mode_camera,
                                         .globals = &shot->globals,
                                         .width = &width,
                                         .height = &height,
                                         .draws = frame->config.draws},
        &mode_bindings, error));
    AbClassicModeFunction *current_mode = &mode_bindings.update;
    AbClassicPlayMode museum = {.kind = AB_CLASSIC_PLAY_MODE_MUSEUM,
                                .menus = menu.manager};
    AbClassicPlayMode *play_mode = &museum, *museum_mode = &museum;
    AbClassicGameMode *mode = NULL;
    CHECK(ab_classic_game_mode_open(
        (AbClassicGameModeConfig){.current = &current_mode,
                                  .update_game = &mode_bindings.update,
                                  .draw_game = &mode_bindings.draw,
                                  .play_mode = &play_mode,
                                  .museum_mode = &museum_mode,
                                  .accelerometer_control = &disabled,
                                  .physics_enabled = physics,
                                  .paused_functions = 4,
                                  .storage_bytes = 4096,
                                  .depth_limit = 8},
        &mode, error));
    AbDataValue mode_arguments[] = {{.kind = AB_DATA_NUMBER}, shot->globals.time};
    CHECK(frame->config.particles == particles &&
          frame->config.physics_enabled == physics &&
          ab_classic_frame_particles_begin(frame, 0, error) &&
          /* Root57000 visits MenuManager above, then EventManager273..279 and
           * ProcessManager280..283 and AnimationWrapper284..288 before GameUpdate.
           * The observed host-system registry is empty. Other root phases and a
           * selected GUI remain outside this controlled zero-time projection. */
          ab_event_manager_tick(shot->events, 0, error) &&
          ab_process_manager_update(processes, 0, &binding->state.skip_input, error) &&
          ab_native_animation_update(&native_animation, 0, error) &&
          ab_classic_game_mode_call(mode, mode_arguments, 2, error) &&
          ab_classic_frame_particles_end(frame, error) && !frame->active &&
          !frame->menu_drawn && *phase == 3 && !binding->state.skip_input.bits &&
          binding->state.skip_trigger_specialty.kind == AB_DATA_BOOLEAN &&
          !binding->state.skip_trigger_specialty.bits &&
          !finish_state->complete.completed);
    size_t prior_draws = frame->config.draws->count;
    CHECK(ab_classic_game_mode_set_paused(mode, true, error) && !*physics &&
          current_mode != &mode_bindings.update &&
          ab_classic_game_mode_call(mode, mode_arguments, 2, error) &&
          frame->config.draws->count > prior_draws && *phase == 3 && !game.failed &&
          !game.updating && ab_classic_game_mode_set_paused(mode, false, error) &&
          *physics && current_mode == &mode_bindings.update);
    *phase = 0;
    order.fail_passive = true;
    AbError late = {0};
    CHECK(!ab_classic_game_update(&game, (AbClassicGameTouchInput){0}, 0,
                                  shot->globals.time, (AbCameraSystemInput){0},
                                  &late) &&
          strstr(late.message, "Intentional composed passive failure") && game.failed &&
          !game.updating && *phase == 2 && order.timer == 0.5f &&
          !ab_classic_game_update(&game, (AbClassicGameTouchInput){0}, 0,
                                  shot->globals.time, (AbCameraSystemInput){0}, NULL));
    ab_classic_game_progress_close(progress);
    ab_classic_game_mode_close(mode);
    ab_classic_loader_close(loader);
    ab_classic_anger_close(anger);
    ab_url_requests_close(urls);
    ab_native_events_close(native_events);
    ab_process_manager_close(processes);
    ab_classic_level_ending_close(ending);
    ab_classic_level_finish_close(finish);
    ab_classic_game_updates_close(updates);
    CHECK(ab_classic_game_input_initialize(original_input, &binding->input, error));
    ab_ui_menu_manager_close(menu.manager);
    CHECK(lifecycle_balloon_edges(shot, balloons, dead, error));
    ab_classic_balloons_close(balloons);
    draw->config.powerups = original_draw_powerups;
    return true;
}

#endif
