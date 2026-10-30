#ifndef AB_TEST_CLASSIC_GAME_RUNTIME_H
#define AB_TEST_CLASSIC_GAME_RUNTIME_H

#include "ab/classic_game_runtime.h"
#include "ab/classic_game_render.h"
#include "ab/classic_balloons.h"
#include "ab/classic_shot_effects.h"
#include "ab/ui_widget.h"

#include <stdlib.h>

typedef struct {
    AbClassicGameRender *render;
    AbClassicGameGlobals *globals;
    AbDrawList draws;
    size_t frames;
    bool ended;
} FreshGame;

typedef struct {
    uint32_t steps;
    size_t frames, commands;
    bool launched, reloaded, ended, won;
} FreshGameResult;

static AbDataValue fresh_number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(value)};
}

static bool fresh_game_service(void *context, AbGameTailCall *call, AbError *error) {
    FreshGame *game = context;
    if (call->operation != AB_GAME_TAIL_DRAW)
        return ab_fail(error, "Unbound reached fresh-game service %u",
                       (unsigned)call->operation);
    ++game->frames;
    return ab_classic_game_draw(ab_classic_game_render_draw(game->render), 1920, 1080,
                                from_bits(game->globals->time.bits), &game->draws,
                                error);
}

static bool fresh_game_ended(void *context, AbDataTable *event, AbError *error) {
    (void)event;
    (void)error;
    ((FreshGame *)context)->ended = true;
    return true;
}

static bool fresh_bird_volume(void *context, AbDataValue receiver, size_t object,
                              AbDataValue *output, AbError *error) {
    (void)context;
    (void)receiver;
    (void)object;
    (void)error;
    *output = fresh_number(ab_level_effect_base_bird_volume());
    return true;
}

static bool fresh_bird_removed(void *context, AbDataValue name, size_t object,
                               AbError *error) {
    (void)context;
    (void)object;
    if (name.kind != AB_DATA_STRING || name.string.size != 14 ||
        memcmp(name.string.data, "redBirdRemoved", 14))
        return ab_fail(error, "Unbound reached bird-removal hook");
    return ab_classic_red_bird_removed(false, error);
}

static bool fresh_game_runtime(PlayfieldTest *test, AbClassicPlayfield *playfield_owner,
                               AbClassicWorldRuntime *world_owner,
                               AbClassicParticles *particles,
                               AbClassicGameGlobals *globals, AbDataValue *flying,
                               bool *physics, FreshGameResult *result, AbError *error) {
    AbClassicPlayfieldView playfield = ab_classic_playfield_view(playfield_owner);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(world_owner);
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(world.dynamics);
    AbDataValue disabled = {.kind = AB_DATA_BOOLEAN},
                enabled = {.kind = AB_DATA_BOOLEAN, .bits = 1}, absent = {0};
    AbDataValue bird_sound = {.kind = AB_DATA_STRING, .string = span("bird_misc")},
                pig_sound = {.kind = AB_DATA_STRING, .string = span("piglette")},
                start_sound = {.kind = AB_DATA_STRING,
                               .string = span("level_start_military")},
                clear_sound = {.kind = AB_DATA_STRING,
                               .string = span("level_clear_military")},
                failed_sound = {.kind = AB_DATA_STRING,
                                .string = span("level_failed_piglets")},
                song = {.kind = AB_DATA_STRING, .string = span("title_theme")},
                music = {.kind = AB_DATA_STRING, .string = span("ambient_theme1")};
    AbClassicGameStartState start = {0};
    AbClassicLevelFinishState finish = {0};
    AbClassicLevelEndingState ending = {
        .skip_ending = disabled,
        .do_not_wait = disabled,
        .cheats_enabled = disabled,
        .level_name = {.kind = AB_DATA_STRING, .string = span("Level1")},
        .score_increment = 10000,
        .floating_scale = 1,
        .default_delay = 3};
    AbClassicGameInputState input = {.skip_input = disabled,
                                     .skip_trigger_specialty = disabled,
                                     .touch_released_after_specialty = enabled,
                                     .using_gestures = disabled,
                                     .delta_sling = disabled,
                                     .accelerometer = disabled,
                                     .touch_count = {.kind = AB_DATA_NUMBER},
                                     .cheats_enabled = disabled};
    AbClassicBirdBonusScores bonus = {0};
    float bonus_entries[16];
    bonus.entries = bonus_entries;
    bonus.capacity = 16;
    AbGameTailState tail = {.time_since_present = true};
    AbClassicControllerEffects effects = {0};
    AbClassicPowerupUpdates active = {0}, passive = {0};
    AbDataTable effect_fields = {0}, extra_draw = {0};
    AbDataValue level_effect = {.kind = AB_DATA_TABLE, .table = &effect_fields};
    float remove_time = 1, remove_speed = 0.0025f, width = 1920;
    float speed = 1, game_timer = 0, particle_timer = 0, score = 0;
    AbVec2 cursor_world = {0}, old_cursor_world = {0};
    AbDataValue level_particles = {0}, will_end = {0};
    AbDataEntry pressed_entry = {{.kind = AB_DATA_STRING, .string = span("LBUTTON")},
                                 {.kind = AB_DATA_BOOLEAN}};
    AbDataEntry held_entry = pressed_entry, released_entry = pressed_entry;
    AbDataTable pressed = {&pressed_entry, 1, 1, NULL},
                held = {&held_entry, 1, 1, NULL},
                released = {&released_entry, 1, 1, NULL};
    AbCameraShake shake;
    AbHeroScreenTinter hero;
    AbBurnManager burn = {0};
    CHECK(ab_camera_shake_init(&shake) && ab_hero_screen_tinter_init(&hero));
    AbClassicGameUpdates *updates = NULL;
    AbClassicBalloons *balloons = NULL;
    CHECK(ab_classic_game_updates_open(16, 65536, &updates, error) &&
          ab_classic_game_updates_clear(updates, error) &&
          ab_classic_balloons_open(
              (AbClassicBalloonConfig){.level = playfield.level,
                                       .updates = updates,
                                       .dead_blocks = world.dead_blocks,
                                       .nodes = 128,
                                       .storage_bytes = 65536},
              &balloons, error));
    /* NotificationsFrame has no init override. Its actual inherited constructor
     * starts with no children; other UI owners populate it at their boundaries. */
    AbUiWidgetContext ui = {.maximum_depth = 64};
    AbUiWidget notifications = {0};
    AbUiFrameFields notification_fields = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span("notifications")}};
    CHECK(ab_ui_widget_init(&notifications, &ui, AB_UI_WIDGET_FRAME,
                            (AbUiWidgetChildren){0}, &notification_fields, NULL, NULL,
                            error));
    FreshGame game = {.globals = globals};
    game.draws.capacity = 8192;
    game.draws.commands = calloc(game.draws.capacity, sizeof(*game.draws.commands));
    CHECK(game.draws.commands);
    AbClassicGameRuntimeConfig config = {
        .playfield = playfield_owner,
        .world = world_owner,
        .particles = particles,
        .audio = test->audio,
        .settings = test->statistics,
        .events = test->events,
        .random = &test->random,
        .flying = flying,
        .birds = {.level_effect = &level_effect,
                  .remove_speed_squared = &remove_speed,
                  .remove_time = &remove_time,
                  .removed_hook = fresh_bird_removed,
                  .removal_volume = fresh_bird_volume,
                  .storage_bytes = 256 * 1024},
        .conditions = {.prevent_ending = &absent,
                       .bird_sound = &bird_sound,
                       .pig_sound = &pig_sound,
                       .eagle_launched = &disabled,
                       .eagle_timer = &absent},
        .effects = {.shake = &shake, .hero = &hero, .burn = &burn},
        .prelude = {.do_not_timeout = &absent,
                    .cursor_world = &cursor_world,
                    .old_cursor_world = &old_cursor_world},
        .start = {.updates = updates,
                  .state = &start,
                  .release_build = &enabled,
                  .started_from_editor = &disabled,
                  .level_start_sound = &start_sound,
                  .main_menu_song = &song,
                  .current_music = &music,
                  .physics_enabled = physics},
        .finish = {.state = &finish,
                   .clear_sound = &clear_sound,
                   .failed_sound = &failed_sound,
                   .notifications = 16,
                   .storage_bytes = 65536},
        .ending = {.state = &ending,
                   .bonus = &bonus,
                   .notifications = 16,
                   .storage_bytes = 65536},
        .progress = {.key_c = &absent,
                     .key_gaming_a = &absent,
                     .quad_click = &absent,
                     .level_will_end_triggered = &will_end,
                     .notifications = 16,
                     .storage_bytes = 65536},
        .powerups = {.active = &active,
                     .passive = &passive,
                     .enabled = &enabled,
                     .physics_speed_factor = &speed,
                     .game_timer = &game_timer},
        .level_particles = {.level_particles = &level_particles,
                            .enabled = &disabled,
                            .timer = &particle_timer},
        .input = {.state = &input,
                  .pressed = &pressed,
                  .held = &held,
                  .released = &released,
                  .screen_width = &width},
        .specialty = {.notifications = &notifications.tree,
                      .red_available = &disabled,
                      .notifications_capacity = 16,
                      .storage_bytes = 65536},
        .controller = {.level_effects = &effects,
                       .effect_kind = AB_CLASSIC_CONTROLLER_EFFECT_BASE,
                       .state = &tail,
                       .score = &score,
                       .wobble_steps = 64,
                       .storage_bytes = 65536,
                       .context = &game,
                       .service = fresh_game_service},
        .characters_bytes = 256 * 1024,
        .touches_bytes = 65536,
        .touches = 8,
        .storage_bytes = 2 * 1024 * 1024};
    AbClassicGameRuntime *runtime = NULL;
    CHECK(ab_classic_game_runtime_open(config, &runtime, error));
    AbClassicGameRuntimeView view = ab_classic_game_runtime_view(runtime);
    AbClassicGameDrawState draw_state = {
        .current_theme =
            field(ab_classic_level_view(playfield.level).data->globals, "theme"),
        .current_game_mode = {.kind = AB_DATA_OBJECT, .object = view.update},
        .editor_mode = {.kind = AB_DATA_OBJECT, .object = &config},
        .zoom_level = {.kind = AB_DATA_NUMBER},
        .old_zoom_level = {.kind = AB_DATA_NUMBER}};
    AbDataValue physics_to_world = fresh_number(20);
    AbSpan font = {0};
    uint8_t white_pixels[4] = {255, 255, 255, 255};
    AbImage white_image = {1, 1, {white_pixels, sizeof(white_pixels)}};
    CHECK(ab_asset_textures_count(test->textures) < 64 &&
          logical_upload(test, 64,
                         (AbSpriteAtlasImage){.image = &white_image,
                                              .alpha = AB_SPRITE_ALPHA_OPAQUE},
                         error));
    AbDataValue theme = field(test->themes.data.globals, "theme1");
    CHECK(theme.kind == AB_DATA_TABLE &&
          ab_classic_game_render_open(
              (AbClassicGameRenderConfig){
                  .playfield = playfield_owner,
                  .powerups = view.powerups,
                  .particles = ab_classic_particles_engine(particles),
                  .provider = &test->grouped_sprites.provider,
                  .theme = theme.table,
                  .settings = ab_settings_statistics_root(test->statistics),
                  .extra_draw_functions = &extra_draw,
                  .physics_to_world = &physics_to_world,
                  .state = &draw_state,
                  .basic_font = span("FONT_BASIC"),
                  .current_font = &font,
                  .white_texture = 64,
                  .layers_bytes = 256 * 1024,
                  .sling_bytes = 65536,
                  .storage_bytes = 512 * 1024,
                  .placement_limit = 4096},
              &game.render, error));
    AbEventListener ended_listener = {span("FreshGameResult"), &game, fresh_game_ended};
    AbDataValue ended_id = {.kind = AB_DATA_STRING, .string = span("EID_LEVEL_ENDED")};
    CHECK(ab_event_manager_add(test->events, ended_id, &ended_listener, error));
    AbShotState *shot = ab_classic_shots_state(playfield.shots);
    size_t press_frame = SIZE_MAX;
    int16_t pcm[512];
    for (size_t frame = 0; frame < 1800 && !game.ended; ++frame) {
        game.draws.count = 0;
        pressed_entry.value.bits = held_entry.value.bits = released_entry.value.bits =
            0;
        AbFrameClockPlan plan;
        CHECK(ab_classic_simulation_advance(
            dynamics.simulation,
            &(AbFrameClockInput){.seconds = 1.0f / 60, .physics_enabled = *physics},
            &plan, error));
        result->steps += plan.steps;
        CHECK(ab_event_manager_tick(test->events, 1.0f / 60, error));
        if (game.ended)
            break;
        CHECK(ab_classic_game_globals_advance_time(globals, 1.0f / 60, error));
        bool visible = false;
        CHECK(ab_classic_sling_is_visible(playfield.sling, &visible, error));
        if (press_frame == SIZE_MAX && frame > 120 && shot->bird_ready && visible &&
            ab_classic_camera_view(playfield.camera).system.input_enabled)
            press_frame = frame;
        if (press_frame != SIZE_MAX && frame <= press_frame + 2) {
            AbVec2 target = ab_classic_sling_view(playfield.sling).band.rest;
            if (frame > press_frame) {
                target.x -= 4;
                target.y += 2;
            }
            AbCoordinateCamera camera =
                ab_classic_camera_view(playfield.camera).system.viewport->rendering;
            CHECK(ab_coordinate_point(&camera, AB_SPACE_PHYSICS, AB_SPACE_SCREEN,
                                      target, &input.cursor));
            pressed_entry.value.bits = frame == press_frame;
            held_entry.value.bits = frame < press_frame + 2;
            released_entry.value.bits = frame == press_frame + 2;
        }
        input.touch_count = fresh_number(held_entry.value.bits ? 1.0f : 0.0f);
        AbCameraSystemInput camera = {.moving_objects =
                                          globals->has_moving_objects.bits != 0};
        CHECK(ab_classic_game_update(view.update, (AbClassicGameTouchInput){0},
                                     1.0f / 60, globals->time, camera, error) &&
              game.draws.count &&
              ab_audio_mixer_mix(test->mixer, 512, pcm, 512, error) &&
              ab_audio_mixer_mix(test->mixer, 223, pcm, 512, error));
        if (*physics)
            CHECK(ab_particles_update(ab_classic_particles_engine(particles), 1.0f / 60,
                                      false, error) &&
                  ab_particles_update_level(ab_classic_particles_engine(particles),
                                            1.0f / 60, error));
        CHECK(ab_particles_update(ab_classic_particles_engine(particles), 1.0f / 60,
                                  true, error));
        result->launched |= shot->birds_shot > 0;
        result->reloaded |=
            ab_classic_sling_view(playfield.sling).current_bird_index >= 2 &&
            shot->bird_ready;
        result->commands += game.draws.count;
    }
    result->frames = game.frames;
    result->ended = game.ended;
    result->won = finish.complete.completed;
    CHECK(result->launched && result->ended && result->won && result->frames > 120 &&
          result->commands &&
          ab_event_manager_remove(test->events, ended_id, &ended_listener, error));
    ab_classic_game_render_close(game.render);
    CHECK(logical_remove(test, 64, error));
    ab_classic_game_runtime_close(runtime);
    CHECK(ab_classic_game_updates_clear(updates, error));
    ab_classic_balloons_close(balloons);
    ab_classic_game_updates_close(updates);
    free(game.draws.commands);
    return true;
}

#endif
