#ifndef AB_GAME_SESSION_H
#define AB_GAME_SESSION_H

#include "ab/classic_balloons.h"
#include "ab/classic_collision_achievements.h"
#include "ab/classic_collision_hooks.h"
#include "ab/classic_game_render.h"
#include "ab/classic_game_runtime.h"
#include "ab/classic_level_runtime.h"
#include "ab/classic_shot_effects.h"
#include "ab/level_effect.h"
#include "ab/classic_highscores.h"
#include "ab/camera_touch.h"
#include "ab/ui_hud_buttons.h"

typedef struct PreviewResults PreviewResults;
typedef struct PreviewPause PreviewPause;

/* Application ownership, above the independently verified runtime phases.
 * Resources/settings/events remain owned by the menu application. */
struct PreviewGame {
    PreviewMenu *menu;
    AbClassicLevelRuntime *level;
    AbClassicWorldRuntime *world;
    AbClassicGameRuntime *game;
    AbClassicGameRender *render;
    AbClassicParticles *particles;
    AbClassicCollisionAchievements *achievements;
    AbClassicHighscores *highscores;
    PreviewResults *results;
    PreviewPause *pause;
    AbUiPointerValue navigation_action;
    AbClassicCollisionHooks hooks;
    AbObjectRemovalNotifyHost removal_notifications;
    AbClassicGameUpdates *updates;
    AbClassicBalloons *balloons;
    AbPowerupUses *uses;
    AbClassicPlayfieldView playfield;
    AbClassicGameGlobals globals;
    AbShotState shot;
    AbClassicGameInputState input;
    AbClassicGameStartState start;
    AbClassicLevelFinishState finish;
    AbClassicLevelEndingState ending;
    AbClassicGameDrawState draw;
    AbGameTailState tail;
    AbClassicControllerEffects effects;
    AbClassicPowerupUpdates active, passive;
    AbClassicBirdBonusScores bonus;
    float *bonus_entries;
    AbCameraShake shake;
    AbCameraTouchHandler camera_input;
    AbCameraTouchShared camera_shared;
    AbHeroScreenTinter hero;
    AbBurnManager burn;
    AbEngineRandom engine_random;
    AbDataValue enabled, disabled, absent, flying, goal_sent, will_end;
    AbDataValue next_sound, bird_sound, pig_sound, start_sound, clear_sound;
    AbDataValue failed_sound, song, music, model, level_name, level_effect;
    AbDataValue physics_to_world, level_particles, level_particles_enabled;
    AbDataValue previous_score;
    AbDataTable effect_fields, extra_draw;
    const AbDataTable *theme;
    AbDataTable pressed, held, released;
    AbDataEntry pressed_key, held_key, released_key;
    AbUiWidget notifications;
    AbUiHudSkipButton skip;
    AbUiWidgetContext notification_context;
    AbUiFrameFields notification_fields;
    AbSpan font;
    AbSpan particle_names[256];
    size_t indices[256];
    AbStringOrderNode order_nodes[256];
    AbStringOrderWorkspace workspace;
    AbVec2 cursor_world, old_cursor_world, screen;
    AbVec2 scroll_point;
    float scroll;
    float threshold, floating_scale, total_blocks, remove_time, remove_speed;
    float speed, game_timer, particle_timer, score, audio_fraction;
    uint32_t renderer_counter;
    AbEventListener result_listener;
    AbEventListener goals_listener;
    bool physics, ended, result_listening, updating, failed, paused;
    bool first_clear, result_complete, save_pending;
    bool goals_listening;
};

static AbDataValue game_text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = menu_span(name)};
}

static bool game_diagnostic(void *context, AbSpan message, AbError *error) {
    (void)context;
    return fprintf(stderr, "%.*s\n", (int)message.size, (const char *)message.data) >=
               0 ||
           ab_fail(error, "Cannot write game diagnostic");
}

static bool game_prediction_log(void *context, size_t raycasts, AbError *error) {
    (void)context;
    return fprintf(stderr, "Impact prediction: %zu raycasts\n", raycasts) >= 0 ||
           ab_fail(error, "Cannot write game prediction diagnostic");
}

static bool game_camera_input(void *context, AbCameraSystem *system, float seconds,
                              AbCameraSystemInput *input, AbError *error) {
    PreviewGame *game = context;
    AbCameraTouch pointer = {.id = 0, .position = game->input.cursor};
    AbCameraTouchOutput output;
    if (!ab_camera_touch_update(
            &game->camera_input, &game->camera_shared, system, seconds,
            &(AbCameraTouchInput){.touches = &pointer,
                                  .count = game->held_key.value.bits ? 1 : 0,
                                  .bird_dragged = game->shot.dragged,
                                  .specialty_available = game->shot.specialty_available,
                                  .released_after_specialty = menu_route_truthy(
                                      game->input.touch_released_after_specialty),
                                  .ingame = !game->ended},
            &output, error))
        return false;
    input->targets = output.targets;
    input->targets_present = output.targets_present;
    input->user_input = output.user_input;
    float scroll = game->scroll;
    game->scroll = 0;
    AbCameraSystemView view = ab_camera_system_view(system);
    if (scroll != 0 && view.input_enabled && !game->shot.dragged && !game->ended) {
        /* Desktop wheel mapping into the recovered camera's ordinary user
         * targets. Eight wheel notches double zoom; enforce actual level limits
         * and retain the world point beneath the pointer. */
        float exponent = fmaxf(-64, fminf(64, scroll / 8));
        AbCameraValues values = {.position = view.mover->current_position,
                                 .zoom = view.mover->current_zoom * exp2f(exponent),
                                 .present = 7};
        if (!ab_camera_limits_zoom(view.limits, &values, &values))
            return ab_fail(error, "Cannot constrain desktop wheel zoom");
        float anchor = 1 / view.mover->current_zoom - 1 / values.zoom;
        values.position.x += (game->scroll_point.x - game->screen.x * 0.5f) * anchor;
        values.position.y += (game->scroll_point.y - game->screen.y * 0.5f) * anchor;
        if (!ab_camera_system_stop_movement(system))
            return ab_fail(error, "Cannot apply desktop wheel camera input");
        input->targets = (AbCameraTargets){
            .values = values, .movement_duration = 0.15f, .zoom_duration = 0.15f};
        input->targets_present = input->user_input = true;
    }
    return true;
}

static bool game_session_scroll(PreviewMenu *menu, float amount, AbVec2 point,
                                AbError *error) {
    PreviewGame *game = menu->game;
    if (!isfinite(amount) || !isfinite(point.x) || !isfinite(point.y))
        return ab_fail(error, "Nonfinite desktop wheel input");
    if (!game || game->ended || game->paused)
        return true;
    game->scroll += amount;
    game->scroll_point = point;
    return isfinite(game->scroll) || ab_fail(error, "Desktop wheel input overflow");
}

/* level_scripts.lua root: 35 direct registrations and episode7/page2's
 * onAfterLevelObjectsCreated loop. Reject unresolved active scripts before
 * loading; the remaining names have an actual absent source registry entry. */
static bool game_scripts_available(const AbClassicLevelRequest *level, AbError *error) {
    static const char *const registered[] = {
        "Level5",       "Level34",     "LevelP2_64",
        "Level1268",    "LevelP2_67",  "LevelP3_271",
        "LevelP3_220",  "LevelP3_313", "LevelP4_426",
        "LevelP4_440",  "LevelP4_444", "LevelP4_478",
        "LevelP5_357",  "LevelP5_650", "LevelP5_354",
        "LevelP5_364",  "Level695",    "Level697",
        "LevelP6_7",    "LevelH564",   "UGC1Level15_custom",
        "LevelH900",    "Level974",    "EggDefender_classic_8",
        "BlackBird_15", "SOUNDBOARD1", "RADIO",
        "KEYBOARD",     "SEQUENCER",   "ACCORDION",
        "LevelGE_20",   "Level1261",   "Level1262",
        "Level1263",    "Level_0003_1"};
    if (menu_route_equal(level->episode_id, menu_route_number(7)) &&
        menu_route_equal(level->page_number, menu_route_number(2)))
        return ab_fail(error,
                       "Selected level requires its registered construction hook");
    for (size_t index = 0; index < sizeof(registered) / sizeof(*registered); ++index)
        if (menu_route_equal(level->name, game_text(registered[index])))
            return ab_fail(error, "Selected level requires its registered script: %s",
                           registered[index]);
    return true;
}

static bool game_group(PreviewMenu *menu, const char *name, AbError *error) {
    AbDataEntry entry = {menu_route_number(1), game_text(name)};
    AbDataTable groups = {&entry, 1, 1, NULL};
    return ab_android_assets_manage(menu->planning, &groups, true,
                                    ab_asset_resources_host, menu->resources, error);
}

static bool game_prepare(void *context, const AbLuaData *level,
                         const AbLuaData *catalog, AbError *error) {
    PreviewGame *game = context;
    PreviewMenu *menu = game->menu;
    (void)catalog;
    AbDataValue theme = menu_route_field(level->globals, "theme");
    if (theme.kind != AB_DATA_STRING || !game_group(menu, "INGAME", error) ||
        !game_group(menu, "INGAME_PIGS_NEW", error) ||
        !menu_theme_select(menu, theme.string, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Selected level has no supported theme");
    AbDataValue record = ab_data_get(menu->theme_data.data.globals, theme)
                             ? *ab_data_get(menu->theme_data.data.globals, theme)
                             : (AbDataValue){0};
    if (record.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing selected level theme definition");
    game->theme = record.table;
    game->draw.current_theme = theme;
    game->music = menu_route_field(record.table, "music");
    return ab_asset_textures_sync(menu->textures,
                                  ab_asset_resources_owners(menu->resources), error);
}

static bool game_particle_sprite(void *context, AbSpan name, uintptr_t *output,
                                 AbError *error) {
    PreviewGame *game = context;
    AbSpriteAtlasBinding sprite;
    (void)error;
    const AbSpriteProvider *provider = &game->menu->sprites.provider;
    *output = provider->sprite(provider->context, name, &sprite)
                  ? (uintptr_t)sprite.image
                  : 0;
    return true;
}

static bool game_sound(void *context, AbBlockCollisionSound kind, size_t object,
                       const AbObjectDefinition *definition, AbDataValue *output,
                       AbError *error) {
    (void)context;
    (void)object;
    (void)error;
    *output = ab_object_property(definition,
                                 menu_span(kind == AB_BLOCK_COLLISION_CONTACT_SOUND
                                               ? "collisionSound"
                                               : "damageSound"));
    return true;
}

static bool game_achievement(void *context, size_t first, size_t second,
                             AbError *error) {
    return ab_classic_collision_achievements_call(
        ((PreviewGame *)context)->achievements, first, second, error);
}

static bool game_start_position(void *context, bool global_class, AbVec2 *output,
                                AbError *error) {
    (void)global_class;
    (void)error;
    *output = ab_classic_sling_start_pose(((PreviewGame *)context)->playfield.sling)
                  .level_start;
    return true;
}

static bool game_collision_print(void *context, AbUiPointerValue value,
                                 AbError *error) {
    (void)context;
    return fprintf(stderr, "Collision callback kind %u\n", (unsigned)value.kind) >= 0 ||
           ab_fail(error, "Cannot write game collision diagnostic");
}

static bool game_collision_binding(void *context, AbDataValue key,
                                   AbUiPointerValue *output, AbError *error) {
    return ab_classic_collision_hooks_binding(&((PreviewGame *)context)->hooks, key,
                                              output, error);
}

static bool game_collision_specialty(void *context, AbUiPointerValue function,
                                     const size_t *flying,
                                     const AbBirdCollisionNamedInput *contact,
                                     AbError *error) {
    return ab_classic_collision_hooks_call(&((PreviewGame *)context)->hooks, function,
                                           flying, contact, error);
}

static bool game_contact_particles(void *context, size_t object, AbDataValue descriptor,
                                   AbError *error) {
    return ab_classic_particles_contact(((PreviewGame *)context)->particles, object,
                                        descriptor, error);
}

static bool game_removal_level(void *context, AbDataValue name,
                               AbObjectRemovalLevel **output, AbError *error) {
    PreviewGame *game = context;
    if (!menu_route_equal(name, game->level_name))
        return ab_fail(error, "Game level-script registry identity changed");
    /* Construction has checked this actual absent source registration. */
    *output = NULL;
    return true;
}

static bool game_bird_volume(void *context, AbDataValue receiver, size_t object,
                             AbDataValue *output, AbError *error) {
    (void)context;
    (void)receiver;
    (void)object;
    (void)error;
    *output = menu_route_number(ab_level_effect_base_bird_volume());
    return true;
}

static bool game_bird_removed(void *context, AbDataValue name, size_t object,
                              AbError *error) {
    (void)context;
    (void)object;
    if (!menu_route_equal(name, game_text("redBirdRemoved")))
        return ab_fail(error, "Unbound game bird-removal hook");
    return ab_classic_red_bird_removed(false, error);
}

static bool game_service(void *context, AbGameTailCall *call, AbError *error) {
    PreviewGame *game = context;
    if (call->operation != AB_GAME_TAIL_DRAW)
        return ab_fail(error, "Unbound reached game service %u",
                       (unsigned)call->operation);
    return ab_classic_game_draw(ab_classic_game_render_draw(game->render),
                                (uint32_t)game->screen.x, (uint32_t)game->screen.y,
                                menu_real(game->globals.time.bits),
                                game->menu->context.draw_list, error);
}

static bool game_result(void *context, AbDataTable *event, AbError *error) {
    PreviewGame *game = context;
    AbDataValue level = menu_route_field(event, "level");
    AbDataValue score = menu_route_field(event, "score");
    if (!menu_route_equal(level, game->level_name) || score.kind != AB_DATA_NUMBER ||
        !isfinite(menu_real(score.bits)))
        return ab_fail(error, "Invalid application level-ended payload");
    /* The ordinary listener pauses before Museum's score processing. */
    game->ended = true;
    game->physics = false;
    game->finish.complete.ending_timer = 0;
    game->globals.last_score_time = menu_route_number(0);
    game->globals.level_timeout = game->disabled;
    if ((game->music.kind == AB_DATA_STRING &&
         !ab_audio_bank_stop(game->menu->audio, game->music.string, error)) ||
        !ab_audio_bank_stop(game->menu->audio, menu_span("wood_rolling"), error) ||
        !ab_audio_bank_stop(game->menu->audio, menu_span("rock_rolling"), error) ||
        !ab_audio_bank_stop(game->menu->audio, menu_span("light_rolling"), error))
        return false;
    game->result_complete = menu_route_truthy(menu_route_field(event, "levelComplete"));
    AbDataValue record = {0};
    const AbDataValue *found = ab_data_get(
        ab_settings_statistics_root(game->menu->highscores), game->level_name);
    if (found)
        record = *found;
    if (record.kind != AB_DATA_NIL && record.kind != AB_DATA_TABLE)
        return ab_fail(error, "Unsupported previous application highscore record");
    game->first_clear = record.kind == AB_DATA_NIL ||
                        !menu_route_truthy(menu_route_field(record.table, "completed"));
    if (!ab_classic_highscores_initialize(game->highscores, level.string, error))
        return false;
    if (game->first_clear && game->result_complete) {
        AbSpan path[] = {level.string, menu_span("completed")};
        if (!ab_settings_statistics_write(game->menu->highscores, path, 2,
                                          game->enabled, error))
            return false;
    }
    AbDataValue used = {.kind = AB_DATA_BOOLEAN,
                        .bits = ab_powerup_uses_count(game->uses) > 0};
    if (!ab_classic_highscores_current(game->highscores, level.string, used,
                                       &game->previous_score, error))
        return false;
    if (game->result_complete &&
        !ab_classic_highscores_improve(game->highscores, level.string,
                                       menu_real(score.bits), game->shot.birds_shot,
                                       used, error))
        return false;
    game->save_pending = true;
    return true;
}

#include "game_skip.h"

static void game_results_free(PreviewGame *game);
static void game_pause_free(PreviewGame *game);
static bool game_pause_open(PreviewGame *game, AbError *error);
static bool game_pause_layout(PreviewGame *game, AbError *error);

static void game_session_free(PreviewMenu *menu) {
    PreviewGame *game = menu->game;
    if (!game)
        return;
    AbError error = {0};
    if (game->result_listening)
        (void)ab_event_manager_remove(menu->events, game_text("EID_LEVEL_ENDED"),
                                      &game->result_listener, &error);
    game_results_free(game);
    game_pause_free(game);
    game_skip_free(game);
    ab_classic_game_render_close(game->render);
    ab_classic_game_runtime_close(game->game);
    ab_classic_highscores_close(game->highscores);
    ab_classic_balloons_close(game->balloons);
    ab_classic_game_updates_close(game->updates);
    (void)ab_classic_world_runtime_close(&game->world, &error);
    ab_classic_collision_achievements_close(game->achievements);
    ab_classic_particles_close(game->particles);
    ab_classic_level_runtime_close(game->level);
    ab_powerup_uses_close(game->uses);
    free(game->bonus_entries);
    free(game);
    menu->game = NULL;
}

#include "game_session_runtime.h"
#include "game_results.h"
#include "game_results_navigation.h"
#include "game_pause.h"

static bool game_session_pointer(PreviewMenu *menu, AbSpan action, AbVec2 point,
                                 AbError *error) {
    PreviewGame *game = menu->game;
    if (!game || game->failed)
        return ab_fail(error, "Missing or failed application game input");
    if (game->results) {
        AbUiPointerReply reply;
        if (!ab_ui_widget_pointer(&game->results->root, action, point, &reply, error))
            return false;
        if (menu_equal(reply.values[0], "RETURN_TO_LEVEL_SELECTION") ||
            menu_equal(reply.values[0], "RESTART_LEVEL") ||
            menu_equal(reply.values[0], "NEXT_LEVEL"))
            game->results->pending_action = reply.values[0];
        return true;
    }
    bool pause_consumed;
    if (!game_pause_pointer(game, action, point, &pause_consumed, error))
        return false;
    if (pause_consumed)
        return true;
    bool consumed;
    if (!game_skip_pointer(game, action, point, &consumed, error))
        return false;
    if (consumed)
        return true;
    game->input.cursor = point;
    if (action.size == 6 && !memcmp(action.data, "LPRESS", 6)) {
        game->pressed_key.value.bits = 1;
        game->held_key.value.bits = 1;
    } else if (action.size == 8 && !memcmp(action.data, "LRELEASE", 8)) {
        game->released_key.value.bits = 1;
        game->held_key.value.bits = 0;
    }
    game->input.touch_count = menu_route_number(game->held_key.value.bits ? 1 : 0);
    return true;
}

static bool game_session_resize(PreviewMenu *menu, AbVec2 screen, AbError *error) {
    PreviewGame *game = menu->game;
    if (!game || (screen.x == game->screen.x && screen.y == game->screen.y))
        return true;
    if (!ab_classic_level_runtime_resize(game->level, screen, error))
        return false;
    game->screen = screen;
    return game_skip_layout(game, error) && game_pause_layout(game, error) &&
           (!game->results || game_results_layout(game, error));
}

static bool game_session_draw(PreviewMenu *menu, float seconds, AbError *error) {
    PreviewGame *game = menu->game;
    if (!game || game->failed || game->updating)
        return ab_fail(error, "Missing, failed or reentrant application game frame");
    game->updating = true;
    ab_draw_list_clear(menu->context.draw_list);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(world.dynamics);
    AbFrameClockPlan plan;
    bool okay =
        ab_classic_simulation_advance(
            dynamics.simulation,
            &(AbFrameClockInput){.seconds = seconds, .physics_enabled = game->physics},
            &plan, error) &&
        ab_event_manager_tick(menu->events, seconds, error) &&
        ab_classic_game_globals_advance_time(&game->globals, seconds, error);
    if (okay && game->save_pending) {
        okay = menu_preview_flush(menu, error);
        if (okay)
            game->save_pending = false;
    }
    if (okay && game->ended && !game->results)
        okay = game_results_open(game, error);
    if (okay && game->results)
        okay = game_results_update(game, seconds, error);
    if (okay && !game->ended && !game->paused) {
        AbClassicGameRuntimeView view = ab_classic_game_runtime_view(game->game);
        okay = ab_classic_game_update(
            view.update, (AbClassicGameTouchInput){0}, seconds, game->globals.time,
            (AbCameraSystemInput){
                .moving_objects = menu_route_truthy(game->globals.has_moving_objects)},
            error);
    } else if (okay && game->paused) {
        okay = game_service(game, &(AbGameTailCall){.operation = AB_GAME_TAIL_DRAW},
                            error);
    } else if (okay) {
        okay = ab_classic_camera_update(game->playfield.camera, seconds,
                                        &(AbCameraSystemInput){0}, error) &&
               game_service(game, &(AbGameTailCall){.operation = AB_GAME_TAIL_DRAW},
                            error);
    }
    if (okay && game->physics)
        okay = ab_particles_update(ab_classic_particles_engine(game->particles),
                                   seconds, false, error) &&
               ab_particles_update_level(ab_classic_particles_engine(game->particles),
                                         seconds, error);
    if (okay)
        okay = ab_particles_update(ab_classic_particles_engine(game->particles),
                                   seconds, true, error);
    if (okay && game->results)
        okay = game_results_draw(game, error);
    else if (okay)
        okay = (game->paused || game_skip_draw(game, seconds, error)) &&
               game_pause_draw(game, seconds, error);
    game->pressed_key.value.bits = game->released_key.value.bits = 0;
    game->updating = false;
    game->failed = !okay;
    menu->time = menu_real(game->globals.time.bits);
    return okay;
}

#endif
