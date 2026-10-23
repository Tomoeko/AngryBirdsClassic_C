#ifndef AB_GAME_SESSION_RUNTIME_H
#define AB_GAME_SESSION_RUNTIME_H

#include "console_common/support/host.h"

enum { GAME_TRAJECTORY_POINTS = 1024 };

typedef struct {
    struct timespec previous;
    bool enabled;
} GameLoadClock;

static bool game_load_phase(PreviewGame *game, GameLoadClock *clock, const char *phase,
                            AbError *error) {
    if (!clock->enabled)
        return true;
    struct timespec current;
    if (!cc_host_time(&current))
        return ab_fail(error, "Cannot sample level-load clock");
    double milliseconds = (double)(current.tv_sec - clock->previous.tv_sec) * 1000 +
                          (double)(current.tv_nsec - clock->previous.tv_nsec) / 1e6;
    fprintf(stderr, "Level load %.*s %s: %.3f ms\n", (int)game->level_name.string.size,
            (const char *)game->level_name.string.data, phase, milliseconds);
    clock->previous = current;
    return true;
}

static bool game_open_level(PreviewGame *game, AbError *error) {
    PreviewMenu *menu = game->menu;
    if (!menu->level_source &&
        !ab_classic_level_source_open(menu->archive, 16 * 1024 * 1024,
                                      &menu->level_source, error))
        return false;
    AbClassicLevelRuntimeConfig config = {
        .archive = menu->archive,
        .source = menu->level_source,
        .level_path = menu->level_request.resource_path,
        .random = &menu->random,
        .world = {.broadphase_limits = {4096, 32768, 32768},
                  .maximum_translation = 2,
                  .maximum_translation_squared = 4,
                  .warm_start = true,
                  .clear_forces = true,
                  .continuous = true},
        .level_limits = {.objects = 2048,
                         .contacts = 8192,
                         .storage_bytes = 32 * 1024 * 1024},
        .resource_bytes = 16 * 1024 * 1024,
        .dynamic_objects_per_bird = 3,
        .prepare_context = game,
        .prepare = game_prepare,
        .playfield = {.context = game,
                      .shot_trails = game_powerup_trails,
                      .sprites = &menu->sprites.provider,
                      .audio = menu->audio,
                      .statistics = menu->settings,
                      .events = menu->events,
                      .powerups = game->uses,
                      .globals = &game->globals,
                      .shot_state = &game->shot,
                      .flying_object = &game->flying,
                      .powerups_enabled = &game->enabled,
                      .red_special_available = &game->disabled,
                      .next_bird_sound = &game->next_sound,
                      .renderer_counter = &game->renderer_counter,
                      .object_blend = AB_BLEND_PREMULTIPLIED,
                      .terrain = &game->terrain,
                      .camera = {.pixel_size = game->screen,
                                 .input_context = game,
                                 .input = game_camera_input,
                                 .absolute_bottom = 200,
                                 .horizontal_leeway = 200,
                                 .sequence_log = game_diagnostic,
                                 .prediction = {.maximum_raycasts = 4096,
                                                .print = game_prediction_log}},
                      .limits = {.visuals = 1024 * 1024,
                                 .scene = 1024 * 1024,
                                 .trajectories = 256 * 1024,
                                 .camera = 1024 * 1024,
                                 .behaviors = 1024 * 1024,
                                 .shots = 256 * 1024,
                                 .services = 65536,
                                 .sling = 65536,
                                 .storage_bytes = 8 * 1024 * 1024,
                                 .name_capacity = 256,
                                 /* Each retained trajectory point owns a renderer. */
                                 .extra_renderers = GAME_TRAJECTORY_POINTS,
                                 .tracks = 128,
                                 .points = GAME_TRAJECTORY_POINTS,
                                 .notifications = 64}}};
    if (!ab_classic_level_runtime_open(config, &game->level, error))
        return false;
    game->playfield =
        ab_classic_playfield_view(ab_classic_level_runtime_view(game->level).playfield);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    game->song = menu->main_song;
    if (level.bird_count > SIZE_MAX / (2 * sizeof(float)))
        return ab_fail(error, "Game bird-bonus capacity overflow");
    game->bonus_entries =
        calloc(level.bird_count ? level.bird_count * 2 : 1, sizeof(float));
    if (!game->bonus_entries)
        return ab_fail(error, "Cannot allocate game bird-bonus scores");
    game->bonus = (AbClassicBirdBonusScores){.entries = game->bonus_entries,
                                             .capacity = level.bird_count * 2};
    game->level_particles = menu_route_field(level.data->globals, "levelParticles");
    game->level_particles_enabled = game->disabled;
    if (game->level_particles.kind == AB_DATA_TABLE &&
        menu_route_truthy(
            menu_route_field(game->level_particles.table, "settingsFrame")))
        game->level_particles_enabled = game->enabled;
    else
        game->level_particles = (AbDataValue){0};
    return true;
}

static bool game_joint_print(void *context, AbSpan name, AbError *error) {
    (void)context;
    return fprintf(stderr, "Destroying joint %.*s\n", (int)name.size, name.data) >= 0 ||
           ab_fail(error, "Cannot write joint destruction diagnostic");
}

static bool game_joint_defer(void *context, AbSpan name, AbError *error) {
    return ab_classic_joint_updates_defer(((PreviewGame *)context)->joint_updates, name,
                                          error);
}

static bool game_open_joints(PreviewGame *game, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t capacity =
        level.joints->storage.joint_capacity + level.joints->storage.glue_capacity;
    if (capacity < 64)
        capacity = 64;
    if (!ab_classic_joint_updates_open(
            (AbClassicJointUpdateConfig){
                .joints = level.joints,
                .level_data = level.data,
                .events = game->menu->events,
                .destroyed_event = menu_event(game->menu, "EID_JOINT_DESTROYED"),
                .context = game,
                .print_destroy = game_joint_print,
                .names = capacity,
                .requests = 65536,
                .storage_bytes = 16 * 1024 * 1024},
            &game->joint_updates, error))
        return false;
    game->tail.joints = ab_classic_joint_updates_tail(game->joint_updates);
    return true;
}

static bool game_open_world(PreviewGame *game, AbError *error) {
    size_t extra_nodes = 1;
    size_t object_count = ab_classic_level_view(game->playfield.level).bodies->count;
    while (extra_nodes < object_count)
        extra_nodes *= 2;
    if (!ab_extra_draws_open(extra_nodes, 1024 * 1024, &game->extra_draws, error))
        return false;
    PreviewMenu *menu = game->menu;
    AbClassicParticleNames names = {game->particle_names, 0, 256, 65536};
    const char *const result_particles[] = {"levelCompleteStars1",
                                            "levelCompleteStars2",
                                            "levelCompleteStars3", "newHighScoreStars"};
    for (size_t index = 0; index < 4; ++index)
        game->particle_names[names.count++] = menu_span(result_particles[index]);
    game->particle_names[names.count++] = menu_span("explosion");
    game->particle_names[names.count++] = menu_span("explosionBuff");
    const char *const powerup_particles[] = {"powerupBuff",
                                             "powerupBurst",
                                             "powerupSmokeBuff",
                                             "superSlingActivation",
                                             "superSlingBurst",
                                             "superSlingTrail",
                                             "superSeedActivation",
                                             "powerPotionExplosion",
                                             "powerPotionBubbles",
                                             "powerPotionTrail",
                                             "smokeBuffBirdsPurple",
                                             "quake",
                                             "quake_smoke",
                                             "woodenBuff",
                                             "rockBuff",
                                             "lightBuff"};
    for (size_t index = 0;
         index < sizeof(powerup_particles) / sizeof(*powerup_particles); ++index)
        game->particle_names[names.count++] = menu_span(powerup_particles[index]);
    if (menu_route_truthy(game->level_particles_enabled)) {
        AbDataValue name = menu_route_field(game->level_particles.table, "particles");
        if (name.kind != AB_DATA_STRING)
            return ab_fail(error, "Missing actual level particle definition");
        game->particle_names[names.count++] = name.string;
    }
    if (!ab_classic_particle_names(game->playfield.level, &names, error) ||
        !ab_classic_particles_open(
            menu->archive,
            (AbClassicParticleConfig){.level = game->playfield.level,
                                      .source = menu->level_source,
                                      .definitions = game->particle_names,
                                      .definition_count = names.count,
                                      .resource_bytes = 16 * 1024 * 1024,
                                      .storage_bytes = 2 * 1024 * 1024,
                                      .capacity = 1024,
                                      .level_capacity = 1024,
                                      .engine_random = &menu->engine_random,
                                      .script_random = &menu->random,
                                      .admission = {75, 150, 0.5f},
                                      .workspace = &game->workspace,
                                      .context = game,
                                      .sprite = game_particle_sprite},
            &game->particles, error) ||
        !ab_classic_collision_hooks_initialize(
            (AbClassicCollisionHookConfig){.red_special_available = &game->disabled,
                                           .print = game_collision_print},
            &game->hooks, error) ||
        !ab_classic_collision_achievements_open(
            (AbClassicCollisionAchievementConfig){.level = game->playfield.level,
                                                  .settings = menu->settings,
                                                  .events = menu->events,
                                                  .eagle_bait_launched =
                                                      &game->eagle_bait_launched,
                                                  .special_game_mode = &game->absent,
                                                  .context = game,
                                                  .start_position = game_start_position,
                                                  .notifications = 128,
                                                  .storage_bytes = 256 * 1024},
            &game->achievements, error))
        return false;
    /* The source registry lookup has been checked by game_scripts_available.
     * No selected level hook is replaced by an empty callback. */
    game->removal_notifications = (AbObjectRemovalNotifyHost){
        .context = game, .level = game_removal_level, .call = game_removal_call};
    AbClassicWorldRuntimeConfig config = {
        .playfield = ab_classic_level_runtime_view(game->level).playfield,
        .object_capacity = ab_classic_level_view(game->playfield.level).bodies->count +
                           3 * ab_classic_level_view(game->playfield.level).bird_count,
        .particles = game->particles,
        .audio = menu->audio,
        .settings = menu->settings,
        .events = menu->events,
        .globals = &game->globals,
        .random = &menu->random,
        .physics_enabled = &game->physics,
        .extra_draws = game->extra_draws,
        .physics_scale = 0.05f,
        .museum = {.context = game, .break_joint = game_joint_defer},
        .blocks = {.sound_force_threshold = &game->threshold,
                   .sprite_workspace = &game->workspace,
                   .sound = game_sound,
                   .particles = {game->particles, NULL, ab_classic_particles_ambient},
                   .storage_bytes = 1024 * 1024},
        .birds = {.flying_object = &game->flying,
                  .specialty_available = &game->globals.specialty_available,
                  .delta_based_slingshot = &game->disabled,
                  .achievement_processor = &game->enabled,
                  .floating_score_scaling = &game->floating_scale,
                  .sound_force_threshold = &game->threshold,
                  .sprite_workspace = &game->workspace,
                  .context = game,
                  .achievement = game_achievement,
                  .print = game_collision_print,
                  .binding = game_collision_binding,
                  .specialty = game_collision_specialty,
                  .contact_particles = game_contact_particles,
                  .damage_particles = {game->particles, NULL,
                                       ab_classic_particles_ambient},
                  .storage_bytes = 2 * 1024 * 1024},
        .dynamics = {.simulation = {.minimum_x = -100000,
                                    .maximum_x = 100000,
                                    .velocity_iterations = 10,
                                    .position_iterations = 10,
                                    .maximum_steps = 4,
                                    .limits = {.storage_bytes = 1024 * 1024,
                                               .queued_forces = 1024,
                                               .glue_pairs = 512}},
                     .physical = {.storage_bytes = 1024 * 1024},
                     .goals = {.disable_check = &game->disabled,
                               .notification_sent = &game->goal_sent,
                               .notifications = 32,
                               .storage_bytes = 65536},
                     .statistics = {.eagle_bait_launched = &game->eagle_bait_launched,
                                    .device_model = &game->model,
                                    .gamer_service = &game->absent,
                                    .total_blocks = &game->total_blocks,
                                    .notifications = 1024,
                                    .event_fields = 8,
                                    .storage_bytes = 1024 * 1024},
                     .removals = {.level_name = &game->level_name,
                                  .context = game,
                                  .explosion = game_explosion,
                                  .notifications = &game->removal_notifications,
                                  .block_increment = 500,
                                  .goal_increment = 5000,
                                  .floating_scaling = 1,
                                  .definition_fields = 256,
                                  .storage_bytes = 4 * 1024 * 1024},
                     .contacts = {.end = ab_classic_contacts_play_mode_end},
                     .effect = AB_CLASSIC_DYNAMICS_BASE_EFFECT,
                     .storage_bytes = 8 * 1024 * 1024},
        .map_nodes = 4096,
        .map_bytes = 1024 * 1024,
        .score_entries = 65536,
        .script_bytes = 1024 * 1024,
        .floating = {512, 128, 256 * 1024},
        .storage_bytes = 32 * 1024 * 1024};
    return ab_classic_world_runtime_open(config, &game->world, error);
}

static bool game_open_update(PreviewGame *game, AbError *error) {
    PreviewMenu *menu = game->menu;
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    game->explosions = (AbClassicExplosions){.level = game->playfield.level,
                                             .visuals = game->playfield.visuals,
                                             .particles = game->particles,
                                             .audio = menu->audio,
                                             .globals = &game->globals,
                                             .shake = &game->shake,
                                             .dead_blocks = world.dead_blocks,
                                             .physical_values = world.physical_values,
                                             .physical_count = world.physical_count,
                                             .workspace = &game->workspace,
                                             .physics_scale = 0.05f,
                                             .base_effect = true};
    if (!ab_classic_game_updates_open(64, 256 * 1024, &game->updates, error) ||
        !ab_classic_game_updates_clear(game->updates, error) ||
        !ab_classic_balloons_open(
            (AbClassicBalloonConfig){.level = game->playfield.level,
                                     .updates = game->updates,
                                     .dead_blocks = world.dead_blocks,
                                     .nodes = 4096,
                                     .storage_bytes = 1024 * 1024},
            &game->balloons, error))
        return false;
    /* Actual inherited NotificationsFrame constructor before UI consumers add
     * children. The complete notification class/GUI remains a separate owner. */
    game->notification_context = menu->context;
    game->notification_context.maximum_depth = 64;
    game->notification_fields.name = menu_text("notifications");
    if (!ab_ui_widget_init(
            &game->notifications, &game->notification_context, AB_UI_WIDGET_FRAME,
            (AbUiWidgetChildren){game->notification_frames, game->notification_draw,
                                 game->notification_pointer, AB_CLASSIC_POWERUPS},
            &game->notification_fields, NULL, NULL, error) ||
        !ab_ui_widget_lifecycle(&game->notifications, AB_UI_FRAME_ENTRY, error))
        return false;
    AbClassicGameRuntimeConfig config = {
        .blues_names = {game->blues_names, sizeof(game->blues_names)},
        .playfield = ab_classic_level_runtime_view(game->level).playfield,
        .world = game->world,
        .particles = game->particles,
        .audio = menu->audio,
        .settings = menu->settings,
        .events = menu->events,
        .random = &menu->random,
        .flying = &game->flying,
        .birds = {.context = game,
                  .specialty_update = game_powerup_bird_update,
                  .level_effect = &game->level_effect,
                  .remove_speed_squared = &game->remove_speed,
                  .remove_time = &game->remove_time,
                  .removed_hook = game_bird_removed,
                  .removal_volume = game_bird_volume,
                  .storage_bytes = 1024 * 1024},
        .conditions = {.prevent_ending = &game->prevent_ending,
                       .bird_sound = &game->bird_sound,
                       .pig_sound = &game->pig_sound,
                       .eagle_launched = &game->eagle_bait_launched,
                       .eagle_timer = &game->absent},
        .effects = {.shake = &game->shake, .hero = &game->hero, .burn = &game->burn},
        .prelude = {.do_not_timeout = &game->absent,
                    .cursor_world = &game->cursor_world,
                    .old_cursor_world = &game->old_cursor_world},
        .start = {.updates = game->updates,
                  .state = &game->start,
                  .release_build = &game->enabled,
                  .started_from_editor = &game->disabled,
                  .level_start_sound = &game->start_sound,
                  .main_menu_song = &game->song,
                  .current_music = &game->music,
                  .physics_enabled = &game->physics,
                  .context = game,
                  .level_update =
                      menu_route_equal(game->level_name, game_text("Level5"))
                          ? game_level5_update
                          : NULL},
        .finish = {.state = &game->finish,
                   .clear_sound = &game->clear_sound,
                   .failed_sound = &game->failed_sound,
                   .notifications = 64,
                   .storage_bytes = 256 * 1024},
        .ending = {.state = &game->ending,
                   .eagle_darkness = &game->globals.eagle_darkness,
                   .bonus = &game->bonus,
                   .stars = menu->star_limits.data.globals,
                   .notifications = 64,
                   .storage_bytes = 256 * 1024},
        .progress = {.key_c = &game->absent,
                     .key_gaming_a = &game->absent,
                     .quad_click = &game->absent,
                     .level_will_end_triggered = &game->will_end,
                     .notifications = 64,
                     .storage_bytes = 256 * 1024},
        .powerups = {.active = &game->active,
                     .passive = &game->passive,
                     .enabled = &game->enabled,
                     .physics_speed_factor = &game->speed,
                     .game_timer = &game->game_timer},
        .level_particles = {.level_particles = &game->level_particles,
                            .enabled = &game->level_particles_enabled,
                            .timer = &game->particle_timer},
        .input = {.state = &game->input,
                  .pressed = &game->pressed,
                  .held = &game->held,
                  .released = &game->released,
                  .screen_width = &game->screen.x},
        .specialty = {.notifications = &game->notifications.tree,
                      .red_available = &game->disabled,
                      .boost = {.force = -925, .physics_scale = 0.05f},
                      .notifications_capacity = 64,
                      .storage_bytes = 256 * 1024},
        .boomerang = {.physics_scale = 0.05f},
        .eggs = {.sprites = &menu->sprites.provider,
                 .default_force = -925,
                 .storage_bytes = 256 * 1024},
        .explosions = &game->explosions,
        .controller = {.level_effects = &game->effects,
                       .effect_kind = AB_CLASSIC_CONTROLLER_EFFECT_BASE,
                       .state = &game->tail,
                       .score = &game->score,
                       .wobble_steps = 64,
                       .storage_bytes = 1024 * 1024,
                       .context = game,
                       .service = game_service},
        .characters_bytes = 2 * 1024 * 1024,
        .touches_bytes = 256 * 1024,
        .touches = 32,
        .storage_bytes = 8 * 1024 * 1024};
    if (!ab_classic_game_runtime_open(config, &game->game, error))
        return false;
    AbClassicGameRuntimeView view = ab_classic_game_runtime_view(game->game);
    if (!ab_classic_flames_bind(ab_classic_block_scripts_view(world.scripts).flames,
                                view.controller, &game->tail.hooks, error))
        return false;
    if (!ab_classic_highscores_open(
            (AbClassicHighscoresConfig){.storage = menu->highscores,
                                        .events = menu->events,
                                        .separate_powerups = &game->disabled,
                                        .started_from_map = &game->absent,
                                        .started_sublevel = &game->absent,
                                        .chapter_service = &game->absent,
                                        .native_gamer_service = &game->absent,
                                        .new_highscore_event =
                                            menu_event(menu, "EID_NEW_HIGHSCORE"),
                                        .notifications = 64,
                                        .storage_bytes = 65536},
            &game->highscores, error))
        return false;
    game->draw.current_game_mode =
        (AbDataValue){.kind = AB_DATA_OBJECT, .object = view.update};
    game->draw.editor_mode = (AbDataValue){.kind = AB_DATA_OBJECT, .object = menu};
    game->draw.level_particles_enabled = game->level_particles_enabled;
    return ab_classic_game_render_open(
        (AbClassicGameRenderConfig){
            .playfield = ab_classic_level_runtime_view(game->level).playfield,
            .powerups = view.powerups,
            .particles = ab_classic_particles_engine(game->particles),
            .provider = &menu->sprites.provider,
            .theme = game->theme,
            .settings = ab_settings_statistics_root(menu->local_settings),
            .extra_draw_functions = &game->extra_draw,
            .extra_draws = game->extra_draws,
            .physics_to_world = &game->physics_to_world,
            .state = &game->draw,
            .basic_font = menu_span("FONT_BASIC"),
            .current_font = &game->font,
            .white_texture = 63,
            .layers_bytes = 1024 * 1024,
            .sling_bytes = 256 * 1024,
            .storage_bytes = 2 * 1024 * 1024,
            .placement_limit = 4096},
        &game->render, error);
}

static bool game_session_open(PreviewMenu *menu, AbError *error) {
    if (menu->game || !menu->level_request_ready || !menu->white_uploaded ||
        !game_scripts_available(&menu->level_request, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error,
                             "Game launch requires its canonical prepared request");
    /* Direct launches must leave the level grid just as intro launches leave
     * their cutscene. Otherwise its live listeners survive gameplay and Menu
     * tries to enter that same subscribed graph a second time. */
    if (menu->entered) {
        if (!ab_ui_widget_lifecycle(menu_active_root(menu), AB_UI_FRAME_EXIT, error))
            return false;
        menu->entered = false;
    }
    /* MuseumMode.loadLevel2711 instructions50..52, after releasing the story
     * view and before constructing this level: res.stopAllAudio(). */
    if (!ab_audio_mixer_all_action(menu->mixer, AB_AUDIO_STOP, error))
        return false;
    menu->level_active = menu->cutscene_active = menu->episode_active = false;
    PreviewGame *game = calloc(1, sizeof(*game));
    if (!game)
        return ab_fail(error, "Cannot allocate application game session");
    menu->game = game;
    game->menu = menu;
    game->screen = menu->screen;
    /* MuseumMode.loadLevel resets and initGame/updateValues defaults. Preserve
     * the application script clock across the menu-to-level transition. */
    game->enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    game->disabled = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    game->prevent_ending = game->disabled;
    game->level_name = menu->level_request.name;
    game->full_version = !menu->free_version;
    game->egg_script = ab_level_egg_script(game->level_name.string);
    if (game->egg_script)
        game->level_removal.on_remove =
            (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = 1};
    game->next_sound = game_text("bird_next_military");
    game->bird_sound = game_text("bird_misc");
    game->pig_sound = game_text("piglette");
    game->start_sound = game_text("level_start_military");
    game->clear_sound = game_text("level_clear_military");
    game->failed_sound = game_text("level_failed_piglets");
    game->song = game_text("title_theme");
    game->model = game_text("android");
    game->physics_to_world = menu_route_number(20);
    game->level_effect =
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &game->effect_fields};
    game->globals = (AbClassicGameGlobals){.time = menu_route_number(menu->time),
                                           .score = menu_route_number(0),
                                           .last_score_time = menu_route_number(0),
                                           .old_score = menu_route_number(0),
                                           .extra_tutorial_timer = menu_route_number(0),
                                           .specialty_available = game->disabled};
    game->input =
        (AbClassicGameInputState){.skip_input = game->disabled,
                                  .skip_trigger_specialty = game->disabled,
                                  .touch_released_after_specialty = game->enabled,
                                  .using_gestures = game->disabled,
                                  .delta_sling = game->disabled,
                                  .accelerometer = game->disabled,
                                  .touch_count = menu_route_number(0),
                                  .cheats_enabled = game->disabled};
    game->ending = (AbClassicLevelEndingState){.skip_ending = game->disabled,
                                               .do_not_wait = game->disabled,
                                               .cheats_enabled = game->disabled,
                                               .level_name = game->level_name,
                                               .score_increment = 10000,
                                               .floating_scale = 1,
                                               .default_delay = 3};
    game->draw.zoom_level = game->draw.old_zoom_level = menu_route_number(0);
    game->tail.time_since_present = true;
    game->threshold = 3;
    game->floating_scale = 1;
    game->remove_time = 1;
    game->remove_speed = 0.0025f;
    game->speed = 1;
    game->pressed_key = (AbDataEntry){game_text("LBUTTON"), game->disabled};
    game->held_key = game->released_key = game->pressed_key;
    game->pressed = (AbDataTable){&game->pressed_key, 1, 1, NULL};
    game->held = (AbDataTable){&game->held_key, 1, 1, NULL};
    game->released = (AbDataTable){&game->released_key, 1, 1, NULL};
    game->workspace =
        (AbStringOrderWorkspace){game->indices, 256, game->order_nodes, 256};
    /* The engine RNG retains its recovered lazy constructor seed. The shared
     * script RNG continues from the menu owner's actual consumption. */
    GameLoadClock clock = {.enabled = getenv("AB_PROFILE_LEVEL_LOAD") != NULL};
    if (clock.enabled && !cc_host_time(&clock.previous)) {
        game_session_free(menu);
        return ab_fail(error, "Cannot start level-load clock");
    }
    bool okay = ab_camera_shake_init(&game->shake) &&
                ab_camera_touch_init(&game->camera_input, &game->camera_shared) &&
                ab_hero_screen_tinter_init(&game->hero) &&
                ab_powerup_uses_open(64, 64, 16384, &game->uses, error) &&
                game_open_level(game, error) && game_open_joints(game, error) &&
                game_load_phase(game, &clock, "resources/world/playfield", error) &&
                game_open_world(game, error) &&
                game_load_phase(game, &clock, "particles/collisions", error) &&
                game_open_update(game, error) && game_skip_open(game, error) &&
                game_pause_open(game, error) && game_powerups_open(game, error) &&
                game_load_phase(game, &clock, "game/update/HUD", error);
    if (okay) {
        game->result_listener =
            (AbEventListener){menu_span("ApplicationLevelResult"), game, game_result};
        okay = ab_event_manager_add(menu->events, game_text("EID_LEVEL_ENDED"),
                                    &game->result_listener, error);
        game->result_listening = okay;
    }
    if (!okay) {
        game_session_free(menu);
        return false;
    }
    /* startLevel24424 delegates to CameraSystem.onStartLevel6389. The input
     * handler/tweens/shake are the fresh actual owners above; select the real
     * initial sequence before the first GameUpdate, without a synthetic tick. */
    AbDataEntry initialized = {menu_data("id"),
                               menu_event(menu, "EID_CAMERAS_INITIALISED")};
    AbDataTable notice = {&initialized, 1, 1, NULL};
    AbCameraSystem *camera = ab_classic_camera_system(game->playfield.camera);
    if (!ab_camera_system_input_enabled(camera, true) ||
        !ab_camera_system_movement_tweens(camera, AB_CAMERA_TWEEN_CUBIC_IN_OUT,
                                          AB_CAMERA_TWEEN_CUBIC_IN_OUT) ||
        !ab_event_manager_notify(menu->events, &notice, error) ||
        !ab_classic_camera_start_sequence(game->playfield.camera,
                                          menu->level_restart
                                              ? "RestartLevelCameraSequence"
                                              : "DefaultLevelStartCameraSequence",
                                          error)) {
        game_session_free(menu);
        return false;
    }
    menu->level_request_ready = false;
    menu->level_restart = false;
    return true;
}

#endif
