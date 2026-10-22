#ifndef AB_HUD_PREVIEW_ASSETS_H
#define AB_HUD_PREVIEW_ASSETS_H

#include "ab/font_draw.h"
#include "ab/game_hud.h"
#include "ab/image.h"
#include "ab/sprite_atlas.h"
#include "ab/text.h"
#include "ab/ui_image_button_draw.h"
#include "ab/ui_image_render.h"
#include "ab/utf8.h"
#include "ab/zip.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { PREVIEW_HUD_CODES = 128 };

typedef struct {
    AbBuffer font_bytes, text_bytes;
    AbBitmapFont font;
    AbTextCatalog text;
    AbImage image;
    AbSpriteSet *sprites;
    AbSpriteAtlas *atlas;
    AbUiImageDraw buttons[3];
    AbUiImageRenderBinding bindings[3];
    uint32_t texture;
    uint32_t labels[2][PREVIEW_HUD_CODES];
    size_t label_lengths[2];
    AbGameHudInput input;
} PreviewHud;

static void hud_preview_free(PreviewHud *hud) {
    ab_sprite_atlas_close(hud->atlas);
    ab_sprite_set_close(hud->sprites);
    ab_image_free(&hud->image);
    ab_font_free(&hud->font);
    ab_text_free(&hud->text);
    ab_buffer_free(&hud->font_bytes);
    ab_buffer_free(&hud->text_bytes);
    *hud = (PreviewHud){0};
}

static bool hud_preview_buttons(const AbZip *archive, PreviewHud *hud, AbError *error) {
    const char *path = "assets/data/images/480x320/HUD_INGAME_1.dat";
    if (!ab_sprite_set_open(archive, &path, 1, 1024 * 1024, &hud->sprites, error) ||
        !ab_sprite_atlas_open(archive, hud->sprites, 8 * 1024 * 1024, 2 * 1024 * 1024,
                              &hud->atlas, error)) {
        return false;
    }
    /* Forward visible child order from the original default GameHud frame.
     * Powerup's toggle is at its container origin; the other children are hidden. */
    const char *names[] = {"INGAME_BUTTON_POWERUP", "INGAME_BUTTON_EAGLE",
                           "INGAME_BUTTON_PAUSE"};
    for (size_t index = 0; index < 3; ++index) {
        AbSpan name = {(const uint8_t *)names[index], strlen(names[index])};
        AbSpriteAtlasBinding binding;
        if (!ab_sprite_atlas_find(hud->atlas, name, &binding) ||
            binding.image_index != 0 || binding.alpha != AB_SPRITE_ALPHA_PRESENT) {
            return ab_fail(error, "Unsupported ordinary HUD button binding");
        }
        const AbSpriteRegion *region = binding.sprite.region;
        hud->buttons[index] = (AbUiImageDraw){
            .frame = {.scale = {1, 1}},
            .size = {(float)region->width, (float)region->height},
            .pivot = {truncf(region->pivot_x + 0.5f), truncf(region->pivot_y + 0.5f)},
            .pivot_present = 3,
            .image = region->name,
            .image_present = true,
            .visible = true,
            .floor_coordinates = true};
        hud->bindings[index] =
            (AbUiImageRenderBinding){.region = region,
                                     .texture = hud->texture + 1,
                                     .texture_width = binding.image->width,
                                     .texture_height = binding.image->height};
    }
    /* GameHud.layout prototype 22112: pause uses 1.1, eagle uses 3.2 and
     * the enabled-eagle powerup container uses floor(pause.px * 5.3).
     * Keep the original binary32 ordering; fixed scale one/zero margins here. */
    AbVec2 pause = hud->buttons[2].pivot;
    float y = pause.y * 1.1f;
    hud->buttons[2].frame.position = (AbVec2){pause.x * 1.1f, y};
    hud->buttons[1].frame.position = (AbVec2){pause.x * 3.2f, y};
    hud->buttons[0].frame.position = (AbVec2){floorf(pause.x * 5.3f), y};
    return true;
}

static bool hud_preview_entry(const AbZip *archive, const char *path, AbBuffer *bytes,
                              AbError *error) {
    const AbZipEntry *entry = ab_zip_find(archive, path);
    return entry ? ab_zip_read(archive, (size_t)(entry - archive->entries),
                               4 * 1024 * 1024, bytes, error)
                 : ab_fail(error, "Required HUD resource is absent");
}

/* Fixed English/480x320 ordinary-score research view. The live pinned APK
 * confirms scale one, zero margins and FONT_BASIC here. Highscore presence is
 * caller state; no save score is embedded. The three static default buttons are
 * drawn, with interaction, other widgets and floating scores separate.
 * Resource bytes and glyph arrays stay owned until close. */
static bool hud_preview_open(const AbZip *archive, uint32_t texture, PreviewHud *output,
                             AbError *error) {
    *output = (PreviewHud){0};
    AbBuffer encoded = {0};
    bool okay = false;
    if (!texture ||
        !hud_preview_entry(archive, "assets/data/fonts/480x320/FONT_BASIC.dat",
                           &output->font_bytes, error) ||
        !ab_font_read((AbSpan){output->font_bytes.data, output->font_bytes.size},
                      1024 * 1024, &output->font, error) ||
        !hud_preview_entry(archive, "assets/data/localization/TEXTS_BASIC.dat",
                           &output->text_bytes, error) ||
        !ab_text_read((AbSpan){output->text_bytes.data, output->text_bytes.size},
                      1024 * 1024, &output->text, error)) {
        goto release_hud;
    }
    static const char prefix[] = "assets/data/fonts/480x320/";
    char path[256];
    AbSpan name = output->font.texture;
    if (!name.data || !name.size || name.size > sizeof(path) - sizeof(prefix) ||
        memchr(name.data, 0, name.size)) {
        ab_fail(error, "Invalid HUD font texture name");
        goto release_hud;
    }
    memcpy(path, prefix, sizeof(prefix) - 1);
    memcpy(path + sizeof(prefix) - 1, name.data, name.size);
    path[sizeof(prefix) - 1 + name.size] = 0;
    if (!hud_preview_entry(archive, path, &encoded, error) ||
        !ab_pvr_decode((AbSpan){encoded.data, encoded.size}, 8 * 1024 * 1024,
                       &output->image, error)) {
        goto release_hud;
    }
    const char *identifiers[] = {"MI_SCORE", "MI_HIGH_SCORE"};
    const AbSpan locale = {(const uint8_t *)"en_EN", 5};
    for (size_t index = 0; index < 2; ++index) {
        AbSpan label;
        const char *identifier = identifiers[index];
        if (!ab_text_find(&output->text, locale,
                          (AbSpan){(const uint8_t *)identifier, strlen(identifier)},
                          &label) ||
            !ab_utf8_to_codes(label, output->labels[index], PREVIEW_HUD_CODES,
                              &output->label_lengths[index])) {
            ab_fail(error, "HUD label localization exceeds supported view");
            goto release_hud;
        }
    }
    output->texture = texture;
    if (!hud_preview_buttons(archive, output, error)) {
        goto release_hud;
    }
    output->input = (AbGameHudInput){.screen_width = 480,
                                     .scale = 1,
                                     .font_leading = output->font.leading,
                                     .show_score = true,
                                     .update_mode = true};
    okay = true;

release_hud:
    ab_buffer_free(&encoded);
    if (!okay) {
        hud_preview_free(output);
    }
    return okay;
}

static bool hud_preview_lookup(void *context, AbSpan image,
                               AbUiImageRenderBinding *output, AbError *error) {
    const PreviewHud *hud = context;
    for (size_t index = 0; index < 3; ++index) {
        AbSpan name = hud->buttons[index].image;
        if (name.size == image.size && memcmp(name.data, image.data, image.size) == 0) {
            *output = hud->bindings[index];
            return true;
        }
    }
    return ab_fail(error, "Unknown HUD preview button image");
}

typedef struct {
    AbUiImageRenderState state;
    AbUiImageRenderBoundaries resources;
    AbDrawList *list;
} PreviewHudButtonDraw;

static bool hud_preview_button_plan(void *context, const AbUiImageDrawPlan *plan,
                                    AbError *error) {
    PreviewHudButtonDraw *draw = context;
    return ab_ui_image_render_plan(plan, &draw->state, &draw->resources, draw->list,
                                   error);
}

static bool hud_preview_draw_buttons(PreviewHud *hud, AbDrawList *list,
                                     AbError *error) {
    PreviewHudButtonDraw draw = {
        .state = {.geometry = {.scale_x = 1, .scale_y = 1}, .alpha = 1},
        .resources = {.context = hud,
                      .lookup = hud_preview_lookup,
                      .viewport_width = 480,
                      .viewport_height = 320,
                      .blend = AB_BLEND_PREMULTIPLIED,
                      .color = {1, 1, 1, 1}},
        .list = list};
    const AbUiImageButtonAnimation animation = {.scale_on_touch = true};
    const AbUiDrawInput input = {0};
    for (size_t index = 0; index < 3; ++index) {
        const AbUiImageButtonDraw button = {.image = hud->buttons + index,
                                            .animation = &animation};
        if (!ab_ui_image_button_draw(&button, &input, (AbVec2){480, 320},
                                     hud_preview_button_plan, &draw, error)) {
            return false;
        }
    }
    return true;
}

static bool hud_preview_number(float value, uint32_t codes[12], size_t *length,
                               AbError *error) {
    if (!isfinite(value) || value < 0 || value >= 2147483648.0f) {
        return ab_fail(error, "Invalid HUD preview score");
    }
    char text[12];
    int count = snprintf(text, sizeof(text), "%" PRId32, (int32_t)value);
    if (count < 0 || (size_t)count >= sizeof(text)) {
        return ab_fail(error, "Cannot format HUD preview score");
    }
    return ab_utf8_to_codes((AbSpan){(const uint8_t *)text, (size_t)count}, codes, 12,
                            length) ||
           ab_fail(error, "Cannot decode HUD preview score");
}

static bool hud_preview_text(const PreviewHud *hud, const AbGameHudCall *call,
                             AbSpriteRenderState state, AbDrawList *list,
                             AbError *error) {
    uint32_t number[12];
    const uint32_t *codes;
    size_t length;
    if (call->text == AB_HUD_SCORE_LABEL || call->text == AB_HUD_HIGHSCORE_LABEL) {
        size_t label = call->text == AB_HUD_SCORE_LABEL ? 0 : 1;
        codes = hud->labels[label];
        length = hud->label_lengths[label];
    } else {
        if (!hud_preview_number((float)call->number, number, &length, error)) {
            return false;
        }
        codes = number;
    }
    AbFontGlyphPlacement glyphs[PREVIEW_HUD_CODES];
    size_t count;
    AbFontPlacement placement = {.placement = {.x = call->position.x,
                                               .y = call->position.y,
                                               .scale_x = 1,
                                               .scale_y = 1,
                                               .vertical = AB_ANCHOR_TOP,
                                               .horizontal = AB_ANCHOR_RIGHT}};
    AbFontNativeDrawStyle style = {.texture = hud->texture,
                                   .texture_width = hud->image.width,
                                   .texture_height = hud->image.height,
                                   .viewport_width = 480,
                                   .viewport_height = 320,
                                   .glyph_scale_x = 1,
                                   .glyph_scale_y = 1,
                                   .state = state,
                                   .blend = AB_BLEND_PREMULTIPLIED,
                                   .color = {1, 1, 1, 1},
                                   .opacity = 1};
    return (ab_font_layout_codes(&hud->font, codes, length, 0, length, placement,
                                 glyphs, PREVIEW_HUD_CODES, &count) ||
            ab_fail(error, "Cannot lay out HUD preview text")) &&
           ab_font_draw_native(glyphs, count, style, list, error);
}

/* World and children are separate caller phases. Execute only the recovered
 * state/font/text portion here after world submission. No frame allocation. */
static bool hud_preview_draw(PreviewHud *hud, AbDrawList *list, AbError *error) {
    uint32_t codes[12];
    size_t length;
    AbFontMeasure measured;
    if (!hud_preview_number(hud->input.score, codes, &length, error) ||
        !ab_font_measure_codes(&hud->font, codes, length, 0, length, &measured)) {
        return ab_fail(error, "Cannot measure HUD preview score");
    }
    AbGameHudInput input = hud->input;
    input.measured_score_width = (float)measured.width;
    AbGameHudPlan plan;
    if (!ab_game_hud_plan(&input, &plan, error)) {
        return false;
    }
    size_t original_count = list->count;
    AbSpriteRenderState state = {.scale_x = 1, .scale_y = 1};
    AbGameHudFont font = AB_HUD_FONT_BASIC;
    for (size_t index = 0; index < plan.count; ++index) {
        const AbGameHudCall *call = plan.calls + index;
        if (call->kind == AB_HUD_SET_STATE) {
            state.translation_x = call->position.x;
            state.translation_y = call->position.y;
            state.scale_x = call->scale.x;
            state.scale_y = call->scale.y;
        } else if (call->kind == AB_HUD_SET_FONT) {
            font = call->font;
        } else if (call->kind == AB_HUD_DRAW_TEXT) {
            if (font != AB_HUD_FONT_BASIC ||
                !hud_preview_text(hud, call, state, list, error)) {
                list->count = original_count;
                return ab_fail(error, "Unsupported HUD preview text submission");
            }
        }
    }
    if (!hud_preview_draw_buttons(hud, list, error)) {
        list->count = original_count;
        return false;
    }
    hud->input.previous_score_width = plan.next_score_width;
    hud->input.previous_width_present = true;
    return true;
}

#endif
