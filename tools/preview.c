#include "ab/common_renderer.h"
#include "font_preview_assets.h"
#include "scene_preview_assets.h"
#include "classic_preview_assets.h"
#include "menu_preview_assets.h"

#include "console_common/platform/window_controls.h"
#include "console_common/support/host.h"

#include <errno.h>
#include <limits.h>

typedef struct {
    const char *apk;
    const char *font;
    const char *text;
    const char *capture;
    const char *scene;
    unsigned frames;
    int window_width, window_height;
    bool fullscreen;
    bool level_one;
    bool main_menu;
    bool fixed_step;
    bool infinite_powerups;
} PreviewOptions;

static void usage(FILE *file) {
    fputs("Usage: angrybirds-preview [--frames N] [--fullscreen] [--window-size WxH] "
          "[--infinite] [--capture NEW_PPM] "
          "APK\n"
          "       angrybirds-preview [OPTIONS] APK FONT_DAT_ENTRY UTF8_TEXT\n"
          "       angrybirds-preview [OPTIONS] --scene SCENE_FILE APK\n"
          "       angrybirds-preview [OPTIONS] [--fixed-step] --level-one APK\n"
          "       angrybirds-preview [OPTIONS] --main-menu APK\n"
          "F toggles fullscreen. Escape pauses/resumes gameplay or returns from "
          "results; otherwise it leaves fullscreen. Close the window to quit.\n",
          file);
}

static bool options_read(int argc, char **argv, PreviewOptions *output) {
    PreviewOptions options = {0};
    int index = 1;
    while (index < argc && argv[index][0] == '-') {
        const char *option = argv[index++];
        if (strcmp(option, "--fullscreen") == 0) {
            options.fullscreen = true;
        } else if (strcmp(option, "--window-size") == 0 && index < argc) {
            const char *value = argv[index++];
            char *end;
            errno = 0;
            unsigned long width = strtoul(value, &end, 10);
            if (errno || value[0] < '0' || value[0] > '9' || *end != 'x' || !width ||
                width > 16384)
                return false;
            value = end + 1;
            errno = 0;
            unsigned long height = strtoul(value, &end, 10);
            if (errno || value[0] < '0' || value[0] > '9' || *end || !height ||
                height > 16384)
                return false;
            options.window_width = (int)width;
            options.window_height = (int)height;
        } else if (strcmp(option, "--main-menu") == 0) {
            options.main_menu = true;
        } else if (strcmp(option, "--inf") == 0 || strcmp(option, "--infinite") == 0) {
            options.infinite_powerups = true;
        } else if (strcmp(option, "--level-one") == 0) {
            options.level_one = true;
        } else if (strcmp(option, "--fixed-step") == 0) {
            options.fixed_step = true;
        } else if (strcmp(option, "--capture") == 0 && index < argc) {
            options.capture = argv[index++];
        } else if (strcmp(option, "--scene") == 0 && index < argc) {
            options.scene = argv[index++];
        } else if (strcmp(option, "--frames") == 0 && index < argc) {
            const char *value = argv[index++];
            char *end;
            errno = 0;
            unsigned long count = strtoul(value, &end, 10);
            if (errno || !value[0] || value[0] == '-' || *end || !count ||
                count > UINT_MAX) {
                return false;
            }
            options.frames = (unsigned)count;
        } else {
            return false;
        }
    }
    unsigned modes = (options.scene ? 1u : 0u) + (options.level_one ? 1u : 0u) +
                     (options.main_menu ? 1u : 0u);
    if (!modes && argc - index == 1) {
        options.main_menu = true;
        modes = 1;
    }
    if (modes > 1 || (options.infinite_powerups && !options.main_menu) ||
        (options.fixed_step && !options.level_one) || argc - index != (modes ? 1 : 3)) {
        return false;
    }
    if (!options.window_width) {
        options.window_width = options.main_menu ? 1024 : PREVIEW_WIDTH;
        options.window_height = options.main_menu ? 768 : PREVIEW_HEIGHT;
    }
    options.apk = argv[index];
    if (!modes) {
        options.font = argv[index + 1];
        options.text = argv[index + 2];
    }
    *output = options;
    return true;
}

static bool menu_upload(void *context, uint32_t texture, AbSpriteAtlasImage image,
                        AbError *error) {
    return ab_common_upload(context, texture, image.image, image.repeat_s,
                            image.repeat_t, error);
}

static bool menu_remove(void *context, uint32_t texture, AbError *error) {
    return ab_common_unload(context, texture, error);
}

static bool level_preview_upload(const PreviewLevel *level, AbCommonRenderer *renderer,
                                 AbError *error) {
    size_t count = level_preview_texture_count(level);
    for (size_t index = 0; index < count; ++index) {
        uint32_t texture;
        const AbImage *image;
        if (!level_preview_texture(level, index, &texture, &image) ||
            !ab_common_upload(renderer, texture, image, false, false, error)) {
            return false;
        }
    }
    return true;
}

static bool capture_frame(CcPlatform *platform, const char *path, AbError *error) {
    CcFramebuffer captured;
    if (!cc_platform_capture_frame(platform, &captured) || !captured.rgba ||
        captured.width <= 0 || captured.height <= 0 || captured.width > 16384 ||
        captured.height > 16384 || captured.stride != (size_t)captured.width * 4) {
        return ab_fail(error,
                       "Preview capture is unavailable or has unsupported dimensions");
    }
    AbImage frame = {(uint32_t)captured.width,
                     (uint32_t)captured.height,
                     {(uint8_t *)captured.rgba,
                      (size_t)captured.width * (size_t)captured.height * 4}};
    return save_ppm(path, &frame, error);
}

static bool present_preview(CcPlatform *platform, AbCommonRenderer *renderer,
                            AbDrawList *list, PreviewLevel *level, PreviewMenu *menu,
                            PreviewOptions options, AbError *error) {
    CcWindowControls controls = {0};
    bool quit = false;
    bool pointer_down = false;
    AbCameraTouch pointer = {0};
    unsigned frame = 0;
    CcFramebuffer capture = {0};
    bool objects = options.scene || options.level_one;
    float clear[] = {objects ? 0 : 0.2f, objects ? 0 : 0.4f, objects ? 0 : 0.6f, 1};
    struct timespec previous = {0};
    if (menu) {
        clear[0] = 11.0f / 255;
        clear[1] = 101.0f / 255;
        clear[2] = 76.0f / 255;
    }
    if ((menu || (level && !options.fixed_step)) && !cc_host_time(&previous)) {
        return ab_fail(error, "Cannot read camera preview clock");
    }
    if (options.fullscreen && !cc_platform_set_fullscreen(platform, true)) {
        return ab_fail(error, "Cannot enter native fullscreen");
    }
    while (!quit && (!options.frames || frame < options.frames)) {
        CcEvent event;
        while (cc_platform_poll(platform, &event)) {
            if (cc_window_controls_event(&controls, platform, &event, false)) {
                continue;
            }
            if (menu) {
                AbUiPointerReply reply = {0};
                AbSpan action = {0};
                AbVec2 point = {(float)event.x, (float)event.y};
                if (event.type == CC_EVENT_POINTER_SCROLL && !event.outside_viewport &&
                    !game_session_scroll(
                        menu, event.scroll_y * (event.scroll_precise ? 0.1f : 1), point,
                        error))
                    return false;
                if (event.cancel_capture) {
                    menu->context.left_button_held = false;
                    menu->context.touch_count = 0;
                    AbUiWidget *root = menu_active_root(menu);
                    if (!ab_ui_widget_clear_pointer_flags(root, error))
                        return false;
                } else if (event.type == CC_EVENT_POINTER_DOWN &&
                           event.button == CC_POINTER_LEFT && !event.outside_viewport) {
                    menu->context.left_button_held = true;
                    menu->context.touch_count = 1;
                    action = menu_span("LPRESS");
                } else if (event.type == CC_EVENT_POINTER_UP &&
                           event.button == CC_POINTER_LEFT) {
                    menu->context.left_button_held = false;
                    menu->context.touch_count = 0;
                    action = menu_span("LRELEASE");
                } else if (event.type == CC_EVENT_POINTER_MOVE) {
                    action =
                        menu_span(menu->context.left_button_held ? "LHOLD" : "HOVER");
                }
                if (action.size &&
                    !menu_preview_pointer(menu, action, point, &reply, error))
                    return false;
                if (menu_equal(reply.values[0], "TOGGLE_FULLSCREEN")) {
                    if (!cc_platform_set_fullscreen(
                            platform, !cc_platform_is_fullscreen(platform)))
                        return ab_fail(error, "Cannot change native fullscreen");
                } else if (menu_equal(reply.values[0], "GOTO_CREDITS") ||
                           menu_equal(reply.values[0], "GOTO_LEVEL")) {
                    fprintf(stdout, "Menu action: %.*s (navigation is unfinished)\n",
                            (int)reply.values[0].text.size,
                            (const char *)reply.values[0].text.data);
                }
            }
            if (level && event.cancel_capture) {
                pointer_down = false;
                (void)ab_camera_touch_init(&level->touch, &level->shared_touch);
            } else if (event.type == CC_EVENT_POINTER_DOWN &&
                       event.button == CC_POINTER_LEFT && !event.outside_viewport) {
                pointer_down = true;
                pointer.position = (AbVec2){(float)event.x, (float)event.y};
            } else if (event.type == CC_EVENT_POINTER_MOVE) {
                pointer.position = (AbVec2){(float)event.x, (float)event.y};
            } else if ((event.type == CC_EVENT_POINTER_UP &&
                        event.button == CC_POINTER_LEFT) ||
                       event.type == CC_EVENT_POINTER_LEAVE) {
                pointer_down = false;
            }
            if (event.type == CC_EVENT_QUIT) {
                quit = true;
            } else if (event.type == CC_EVENT_KEY_UP && event.key == CC_KEY_ESCAPE) {
                if (menu && menu->game) {
                    if (!game_session_escape(menu, error))
                        return false;
                } else if (cc_platform_is_fullscreen(platform) &&
                           !cc_platform_set_fullscreen(platform, false)) {
                    return ab_fail(error, "Cannot leave native fullscreen");
                }
            }
        }
        if (quit) {
            break;
        }
        int native_width = 0, native_height = 0;
        if (menu) {
            if (!cc_platform_drawable_size(platform, &native_width, &native_height)) {
                cc_host_sleep(10);
                continue;
            }
            if (!menu_preview_resize(
                    menu, (AbVec2){(float)native_width, (float)native_height}, error))
                return false;
        }
        if (options.capture) {
            int width, height;
            if (!cc_platform_drawable_size(platform, &width, &height)) {
                cc_host_sleep(10);
                continue;
            }
            if (capture.width != width || capture.height != height) {
                cc_platform_capture_end(platform);
                if (!cc_platform_capture_begin(platform, &capture))
                    return ab_fail(error, "Cannot prepare native preview capture");
            }
        }
        if (level) {
            float seconds = 0x1.111112p-5f;
            if (!options.fixed_step) {
                struct timespec current;
                if (!cc_host_time(&current)) {
                    return ab_fail(error, "Cannot advance camera preview clock");
                }
                double elapsed = (double)current.tv_sec - (double)previous.tv_sec;
                elapsed += ((double)current.tv_nsec - (double)previous.tv_nsec) * 1e-9;
                previous = current;
                seconds = ab_app_frame_seconds((float)elapsed);
            }
            AbCameraTouchInput input = {.touches = &pointer,
                                        .count = pointer_down ? 1u : 0u,
                                        .ingame = true,
                                        .released_after_specialty = true};
            if (!level_preview_draw(level, seconds, list, &input, error)) {
                return false;
            }
            theme_preview_clear(&level->theme, clear);
        }
        if (menu) {
            struct timespec current;
            if (!cc_host_time(&current))
                return ab_fail(error, "Cannot advance menu preview clock");
            double elapsed = (double)current.tv_sec - (double)previous.tv_sec;
            elapsed += ((double)current.tv_nsec - (double)previous.tv_nsec) * 1e-9;
            previous = current;
            if (!menu_preview_draw(menu, ab_app_frame_seconds((float)elapsed), error))
                return false;
            if (menu->background_enabled)
                menu_background_clear(menu, clear);
            if (!ab_audio_output_pump(menu->audio_output, error))
                return false;
        }
        bool rendered =
            menu ? ab_common_render_native(renderer, list, clear, native_width,
                                           native_height, error)
                 : ab_common_render(renderer, list, clear, error);
        if (!rendered)
            return false;
        ++frame;
        if (options.capture && (!options.frames || frame == options.frames)) {
            if (!capture_frame(platform, options.capture, error)) {
                return false;
            }
            options.capture = NULL;
            cc_platform_capture_end(platform);
        }
        cc_host_sleep(1);
    }
    if (menu && options.frames) {
        AbAudioOutputView audio = ab_audio_output_view(menu->audio_output);
        fprintf(stdout,
                "Native audio: 16000 Hz, %llu frames, %llu nonzero samples, "
                "%llu underrun frames.\n",
                (unsigned long long)audio.rendered_frames,
                (unsigned long long)audio.nonzero_samples,
                (unsigned long long)audio.underrun_frames);
    }
    return ab_common_wait(renderer, error);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        usage(stdout);
        return 0;
    }
    PreviewOptions options;
    if (!options_read(argc, argv, &options)) {
        usage(stderr);
        return 2;
    }
    int result = 1;
    AbError error = {0};
    AbBuffer apk = {0}, font_bytes = {0}, encoded = {0};
    AbZip zip = {0};
    AbBitmapFont font = {0};
    AbImage image = {0};
    AbDrawList list = {0};
    PreviewScene scene = {0};
    PreviewLevel level = {0};
    PreviewMenu menu = {0};
    CcPlatform *platform = NULL;
    AbCommonRenderer *renderer = NULL;
    if (!pinned_archive(options.apk, &apk, &zip, &error)) {
        goto release_preview;
    }
    size_t draw_capacity = TEXT_LIMIT, texture_capacity = 1;
    if (options.main_menu) {
        draw_capacity = 8192;
        texture_capacity = 128;
        if (!ab_draw_list_init(draw_capacity, 4 * 1024 * 1024, &list, &error))
            goto release_preview;
    } else if (options.level_one) {
        if (!level_preview_open(&zip, &level, &error)) {
            goto release_preview;
        }
        draw_capacity = ab_classic_scene_view(level.scene).count + PREVIEW_THEME_DRAWS;
        texture_capacity = level_preview_texture_count(&level);
        if (!ab_draw_list_init(draw_capacity, 1024 * 1024, &list, &error)) {
            goto release_preview;
        }
    } else if (options.scene) {
        if (!scene_read(options.scene, &zip, &scene, &error) ||
            !ab_draw_list_init(scene.count, 1024 * 1024, &list, &error) ||
            !ab_sprite_scene_draw(scene.items, scene.count, scene.order, scene.camera,
                                  PREVIEW_WIDTH, PREVIEW_HEIGHT, &list, &error)) {
            goto release_preview;
        }
        draw_capacity = scene.count;
        texture_capacity = scene.atlas_count;
    } else if (!read_entry(&zip, options.font, &font_bytes, &error) ||
               !ab_font_read((AbSpan){font_bytes.data, font_bytes.size}, 1024 * 1024,
                             &font, &error) ||
               !metadata_image(&zip, options.font, font.texture, &encoded, &image,
                               &error) ||
               !ab_draw_list_init(TEXT_LIMIT, 1024 * 1024, &list, &error) ||
               !build_text(&font, &image, options.text, true, &list, &error)) {
        goto release_preview;
    }
    platform = cc_platform_create("Angry Birds Classic C", options.window_width,
                                  options.window_height);
    if (!platform) {
        ab_fail(&error, "Cannot create the shared native rendering platform");
        goto release_preview;
    }
    cc_platform_set_cursor_visible(platform, true);
    if (!ab_common_create(platform, draw_capacity, texture_capacity, 4 * 1024 * 1024,
                          &renderer, &error)) {
        goto release_preview;
    }
    if (options.main_menu) {
        int width, height;
        cc_platform_set_native_pointer(platform, true);
        if (!cc_platform_drawable_size(platform, &width, &height)) {
            ab_fail(&error, "Cannot query the initial native menu drawable");
            goto release_preview;
        }
        menu.highest_resources = true;
        menu.infinite_powerups = options.infinite_powerups;
        menu.state_directory = "Files/AngryBirdsC/userstate";
        menu.screen = (AbVec2){(float)width, (float)height};
    }
    bool uploaded =
        options.main_menu
            ? menu_preview_open(
                  &zip, (AbAssetTextureHost){renderer, menu_upload, menu_remove}, &list,
                  &menu, &error)
        : options.level_one ? level_preview_upload(&level, renderer, &error)
        : options.scene     ? scene_upload(&scene, renderer, &error)
                        : ab_common_upload(renderer, 1, &image, false, false, &error);
    if (!uploaded) {
        goto release_preview;
    }
    menu.navigation_enabled = options.main_menu;
    menu.route_enabled = options.main_menu;
    menu.launch_enabled = options.main_menu;
    if (options.main_menu && !menu_background_open(&menu, &error))
        goto release_preview;
    if (options.main_menu &&
        !ab_audio_output_open(menu.mixer, &menu.audio_output, &error))
        goto release_preview;
    if (!present_preview(platform, renderer, &list, options.level_one ? &level : NULL,
                         options.main_menu ? &menu : NULL, options, &error)) {
        goto release_preview;
    }
    if (options.main_menu && !menu_preview_flush(&menu, &error))
        goto release_preview;
    result = 0;

release_preview:
    if (result) {
        fprintf(stderr, "%s\n", error.message);
    }
    menu_preview_free(&menu);
    ab_common_free(renderer);
    cc_platform_destroy(platform);
    ab_draw_list_free(&list);
    scene_free(&scene);
    level_preview_free(&level);
    ab_image_free(&image);
    ab_font_free(&font);
    ab_buffer_free(&encoded);
    ab_buffer_free(&font_bytes);
    ab_zip_close(&zip);
    ab_buffer_free(&apk);
    return result;
}
