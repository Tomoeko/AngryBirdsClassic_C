#include "../tools/classic_preview_assets.h"

#include <stdio.h>

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool loaded_sling_checks(const PreviewLevel *level, AbError *error) {
    const PreviewSling *sling = &level->sling;
    /* Read-only pinned-APK level-one idle observation. These values are derived
     * from the loaded bird, back-sprite metadata and recovered C pose routines. */
    const uint32_t expected[] = {3242459136u, 3238394405u, 3242459136u, 3212393459u,
                                 3242563994u, 3238499263u, 3279224832u, 3274143918u,
                                 3280067885u, 3273440854u, 3209236443u, 1103626240u};
    const uint32_t actual[] = {bits(sling->start.level_start.x),
                               bits(sling->start.level_start.y),
                               bits(sling->start.base.x),
                               bits(sling->start.base.y),
                               bits(sling->start.band.x),
                               bits(sling->start.band.y),
                               bits(sling->pose.position.x),
                               bits(sling->pose.position.y),
                               bits(sling->pose.stretch_position.x),
                               bits(sling->pose.stretch_position.y),
                               bits(sling->pose.angle),
                               bits(sling->pose.width)};
    if (memcmp(actual, expected, sizeof(actual)) != 0) {
        for (size_t field = 0; field < sizeof(actual) / sizeof(*actual); ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr, "Loaded sling field %zu: %u expected %u\n", field,
                        actual[field], expected[field]);
            }
        }
        return ab_fail(error, "Loaded sling pose differs from original idle state");
    }
    return true;
}

static size_t sling_image(const PreviewSling *sling, const AbDrawCommand *command) {
    for (size_t image = 0; image < 4; ++image) {
        if (sling->images[image].texture != command->texture) {
            continue;
        }
        bool equal = true;
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            equal = equal &&
                    command->quad.vertices[vertex].u ==
                        sling->images[image].uv[vertex * 2] &&
                    command->quad.vertices[vertex].v ==
                        sling->images[image].uv[vertex * 2 + 1];
        }
        if (equal) {
            return image;
        }
    }
    return SIZE_MAX;
}

static bool loaded_scene_checks(PreviewLevel *level, const AbDrawList *list,
                                AbError *error) {
    AbClassicLevelView world = ab_classic_level_view(level->level);
    AbClassicSceneView scene = ab_classic_scene_view(level->scene);
    AbCameraSystemView camera = ab_camera_system_view(level->camera);
    /* Original integrated camera corpus, first case after default completion.
     * The cubic endpoint preserves subtraction/addition rounding; do not force
     * current X to the authored snap's four-ULP-different value. */
    if (world.bodies->count != 23 || scene.count != 23 || list->count <= 4 ||
        ab_sprite_atlas_count(level->atlas) != 4 ||
        bits(camera.mover->current_position.x) != UINT32_C(0xc1ea7280) ||
        bits(camera.mover->current_position.y) != UINT32_C(0xc398eb9c) ||
        bits(camera.mover->current_zoom) != UINT32_C(0x3eb5638d) ||
        camera.sequence->current || camera.automatic_present ||
        !camera.limits->enabled || !camera.input_enabled) {
        return ab_fail(error, "Loaded level/camera endpoint differs from reference");
    }
    if (ab_theme_layers_count(level->theme.layers) != 5 ||
        theme_preview_texture_count(&level->theme) != 5 ||
        level->theme.first_texture != 5 || level->theme.white_texture != 9 ||
        level->theme.image_count != 19 ||
        level->theme.background != UINT32_C(0xff92ccde)) {
        return ab_fail(error,
                       "Loaded theme resources differ from selected live bindings");
    }
    if (level->hud.texture != 10 || level->hud.font.leading != 20 ||
        level->hud.font.tracking != -1 || level->hud.input.scale != 1 ||
        level->hud.input.margin.x != 0 || level->hud.input.margin.y != 0 ||
        !level->hud.input.show_score || level->hud.input.highscore_present ||
        level_preview_texture_count(level) != 11) {
        return ab_fail(error, "HUD resources differ from native ordinary score view");
    }
    const uint32_t button_positions[3][2] = {{1124663296u, 1107243828u},
                                             {1118201447u, 1107243828u},
                                             {1105513677u, 1107243828u}};
    for (size_t index = 0; index < 3; ++index) {
        const AbUiImageDraw *button = level->hud.buttons + index;
        if (bits(button->frame.position.x) != button_positions[index][0] ||
            bits(button->frame.position.y) != button_positions[index][1] ||
            button->pivot.x != 26 || button->pivot.y != 29 ||
            button->size.x != (index ? 52 : 53) || button->size.y != 58) {
            return ab_fail(error, "HUD button layout differs from live idle state");
        }
    }
    for (size_t index = 0; index < level_preview_texture_count(level); ++index) {
        uint32_t texture;
        const AbImage *image;
        if (!level_preview_texture(level, index, &texture, &image) ||
            texture != index + 1 || !image->rgba.data || !image->rgba.size) {
            return ab_fail(error, "Loaded texture lifetime/identifier differs");
        }
    }
    for (size_t index = 0; index < theme_preview_texture_count(&level->theme);
         ++index) {
        const AbImage *image;
        uint32_t texture;
        if (!theme_preview_texture(&level->theme, index, &texture, &image) ||
            texture != 5 + index || !image->width || !image->height ||
            !image->rgba.data || !image->rgba.size) {
            return ab_fail(error, "Theme preview texture lifetime/identifier differs");
        }
    }
    float clear[4];
    theme_preview_clear(&level->theme, clear);
    if (clear[0] <= 0 || clear[1] <= 0 || clear[2] <= 0 || clear[3] != 1) {
        return ab_fail(error, "Theme background did not reach the clear color");
    }
    size_t birds = 0, bird_draws = 0, object_draws = 0;
    size_t first_object = SIZE_MAX, last_object = 0, ground = SIZE_MAX;
    size_t sling_indices[3] = {SIZE_MAX, SIZE_MAX, SIZE_MAX}, band_count = 0;
    size_t hud_glyphs = 0, hud_buttons = 0;
    for (size_t index = 0; index < scene.count; ++index) {
        const AbSceneSprite *item = scene.items + index;
        if (item->collider != 3) {
            continue;
        }
        if (!item->region || item->texture != 1 || item->region->name.size != 8 ||
            memcmp(item->region->name.data, "BIRD_RED", 8) != 0 ||
            item->region->x != 675 || item->region->y != 951 ||
            item->region->width != 46 || item->region->height != 45) {
            return ab_fail(error, "Loaded initial bird sprite differs from reference");
        }
        ++birds;
    }
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        if (command->texture == level->hud.texture) {
            if (index < list->count - 9 || index >= list->count - 3 ||
                command->coordinates != AB_DRAW_CLIP ||
                command->blend != AB_BLEND_PREMULTIPLIED) {
                return ab_fail(error, "HUD glyph order/material differs");
            }
            ++hud_glyphs;
        }
        if (command->texture == level->hud.texture + 1) {
            if (index < list->count - 3 || command->coordinates != AB_DRAW_CLIP ||
                command->blend != AB_BLEND_PREMULTIPLIED) {
                return ab_fail(error, "HUD button order/material differs");
            }
            ++hud_buttons;
        }
        size_t selected_sling = sling_image(&level->sling, command);
        if (selected_sling < 3) {
            if (sling_indices[selected_sling] != SIZE_MAX) {
                return ab_fail(error, "Duplicate sling sprite in selected idle view");
            }
            sling_indices[selected_sling] = index;
        } else if (selected_sling == 3) {
            ++band_count;
        } else if (command->texture <= 4) {
            if (!object_draws) {
                first_object = index;
            }
            ++object_draws;
            last_object = index;
        } else if (command->texture == level->theme.white_texture) {
            if (ground != SIZE_MAX || command->blend != AB_BLEND_OPAQUE ||
                bits(command->color[0]) != UINT32_C(0x3d20a0a1) ||
                bits(command->color[1]) != UINT32_C(0x3d989899) ||
                bits(command->color[2]) != UINT32_C(0x3e64e4e6)) {
                return ab_fail(error, "Theme ground command differs from native color");
            }
            ground = index;
        }
        if (bits(command->quad.vertices[0].u) == UINT32_C(0x3f296969) &&
            bits(command->quad.vertices[0].v) == UINT32_C(0x3f6e72d6) &&
            command->texture == 1 && command->blend == AB_BLEND_PREMULTIPLIED &&
            command->coordinates == AB_DRAW_CLIP) {
            ++bird_draws;
        }
    }
    return birds == 3 && bird_draws == 3 && object_draws == 4 && first_object > 0 &&
                   last_object < ground && ground + 1 < list->count &&
                   band_count == 2 && sling_indices[0] < first_object &&
                   sling_indices[1] > last_object && sling_indices[1] < ground &&
                   hud_glyphs == 6 && hud_buttons == 3 &&
                   sling_indices[2] == list->count - 10 &&
                   loaded_sling_checks(level, error)
               ? true
               : ab_fail(error, "Loaded bird bindings did not reach the draw list");
}

typedef struct {
    const AbSpriteProvider *source;
    AbThemeDraw *draw;
    AbThemeDrawInput input;
    AbDrawList *list;
    size_t calls;
    bool fail_texture;
} ThemeDrawCheck;

static bool theme_draw_sprite(void *context, AbSpan name,
                              AbSpriteAtlasBinding *output) {
    ThemeDrawCheck *check = context;
    ++check->calls;
    if (ab_theme_draw_background(check->draw, check->input, check->list, NULL) ||
        ab_theme_draw_foreground(check->draw, check->input, check->list, NULL))
        return false;
    return check->source->sprite(check->source->context, name, output);
}

static bool theme_draw_texture(void *context, const AbImage *image, uint32_t *texture) {
    ThemeDrawCheck *check = context;
    return !check->fail_texture &&
           check->source->texture(check->source->context, image, texture);
}

static bool theme_draw_checks(PreviewLevel *level, AbDrawList *list, AbError *error) {
    AbThemeDraw draw = {0};
    AbThemeDrawInput input = {ab_camera_system_view(level->camera).viewport->rendering,
                              480, 320, 0, 0};
    ThemeDrawCheck check = {&level->theme.provider, &draw, input, list, 0, false};
    AbSpriteProvider provider = {.context = &check,
                                 .sprite = theme_draw_sprite,
                                 .composite = NULL,
                                 .texture = theme_draw_texture};
    AbThemeLayerRenderState state = level->theme.render;
    uint32_t background = 0;
    AbThemeDrawConfig config = level->theme.draw.config;
    config.provider = &provider;
    config.render = &state;
    config.background_argb = &background;
    list->count = 0;
    if (!ab_theme_draw_initialize(config, &draw, error) ||
        !ab_theme_draw_background(&draw, input, list, error) || !check.calls ||
        !list->count || draw.calling || draw.failed || state.alpha != 1)
        return ab_fail(error, "Owned theme draw/reentry composition failed");
    size_t count = list->count;
    check.fail_texture = true;
    background = UINT32_C(0xdeadbeef);
    AbError late = {0};
    if (ab_theme_draw_background(&draw, input, list, &late) || !draw.failed ||
        draw.calling || list->count != count || background == UINT32_C(0xdeadbeef) ||
        !strstr(late.message, "retained theme image binding") ||
        ab_theme_draw_foreground(&draw, input, list, NULL) || list->count != count)
        return ab_fail(error, "Owned theme draw lost its native late-failure prefix");
    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    PreviewLevel level = {0};
    AbDrawList list = {0};
    AbError error = {0};
    bool okay = ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                level_preview_open(&archive, &level, &error) &&
                ab_draw_list_init(PREVIEW_THEME_DRAWS + 32, 128 * 1024, &list, &error);
    for (size_t frame = 0; okay && frame < 128; ++frame) {
        okay = level_preview_draw(&level, 0x1.111112p-5f, &list, NULL, &error);
    }
    okay = okay && loaded_scene_checks(&level, &list, &error);
    size_t default_count = list.count;
    if (okay) {
        float initial_x = ab_camera_system_view(level.camera).mover->current_position.x;
        AbCameraTouch pointer = {3, {120, 130}};
        AbCameraTouchInput input = {.touches = &pointer,
                                    .count = 1,
                                    .ingame = true,
                                    .released_after_specialty = true};
        okay = level_preview_draw(&level, 0x1.111112p-5f, &list, &input, &error);
        pointer.position.x = 100;
        okay =
            okay && level_preview_draw(&level, 0x1.111112p-5f, &list, &input, &error) &&
            level.touch.state == AB_CAMERA_TOUCH_DRAG &&
            ab_camera_system_view(level.camera).mover->current_position.x > initial_x &&
            list.count != 0;
        if (!okay && !error.message[0]) {
            ab_fail(&error, "Loaded preview did not apply resolved camera touch input");
        }
    }
    if (okay) {
        printf(
            "Direct APK preview reaches the native camera endpoint with %zu commands; "
            "the prepared drag frame emits %zu. Both include the sling, seated bird, "
            "background, four object sprites, ground, foreground and score text.\n",
            default_count, list.count);
        okay = theme_draw_checks(&level, &list, &error);
        if (!okay)
            fprintf(stderr, "%s\n", error.message);
    } else {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_draw_list_free(&list);
    level_preview_free(&level);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
