#include "ab/ui_hud_powerups.h"
#include "ab/font_asset.h"
#include "ab/ui_text_clipping.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "HUD powerup check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbSpriteSet *sprites;
    AbSpriteAtlas *atlas;
    AbCompositeResources *composites;
    AbFontAsset assets[3];
    AbUiFontBinding bindings[3];
    AbUiFontResources fonts;
    AbTextResources strings;
    uint32_t codes[128];
    AbFontGlyphPlacement glyphs[128];
    AbPowerupCatalog *catalog;
    AbTargetData events;
} Resources;

typedef struct {
    Resources *resources;
    AbUiHudPowerups owner;
    AbUiWidget root, pause;
    AbUiWidgetContext context;
    AbUiScaling scaling;
    AbUiImageRenderState render;
    AbVec2 screen;
    AbUiPointerValue enabled[AB_UI_HUD_POWERUPS];
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    AbDrawCommand commands[128];
    AbDrawList list;
    size_t sounds;
} Fixture;

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbUiPointerValue text(const char *name) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = span(name)};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static float real(AbUiPointerValue value) {
    float output;
    memcpy(&output, &value.number_bits, sizeof(output));
    return output;
}

static bool equal(AbUiPointerValue value, const char *name) {
    return value.kind == AB_UI_POINTER_TEXT && value.text.size == strlen(name) &&
           !memcmp(value.text.data, name, value.text.size);
}

static bool localize(void *context, AbSpan image, AbSpan *output, AbError *error) {
    (void)context;
    (void)error;
    *output = image;
    return true;
}

static bool geometry(void *context, AbSpan image, AbVec2 *output, bool pivot,
                     AbError *error) {
    Fixture *fixture = context;
    if (!image.size) {
        *output = (AbVec2){0};
        return true;
    }
    AbSpriteBinding binding;
    if (!ab_sprite_set_find(fixture->resources->sprites, image, &binding)) {
        AbResolvedComposite composite;
        if (!ab_composite_resources_find(fixture->resources->composites, image,
                                         &composite))
            return ab_fail(error, "HUD test sprite is absent: %.*s", (int)image.size,
                           image.data);
        *output = pivot ? (AbVec2){(float)composite.bounds.pivot_x,
                                   (float)composite.bounds.pivot_y}
                        : (AbVec2){(float)composite.bounds.width,
                                   (float)composite.bounds.height};
        return true;
    }
    const AbSpriteRegion *region = binding.region;
    *output = pivot ? (AbVec2){region->pivot_x, region->pivot_y}
                    : (AbVec2){region->width, region->height};
    return true;
}

static bool bounds(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    return geometry(context, image, output, false, error);
}

static bool pivot(void *context, AbSpan image, AbVec2 *output, AbError *error) {
    return geometry(context, image, output, true, error);
}

static bool lookup(void *context, AbSpan image, AbUiImageRenderBinding *output,
                   AbError *error) {
    Fixture *fixture = context;
    if (!image.size) {
        *output = (AbUiImageRenderBinding){0};
        return true;
    }
    AbSpriteAtlasBinding binding;
    if (!ab_sprite_atlas_find(fixture->resources->atlas, image, &binding)) {
        AbResolvedComposite composite;
        if (!ab_composite_resources_find(fixture->resources->composites, image,
                                         &composite))
            return ab_fail(error, "HUD test atlas image is absent");
        *output = (AbUiImageRenderBinding){.composite = composite};
        return true;
    }
    *output = (AbUiImageRenderBinding){.region = binding.sprite.region,
                                       .texture = 1 + (uint32_t)binding.image_index,
                                       .texture_width = binding.image->width,
                                       .texture_height = binding.image->height,
                                       .alpha = binding.alpha};
    return true;
}

static bool part(void *context, const AbSpriteBinding *sprite,
                 AbUiImageRenderBinding *output, AbError *error) {
    Fixture *fixture = context;
    AbSpriteAtlasBinding binding;
    if (!ab_sprite_atlas_resolve(fixture->resources->atlas, *sprite, &binding))
        return ab_fail(error, "Missing actual HUD composite part image");
    *output = (AbUiImageRenderBinding){.region = binding.sprite.region,
                                       .texture = 1 + (uint32_t)binding.image_index,
                                       .texture_width = binding.image->width,
                                       .texture_height = binding.image->height,
                                       .alpha = binding.alpha};
    return true;
}

static bool environment(void *context, AbUiScalingEnvironment *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    *output = (AbUiScalingEnvironment){.screen = fixture->screen, .use_scaling = true};
    return true;
}

static bool notifications(void *context, bool *output, AbError *error) {
    (void)context;
    (void)error;
    *output = false;
    return true;
}

static bool audio(void *context, AbUiPointerValue sound, float gain, bool loop,
                  AbError *error) {
    Fixture *fixture = context;
    if (sound.kind != AB_UI_POINTER_TEXT || gain != 1 || loop)
        return ab_fail(error, "Unexpected HUD test audio boundary");
    ++fixture->sounds;
    return true;
}

static bool initialize(Fixture *fixture, Resources *resources, bool desktop, bool shop,
                       bool trial, bool no_birds, AbError *error) {
    *fixture = (Fixture){.resources = resources, .screen = {1920, 1080}};
    fixture->scaling =
        (AbUiScaling){.host = {.context = fixture, .environment = environment},
                      .default_scaling = 1.25f,
                      .initialized = true};
    fixture->list = (AbDrawList){fixture->commands, 0, 128};
    fixture->render.alpha = 1;
    fixture->context = (AbUiWidgetContext){.images = {fixture, localize, bounds, pivot},
                                           .render = {.context = fixture,
                                                      .lookup = lookup,
                                                      .part = part,
                                                      .viewport_width = 1920,
                                                      .viewport_height = 1080,
                                                      .blend = AB_BLEND_PREMULTIPLIED,
                                                      .color = {1, 1, 1, 1}},
                                           .render_state = &fixture->render,
                                           .draw_list = &fixture->list,
                                           .maximum_depth = 16,
                                           .animation_installed = true,
                                           .audio_context = fixture,
                                           .play_audio = audio};
    for (size_t index = 0; index < AB_UI_HUD_POWERUPS; ++index)
        fixture->enabled[index] =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    AbUiFrameFields frame = {.name = text("gameHud")};
    AbUiImageButtonArguments pause = {.image.frame.name = text("pauseButton"),
                                      .image.image = text("INGAME_BUTTON_PAUSE")};
    CHECK(ab_ui_widget_init(
              &fixture->root, &fixture->context, AB_UI_WIDGET_FRAME,
              (AbUiWidgetChildren){fixture->frames, fixture->draw, fixture->pointer, 1},
              &frame, NULL, NULL, error) &&
          ab_ui_widget_init(&fixture->pause, &fixture->context,
                            AB_UI_WIDGET_IMAGE_BUTTON, (AbUiWidgetChildren){0}, NULL,
                            NULL, &pause, error));
    fixture->pause.frame->fields.y = number(53.5f);
    AbUiHudPowerupsHost host = {
        .slider = {&fixture->scaling, &fixture->screen, fixture, notifications},
        .fonts = &resources->fonts,
        .catalog = resources->catalog,
        .registered_enabled = fixture->enabled,
        .device_model = span(desktop ? "osx" : "android")};
    AbUiHudPowerupsSource source = {
        .visible = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
        .bird_count = 3,
        .birds_shot = no_birds ? 3.0f : 0.0f,
        .plus_signs = shop,
        .trial = trial};
    CHECK(
        ab_ui_hud_powerups_init(&fixture->owner, &fixture->root, host, source, error));
    return true;
}

static bool click(Fixture *fixture, AbUiWidget *button, const char *expected,
                  AbUiPointerReply *reply, AbError *error) {
    AbVec2 point, scale;
    CHECK(ab_ui_widget_screen_position(button, &point, error) &&
          ab_ui_widget_cumulative_scale(button, &scale, error));
    point.x += (real(button->frame->fields.w) * 0.5f -
                real(button->state.image.resource_pivot_x)) *
               scale.x;
    point.y += (real(button->frame->fields.h) * 0.5f -
                real(button->state.image.resource_pivot_y)) *
               scale.y;
    fixture->context.cursor = point;
    fixture->context.left_button_held = true;
    fixture->context.touch_count = 1;
    CHECK(ab_ui_widget_pointer(&fixture->owner.slider.box.widget, span("LPRESS"), point,
                               reply, error));
    fixture->context.left_button_held = false;
    fixture->context.touch_count = 0;
    CHECK(ab_ui_widget_pointer(&fixture->owner.slider.box.widget, span("LRELEASE"),
                               point, reply, error));
    CHECK(equal(reply->values[0], expected));
    return true;
}

static bool graph(Resources *resources, AbError *error) {
    for (unsigned variant = 0; variant < 16; ++variant) {
        Fixture fixture;
        bool desktop = variant & 1, shop = variant & 2, trial = variant & 4,
             no_birds = variant & 8;
        CHECK(initialize(&fixture, resources, desktop, shop, trial, no_birds, error));
        AbUiHudPowerups *owner = &fixture.owner;
        AbUiSlider *slider = &owner->slider;
        CHECK(owner->count == 6 && slider->count == (shop && !trial ? 5u : 4u) &&
              owner->shop_present == (shop && !trial) && !owner->hide_powerups &&
              slider->box.widget.frame->child_count == slider->count + 2 &&
              fixture.frames[0] == &slider->box.widget.tree &&
              equal(slider->toggle.state.return_value, "TOGGLE_POWERUPS") &&
              equal(slider->toggle.state.disabled_return_value,
                    "TOGGLE_POWERUPS_DISABLED") &&
              real(slider->toggle.padding[2]) == (desktop ? 50 : 0));
        for (size_t index = 0; index < 6; ++index) {
            AbUiHudPowerup *button = owner->powerups + index;
            CHECK(button->present == (index < 4));
            if (!button->present)
                continue;
            CHECK(button->has_counter && owner->frames[index] == &button->button.tree &&
                  button->button.frame->child_count == 2 &&
                  button->frames[0] == &button->background.tree &&
                  button->frames[1] == &button->tag.tree &&
                  button->background_frames[0] == &button->counter.widget.tree &&
                  equal(button->counter.widget.frame->fields.name, "counter") &&
                  equal(button->counter.state.fields.font, "FONT_LS_SMALL") &&
                  equal(button->counter.state.fields.text, "0") &&
                  !ab_ui_pointer_value_truthy(&button->tag.frame->fields.visible) &&
                  button->button.state.meta.object == (uintptr_t)&button->meta &&
                  button->tag.state.meta.object == (uintptr_t)&button->tag_meta &&
                  &button->meta != &button->tag_meta &&
                  button->meta.entries != button->tag_meta.entries &&
                  ab_ui_pointer_value_truthy(&button->button.state.enabled) ==
                      (!no_birds || index == 3) &&
                  ab_ui_pointer_value_truthy(&button->background.state.enabled) ==
                      (!no_birds || index == 3));
            if (!variant && !index) {
                CHECK(ab_ui_hud_powerup_enable(
                          button, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN},
                          error) &&
                      !ab_ui_pointer_value_truthy(&button->background.state.enabled) &&
                      ab_ui_hud_powerup_enable(button, number(0), error) &&
                      button->button.state.enabled.kind == AB_UI_POINTER_NUMBER &&
                      button->background.state.enabled.kind == AB_UI_POINTER_NUMBER &&
                      !button->button.state.enabled.number_bits &&
                      !button->background.state.enabled.number_bits);
            }
            if (desktop)
                CHECK(real(button->button.padding[2]) == 50 &&
                      real(button->button.padding[3]) ==
                          500 / real(button->button.frame->fields.h));
        }
        CHECK(ab_ui_widget_lifecycle(&fixture.root, AB_UI_FRAME_ENTRY, error) &&
              ab_ui_hud_powerups_layout(owner, &fixture.pause, (AbVec2){7, 11}, true,
                                        error));
        float px = real(fixture.pause.state.image.resource_pivot_x);
        CHECK(real(slider->box.widget.frame->fields.x) ==
                  floorf((px * 5.3f) * 1.25f + 7) &&
              real(slider->box.widget.frame->fields.y) == 53.5f &&
              slider->child_size ==
                  floorf(real(fixture.pause.frame->fields.w) * 0.97f));
        CHECK(ab_ui_slider_open(slider, error) &&
              ab_ui_widget_update(&fixture.root, 2, 2, error) &&
              slider->state == AB_UI_SLIDER_OPEN);
        AbUiDrawInput input = {0};
        CHECK(ab_ui_widget_draw(&fixture.root, &input, error) &&
              fixture.list.count > 8);
        owner->powerups[0].counter.state.fields.text = number(123.25f);
        fixture.list.count = 0;
        CHECK(ab_ui_widget_draw(&fixture.root, &input, error) &&
              owner->powerups[0].counter.state.fields.text.kind ==
                  AB_UI_POINTER_NUMBER &&
              real(owner->powerups[0].counter.state.fields.text) == 123.25f &&
              fixture.list.count > 8);
        AbUiPointerReply reply;
        CHECK(click(&fixture, &owner->powerups[3].button, "ACTIVATE_POWERUP", &reply,
                    error) &&
              reply.values[1].object == (uintptr_t)&owner->powerups[3].meta &&
              fixture.sounds == 1);
        CHECK(ab_ui_hud_powerups_layout(owner, &fixture.pause, (AbVec2){7, 11}, false,
                                        error) &&
              real(slider->box.widget.frame->fields.x) ==
                  floorf((px * 3.2f) * 1.25f + 7));
    }
    return true;
}

static bool clipping(Resources *resources, AbError *error) {
    Fixture fixture;
    CHECK(initialize(&fixture, resources, false, false, false, false, error));
    AbUiWidget parent = {0};
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    AbUiFrameFields frame = {.x = number(20),
                             .y = number(10),
                             .scale_x = number(2),
                             .scale_y = number(1.5f)};
    CHECK(ab_ui_widget_init(&parent, &fixture.context, AB_UI_WIDGET_FRAME,
                            (AbUiWidgetChildren){frames, draw, pointer, 1}, &frame,
                            NULL, NULL, error));
    AbUiTextWidget label = {0};
    AbUiTextArguments args = {.text = text("1-1 12-34 56-78 90-12"),
                              .font = text("FONT_LS_SMALL"),
                              .box_size = number(180)};
    CHECK(ab_ui_text_widget_init(&label, &fixture.context, (AbUiWidgetChildren){0},
                                 &resources->fonts, NULL, &args, error) &&
          ab_ui_widget_add(&parent, &label.widget, error));
    label.widget.frame->fields.x = number(300);
    label.widget.frame->fields.y = number(200);
    label.widget.frame->fields.scale_x = number(0.5f);
    label.widget.frame->fields.scale_y = number(0.5f);
    AbUiTextState lines[32];
    char names[32][40];
    AbFontWrappedLine wrapped[16];
    AbSpan labels[16];
    uint32_t codes[128];
    uint8_t bytes[4096];
    AbUiTextClipping owner = {0};
    CHECK(ab_ui_text_clipping_init(&owner,
                                   (AbUiTextClipStorage){lines, names, 32, wrapped,
                                                         labels, 16, codes, 128, bytes,
                                                         sizeof(bytes)},
                                   error) &&
          ab_ui_text_widget_clip(&label, &owner, (AbUiPointerValue){0},
                                 (AbUiPointerValue){0}, error));
    CHECK(owner.generation.count > 1 && owner.generation.count < 16 &&
          owner.generation.clipped && !label.widget.frame->child_count &&
          label.widget.tree.parent == &parent.tree &&
          owner.used_lines == owner.generation.count);
    size_t previous_count = owner.generation.count;
    AbUiTextState previous = lines[0];
    const uint8_t *previous_text = previous.fields.text.text.data;
    uint8_t retained_text[128];
    CHECK(previous.fields.text.text.size <= sizeof(retained_text));
    memcpy(retained_text, previous_text, previous.fields.text.text.size);
    int leading = resources->assets[1].font.leading;
    CHECK(owner.generation.text_block_height ==
          (float)previous_count * ((float)leading * 0.75f));
    for (size_t index = 0; index < previous_count; ++index)
        CHECK(real(lines[index].frame.fields.scale_x) == 1 &&
              real(lines[index].frame.fields.scale_y) == 1 &&
              equal(lines[index].fields.group, "TEXTS_BASIC"));
    float height;
    CHECK(ab_ui_text_widget_get_height(&label, &height, error) &&
          height == ((float)previous_count * (float)leading) * 0.5f);
    AbUiDrawInput input = {0};
    CHECK(ab_ui_widget_draw(&parent, &input, error) && fixture.list.count > 10);
    size_t glyph_count = fixture.list.count;
    lines[0].frame.fields.visible = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    fixture.list.count = 0;
    CHECK(ab_ui_widget_draw(&parent, &input, error) &&
          fixture.list.count == glyph_count); /* Native lines ignore visibility. */
    CHECK(ab_ui_text_widget_clip(
              &label, &owner,
              (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
              number(2), error) &&
          owner.generation.lines == lines + previous_count &&
          owner.generation.count < previous_count &&
          owner.used_lines == previous_count + owner.generation.count &&
          lines[0].fields.text.text.data == previous_text &&
          !memcmp(previous_text, retained_text, previous.fields.text.text.size) &&
          owner.generation.text_block_height ==
              (float)owner.generation.count * (float)leading);
    CHECK(!ab_ui_text_widget_clip(&label, &owner, (AbUiPointerValue){0},
                                  text("invalid"), NULL) &&
          owner.failed && fixture.context.failed &&
          !ab_ui_text_widget_get_height(&label, &height, NULL));
    return true;
}

#include "ui_pause_page_apk.h"
#include "ui_text_getter_apk.h"
#include "ui_mighty_eagle_apk.h"
#include "ui_animated_image_apk.h"
#include "ui_game_hud_controls_apk.h"

static bool resolve_part(void *context, AbSpan name, AbSpriteBinding *output,
                         AbError *error) {
    Resources *resources = context;
    return ab_sprite_set_find(resources->sprites, name, output) ||
           ab_fail(error, "Missing actual HUD composite part: %.*s", (int)name.size,
                   name.data);
}

static bool check(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    Resources resources = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay =
        ab_file_read(path, 64 * 1024 * 1024, &bytes, &error) &&
        ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
        ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
        ab_powerup_catalog_open(&archive, (AbSpan){key, sizeof(key)}, (AbDataValue){0},
                                1024 * 1024, &resources.catalog, &error) &&
        ab_target_data_open(&archive, "assets/data/scripts_common/events.lua",
                            (AbSpan){key, sizeof(key)}, 1024 * 1024, NULL,
                            &resources.events, &error);
    const char *const sheets[] = {"assets/data/images/1024x768/HUD_INGAME_1.dat",
                                  "assets/data/images/1024x768/MENU_ELEMENTS_1.dat",
                                  "assets/data/images/1024x768/NEW_SHOP_ELEMENTS_1.dat",
                                  "assets/data/images/1024x768/MENU_ELEMENTS_2.dat",
                                  "assets/data/images/1024x768/BUTTONS_SHEET_1.dat",
                                  "assets/data/images/1024x768/BUTTONS_SHEET_2.dat"};
    const char *const names[] = {"FONT_BASIC", "FONT_LS_SMALL", "FONT_MENU"};
    if (okay)
        okay = ab_sprite_set_open(&archive, sheets, 6, 2 * 1024 * 1024,
                                  &resources.sprites, &error) &&
               ab_sprite_atlas_open(&archive, resources.sprites, 32 * 1024 * 1024,
                                    4 * 1024 * 1024, &resources.atlas, &error);
    const char *const composites[] = {
        "assets/data/images/1024x768/BUTTONS_COMPOSPRITES.dat"};
    if (okay)
        okay = ab_composite_resources_open_resolved(
            &archive, composites, 1, 2 * 1024 * 1024, 1024 * 1024, resolve_part,
            &resources, &resources.composites, &error);
    for (size_t index = 0; okay && index < 3; ++index) {
        char font_path[128];
        int count = snprintf(font_path, sizeof(font_path),
                             "assets/data/fonts/1024x768/%s.dat", names[index]);
        okay = count > 0 && (size_t)count < sizeof(font_path) &&
               ab_font_asset_open(&archive, font_path, 8 * 1024 * 1024, 2 * 1024 * 1024,
                                  resources.assets + index, &error);
        resources.bindings[index] = (AbUiFontBinding){
            span(names[index]), &resources.assets[index].font, 61 + (uint32_t)index,
            resources.assets[index].image.width, resources.assets[index].image.height};
    }
    resources.strings.locale = span("en_EN");
    resources.fonts = (AbUiFontResources){.bindings = resources.bindings,
                                          .count = 3,
                                          .strings = &resources.strings,
                                          .codes = resources.codes,
                                          .glyphs = resources.glyphs,
                                          .capacity = 128,
                                          .screen_width = 1920};
    if (okay)
        okay =
            graph(&resources, &error) && clipping(&resources, &error) &&
            pause_page(&resources, &error) && font_getters(&resources, &error) &&
            mighty_eagle(&resources, &error) && animated_image(&resources, &error) &&
            game_hud_controls(&resources, &archive, (AbSpan){key, sizeof(key)}, &error);
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    for (size_t index = 0; index < 3; ++index)
        ab_font_asset_close(resources.assets + index);
    ab_sprite_atlas_close(resources.atlas);
    ab_composite_resources_close(resources.composites);
    ab_sprite_set_close(resources.sprites);
    ab_powerup_catalog_close(resources.catalog);
    ab_target_data_close(&resources.events);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    memset(key, 0, sizeof(key));
    return okay;
}

int main(int argc, char **argv) {
    return argc == 2 ? (check(argv[1]) ? 0 : 1) : 2;
}
