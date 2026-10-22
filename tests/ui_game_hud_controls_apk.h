#ifndef AB_UI_GAME_HUD_CONTROLS_APK_TEST_H
#define AB_UI_GAME_HUD_CONTROLS_APK_TEST_H

#include "ab/episode_catalog.h"
#include "ab/classic_pause_actions.h"
#include "ab/ui_game_hud_controls.h"
#include "ab/ui_game_hud_services.h"

typedef struct {
    bool premium, offer;
    size_t queries[8];
} HudConstructionQueries;

/* Observe the enclosing GameHud's still-separate builtin/SDK method boundaries.
 * These are class-construction fixtures, not application service replacements. */
static bool construction_query(void *context, AbUiGameHudControls *controls,
                               AbUiGameHudQuery kind, AbUiPointerValue *output,
                               AbError *error) {
    HudConstructionQueries *fixture = context;
    if ((unsigned)kind >= 8)
        return ab_fail(error, "Unexpected GameHud construction query");
    ++fixture->queries[kind];
    bool value = false;
    if (kind == AB_UI_HUD_PREMIUM)
        value = fixture->premium;
    else if (kind == AB_UI_HUD_OFFER_ON_START) {
        if (!controls->powerups_present || !controls->highscore_present)
            return ab_fail(error,
                           "GameHud offer query preceded its actual graph prefix");
        value = fixture->offer;
    } else if (kind == AB_UI_HUD_POWERUP_VISIBLE || kind == AB_UI_HUD_RMF_VISIBLE)
        value = true;
    *output = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = value};
    return true;
}

static AbDataValue construction_boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value ? 1 : 0};
}

static AbDataValue construction_number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = number(value).number_bits};
}

static AbDataEntry construction_field(const char *name, AbDataValue value) {
    return (AbDataEntry){{.kind = AB_DATA_STRING, .string = span(name)}, value};
}

typedef struct {
    AbClassicPauseActions actions;
    AbUiGameHudServices *live;
    bool audio;
} HudPauseServices;

static bool construction_audio(void *context, bool *output, AbError *error) {
    HudPauseServices *services = context;
    if (services->live)
        return ab_ui_game_hud_services_audio(services->live, output, error);
    (void)error;
    *output = services->audio;
    return true;
}

static bool construction_show_pause(void *context, AbError *error) {
    HudPauseServices *services = context;
    return ab_classic_pause_show(&services->actions, error);
}

static bool construction_scaling(void *context, bool enabled, AbError *error) {
    HudPauseServices *services = context;
    return ab_classic_pause_scaling(&services->actions, enabled, error);
}

static bool construction_pause_actions(AbUiGameHudControls *controls,
                                       HudPauseServices *services, AbDataValue *zoom,
                                       AbError *error) {
    CHECK(ab_ui_game_hud_controls_pause(controls, AB_UI_GAME_HUD_SHOW_PAUSE, error));
    CHECK(!ab_ui_pointer_value_truthy(&controls->pause.widget.frame->fields.visible) &&
          !ab_ui_pointer_value_truthy(&controls->skip.widget.frame->fields.visible) &&
          ab_ui_pointer_value_truthy(
              &controls->pause_page.widget.frame->fields.pause_page_show) &&
          services->actions.config.restarted_from->kind == AB_DATA_STRING &&
          services->actions.config.restarted_from->string.size == 10 &&
          !services->actions.scaling_enabled && services->actions.notice_count == 1);
    if (controls->powerups_present)
        CHECK(!ab_ui_pointer_value_truthy(
                  &controls->powerups.slider.box.widget.frame->fields.visible) &&
              !ab_ui_pointer_value_truthy(
                  &controls->blackbird.button.frame->fields.visible));
    *zoom = construction_number(3);
    CHECK(ab_ui_game_hud_controls_pause(controls, AB_UI_GAME_HUD_INSTANT_HIDE_PAUSE,
                                        error) &&
          controls->pause_page.extend_state == 0 &&
          !ab_ui_pointer_value_truthy(
              &controls->pause_page.widget.frame->fields.visible) &&
          ab_ui_pointer_value_truthy(&controls->pause.widget.frame->fields.visible) &&
          services->actions.scaling_enabled &&
          zoom->bits == construction_number(1.25f).bits);
    CHECK(ab_ui_game_hud_controls_pause(controls, AB_UI_GAME_HUD_INSTANT_SHOW_PAUSE,
                                        error) &&
          controls->pause_page.extend_state == 1 &&
          ab_ui_pointer_value_truthy(
              &controls->pause_page.widget.frame->fields.visible) &&
          services->actions.notice_count == 2 && !services->actions.scaling_enabled);
    CHECK(ab_ui_game_hud_controls_pause(controls, AB_UI_GAME_HUD_INSTANT_HIDE_PAUSE,
                                        error) &&
          services->actions.scaling_enabled);
    /* This fixture deliberately owns no gameplay sling. The actual Page hide,
     * scaling restoration and fresh queued event must precede that failure. */
    CHECK(!ab_ui_game_hud_controls_pause(controls, AB_UI_GAME_HUD_RETURN_TO_GAME,
                                         error) &&
          controls->widget.context->failed && !controls->pause_active &&
          strstr(error->message, "slingshot owner") && controls->notice_count == 1 &&
          ab_event_manager_view(controls->host.pause.events).queued_events == 1 &&
          services->actions.scaling_enabled &&
          !ab_ui_pointer_value_truthy(
              &controls->pause_page.widget.frame->fields.pause_page_show));
    const AbDataValue *id =
        ab_data_get(&controls->host.notices[0].table,
                    (AbDataValue){.kind = AB_DATA_STRING, .string = span("id")});
    const AbDataValue *expected =
        ab_data_get(controls->host.pause.event_ids,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = span("EID_BACK_TO_GAME_CLICKED")});
    CHECK(id && expected && id->kind == expected->kind && id->bits == expected->bits &&
          !ab_ui_game_hud_controls_pause(controls, AB_UI_GAME_HUD_SHOW_PAUSE, NULL));
    error->message[0] = 0;
    CHECK(ab_event_manager_tick(controls->host.pause.events, 0, error) &&
          ab_event_manager_view(controls->host.pause.events).queued_events == 0);
    return true;
}

static bool construction_profile(Resources *resources, const AbDataTable *episodes,
                                 const AbClassicLevelRequest *request,
                                 AbEventManager *events, AbClassicLevelFlow *flow,
                                 AbAudioBank *audio,
                                 AbSettingsStatistics *local_settings,
                                 AbUiGameHudControls *controls, unsigned profile,
                                 AbError *error) {
    Fixture fixture;
    CHECK(initialize(&fixture, resources, false, false, false, false, error));
    fixture.scaling = (AbUiScaling){0};
    CHECK(ab_ui_scaling_init(
        &fixture.scaling,
        (AbUiScalingHost){events, pause_id(resources, "EID_RESOLUTION_CHANGED"),
                          &fixture, environment},
        error));
    AbDataTable empty = {0};
    AbDataEntry purchased_field = construction_field(
        "redsMightyFeathersPurchased", construction_boolean(profile == 5));
    AbDataEntry unlocked_field = construction_field("shockwavePowerupUnlocked",
                                                    construction_boolean(profile == 5));
    AbDataTable flags = {&purchased_field, 1, 1, NULL};
    AbDataTable synced_flags = {&unlocked_field, 1, 1, NULL};
    AbDataEntry settings_fields[] = {
        construction_field("flagTable",
                           (AbDataValue){.kind = AB_DATA_TABLE, .table = &flags}),
        construction_field("syncedFlagTable", (AbDataValue){.kind = AB_DATA_TABLE,
                                                            .table = &synced_flags})};
    AbDataTable settings = {settings_fields, 2, 2, NULL};
    AbDataEntry fields[] = {
        construction_field("deviceModel", (AbDataValue){.kind = AB_DATA_STRING,
                                                        .string = span("android")}),
        construction_field("levelName", request->name),
        construction_field("g_powerups_enabled", construction_boolean(profile != 2)),
        construction_field("g_use_ads", construction_boolean(profile != 2)),
        construction_field("hasIaps", construction_boolean(profile != 1)),
        construction_field("g_separate_powerup_highscore", construction_boolean(true)),
        construction_field("g_show_ingame_black_bird_button",
                           construction_boolean(true)),
        construction_field("g_use_plus_signs_on_powerups", construction_boolean(true)),
        construction_field("birdsShot", construction_number(0)),
        construction_field("birdCount", construction_number(3)),
        construction_field("g_freeEagleUnlockStatus",
                           (AbDataValue){.kind = AB_DATA_TABLE, .table = &empty}),
        construction_field("g_isAccelerometerControl",
                           construction_boolean(profile == 1)),
        construction_field("g_tutorialActive", (AbDataValue){0}),
        construction_field("blockTutorialPopups", (AbDataValue){0}),
        construction_field("currentEpisode", request->episode_id),
        construction_field("g_level_completing", (AbDataValue){0}),
        construction_field("eagleBaitLaunched", (AbDataValue){0})};
    AbDataTable globals = {fields, sizeof(fields) / sizeof(*fields),
                           sizeof(fields) / sizeof(*fields), NULL};
    AbUiEagleServices services = {.progress = {.episodes = episodes,
                                               .local_settings = &empty,
                                               .globals = &globals,
                                               .purchases = &empty},
                                  .settings = &settings,
                                  .level = request};
    AbUiGameHudServices hud_services = {.eagle = &services};
    if (profile == 5) {
        hud_services.local_settings = local_settings;
        services.progress.local_settings = ab_settings_statistics_root(local_settings);
    }
    FontGetterFixture global = {.value = text("FONT_BASIC")};
    AbUiFontGetter getter = {&global, font_getter};
    resources->fonts.getters = &getter;
    resources->fonts.getter_count = 1;
    AbUiTextState eagle_lines[16], pause_lines[16];
    char eagle_names[16][40], pause_names[16][40];
    AbFontWrappedLine eagle_wrapped[4], pause_wrapped[4];
    AbSpan eagle_labels[4], pause_labels[4];
    uint32_t eagle_codes[64], pause_codes[64];
    uint8_t eagle_bytes[512], pause_bytes[512];
    AbUiPauseNotice notices[16];
    AbUiGameHudNotice hud_notices[4];
    AbClassicPauseNotice common_notices[4];
    AbDataValue restarted = {0}, zoom = construction_number(1.25f), typo = {0};
    HudPauseServices pause_services = {0};
    if (profile == 5)
        pause_services.live = &hud_services;
    CHECK(ab_classic_pause_actions_init(
        &pause_services.actions,
        (AbClassicPauseActionConfig){.audio = audio,
                                     .events = events,
                                     .event_ids = resources->events.data.globals,
                                     .restarted_from = &restarted,
                                     .zoom_level = &zoom,
                                     .sotred_zoom_level = &typo,
                                     .notices = common_notices,
                                     .notice_capacity = 4},
        error));
    AbEventTimerEntry timers[4];
    AbDataValue time = {0}, music = {0}, input = {0};
    AbDataValue requested = construction_boolean(profile != 2 && profile != 4);
    AbVec2 margin = {0};
    AbUiPausePageEnvironment pause = {.powerups_enabled = profile != 2,
                                      .purchase_elements = true,
                                      .use_scaling = true,
                                      .width_ratio = 0.2f,
                                      .device_model = span("android"),
                                      .menu_font = text("FONT_MENU")};
    HudConstructionQueries queries = {.premium = profile == 1, .offer = profile == 3};
    AbUiGameHudControlsHost host = {
        .globals = &globals,
        .level = request,
        .powerups = fixture.owner.host,
        .eagle = {.scaling = &fixture.scaling,
                  .fonts = &resources->fonts,
                  .events = events,
                  .event_ids = resources->events.data.globals,
                  .basic_font_getter = {.kind = AB_UI_POINTER_FUNCTION,
                                        .function = (uintptr_t)&getter},
                  .timer_storage = {eagle_lines, eagle_names, 16, eagle_wrapped,
                                    eagle_labels, 4, eagle_codes, 64, eagle_bytes,
                                    512}},
        .pause = {.scaling = &fixture.scaling,
                  .fonts = &resources->fonts,
                  .events = events,
                  .event_ids = resources->events.data.globals,
                  .screen = &fixture.screen,
                  .margin = &margin,
                  .environment = &pause,
                  .time = &time,
                  .tutorial_active = &fields[12].value,
                  .current_music = &music,
                  .skip_input = &input,
                  .notices = notices,
                  .notice_capacity = 16,
                  .timers = timers,
                  .timer_capacity = 4,
                  .label_storage = {pause_lines, pause_names, 16, pause_wrapped,
                                    pause_labels, 4, pause_codes, 64, pause_bytes, 512},
                  .context = &pause_services,
                  .audio_enabled = construction_audio,
                  .level_display = pause_display,
                  .show_pause_menu = construction_show_pause,
                  .scaling_enabled = construction_scaling},
        .flow = profile == 2 ? flow : NULL,
        .notices = hud_notices,
        .notice_capacity = 4,
        .tutorials_requested = &requested,
        .restart_control = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = profile == 1},
        .default_profile = 1024,
        .context = &queries,
        .query = construction_query};
    CHECK(ab_ui_eagle_services_bind(&services, &host.eagle, error));
    if (profile == 5) {
        host.context = &hud_services;
        host.query = ab_ui_game_hud_services_query;
    }
    AbUiFrameFields arguments = {.name = text("callerName")};
    bool okay = ab_ui_game_hud_controls_init(
        controls, &fixture.context, &arguments,
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = profile == 5},
        host, error);
    if (profile == 3) {
        CHECK(!okay && fixture.context.failed && controls->powerups_present &&
              controls->highscore_present && !controls->eagle_present &&
              controls->hud_button_count == 2 && !controls->pause.widget.initialized &&
              strstr(error->message, "SaleBanner"));
        error->message[0] = 0;
        CHECK(ab_ui_scaling_detach(&fixture.scaling, error));
        return true;
    }
    if (profile == 4) {
        CHECK(!okay && fixture.context.failed && requested.kind == AB_DATA_BOOLEAN &&
              requested.bits && controls->pause_page.initialized &&
              !controls->allow_pause && strstr(error->message, "LevelStartScreenFlow"));
        error->message[0] = 0;
        CHECK(ab_ui_scaling_detach(&fixture.scaling, error));
        return true;
    }
    CHECK(okay && controls->initialized && controls->allow_pause &&
          equal(controls->widget.frame->fields.name, "gameHud") &&
          equal(controls->text_group, "TEXTS_BASIC") &&
          equal(controls->text_id, "MI_SCORE") &&
          controls->restart_present == (profile == 1) &&
          controls->ads_present == (profile != 1 && profile != 2) &&
          controls->powerups_present == (profile != 2) &&
          controls->blackbird_present == (profile != 2) &&
          controls->eagle_present == (profile != 1) &&
          controls->highscore_present == (profile != 2) &&
          controls->rmf_present == (profile == 5) &&
          controls->pause_page.widget.tree.parent == &controls->widget.tree &&
          controls->widget.frame->child_count == (profile == 0   ? 8u
                                                  : profile == 1 ? 7u
                                                  : profile == 5 ? 9u
                                                                 : 4u) &&
          controls->hud_button_count == (profile == 0   ? 5u
                                         : profile == 1 ? 4u
                                         : profile == 5 ? 6u
                                                        : 3u));
    if (profile != 2) {
        CHECK(controls->blackbird.button.frame->child_count == 3 &&
              controls->blackbird_frames[0] == &controls->blackbird.background.tree &&
              controls->blackbird_frames[1] == &controls->blackbird.tag.tree &&
              controls->blackbird_frames[2] == &controls->electricity.widget.tree &&
              equal(controls->blackbird.background.state.return_value,
                    "BB_BUTTON_CLICKED") &&
              controls->electricity.frames == &controls->electricity_frames &&
              controls->electricity_frames.count == 6 &&
              real(controls->electricity.widget.frame->fields.scale_x) == 0.75f &&
              !ab_ui_pointer_value_truthy(
                  &controls->highscore_indicator.frame->fields.visible));
    }
    if (profile == 1)
        CHECK(
            !ab_ui_pointer_value_truthy(&controls->pause.widget.state.hover_enabled) &&
            !ab_ui_pointer_value_truthy(&controls->restart.widget.state.hover_enabled));
    if (profile == 2) {
        AbClassicLevelFlowView view = ab_classic_level_flow_view(flow);
        CHECK(view.staging.count == 1 && view.staging.items[0]->appearing == 30 &&
              view.staging.items[0]->listener == &controls->tutorials &&
              !controls->can_show_tutorials && requested.bits);
        AbDataValue done;
        CHECK(controls->tutorials.on_show(controls->tutorials.context, error) &&
              controls->can_show_tutorials &&
              controls->tutorials.is_done(controls->tutorials.context, &done, error) &&
              done.kind == AB_DATA_BOOLEAN && done.bits);
        fields[12].value = construction_boolean(false);
        CHECK(controls->tutorials.is_done(controls->tutorials.context, &done, error) &&
              done.kind == AB_DATA_BOOLEAN && !done.bits);
    }
    const AbVec2 screens[] = {{1920, 1080}, {1600, 1200}, {900, 1600}};
    for (size_t index = 0; index < 3; ++index) {
        fixture.screen = screens[index];
        CHECK(ab_ui_scaling_update(&fixture.scaling, error) &&
              ab_ui_game_hud_controls_layout(controls, error));
        float scale = fixture.scaling.default_scaling;
        float px = real(controls->pause.widget.state.image.resource_pivot_x);
        float py = real(controls->pause.widget.state.image.resource_pivot_y);
        float pause_y = (py * 1.1f) * scale;
        CHECK(real(controls->pause.widget.frame->fields.x) == (px * 1.1f) * scale &&
              real(controls->pause.widget.frame->fields.y) == pause_y &&
              real(controls->pause.widget.frame->fields.scale_x) == scale &&
              real(controls->skip.widget.frame->fields.x) ==
                  fixture.screen.x -
                      (real(controls->skip.widget.frame->fields.w) * 0.55f) * scale &&
              real(controls->skip.widget.frame->fields.y) ==
                  fixture.screen.y -
                      (real(controls->skip.widget.frame->fields.h) * 0.55f) * scale);
        if (controls->eagle_present)
            CHECK(real(controls->eagle.widget.frame->fields.x) == (scale * px) * 3.2f &&
                  real(controls->eagle.widget.frame->fields.y) == pause_y);
        if (controls->powerups_present)
            CHECK(real(controls->powerups.slider.box.widget.frame->fields.y) ==
                      pause_y &&
                  real(controls->blackbird.button.frame->fields.y) ==
                      pause_y + real(controls->pause.widget.frame->fields.h) * scale &&
                  real(controls->blackbird.tag.frame->fields.scale_x) == 1.2f);
    }
    if (profile == 5) {
        AbUiPointerValue result = text("retained");
        CHECK(ab_ui_game_hud_services_query(&hud_services, controls, AB_UI_HUD_PREMIUM,
                                            &result, error) &&
              result.kind == AB_UI_POINTER_NONE);
        fields[2].value = (AbDataValue){0};
        CHECK(ab_ui_game_hud_services_query(
                  &hud_services, controls, AB_UI_HUD_POWERUP_VISIBLE, &result, error) &&
              result.kind == AB_UI_POINTER_NONE);
        fields[2].value = construction_number(0);
        controls->hide_powerups = number(0);
        CHECK(ab_ui_game_hud_services_query(
                  &hud_services, controls, AB_UI_HUD_POWERUP_VISIBLE, &result, error) &&
              result.kind == AB_UI_POINTER_BOOLEAN && !result.boolean);
        controls->hide_powerups = (AbUiPointerValue){0};
        CHECK(ab_ui_game_hud_services_query(
                  &hud_services, controls, AB_UI_HUD_POWERUP_VISIBLE, &result, error) &&
              result.boolean);
        fields[15].value = construction_number(0);
        CHECK(ab_ui_game_hud_services_query(&hud_services, controls,
                                            AB_UI_HUD_RMF_VISIBLE, &result, error) &&
              !result.boolean);
        fields[15].value = (AbDataValue){0};
        purchased_field.value = construction_boolean(false);
        /* A false RMF predicate leaves its previous visible state untouched in
         * instantlyHide; the source calls setVisible only on a true result. */
        controls->rmf.frame->fields.visible = number(7);
        CHECK(ab_ui_game_hud_controls_pause(controls, AB_UI_GAME_HUD_INSTANT_HIDE_PAUSE,
                                            error) &&
              real(controls->rmf.frame->fields.visible) == 7);
        purchased_field.value = construction_number(0);
        CHECK(ab_ui_game_hud_services_query(&hud_services, controls,
                                            AB_UI_HUD_RMF_PURCHASED, &result, error) &&
              result.kind == AB_UI_POINTER_NUMBER && result.number_bits == 0);
        settings_fields[1].value = construction_number(0);
        result = text("retained");
        CHECK(!ab_ui_game_hud_services_query(&hud_services, controls,
                                             AB_UI_HUD_SHOCKWAVE_VISIBLE, &result,
                                             error) &&
              equal(result, "retained") && !fixture.context.failed);
        settings_fields[1].value =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &synced_flags};
        error->message[0] = 0;
    }
    CHECK(construction_pause_actions(controls, &pause_services, &zoom, error) &&
          ab_ui_scaling_detach(&fixture.scaling, error));
    return true;
}

static bool game_hud_controls(Resources *resources, const AbZip *archive, AbSpan key,
                              AbError *error) {
    AbEpisodeCatalog *catalog = NULL;
    AbAudioBank *audio = NULL;
    AbAudioMixer *mixer = NULL;
    AbRandom48 random = {0};
    AbSettingsStatistics *local_settings = NULL;
    bool okay = ab_episode_catalog_open(
        archive, key, (AbEpisodeCatalogConfig){.device_model = span("android")},
        4 * 1024 * 1024, &catalog, error);
    if (okay)
        okay = ab_episode_catalog_annotate_levels(catalog, error);
    if (okay)
        okay = ab_settings_statistics_open(
                   (AbSettingsStatisticsConfig){span("hud-device"), span("hud-install"),
                                                2, 16, 65536},
                   &local_settings, error) &&
               ab_random48_seed(&random, 7) &&
               ab_audio_mixer_open((AbAudioMixerLimits){32, 512, 1, 256 * 1024}, &mixer,
                                   error) &&
               ab_audio_bank_open(archive, key,
                                  (AbAudioBankConfig){mixer, &random, 64 * 1024 * 1024,
                                                      4 * 1024 * 1024, 4 * 1024 * 1024},
                                  &audio, error);
    const AbDataTable *episodes = ab_episode_catalog_episodes(catalog);
    AbDataEntry fields[] = {
        construction_field("episode", construction_number(1)),
        construction_field("page", construction_number(1)),
        construction_field("level", construction_number(1)),
        construction_field("levelName", (AbDataValue){.kind = AB_DATA_STRING,
                                                      .string = span("Level1")})};
    AbDataTable metadata = {fields, 4, 4, NULL};
    AbClassicLevelRequest request;
    if (okay)
        okay = ab_classic_level_request_resolve(episodes, &metadata, &request, error);
    for (unsigned profile = 0; okay && profile < 6; ++profile) {
        AbEventManager *events = NULL;
        AbClassicLevelFlow *flow = NULL;
        AbUiGameHudControls controls = {0};
        okay = ab_event_manager_open((AbEventManagerLimits){16, 4, 8, 8, 8, 65536},
                                     (AbEventManagerHost){0}, &events, error);
        if (okay && profile == 2) {
            /* Only actual flow construction/push are reached in this fixture;
             * no scene queries, pause service or flow update is substituted. */
            okay =
                ab_classic_level_flow_open(
                    (AbClassicLevelFlowConfig){
                        .events = events,
                        .loading_init = pause_id(resources, "EID_LEVEL_LOADING_INIT"),
                        .loading_done = pause_id(resources, "EID_LEVEL_LOADING_DONE"),
                        .flow_done =
                            pause_id(resources, "EID_LEVEL_START_SCREEN_FLOW_DONE"),
                        .requests = 8,
                        .queue_generations = 4,
                        .notifications_count = 8,
                        .storage_bytes = 65536,
                        .depth_limit = 8,
                        .work_limit = 128},
                    &flow, error) &&
                ab_classic_level_flow_initialize(flow, error);
        }
        if (okay)
            okay =
                construction_profile(resources, episodes, &request, events, flow, audio,
                                     local_settings, &controls, profile, error);
        ab_classic_level_flow_close(flow);
        ab_event_manager_close(events);
        resources->fonts.getters = NULL;
        resources->fonts.getter_count = 0;
    }
    ab_episode_catalog_close(catalog);
    ab_settings_statistics_close(local_settings);
    ab_audio_mixer_close(mixer);
    ab_audio_bank_close(audio);
    return okay;
}

#endif
