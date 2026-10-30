#ifndef AB_MENU_PREVIEW_ASSETS_H
#define AB_MENU_PREVIEW_ASSETS_H

#include "ab/android_assets.h"
#include "ab/audio_output.h"
#include "ab/asset_sprite_provider.h"
#include "ab/dynamic_asset_requirements.h"
#include "ab/target_data.h"
#include "ab/text_resources.h"
#include "ab/ui_main_menu_options.h"
#include "ab/ui_toggle_indicator.h"
#include "ab/ui_widget_resources.h"
#include "ab/episode_catalog.h"
#include "ab/font_asset.h"
#include "ab/ui_episode_selection.h"
#include "ab/ui_episode_services.h"
#include "ab/main_menu_resources.h"
#include "ab/theme_draw.h"
#include "ab/ui_menu_birds.h"
#include "ab/ui_level_selection.h"
#include "ab/ui_level_hidden_eggs.h"
#include "ab/ui_level_rmf.h"
#include "ab/ui_level_pigs.h"
#include "ab/ui_level_eggs.h"
#include "ab/ui_level_services.h"
#include "ab/settings_file.h"
#include "ab/ui_cutscene.h"
#include "ab/classic_root.h"
#include "ab/classic_level_request.h"
#include "ab/classic_level_source.h"
#include "ab/powerup_catalog.h"
#include "ab/sling_scope.h"
#include "console_common/support/directory.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Menu component preview with explicit resource and display profiles.
 * A zero configuration retains the controlled 480x320 research fixture.
 * Interactive hosts select highest_resources and their native drawable size.
 * Startup, service placements,
 * complete root behavior and navigation into levels remain open. */
typedef struct PreviewGame PreviewGame;
typedef struct PreviewReward PreviewReward;
typedef struct PreviewPigs PreviewPigs;
typedef struct PreviewMinigame PreviewMinigame;

typedef struct {
    AbAndroidAssets *planning;
    AbAssetResources *resources;
    AbAssetTextures *textures;
    AbDynamicAssetRequirements requirements;
    AbTargetData event_data;
    AbBuffer text_bytes;
    AbTextCatalog texts;
    AbTextResourceGroup text_group;
    AbTextResources text_resources;
    AbUiLocalizationSource strings;
    AbAudioMixer *mixer;
    AbAudioBank *audio;
    AbAudioOutput *audio_output;
    AbRandom48 random;
    AbEngineRandom engine_random;
    AbAssetSpriteProvider sprites;
    AbUiWidgetResources adapter;
    AbEventManager *events;
    AbUiScaling scaling, cutscene_scaling;
    AbUiImageRenderState render;
    AbUiWidgetContext context;
    AbUiWidget root, play, logo, options_button;
    AbUiMainMenuSlider slider;
    AbUiMainMenuOptions options;
    AbUiToggleIndicator indicator;
    AbUiFrameNode *root_frames[4], *slider_frames[AB_UI_OPTION_COUNT],
        *button_frames[1];
    AbUiDrawChild root_draw[4], slider_draw[AB_UI_OPTION_COUNT], button_draw[1];
    AbUiPointerChild root_pointer[4], slider_pointer[AB_UI_OPTION_COUNT],
        button_pointer[1];
    uint8_t key[32];
    float time, tap_radius;
    AbVec2 screen;
    bool highest_resources;
    bool free_version;
    bool infinite_powerups, keep_powerups_open;
    AbSlingScopeState scope_state;
    bool entered;
    const AbZip *archive;
    AbAssetTextureHost texture_host;
    AbEpisodeCatalog *catalog;
    AbPowerupCatalog *powerup_catalog;
    AbTargetData star_limits;
    AbSettingsStatistics *local_settings, *settings, *highscores;
    const char *state_directory;
    AbSettingsFile *local_file, *settings_file, *highscores_file;
    AbEpisodePreferences preferences;
    AbEpisodeStatistics statistics;
    AbEpisodeProgress progress;
    AbUiEpisodeServices episode_services;
    AbUiEpisodeUgcClass ugc;
    AbUiEpisodeSelection *episodes;
    AbUiLevelSelection *levels;
    PreviewMinigame *minigame;
    AbUiLevelHiddenEggs *hidden_eggs;
    AbUiLevelServices level_services;
    AbLevelProgress level_progress;
    AbClassicLoader *loader;
    AbClassicLoaderState loader_state;
    AbClassicLoaderRequest loader_request;
    AbDataValue loader_time, skip_input, do_loading, show_loading, hide_loading;
    AbDataValue pending_scene;
    AbDataTable *requested_launch;
    bool route_enabled, launch_enabled, launch_pending, level_active, loading_page;
    AbUiCutscene *cutscene;
    AbUiCutsceneClass cutscene_class;
    AbTargetData cutscene_data;
    AbDataTable intro_play, intro_loading, intro_items, intro_end;
    AbDataEntry intro_play_fields[3], intro_loading_fields[3], intro_item,
        intro_end_fields[2];
    AbClassicLoaderCall intro_call, *intro_calls_storage[2];
    AbClassicLoaderCalls intro_calls;
    AbDataTable *pending_intro;
    bool cutscene_active;
    AbDataValue cutscene_group;
    AbClassicRootState root_clicks;
    bool root_pressed, root_released;
    AbClassicLevelRequest level_request;
    bool level_request_ready, level_restart;
    AbFontAsset font_assets[2];
    AbUiFontBinding font_bindings[6];
    AbUiFontResources fonts;
    uint32_t codes[128];
    AbFontGlyphPlacement glyphs[128];
    AbDataTable episode_globals, margin, empty;
    AbDataEntry features[8], margin_fields[2];
    AbEventListener navigation;
    float input_seconds, wheel;
    size_t uploaded_fonts;
    bool navigation_enabled, episode_active, pending_main;
    AbTargetData theme_data;
    AbMainMenuSettings menu_settings;
    AbMainMenuThemeState menu_theme;
    AbThemeLayers *theme_layers;
    AbThemeDraw theme_draw;
    AbThemeLayerRenderState theme_render;
    AbCoordinateCamera menu_camera;
    AbSpan theme_name, requested_effects;
    AbDataValue main_song;
    void *ui_audio_context;
    bool (*ui_audio)(void *context, AbUiPointerValue sound, float volume, bool loop,
                     AbError *error);
    bool pointer_audio_active, pointer_confirm_played;
    uint32_t background_argb;
    float menu_scroll, camera_top;
    bool background_enabled, white_uploaded;
    AbUiMenuBirds birds;
    PreviewGame *game;
    AbClassicLevelSource *level_source;
    AbUiLevelRmf *rmf_levels;
    AbUiLevelEggs *egg_levels;
    PreviewPigs *pigs;
    PreviewReward *reward;
    AbTargetData gift_definition;
    AbParticleSource gift_source;
    AbParticles *gift_particles;
} PreviewMenu;

static AbSpan menu_span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbUiPointerValue menu_text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = menu_span(name)};
}

static AbUiPointerValue menu_number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static float menu_real(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool menu_equal(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           !memcmp(value.text.data, name, value.text.size);
}

static bool menu_dimensions(AbVec2 screen) {
    return isfinite(screen.x) && isfinite(screen.y) && screen.x > 0 && screen.y > 0 &&
           screen.x < 0x1p31f && screen.y < 0x1p31f && floorf(screen.x) == screen.x &&
           floorf(screen.y) == screen.y;
}

static bool menu_environment(void *context, AbUiScalingEnvironment *output,
                             AbError *error) {
    (void)error;
    PreviewMenu *menu = context;
    /* The desktop policy explicitly selects the largest APK reference profile;
     * it stays fixed even when the window is smaller than that profile. */
    *output = (AbUiScalingEnvironment){.screen = menu->screen,
                                       .reference_size = {1024, 768},
                                       .use_scaling = true,
                                       .template_layout = menu->highest_resources};
    return true;
}

static AbUiWidget *menu_minigame_root(PreviewMenu *menu);
static bool menu_minigame_open(PreviewMenu *menu, AbError *error);
static bool menu_minigame_close(PreviewMenu *menu, AbError *error);
static bool menu_minigame_leave(PreviewMenu *menu, AbError *error);
static bool menu_minigame_is_loading(PreviewMenu *menu, AbDataTable *event);
static bool menu_minigame_start_loader(PreviewMenu *menu, AbError *error);
static bool menu_minigame_is_target(PreviewMenu *menu, AbDataValue target);
static bool menu_minigame_enter(PreviewMenu *menu, AbError *error);

static AbUiWidget *menu_active_root(PreviewMenu *menu) {
    if (menu->minigame && menu_minigame_root(menu))
        return menu_minigame_root(menu);
    if (menu->cutscene_active)
        return &menu->cutscene->widget;
    if (menu->level_active)
        return &menu->levels->widget;
    return menu->episode_active ? &menu->episodes->widget : &menu->root;
}

static bool menu_cutscene_environment(void *context, AbUiScalingEnvironment *output,
                                      AbError *error) {
    if (!menu_environment(context, output, error))
        return false;
    /* The story sheets retain the APK's 480x320 canvas. Menu atlas selection
     * must not shrink the comic until the source layout disables its pan. */
    output->reference_size = (AbVec2){480, 320};
    output->template_layout = true;
    return true;
}

static bool menu_route_event(PreviewMenu *menu, AbDataTable *event, AbError *error);
static bool menu_route_apply(PreviewMenu *menu, AbError *error);
static bool menu_route_update(PreviewMenu *menu, AbError *error);
static void menu_route_free(PreviewMenu *menu);
static bool menu_load_theme(void *context, AbSpan theme, AbError *error);
static bool menu_audio_initialize(PreviewMenu *menu, AbError *error);
static bool menu_audio_music(PreviewMenu *menu, AbError *error);
static bool menu_audio_enabled(PreviewMenu *menu, bool *output, AbError *error);
static bool menu_audio_toggle(PreviewMenu *menu, AbError *error);
static bool menu_reward_reveal(PreviewMenu *menu, AbDataTable *event, AbError *error);
static bool menu_reward_show(PreviewMenu *menu, AbError *error);
static bool menu_reward_show_image(PreviewMenu *menu, AbDataValue image,
                                   AbDataValue sound, AbError *error);
static bool menu_reward_show_star(PreviewMenu *menu, bool first, AbError *error);
static bool menu_reward_draw(PreviewMenu *menu, AbError *error);

#include "menu_settings_preview.h"
#include "menu_gift_preview.h"
#include "menu_episode_preview.h"
#include "menu_background_preview.h"
#include "menu_pigs_preview.h"
#include "menu_route_preview.h"
#include "game_session.h"
#include "menu_audio_preview.h"
#include "menu_rewards_preview.h"
#include "menu_minigame.h"

/* MainMenu.layout 31235, non-template Android branch, words 13..128.
 * The component feature profile supplies zero g_layoutMargin. */
static bool menu_main_layout(PreviewMenu *menu, AbError *error) {
    if (!menu->highest_resources)
        return ab_ui_widget_lifecycle(&menu->root, AB_UI_FRAME_LAYOUT, error);
    AbVec2 empty, about;
    float scale;
    if (!ab_ui_scaling_get(&menu->scaling, &scale, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("BUTTON_EMPTY"), false, &empty, error) ||
        !ab_ui_scaling_sprite(&menu->scaling, &menu->context.images,
                              menu_span("BUTTON_ABOUT_SMALL"), false, &about, error))
        return false;
    AbUiFrameFields *play = menu->play.frame ? &menu->play.frame->fields : NULL;
    AbUiFrameFields *logo = &menu->logo.state.image.frame.fields;
    AbUiFrameFields *options = &menu->options_button.state.image.frame.fields;
    AbUiFrameFields *slider = &menu->slider.box.widget.state.image.frame.fields;
    if (!play)
        return ab_fail(error, "Missing concrete menu Play frame");
    play->x = logo->x = menu_number(menu->screen.x * 0.5f);
    play->y = menu_number(menu->screen.y * menu_real(UINT32_C(0x3f0e147b)));
    logo->y = menu_number(menu->screen.y * menu_real(UINT32_C(0x3cb33333)));
    float first = about.x * menu_real(UINT32_C(0x3f933333));
    float second = empty.x * menu_real(UINT32_C(0x3f19999a));
    menu->slider.unscaled_width = (first < second ? first : second) / scale;
    menu->slider.bottom_margin = empty.y * 0.5f;
    menu->slider.element_height = about.y;
    float x = empty.x * menu_real(UINT32_C(0x3f0ccccd));
    float y = menu->screen.y - empty.y * menu_real(UINT32_C(0x3f0ccccd));
    options->x = menu_number(x);
    options->y = slider->y = menu_number(y);
    slider->x = menu_number(x + menu_real(options->w.number_bits) *
                                    menu_real(UINT32_C(0x3c75c28f)) * scale);
    return ab_ui_scaling_set_to(&menu->scaling, play, error) &&
           ab_ui_scaling_set_to(&menu->scaling, logo, error) &&
           ab_ui_scaling_set_to(&menu->scaling, options, error) &&
           ab_ui_widget_lifecycle(&menu->root, AB_UI_FRAME_LAYOUT, error);
}

static bool menu_preview_resize(PreviewMenu *menu, AbVec2 screen, AbError *error) {
    if (!menu_dimensions(screen))
        return ab_fail(error, "Invalid menu drawable dimensions");
    if (menu->screen.x == screen.x && menu->screen.y == screen.y)
        return true;
    menu->screen = screen;
    menu->context.render.viewport_width = (uint32_t)screen.x;
    menu->context.render.viewport_height = (uint32_t)screen.y;
    menu->fonts.screen_width = screen.x;
    if (!ab_ui_scaling_update(&menu->scaling, error) ||
        !ab_ui_scaling_update(&menu->cutscene_scaling, error) ||
        !menu_main_layout(menu, error) ||
        (menu->episode_active &&
         !ab_ui_widget_lifecycle(&menu->episodes->widget, AB_UI_FRAME_LAYOUT, error)) ||
        (menu->level_active &&
         !ab_ui_widget_lifecycle(&menu->levels->widget, AB_UI_FRAME_LAYOUT, error)) ||
        (menu->cutscene_active &&
         !ab_ui_widget_lifecycle(&menu->cutscene->widget, AB_UI_FRAME_LAYOUT, error)) ||
        (menu->minigame && menu_minigame_root(menu) &&
         !ab_ui_widget_lifecycle(menu_minigame_root(menu), AB_UI_FRAME_LAYOUT, error)))
        return false;
    return game_session_resize(menu, screen, error) &&
           (!menu->background_enabled || menu_background_layout(menu, error));
}

static bool menu_graph(PreviewMenu *menu, AbDataValue slider_event, AbError *error) {
    AbUiWidgetContext *context = &menu->context;
    AbUiFrameFields frame = {.name = menu_text("mainMenu")};
    AbUiImageButtonArguments play = {
        .image.frame = {.name = menu_text("playButton"),
                        .x = menu_number(240),
                        .y = menu_number(320 * menu_real(UINT32_C(0x3f0e147b)))},
        .return_value = menu_text("PLAY")};
    AbUiImageArguments logo = {
        .frame = {.name = menu_text("gameLogo"),
                  .x = menu_number(240),
                  .y = menu_number(320 * menu_real(UINT32_C(0x3cb33333)))}};
    AbUiImageButtonArguments options = {
        .image.frame = {.name = menu_text("optionsButton"),
                        .x = menu_number(52),
                        .y = menu_number(260)},
        .return_value = menu_text("TOGGLE_OPTIONS"),
        .activate_on_release = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    /* Explicit APK preview feature inputs; no SDK/settings result is implied.
     * The Android resource set does not contain the desktop fullscreen icon. */
    AbUiMainMenuOptionsSource profile = {.device_model = menu_span("android"),
                                         .notifications_enabled = true};
    if (!ab_ui_widget_init(&menu->root, context, AB_UI_WIDGET_FRAME,
                           (AbUiWidgetChildren){menu->root_frames, menu->root_draw,
                                                menu->root_pointer, 4},
                           &frame, NULL, NULL, error) ||
        !ab_ui_widget_init(&menu->play, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){0}, NULL, NULL, &play, error) ||
        !ab_ui_image_button_state_set_image(
            &menu->play.state, menu_text("BUTTON_TOONS_PLAY"), (AbUiPointerValue){0},
            &context->images, error) ||
        !ab_ui_widget_init(&menu->logo, context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL, &logo, NULL, error) ||
        !ab_ui_image_state_set_image(&menu->logo.state.image, menu_text("MENU_LOGO"),
                                     &context->images, error) ||
        !ab_ui_main_menu_slider_init(
            &menu->slider, context,
            (AbUiWidgetChildren){menu->slider_frames, menu->slider_draw,
                                 menu->slider_pointer, AB_UI_OPTION_COUNT},
            NULL, &menu->scaling, menu->events, slider_event, error) ||
        !ab_ui_main_menu_options_init(&menu->options, &menu->root, &menu->slider,
                                      &profile, error) ||
        !ab_ui_widget_init(&menu->options_button, context, AB_UI_WIDGET_IMAGE_BUTTON,
                           (AbUiWidgetChildren){menu->button_frames, menu->button_draw,
                                                menu->button_pointer, 1},
                           NULL, NULL, &options, error) ||
        !ab_ui_image_button_state_set_image(
            &menu->options_button.state, menu_text("BUTTON_EMPTY"),
            (AbUiPointerValue){0}, &context->images, error) ||
        !ab_ui_toggle_indicator_init(&menu->indicator, context, (AbUiWidgetChildren){0},
                                     error) ||
        !ab_ui_image_state_set_image(&menu->indicator.widget.state.image,
                                     menu_text("BUTTON_OPTIONS"), &context->images,
                                     error))
        return false;
    menu->slider.box.widget.state.image.frame.fields.x = menu_number(52);
    menu->slider.box.widget.state.image.frame.fields.y = menu_number(260);
    menu->indicator.widget.state.image.frame.fields.name = menu_text("optionsImage");
    if (!ab_ui_widget_add(&menu->options_button, &menu->indicator.widget, error) ||
        !ab_ui_widget_add(&menu->root, &menu->slider.box.widget, error) ||
        !ab_ui_widget_add(&menu->root, &menu->play, error) ||
        !ab_ui_widget_add(&menu->root, &menu->logo, error) ||
        !ab_ui_widget_add(&menu->root, &menu->options_button, error) ||
        !ab_ui_widget_lifecycle(&menu->root, AB_UI_FRAME_ENTRY, error))
        return false;
    menu->entered = true;
    return menu_main_layout(menu, error);
}

static void menu_preview_free(PreviewMenu *menu) {
    AbError error = {0};
    ab_audio_output_close(menu->audio_output);
    menu->audio_output = NULL;
    game_session_free(menu);
    ab_classic_level_source_close(menu->level_source);
    menu_reward_free(menu);
    if (menu->entered && !menu->context.failed)
        (void)ab_ui_widget_lifecycle(menu_active_root(menu), AB_UI_FRAME_EXIT, &error);
    (void)menu_minigame_close(menu, &error);
    menu_route_free(menu);
    menu_episode_free(menu);
    ab_particles_close(menu->gift_particles);
    ab_target_data_close(&menu->gift_definition);
    ab_powerup_catalog_close(menu->powerup_catalog);
    menu_background_free(menu);
    (void)ab_ui_scaling_detach(&menu->scaling, &error);
    (void)ab_ui_scaling_detach(&menu->cutscene_scaling, &error);
    ab_event_manager_close(menu->events);
    (void)ab_asset_textures_close(&menu->textures, &error);
    ab_asset_resources_close(menu->resources);
    ab_android_assets_close(menu->planning);
    ab_dynamic_asset_requirements_close(&menu->requirements);
    ab_target_data_close(&menu->event_data);
    ab_audio_mixer_close(menu->mixer);
    ab_audio_bank_close(menu->audio);
    ab_text_free(&menu->texts);
    ab_buffer_free(&menu->text_bytes);
    memset(menu->key, 0, sizeof(menu->key));
    *menu = (PreviewMenu){0};
}

static bool menu_preview_open(const AbZip *zip, AbAssetTextureHost textures,
                              AbDrawList *list, PreviewMenu *menu, AbError *error) {
    menu->archive = zip;
    menu->texture_host = textures;
    if (menu->screen.x == 0 && menu->screen.y == 0)
        menu->screen = (AbVec2){480, 320};
    if (!menu_dimensions(menu->screen))
        return ab_fail(error, "Invalid initial menu display size");
    menu->tap_radius = (15 * menu->screen.x) / 480;
    AbVec2 profile = menu->highest_resources ? (AbVec2){1024, 768} : (AbVec2){480, 320};
    if (!ab_target_resource_key(zip, 16 * 1024 * 1024, menu->key, error) ||
        !ab_android_assets_open(zip, (AbSpan){menu->key, sizeof(menu->key)},
                                (AbAndroidAssetLimits){profile.x, profile.y, false,
                                                       false, false, 64, 8,
                                                       1024 * 1024},
                                &menu->planning, error) ||
        !ab_asset_resources_open(
            zip,
            (AbAssetResourceLimits){128, 32768, 128 * 1024 * 1024, 16 * 1024 * 1024},
            &menu->resources, error) ||
        !ab_asset_textures_open(textures, 60, 128 * 1024 * 1024, &menu->textures,
                                error) ||
        !ab_dynamic_asset_requirements_open(zip, (AbSpan){menu->key, sizeof(menu->key)},
                                            1024 * 1024, &menu->requirements, error) ||
        !ab_target_data_open(zip, "assets/data/scripts_common/events.lua",
                             (AbSpan){menu->key, sizeof(menu->key)}, 1024 * 1024, NULL,
                             &menu->event_data, error))
        return false;
    const char *const groups[] = {"common", "mainMenuRoot"};
    for (size_t group = 0; group < 2; ++group) {
        const AbDataTable *requirements = ab_dynamic_asset_requirements_get(
            &menu->requirements, menu_span(groups[group]));
        if (!requirements ||
            !ab_android_assets_manage(menu->planning, requirements, true,
                                      ab_asset_resources_host, menu->resources, error))
            return false;
    }
    const AbZipEntry *text =
        ab_zip_find(zip, "assets/data/localization/TEXTS_BASIC.dat");
    if (!ab_asset_textures_sync(menu->textures,
                                ab_asset_resources_owners(menu->resources), error) ||
        !text ||
        !ab_zip_read(zip, (size_t)(text - zip->entries), 16 * 1024 * 1024,
                     &menu->text_bytes, error) ||
        !ab_text_read((AbSpan){menu->text_bytes.data, menu->text_bytes.size},
                      8 * 1024 * 1024, &menu->texts, error))
        return false;
    AbSpan locale;
    if (!ab_text_locale_select(menu->texts.locales, menu->texts.locale_count,
                               menu_span("en_US"), &locale, error) ||
        !ab_audio_mixer_open((AbAudioMixerLimits){32, 512, 1, 256 * 1024}, &menu->mixer,
                             error) ||
        !ab_random48_seed(&menu->random, 1) ||
        !ab_audio_bank_open(zip, (AbSpan){menu->key, sizeof(menu->key)},
                            (AbAudioBankConfig){menu->mixer, &menu->random,
                                                64 * 1024 * 1024, 4 * 1024 * 1024,
                                                4 * 1024 * 1024},
                            &menu->audio, error))
        return false;
    /* Native device output is started after resource/bootstrap loading. */
    menu->text_group = (AbTextResourceGroup){menu_span("TEXTS_BASIC"), &menu->texts};
    menu->text_resources = (AbTextResources){&menu->text_group, 1, locale};
    menu->strings = (AbUiLocalizationSource){
        &menu->text_group.name, 1, &menu->text_resources, ab_text_resources_lookup};
    menu->render = (AbUiImageRenderState){.alpha = 1};
    menu->context =
        (AbUiWidgetContext){.render = {.viewport_width = (uint32_t)menu->screen.x,
                                       .viewport_height = (uint32_t)menu->screen.y,
                                       .blend = AB_BLEND_PREMULTIPLIED,
                                       .color = {1, 1, 1, 1}},
                            .render_state = &menu->render,
                            .draw_list = list,
                            .maximum_depth = 32,
                            .animation_installed = true,
                            .scale_buttons_on_touch = true};
    const AbDataValue *resolution =
        ab_data_get(menu->event_data.data.globals,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = menu_span("EID_RESOLUTION_CHANGED")});
    const AbDataValue *slider =
        ab_data_get(menu->event_data.data.globals,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = menu_span("EID_SLIDER_STATE_CHANGED")});
    if (!resolution || !slider)
        return ab_fail(error, "Missing APK UI event definitions");
    if (!ab_asset_sprite_provider_init(menu->resources, menu->textures, &menu->sprites,
                                       error) ||
        !ab_ui_widget_resources_initialize(&menu->sprites.provider, &menu->strings,
                                           menu->audio, &menu->adapter, &menu->context,
                                           error) ||
        !ab_event_manager_open(
            (AbEventManagerLimits){128, 32, 256, 64, 16, 1024 * 1024},
            (AbEventManagerHost){0}, &menu->events, error) ||
        !ab_ui_scaling_init(
            &menu->scaling,
            (AbUiScalingHost){menu->events, *resolution, menu, menu_environment},
            error) ||
        !ab_ui_scaling_init(&menu->cutscene_scaling,
                            (AbUiScalingHost){menu->events, *resolution, menu,
                                              menu_cutscene_environment},
                            error))
        return false;
    menu_audio_bind(menu);
    return menu_graph(menu, *slider, error);
}

static bool menu_pointer_dispatch(PreviewMenu *menu, AbSpan event, AbVec2 point,
                                  AbUiPointerReply *reply, AbError *error) {
    menu->context.cursor = point;
    *reply = (AbUiPointerReply){0};
    if (menu->reward)
        return true;
    if (menu->game)
        return game_session_pointer(menu, event, point, error);
    if (menu->route_enabled &&
        (menu->loader ||
         (menu->skip_input.kind == AB_DATA_BOOLEAN && menu->skip_input.bits)))
        return true;
    AbUiWidget *root = menu_active_root(menu);
    if (!ab_ui_widget_pointer(root, event, point, reply, error))
        return false;
    if (menu->minigame)
        return menu_route_apply(menu, error);
    if (menu->navigation_enabled && menu_equal(reply->values[0], "PLAY")) {
        if ((!menu->episodes && !menu_episode_open(menu, error)) ||
            !menu_episode_change(menu, true, error))
            return false;
    }
    if (menu->pending_main) {
        menu->pending_main = false;
        if (!menu_episode_change(menu, false, error))
            return false;
    }
    if (menu_equal(reply->values[0], "TOGGLE_OPTIONS"))
        return ab_ui_main_menu_slider_toggle(&menu->slider, error) &&
               ab_ui_toggle_indicator_toggle(&menu->indicator, error);
    if (menu_equal(reply->values[0], "TOGGLE_SOUND"))
        return menu_audio_toggle(menu, error);
    return menu_route_apply(menu, error);
}

static bool menu_preview_pointer(PreviewMenu *menu, AbSpan event, AbVec2 point,
                                 AbUiPointerReply *reply, AbError *error) {
    if (menu->pointer_audio_active)
        return ab_fail(error, "Reentrant application pointer dispatch");
    if (event.size == 6 && !memcmp(event.data, "LPRESS", 6))
        menu->root_pressed = true;
    if (event.size == 8 && !memcmp(event.data, "LRELEASE", 8))
        menu->root_released = true;
    menu->pointer_audio_active = true;
    menu->pointer_confirm_played = false;
    bool okay = menu_pointer_dispatch(menu, event, point, reply, error);
    menu->pointer_audio_active = false;
    return okay;
}

static bool menu_preview_draw(PreviewMenu *menu, float seconds, AbError *error) {
    if (!ab_classic_root_double_click(&menu->root_clicks, menu->root_pressed,
                                      menu->root_released, seconds, error))
        return false;
    menu->root_pressed = menu->root_released = false;
    if (!menu_reward_update(menu, seconds, error))
        return false;
    if (!game_results_route(menu, error))
        return false;
    if (menu->level_request_ready) {
        bool soundboard = menu_minigame_name(menu->level_request.name);
        if (!(soundboard ? menu_minigame_open(menu, error)
                         : game_session_open(menu, error)))
            return false;
    }
    if (menu->game)
        return game_session_draw(menu, seconds, error) &&
               menu_gift_tail(menu, seconds, error);
    menu->time += seconds;
    menu->input_seconds = seconds;
    if (!menu_route_update(menu, error))
        return false;
    AbUiWidget *root = menu_active_root(menu);
    if (!ab_ui_widget_update(root, seconds, menu->time, error))
        return false;
    if (!menu_route_apply(menu, error))
        return false;
    root = menu_active_root(menu);
    ab_draw_list_clear(menu->context.draw_list);
    if ((menu->level_active || menu->episode_active) &&
        !ab_ui_level_selection_background(&menu->context, menu->screen, false, error))
        return false;
    if (menu->background_enabled && !menu->cutscene_active && !menu->minigame &&
        !menu_background_draw(menu, seconds, error))
        return false;
    AbUiDrawInput input = {0};
    return (!menu->level_active || !menu->rmf_levels ||
            ab_ui_level_rmf_background(menu->rmf_levels, error)) &&
           ab_ui_widget_draw(root, &input, error) && menu_reward_draw(menu, error) &&
           menu_pigs_tail(menu, seconds, error) && menu_gift_tail(menu, seconds, error);
}

#endif
