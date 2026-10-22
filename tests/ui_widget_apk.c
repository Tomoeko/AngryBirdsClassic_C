#include "ab/android_assets.h"
#include "ab/asset_sprite_provider.h"
#include "ab/dynamic_asset_requirements.h"
#include "ab/hash.h"
#include "ab/metal.h"
#include "ab/target_resources.h"
#include "ab/text_resources.h"
#include "ab/ui_widget_resources.h"
#include "ab/ui_main_menu_slider.h"
#include "ab/ui_main_menu_options.h"
#include "ab/ui_toggle_indicator.h"
#include "ab/ui_scroll_frame.h"
#include "ab/font_asset.h"
#include "ab/ui_text_widget.h"

#include "../src/platform/apple/objects.h"
#include "../tools/menu_preview_assets.h"

#include <stdio.h>
#include <string.h>

extern id MTLCreateSystemDefaultDevice(void);
extern id MTLCopyAllDevices(void);

static bool upload(void *context, uint32_t texture, AbSpriteAtlasImage image,
                   AbError *error) {
    return ab_metal_upload(context, texture, image.image, false, false, error);
}

static bool remove_texture(void *context, uint32_t texture, AbError *error) {
    return ab_metal_remove_texture(context, texture, error);
}

static bool save(const char *path, const AbImage *image, AbError *error) {
    if (!strcmp(path, "-"))
        return true;
    FILE *file = fopen(path, "wbx");
    if (!file)
        return ab_fail(error, "Cannot create a new UI capture");
    bool okay = fprintf(file, "P6\n%u %u\n255\n", image->width, image->height) > 0;
    for (size_t index = 0; okay && index < image->rgba.size; index += 4)
        okay = fwrite(image->rgba.data + index, 1, 3, file) == 3;
    if (fclose(file))
        okay = false;
    return okay || ab_fail(error, "Cannot finish UI capture");
}

static bool action(PreviewMenu *menu, AbUiWidget *button, AbVec2 point,
                   const char *expected, AbError *error) {
    AbUiPointerReply reply;
    menu->context.left_button_held = true;
    menu->context.touch_count = 1;
    if (!menu_preview_pointer(menu, menu_span("LPRESS"), point, &reply, error))
        return false;
    menu->context.left_button_held = false;
    menu->context.touch_count = 0;
    if (!menu_preview_pointer(menu, menu_span("LRELEASE"), point, &reply, error))
        return false;
    return (menu_equal(reply.values[0], expected) &&
            reply.values[2].kind == AB_UI_POINTER_OBJECT &&
            reply.values[2].object == (uintptr_t)button) ||
           ab_fail(error, "Actual menu component action differs: %s", expected);
}

static bool widgets(PreviewMenu *menu, AbError *error) {
    if (!action(menu, &menu->options_button, (AbVec2){52, 260}, "TOGGLE_OPTIONS",
                error) ||
        !menu_preview_draw(menu, 1, error))
        return false;
    const AbUiMainMenuOption roles[] = {AB_UI_OPTION_CREDITS, AB_UI_OPTION_SOUND,
                                        AB_UI_OPTION_PUSH};
    const char *returns[] = {"GOTO_CREDITS", "TOGGLE_SOUND",
                             "TOGGLE_PUSH_NOTIFICATIONS"};
    for (size_t index = 0; index < 3; ++index) {
        AbUiWidget *control = menu->options.controls[roles[index]];
        if (!control)
            return ab_fail(error, "Missing APK preview control");
        AbVec2 point = {
            52, 260 + menu_real(control->state.image.frame.fields.y.number_bits)};
        if (!action(menu, control, point, returns[index], error))
            return false;
    }
    if (!action(menu, &menu->play, (AbVec2){240, 320 * menu_real(UINT32_C(0x3f0e147b))},
                "PLAY", error) ||
        !menu_preview_draw(menu, 1, error))
        return false;
    if (!menu_equal(menu->options.controls[AB_UI_OPTION_SOUND]->state.image.image,
                    "BUTTON_MAINMENU_SFX_OFF") ||
        menu->context.draw_list->count != 15 ||
        ab_audio_mixer_view(menu->mixer).pending_count != 5)
        return ab_fail(error, "Actual menu options state/draw/audio count differs");
    return true;
}

/* Real owned scroller, clip setter, active APK sprites and Metal pixels.
 * Layout is an explicit fixture, separate from the episode-selection class. */
static bool clipped_widgets(PreviewMenu *menu, AbMetal *renderer, const float clear[4],
                            const char *path, AbError *error) {
    AbUiScrollFrame scroll = {0};
    AbUiWidget images[3] = {0};
    AbUiFrameNode *frames[3];
    AbUiDrawChild draws[3];
    AbUiPointerChild pointers[3];
    AbVec2 anchors[2];
    float seconds = 0.02f, wheel = 0;
    AbVec2 offset = {0.5f, -0.5f};
    AbImage captured = {0};
    bool okay = false, entered = false;
    const AbDataValue *pause = ab_data_get(
        menu->event_data.data.globals,
        (AbDataValue){.kind = AB_DATA_STRING, .string = menu_span("EID_GAME_PAUSED")});
    if (!pause)
        return ab_fail(error, "Missing actual APK game pause event");
    menu->context.screen_clip_offset = &offset;
    if (!ab_ui_scroll_frame_init(&scroll, &menu->context,
                                 (AbUiWidgetChildren){frames, draws, pointers, 3},
                                 anchors, 2, NULL,
                                 (AbUiScrollHost){.events = menu->events,
                                                  .pause_event = *pause,
                                                  .input_seconds = &seconds,
                                                  .wheel = &wheel},
                                 error))
        goto release;
    const char *const names[] = {"left", "middle", "right"};
    for (size_t index = 0; index < 3; ++index) {
        AbUiImageArguments arguments = {
            .frame = {.name = menu_text(names[index]),
                      .x = menu_number(70 + (float)index * 80),
                      .y = menu_number(160)}};
        if (!ab_ui_widget_init(images + index, &menu->context, AB_UI_WIDGET_IMAGE,
                               (AbUiWidgetChildren){0}, NULL, &arguments, NULL,
                               error) ||
            !ab_ui_image_state_set_image(&images[index].state.image,
                                         menu_text("BUTTON_TOONS_PLAY"),
                                         &menu->context.images, error) ||
            !ab_ui_widget_add(&scroll.widget, images + index, error))
            goto release;
    }
    if (!ab_ui_widget_lifecycle(&scroll.widget, AB_UI_FRAME_ENTRY, error))
        goto release;
    entered = true;
    if (!ab_ui_scroll_frame_set_clip(
            &scroll, &(AbUiScrollClip){80.25f, 120.25f, 140, 80}, error))
        goto release;
    ab_draw_list_clear(menu->context.draw_list);
    AbUiDrawInput input = {0};
    if (!ab_ui_widget_draw(&scroll.widget, &input, error))
        goto release;
    for (size_t index = 0; index < menu->context.draw_list->count; ++index) {
        AbDrawClip clip = menu->context.draw_list->commands[index].scissor;
        if (!clip.enabled || clip.left != 81 || clip.right != 221 || clip.top != 120 ||
            clip.bottom != 200) {
            ab_fail(error, "Owned scroller lost a clip across Image render states");
            goto release;
        }
    }
    AbDrawClip reset = menu->render.clip;
    if (!reset.enabled || reset.left != 1 || reset.top != -1 || reset.right != 481 ||
        reset.bottom != 320 || menu->context.draw_list->count != 3) {
        ab_fail(error, "Owned scroller clip reset/command count differs");
        goto release;
    }
    if (!ab_metal_capture(renderer, menu->context.draw_list, 480, 320, clear,
                          1024 * 1024, &captured, error))
        goto release;
    size_t colored = 0;
    for (uint32_t y = 0; y < captured.height; ++y)
        for (uint32_t x = 0; x < captured.width; ++x) {
            const uint8_t *pixel =
                captured.rgba.data + ((size_t)y * captured.width + x) * 4;
            bool changed = memcmp(pixel, captured.rgba.data, 4) != 0;
            if (x < 81 || x >= 221 || y < 120 || y >= 200) {
                if (changed) {
                    ab_fail(error, "Owned scroller drew outside screen clip at %u,%u",
                            x, y);
                    goto release;
                }
            } else if (changed)
                ++colored;
        }
    if (!colored || (path && !save(path, &captured, error)))
        goto release;
    printf("Owned APK scroller: 3 quads, offset clip, %zu visible sprite pixels, "
           "all outside pixels retain clear color.\n",
           colored);
    okay = true;
release:
    if (entered && !menu->context.failed &&
        !ab_ui_widget_lifecycle(&scroll.widget, AB_UI_FRAME_EXIT, error))
        okay = false;
    if (!ab_ui_scroll_frame_detach(&scroll, error))
        okay = false;
    menu->context.screen_clip_offset = NULL;
    menu->render.clip = (AbDrawClip){0};
    ab_image_free(&captured);
    return okay;
}

#include "ui_episode_apk.h"

static bool text_widgets(const AbZip *zip, PreviewMenu *menu, AbMetal *renderer,
                         const float clear[4], const char *path,
                         const char *episode_path, const char *first_path,
                         const char *last_path, AbError *error) {
    const char *const paths[] = {"assets/data/fonts/480x320/FONT_BASIC.dat",
                                 "assets/data/fonts/480x320/FONT_LS_SMALL.dat"};
    const char *const names[] = {"FONT_BASIC", "FONT_LS_SMALL"};
    AbFontAsset assets[2] = {0};
    AbUiFontBinding bindings[2] = {0};
    size_t uploaded = 0;
    uint32_t codes[128];
    AbFontGlyphPlacement glyphs[128];
    AbUiFontResources fonts = {.bindings = bindings,
                               .count = 2,
                               .strings = &menu->text_resources,
                               .codes = codes,
                               .glyphs = glyphs,
                               .capacity = 128,
                               .screen_width = 480};
    AbUiTextWidget text = {0};
    AbUiWidget icon = {0};
    AbUiFrameNode *frames[1];
    AbUiDrawChild draws[1];
    AbUiPointerChild pointers[1];
    AbImage captured = {0};
    bool okay = false;
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_font_asset_open(zip, paths[index], 8 * 1024 * 1024, 1024 * 1024,
                                assets + index, error) ||
            !ab_metal_upload(renderer, 61 + (uint32_t)index, &assets[index].image,
                             false, false, error))
            goto release;
        ++uploaded;
        bindings[index] = (AbUiFontBinding){
            menu_span(names[index]), &assets[index].font, 61 + (uint32_t)index,
            assets[index].image.width, assets[index].image.height};
    }
    AbUiFrameFields frame = {.name = menu_text("episodeScore"),
                             .x = menu_number(240),
                             .y = menu_number(160)};
    if (!ab_ui_text_widget_init(&text, &menu->context,
                                (AbUiWidgetChildren){frames, draws, pointers, 1},
                                &fonts, &frame, NULL, error))
        goto release;
    /* EpisodeButton.init assigns these fields after Text.new's actual BASIC
     * initialization. Do not collapse those two source font selections. */
    text.state.fields.text = menu_text("12345");
    text.state.fields.font = menu_text("FONT_LS_SMALL");
    text.alpha = menu_number(0.25f);
    AbUiImageArguments image = {
        .frame = {.name = menu_text("scoreChild"), .x = menu_number(60)}};
    if (!ab_ui_widget_init(&icon, &menu->context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL, &image, NULL, error) ||
        !ab_ui_image_state_set_image(&icon.state.image, menu_text("BUTTON_TOONS_PLAY"),
                                     &menu->context.images, error) ||
        !ab_ui_widget_add(&text.widget, &icon, error))
        goto release;
    float width, height;
    AbVec2 screen;
    if (!ab_ui_text_widget_get_width(&text, &width, error) ||
        !ab_ui_text_widget_get_height(&text, &height, error) ||
        !ab_ui_widget_screen_position(&icon, &screen, error))
        goto release;
    if (text.widget.frame != &text.state.frame ||
        text.widget.tree.state != &text.state.frame ||
        text.state.frame.child_count != 1 || !(width > 0) ||
        height != (float)assets[1].font.leading || screen.x != 300 || screen.y != 160 ||
        fonts.current != bindings + 1) {
        ab_fail(error, "Concrete Text lost canonical tree/metrics/font state");
        goto release;
    }
    menu->render.clip = (AbDrawClip){100, 100, 380, 220, true};
    ab_draw_list_clear(menu->context.draw_list);
    AbUiDrawInput input = {0};
    if (!ab_ui_widget_draw(&text.widget, &input, error))
        goto release;
    AbDrawList *list = menu->context.draw_list;
    if (list->count != 6 || menu->render.alpha != 1) {
        ab_fail(error, "Concrete Text glyph/child/alpha sequence differs");
        goto release;
    }
    for (size_t index = 0; index < list->count; ++index) {
        const AbDrawCommand *command = list->commands + index;
        if ((index < 5 ? command->texture != 62 : command->texture == 62) ||
            command->opacity != (index < 5 ? 0.25f : 1) || !command->scissor.enabled ||
            command->scissor.left != 100 || command->scissor.right != 380) {
            ab_fail(error, "Concrete Text lost font/alpha/child order or clip");
            goto release;
        }
    }
    if (!ab_metal_capture(renderer, list, 480, 320, clear, 1024 * 1024, &captured,
                          error) ||
        (path && !save(path, &captured, error)))
        goto release;
    size_t colored = 0;
    for (size_t index = 0; index < captured.rgba.size; index += 4)
        if (captured.rgba.data[index] != 11 || captured.rgba.data[index + 1] != 101 ||
            captured.rgba.data[index + 2] != 76)
            ++colored;
    if (!colored) {
        ab_fail(error, "Concrete Text Metal capture contains no resource pixels");
        goto release;
    }
    /* Text.draw still invokes Frame.draw when its own native body is hidden. */
    text.state.frame.fields.visible = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    ab_draw_list_clear(list);
    if (!ab_ui_widget_draw(&text.widget, &input, error) || list->count != 1 ||
        list->commands[0].texture == 62) {
        ab_fail(error, "Hidden concrete Text skipped its source child traversal");
        goto release;
    }
    icon.tag = menu_text("scores");
    AbUiPointerValue filter = menu_text("boxes");
    ab_draw_list_clear(list);
    if (!ab_ui_widget_draw_with_tag(&text.widget, &input, &filter, error) ||
        list->count)
        goto release;
    filter = menu_text("scores");
    if (!ab_ui_widget_draw_with_tag(&text.widget, &input, &filter, error) ||
        list->count != 1)
        goto release;
    printf("Actual APK Text: two decoded fonts, five ordered score glyphs then "
           "Image child, canonical Frame queries, alpha/clip, %zu Metal pixels.\n",
           colored);
    okay = episode_widgets(zip, menu, &fonts, renderer, clear, episode_path, first_path,
                           last_path, error);

release:
    menu->render.clip = (AbDrawClip){0};
    for (size_t index = 0; index < uploaded; ++index) {
        AbError cleanup = {0};
        if (!ab_metal_remove_texture(renderer, 61 + (uint32_t)index, &cleanup))
            okay = false;
    }
    for (size_t index = 0; index < 2; ++index)
        ab_font_asset_close(assets + index);
    ab_image_free(&captured);
    return okay;
}

#include "ui_episode_navigation_apk.h"
#include "ui_menu_birds_apk.h"
#include "ui_menu_resolution_apk.h"
#include "ui_level_apk.h"
#include "ui_level_scroll_apk.h"
#include "ui_level_selection_apk.h"
#include "ui_menu_level_route_apk.h"
#include "ui_cutscene_apk.h"
#include "ui_game_session_apk.h"
#include "ui_intro_route_apk.h"
#include "ui_menu_audio_apk.h"
#include "ui_level_hidden_eggs_apk.h"
#include "ui_bird_abilities_apk.h"

int main(int argc, char **argv) {
    if (argc < 2 || argc > 17)
        return 2;
    id pool = ab_apple_new("NSAutoreleasePool");
    id device = MTLCreateSystemDefaultDevice();
    if (!device) {
        id devices = MTLCopyAllDevices();
        uintptr_t count =
            ((uintptr_t (*)(id, SEL))objc_msgSend)(devices, sel_registerName("count"));
        if (count)
            device = ab_apple_retain(((id(*)(id, SEL, uintptr_t))objc_msgSend)(
                devices, sel_registerName("objectAtIndex:"), 0));
        ab_apple_release(devices);
    }
    if (!device) {
        ab_apple_release(pool);
        return 77;
    }
    AbError error = {0};
    AbBuffer apk = {0};
    AbZip zip = {0};
    AbMetal *renderer = NULL;
    PreviewMenu menu = {0};
    AbImage captured = {0};
    AbDrawList list = {0};
    uint8_t digest[32];
    char hash[65];
    bool okay = false;
    if (!ab_file_read(argv[1], 64 * 1024 * 1024, &apk, &error) ||
        !ab_sha256((AbSpan){apk.data, apk.size}, digest, &error))
        goto release;
    ab_digest_hex(digest, hash);
    if (strcmp(hash,
               "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad")) {
        ab_fail(&error, "Expected the pinned Android 5.2.0 APK");
        goto release;
    }
    float clear[] = {11.0f / 255, 101.0f / 255, 76.0f / 255, 1};
    if (!ab_draw_list_init(8192, 4 * 1024 * 1024, &list, &error) ||
        !ab_zip_open((AbSpan){apk.data, apk.size}, &zip, &error) ||
        !ab_metal_create(device, AB_METAL_RGBA8, 128, 1024 * 1024, &renderer, &error) ||
        !menu_preview_open(&zip, (AbAssetTextureHost){renderer, upload, remove_texture},
                           &list, &menu, &error) ||
        !ab_audio_mixer_start(menu.mixer, true, &error) || !widgets(&menu, &error) ||
        !ab_metal_capture(renderer, &list, 480, 320, clear, 1024 * 1024, &captured,
                          &error) ||
        (argc >= 3 && !save(argv[2], &captured, &error)))
        goto release;
    printf("Actual APK menu components: %zu quads, %zu textures, options/"
           "credits/sound/push/play input, muted Metal capture. "
           "Complete startup/navigation/gameplay remains open.\n",
           list.count, ab_asset_textures_count(menu.textures));
    if (!clipped_widgets(&menu, renderer, clear, argc >= 4 ? argv[3] : NULL, &error) ||
        !text_widgets(&zip, &menu, renderer, clear, argc >= 5 ? argv[4] : NULL,
                      argc >= 6 ? argv[5] : NULL, argc >= 7 ? argv[6] : NULL,
                      argc >= 8 ? argv[7] : NULL, &error) ||
        !episode_navigation_widgets(&menu, renderer, clear, argc >= 9 ? argv[8] : NULL,
                                    &error) ||
        !episode_background_widgets(&menu, renderer, argc >= 10 ? argv[9] : NULL,
                                    &error) ||
        !menu_bird_widgets(&menu, renderer, argc >= 11 ? argv[10] : NULL, &error))
        goto release;
    if (!menu_resolution_widgets(&zip, &menu, renderer, &list, &error) ||
        !level_grid_widgets(&menu, renderer, argc >= 12 ? argv[11] : NULL, &error) ||
        !level_scroller_widgets(&menu, renderer, argc >= 13 ? argv[12] : NULL,
                                &error) ||
        !level_selection_widgets(&menu, renderer, argc >= 14 ? argv[13] : NULL,
                                 &error) ||
        !menu_level_route_widgets(&menu, renderer, argc >= 15 ? argv[14] : NULL,
                                  &error) ||
        !cutscene_widgets(&menu, renderer, argc >= 16 ? argv[15] : NULL, &error) ||
        !intro_route_widgets(&zip, &menu, renderer, &list, argc >= 17 ? argv[16] : NULL,
                             &error) ||
        !menu_audio_widgets(&zip, &menu, renderer, &list, &error) ||
        !hidden_egg_widgets(&menu, renderer, &error) ||
        !ability_test_yellow(&menu, renderer, &error) ||
        !ability_test_failure(&menu, renderer, &error))
        goto release;
    okay = true;

release:
    if (!okay)
        fprintf(stderr, "%s\n", error.message);
    menu_preview_free(&menu);
    ab_draw_list_free(&list);
    ab_image_free(&captured);
    ab_zip_close(&zip);
    ab_buffer_free(&apk);
    ab_metal_free(renderer);
    ab_apple_release(device);
    ab_apple_release(pool);
    return okay ? 0 : 1;
}
