#include "ab/game_hud.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Game HUD check failed at %d\n", __LINE__);                \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static void call_words(const AbGameHudCall *call, uint32_t output[12]) {
    memset(output, 0, 12 * sizeof(*output));
    output[0] = (uint32_t)call->kind + 1;
    switch (call->kind) {
        case AB_HUD_SET_FONT:
            output[1] = 1;
            output[2] = (uint32_t)call->font + 1;
            break;
        case AB_HUD_SET_STATE:
            output[1] = 4;
            output[2] = bits(call->position.x);
            output[3] = bits(call->position.y);
            output[4] = bits(call->scale.x);
            output[5] = bits(call->scale.y);
            break;
        case AB_HUD_DRAW_WORLD:
            break;
        case AB_HUD_DRAW_TEXT:
            output[2] = call->text == AB_HUD_SCORE_LABEL       ? 1u
                        : call->text == AB_HUD_HIGHSCORE_LABEL ? 2u
                                                               : 3u;
            output[3] = (uint32_t)call->number;
            output[4] = bits(call->position.x);
            output[5] = bits(call->position.y);
            break;
        case AB_HUD_DRAW_CHILDREN:
            /* The oracle forwards these supplied parent arguments unchanged.
             * The plan leaves the caller's actual parent arguments external. */
            output[1] = 4;
            output[4] = output[5] = bits(1);
            break;
    }
}

static bool edges(void) {
    AbGameHudInput input = {.screen_width = NAN,
                            .margin = {NAN, NAN},
                            .scale = 1,
                            .score = NAN,
                            .font_leading = NAN,
                            .measured_score_width = 20,
                            .previous_score_width = NAN,
                            .update_mode = true};
    AbGameHudPlan output;
    AbError error = {0};
    CHECK(ab_game_hud_plan(&input, &output, &error) && output.count == 5 &&
          output.next_score_width == 20);
    uint8_t retained[sizeof(output)];
    memcpy(retained, &output, sizeof(output));
    CHECK(!ab_game_hud_plan(NULL, &output, &error));
    CHECK(!ab_game_hud_plan(&input, NULL, &error));
    input.retina = true;
    CHECK(!ab_game_hud_plan(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    input = (AbGameHudInput){.screen_width = 480,
                             .scale = 0,
                             .font_leading = 26,
                             .measured_score_width = 20,
                             .show_score = true,
                             .update_mode = true};
    CHECK(!ab_game_hud_plan(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    input.scale = 1;
    input.score = 2147483648.0f;
    CHECK(!ab_game_hud_plan(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    input.score = -1;
    CHECK(!ab_game_hud_plan(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    input.hide_scores = true;
    CHECK(ab_game_hud_plan(&input, &output, &error) && output.count == 7);
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_HUD_DRAW_V1 4096\n") == 0);
    size_t calls = 0, texts = 0, worlds = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[11], count = 0;
        CHECK(words(file, input, 11) && words(file, &count, 1) && input[9] <= 127 &&
              count <= AB_GAME_HUD_CALLS);
        AbGameHudInput state = {.screen_width = number(input[0]),
                                .margin = {number(input[3]), number(input[4])},
                                .scale = number(input[2]),
                                .score = number(input[5]),
                                .highscore = number(input[6]),
                                .font_leading = number(input[7]),
                                .measured_score_width = number(input[8]),
                                .previous_score_width = number(input[10]),
                                .previous_width_present = (input[9] & 16) != 0,
                                .retina = (input[9] & 1) != 0,
                                .highscore_present = (input[9] & 2) != 0,
                                .show_score = (input[9] & 8) != 0,
                                .hide_scores = (input[9] & 4) != 0,
                                .update_mode = (input[9] & 32) == 0,
                                .tutorial_active = (input[9] & 64) != 0};
        AbGameHudPlan plan;
        AbError error = {0};
        CHECK(ab_game_hud_plan(&state, &plan, &error) && plan.count == count);
        for (size_t event = 0; event < count; ++event) {
            uint32_t expected[12], actual[12];
            CHECK(words(file, expected, 12));
            call_words(plan.calls + event, actual);
            if (memcmp(actual, expected, sizeof(actual)) != 0) {
                fprintf(stderr, "HUD differs at case %zu event %zu\n", item, event);
                for (size_t field = 0; field < 12; ++field) {
                    if (actual[field] != expected[field]) {
                        fprintf(stderr, "%zu: %08" PRIx32 " expected %08" PRIx32 "\n",
                                field, actual[field], expected[field]);
                    }
                }
                return false;
            }
            ++calls;
            texts += plan.calls[event].kind == AB_HUD_DRAW_TEXT;
            worlds += plan.calls[event].kind == AB_HUD_DRAW_WORLD;
        }
        uint32_t retained_width;
        CHECK(words(file, &retained_width, 1) &&
              bits(plan.next_score_width) == retained_width);
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game HUD draw reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 native HUD states, %zu ordered calls, %zu texts, %zu world draws "
           "and all retained widths match.\n",
           calls, texts, worlds);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    bool okay = edges();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        okay = file && reference(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
