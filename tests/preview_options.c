#include <stdbool.h>

enum { PREVIEW_WIDTH = 480, PREVIEW_HEIGHT = 320 };

#include "../tools/preview_options.h"

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Preview options failed at line %d\n", __LINE__);          \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

int main(void) {
    PreviewOptions options = {0};
    char *normal[] = {"preview", "--record", "Files/game.apk"};
    CHECK(options_read(3, normal, &options) && options.record && !options.record_half &&
          options.main_menu && options.record_audio == CC_CAPTURE_AUDIO_NORMAL &&
          !strcmp(options.apk, "Files/game.apk"));
    char *half[] = {"preview",    "--record",      "half",     "--audio",       "web",
                    "--infinite", "--window-size", "1280x720", "Files/game.apk"};
    CHECK(options_read(9, half, &options) && options.record && options.record_half &&
          options.record_audio == CC_CAPTURE_AUDIO_WEB && options.infinite_powerups &&
          options.window_width == 1280 && options.window_height == 720);
    char *order[] = {"preview",      "--audio",  "web", "--record",
                     "--fullscreen", "--frames", "60",  "Files/game.apk"};
    CHECK(options_read(8, order, &options) && options.record && options.fullscreen &&
          options.frames == 60 && options.record_audio == CC_CAPTURE_AUDIO_WEB);
    char *plain[] = {"preview", "Files/game.apk"};
    CHECK(options_read(2, plain, &options) && !options.record && options.main_menu &&
          options.window_width == 1024 && options.window_height == 768);
    char *requested[] = {"preview", "--inf",
                         "Files/com.rovio.angrybirds_5.2.0-12200520.apk", "--record",
                         "half"};
    CHECK(options_read(5, requested, &options) && options.infinite_powerups &&
          options.main_menu && options.record && options.record_half &&
          !strcmp(options.apk, requested[2]));
    char *trailing[] = {
        "preview", "Files/game.apk", "--record", "half",          "--audio",
        "web",     "--frames",       "60",       "--window-size", "1280x720"};
    CHECK(options_read(10, trailing, &options) && options.record_half &&
          options.record_audio == CC_CAPTURE_AUDIO_WEB && options.frames == 60 &&
          options.window_width == 1280 && options.window_height == 720);
    char *font[] = {"preview", "Files/game.apk", "FONT.dat", "--", "--sample text"};
    CHECK(options_read(5, font, &options) && !options.main_menu &&
          !strcmp(options.font, "FONT.dat") && !strcmp(options.text, "--sample text"));
    char *missing[] = {"preview", "--record", "half"};
    char *audio_only[] = {"preview", "--audio", "web", "Files/game.apk"};
    char *bad_audio[] = {"preview", "--record", "--audio", "other", "Files/game.apk"};
    char *bad_half[] = {"preview", "--record", "quarter", "Files/game.apk"};
    char *bad_frames[] = {"preview", "--record", "--frames", "0", "Files/game.apk"};
    char *bad_trailing[] = {"preview", "Files/game.apk", "--record", "--frames"};
    char *unknown[] = {"preview", "Files/game.apk", "--unknown"};
    char *extra[] = {"preview", "a", "b", "c", "d"};
    CHECK(
        !options_read(3, missing, &options) && !options_read(4, audio_only, &options) &&
        !options_read(5, bad_audio, &options) && !options_read(4, bad_half, &options) &&
        !options_read(5, bad_frames, &options) &&
        !options_read(4, bad_trailing, &options) &&
        !options_read(3, unknown, &options) && !options_read(5, extra, &options));
    FILE *help = tmpfile();
    CHECK(help);
    usage(help);
    CHECK(!ferror(help) && fclose(help) == 0);
    puts("Preview recording options: normal/half/web, option order, default game, "
         "missing operands and invalid combinations.");
    return 0;
}
