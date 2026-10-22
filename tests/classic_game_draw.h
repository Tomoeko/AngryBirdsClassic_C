#ifndef AB_TEST_CLASSIC_GAME_DRAW_H
#define AB_TEST_CLASSIC_GAME_DRAW_H

#include "ab/classic_game_draw.h"
#include "../tools/theme_preview_assets.h"

typedef struct {
    PreviewTheme theme;
    AbClassicGameDraw owner;
    AbClassicGameDrawState state;
    AbClassicSlingDraw *sling;
    AbClassicGamePowerups powerups;
    AbClassicPowerupUpdates active, passive;
    AbClassicPowerupUpdate *active_items[4], *passive_items[4];
    AbDataTable settings, extras, mode, editor;
    AbDataValue enabled, cheats, skip, physics_to_world;
    float speed, timer;
    AbSpan font;
} LifecycleGameDraw;

#include "classic_game_draw_native.h"

static bool
lifecycle_game_draw_open(ShotTest *shot, const AbZip *archive, AbClassicShots *shots,
                         AbClassicSling *sling, AbClassicVisuals *visuals,
                         AbClassicBehaviors *behaviors, AbClassicRenderScene *scene,
                         AbClassicCamera *camera, const AbSpriteProvider *provider,
                         AbParticles *particles, LifecycleGameDraw *output,
                         AbError *error) {
    *output = (LifecycleGameDraw){0};
    uint8_t key[32] = {0};
    size_t images = ab_sprite_atlas_count(provider->context);
    bool loaded = images == 4 &&
                  ab_target_resource_key(archive, 16 * 1024 * 1024, key, error) &&
                  theme_preview_open(archive, shot->level.data->globals,
                                     (AbSpan){key, sizeof(key)}, (uint32_t)images + 1,
                                     &output->theme, error);
    memset(key, 0, sizeof(key));
    CHECK(loaded);
    CHECK(theme_preview_texture_count(&output->theme) == 5);
    for (size_t index = 0; index < 5; ++index) {
        uint32_t texture;
        const AbImage *image;
        CHECK(theme_preview_texture(&output->theme, index, &texture, &image) &&
              texture == images + index + 1 && image && image->width && image->height);
    }
    output->physics_to_world = field(shot->level.data->globals, "physicsToWorld");
    CHECK(ab_classic_sling_draw_open(
        (AbClassicSlingDrawConfig){sling, shots, provider, &output->physics_to_world,
                                   &output->theme.render.alpha, 65536},
        &output->sling, error));
    output->active = (AbClassicPowerupUpdates){output->active_items, 4};
    output->passive = (AbClassicPowerupUpdates){output->passive_items, 4};
    output->enabled = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    output->speed = 1;
    CHECK(ab_classic_game_powerups_initialize(
        (AbClassicGamePowerupConfig){.sling = sling,
                                     .active = &output->active,
                                     .passive = &output->passive,
                                     .enabled = &output->enabled,
                                     .cheats_enabled = &output->cheats,
                                     .released_keys = &output->settings,
                                     .physics_speed_factor = &output->speed,
                                     .skip_input = &output->skip,
                                     .game_timer = &output->timer},
        &output->powerups, error));
    output->state.current_theme = field(shot->level.data->globals, "theme");
    output->state.current_game_mode =
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &output->mode};
    output->state.editor_mode =
        (AbDataValue){.kind = AB_DATA_TABLE, .table = &output->editor};
    output->state.zoom_level = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(2)};
    AbDataValue theme = field(output->theme.data.data.globals, "theme1");
    CHECK(theme.kind == AB_DATA_TABLE);
    CHECK(ab_classic_game_draw_initialize(
        (AbClassicGameDrawConfig){.level = shot->owner,
                                  .visuals = visuals,
                                  .behaviors = behaviors,
                                  .scene = scene,
                                  .camera = camera,
                                  .sling = output->sling,
                                  .powerups = &output->powerups,
                                  .particles = particles,
                                  .provider = provider,
                                  .theme = &output->theme.draw,
                                  .theme_definition = theme.table,
                                  .local_settings = &output->settings,
                                  .extra_draw_functions = &output->extras,
                                  .state = &output->state,
                                  .basic_font = span("FONT_BASIC"),
                                  .current_font = &output->font},
        &output->owner, error));
    return true;
}

typedef struct {
    LifecycleGameDraw *draw;
    unsigned phase;
} LifecyclePowerupDraw;

static bool lifecycle_powerup_draw_passive(void *context, float zoom, AbDrawList *list,
                                           AbError *error) {
    LifecyclePowerupDraw *test = context;
    (void)error;
    CHECK(isfinite(zoom) && !test->phase && list->count &&
          !ab_classic_game_draw(&test->draw->owner, 480, 320, 4, list, NULL));
    test->phase = 1;
    return true;
}

static bool lifecycle_powerup_draw_active(void *context, float zoom, AbDrawList *list,
                                          AbError *error) {
    LifecyclePowerupDraw *test = context;
    (void)zoom;
    (void)list;
    (void)error;
    CHECK(test->phase == 1);
    test->phase = 2;
    return true;
}

static bool lifecycle_powerup_draw_alpha(void *context, float zoom, AbDrawList *list,
                                         AbError *error) {
    LifecycleGameDraw *draw = context;
    (void)zoom;
    (void)list;
    (void)error;
    draw->theme.render.alpha = 0.25f;
    return true;
}

/* Compose actual loaded producers, then probe source gates/partial failures.
 * These controls establish C integration, not complete native frame fidelity. */
static bool lifecycle_game_draw_checks(LifecycleGameDraw *draw, AbDrawList *list,
                                       AbError *error) {
    CHECK(lifecycle_game_draw_native(draw, list, error));
    AbCoordinateCamera camera =
        ab_classic_camera_view(draw->owner.config.camera).system.viewport->rendering;
    list->count = 0;
    CHECK(theme_preview_draw(&draw->theme, camera, false, list, error) && list->count);
    float clear[4];
    theme_preview_clear(&draw->theme, clear);
    CHECK(draw->theme.background == UINT32_C(0xff92ccde) && clear[3] == 1);
    list->count = 0;
    CHECK(ab_classic_game_draw(&draw->owner, 480, 320, 4, list, error) && list->count &&
          draw->state.old_zoom_level.bits == bits(2) && draw->font.size == 10 &&
          !memcmp(draw->font.data, "FONT_BASIC", 10));
    size_t ordinary_count = list->count;
    AbDrawCommand ordinary_first = list->commands[0];
    CHECK(ordinary_first.texture == draw->theme.white_texture &&
          ordinary_first.blend == AB_BLEND_OPAQUE &&
          ordinary_first.quad.vertices[0].x == -1 &&
          ordinary_first.quad.vertices[0].y == 1);
    /* Truthy zero quality suppresses background/layer particles. Foreground
     * still executes, including its alpha reset even when ground is culled. */
    AbDataEntry quality = {{.kind = AB_DATA_STRING, .string = span("gfxLowQuality")},
                           {.kind = AB_DATA_NUMBER}};
    draw->settings = (AbDataTable){&quality, 1, 1, NULL};
    AbClassicPowerupUpdate alpha = {.context = draw,
                                    .draw = lifecycle_powerup_draw_alpha};
    draw->active_items[0] = &alpha;
    list->count = 0;
    CHECK(ab_classic_game_draw(&draw->owner, 480, 320, 4, list, error) &&
          list->count < ordinary_count && draw->theme.render.alpha == 1);
    AbDataValue theme = draw->state.current_theme;
    draw->state.current_theme =
        (AbDataValue){.kind = AB_DATA_STRING, .string = span("themeUGC")};
    list->count = 0;
    CHECK(ab_classic_game_draw(&draw->owner, 480, 320, 4, list, error) &&
          draw->theme.render.alpha == 0.25f);
    draw->state.current_theme = theme;
    draw->active_items[0] = NULL;
    draw->settings = (AbDataTable){0};
    draw->state.eagle_darkness =
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(0.25f)};
    list->count = 0;
    CHECK(ab_classic_game_draw(&draw->owner, 480, 320, 4, list, error) &&
          list->count == ordinary_count + 1 &&
          draw->state.eagle_darkness.bits == bits(0.25f));
    AbDrawCommand darkness = list->commands[list->count - 1];
    CHECK(darkness.texture == draw->theme.white_texture &&
          darkness.blend == AB_BLEND_STRAIGHT && darkness.color[0] == 0 &&
          darkness.color[3] == 63.0f * (1.0f / 255.0f));
    draw->state.eagle_darkness = (AbDataValue){0};
    LifecyclePowerupDraw powerup = {draw, 0};
    AbClassicPowerupUpdate passive = {.context = &powerup,
                                      .draw = lifecycle_powerup_draw_passive};
    AbClassicPowerupUpdate active = {.context = &powerup,
                                     .draw = lifecycle_powerup_draw_active};
    AbClassicPowerupUpdate no_draw = {0};
    draw->passive_items[0] = &passive;
    draw->active_items[0] = &no_draw;
    draw->active_items[1] = &active;
    list->count = 0;
    CHECK(ab_classic_game_draw(&draw->owner, 480, 320, 4, list, error) &&
          powerup.phase == 2 && list->count == ordinary_count);
    draw->passive_items[0] = NULL;
    draw->active_items[0] = draw->active_items[1] = NULL;
    /* false is nonnil: popup lookup is reached after oldZoomLevel publication. */
    AbClassicGameDraw failure = {0};
    CHECK(ab_classic_game_draw_initialize(draw->owner.config, &failure, error));
    draw->state.popup_page = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    draw->state.zoom_level = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    list->count = 0;
    AbError late = {0};
    CHECK(!ab_classic_game_draw(&failure, 480, 320, 4, list, &late) &&
          strstr(late.message, "Missing reached actual popup") && failure.failed &&
          !failure.drawing && list->count == ordinary_count &&
          draw->state.old_zoom_level.kind == AB_DATA_BOOLEAN &&
          draw->state.old_zoom_level.bits &&
          !ab_classic_game_draw(&failure, 480, 320, 4, list, NULL));
    draw->state.popup_page = (AbDataValue){0};
    return true;
}

static void lifecycle_game_draw_close(LifecycleGameDraw *draw) {
    ab_classic_sling_draw_close(draw->sling);
    theme_preview_free(&draw->theme);
}

#endif
