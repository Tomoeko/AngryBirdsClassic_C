#ifndef AB_PREVIEW_OPTIONS_H
#define AB_PREVIEW_OPTIONS_H

#include "console_common/capture/capture_writer.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    bool record, record_half;
    CcCaptureAudioMode record_audio;
} PreviewOptions;

static void usage(FILE *file) {
    fputs("Usage: angrybirds-preview [--frames N] [--fullscreen] [--window-size WxH] "
          "[--infinite] [--record [half]] [--audio web] [--capture NEW_PPM] "
          "APK [OPTIONS]\n"
          "       angrybirds-preview [OPTIONS] APK FONT_DAT_ENTRY UTF8_TEXT\n"
          "       angrybirds-preview [OPTIONS] --scene SCENE_FILE APK\n"
          "       angrybirds-preview [OPTIONS] [--fixed-step] --level-one APK\n"
          "       angrybirds-preview [OPTIONS] --main-menu APK\n"
          "Options may appear before or after APK. Use -- to end options.\n"
          "F toggles fullscreen. Escape pauses/resumes gameplay or returns from "
          "results; otherwise it leaves fullscreen. Close the window to quit.\n",
          file);
}

static bool options_read(int argc, char **argv, PreviewOptions *output) {
    PreviewOptions options = {0};
    const char *positional[3] = {0};
    size_t positional_count = 0;
    bool read_options = true;
    int index = 1;
    while (index < argc) {
        const char *option = argv[index++];
        if (read_options && !strcmp(option, "--")) {
            read_options = false;
            continue;
        }
        if (!read_options || option[0] != '-') {
            if (positional_count == sizeof(positional) / sizeof(*positional))
                return false;
            positional[positional_count++] = option;
            continue;
        }
        if (strcmp(option, "--fullscreen") == 0) {
            options.fullscreen = true;
        } else if (strcmp(option, "--record") == 0) {
            options.record = true;
            options.record_half = index < argc && !strcmp(argv[index], "half");
            if (options.record_half)
                ++index;
        } else if (strcmp(option, "--audio") == 0 && index < argc &&
                   !strcmp(argv[index], "web")) {
            options.record_audio = CC_CAPTURE_AUDIO_WEB;
            ++index;
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
    if (!modes && positional_count == 1) {
        options.main_menu = true;
        modes = 1;
    }
    if (modes > 1 ||
        (options.record_audio == CC_CAPTURE_AUDIO_WEB && !options.record) ||
        (options.infinite_powerups && !options.main_menu) ||
        (options.fixed_step && !options.level_one) ||
        positional_count != (modes ? 1u : 3u)) {
        return false;
    }
    if (!options.window_width) {
        options.window_width = options.main_menu ? 1024 : PREVIEW_WIDTH;
        options.window_height = options.main_menu ? 768 : PREVIEW_HEIGHT;
    }
    options.apk = positional[0];
    if (!modes) {
        options.font = positional[1];
        options.text = positional[2];
    }
    *output = options;
    return true;
}

#endif
