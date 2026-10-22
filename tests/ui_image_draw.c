#include "ab/ui_image_draw.h"

#include "ui_widget_draw_fixture.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI image draw failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static AbSpan sprite_name(void) {
    static const char name[] = "controlled-sprite";
    return (AbSpan){(const uint8_t *)name, sizeof(name) - 1};
}

static bool edge_checks(void) {
    AbUiImageDraw image = {.frame = {{10, 20}, {1, 1}, 0},
                           .size = {8.75f, 6.5f},
                           .image = sprite_name(),
                           .image_present = true,
                           .visible = true,
                           .floor_coordinates = true};
    AbUiDrawInput input = {0};
    AbUiImageDrawPlan plan;
    CHECK(ab_ui_image_draw_plan(&image, &input, (AbVec2){480, 320}, &plan) &&
          plan.count == 3 && !plan.culled && plan.calls[0].state.count == 7 &&
          plan.calls[0].state.arguments[0] == 10 &&
          plan.calls[0].state.arguments[1] == 20 &&
          plan.calls[1].kind == AB_UI_IMAGE_SPRITE &&
          plan.calls[1].sprite.size.x == 8 && plan.calls[1].sprite.size.y == 6 &&
          plan.calls[2].state.count == 7);
    image.alpha_present = true;
    image.alpha = 0.5f;
    image.image_present = false;
    CHECK(ab_ui_image_draw_plan(&image, &input, (AbVec2){480, 320}, &plan) &&
          plan.count == 1 && plan.calls[0].state.count == 8 &&
          plan.calls[0].state.arguments[7] == 1);
    image.image_present = true;
    image.frame.position.x = -1000;
    CHECK(ab_ui_image_draw_plan(&image, &input, (AbVec2){480, 320}, &plan) &&
          plan.culled && !plan.count);
    image.frame.angle = 1;
    CHECK(ab_ui_image_draw_plan(&image, &input, (AbVec2){480, 320}, &plan) &&
          !plan.culled && plan.count == 3 && plan.calls[0].state.arguments[7] == 0.5f);
    image.frame = (AbUiDrawFrame){{0, 0}, {0, -0.0f}, 0};
    CHECK(ab_ui_image_draw_plan(&image, &input, (AbVec2){480, 320}, &plan) &&
          plan.count == 3 &&
          bits(plan.calls[0].state.arguments[0]) == UINT32_C(0x7fc00000) &&
          bits(plan.calls[0].state.arguments[1]) == UINT32_C(0x7fc00000));
    AbUiImageDrawPlan saved = plan;
    image.frame.angle = NAN;
    CHECK(!ab_ui_image_draw_plan(&image, &input, (AbVec2){480, 320}, &plan) &&
          memcmp(&saved, &plan, sizeof(plan)) == 0 &&
          !ab_ui_image_draw_plan(NULL, &input, (AbVec2){480, 320}, &plan));
    puts("UI image state/sprite/reset order, alpha, culling, rotation bypass, floored "
         "size, zero-scale NaNs and failed-plan preservation pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_IMAGE_DRAW_V1 4096\n") == 0);
    size_t call_counts[2] = {0}, culled = 0, nonfinite = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[25];
        CHECK(words(file, setup, 25) && setup[0] < 4 && setup[1] < 4 && setup[2] < 64 &&
              setup[3] < 16 && setup[4] < 2);
        AbUiImageState state = widget_image(setup, sprite_name());
        AbUiImageDraw image;
        CHECK(ab_ui_image_state_draw_view(&state, &image));
        AbUiDrawInput input = {.transform = {{number(setup[7]), number(setup[8])},
                                             {number(setup[9]), number(setup[10])},
                                             number(setup[11]),
                                             number(setup[12])},
                               .present = setup[2]};
        AbUiImageDrawPlan plan;
        CHECK(ab_ui_image_draw_plan(
            &image, &input, (AbVec2){number(setup[5]), number(setup[6])}, &plan));
        uint32_t count;
        CHECK(words(file, &count, 1) && count <= 3);
        if (count != plan.count || (count == 0) != plan.culled) {
            fprintf(stderr, "UI image draw case %zu: count %zu != %u\n", item,
                    plan.count, count);
            return false;
        }
        culled += plan.culled;
        for (size_t index = 0; index < plan.count; ++index) {
            uint32_t expected[10], actual[10] = {0};
            CHECK(words(file, expected, 10));
            const AbUiImageDrawCall *call = plan.calls + index;
            actual[0] = (uint32_t)call->kind;
            if (call->kind == AB_UI_IMAGE_RENDER_STATE) {
                CHECK(call->state.count >= 7 && call->state.count <= 8);
                actual[1] = (uint32_t)call->state.count;
                for (size_t field = 0; field < call->state.count; ++field) {
                    actual[2 + field] = bits(call->state.arguments[field]);
                    nonfinite += !isfinite(call->state.arguments[field]);
                }
            } else {
                CHECK(call->kind == AB_UI_IMAGE_SPRITE &&
                      call->sprite.image.size == sprite_name().size &&
                      memcmp(call->sprite.image.data, sprite_name().data,
                             sprite_name().size) == 0);
                actual[1] = 8;
                actual[4] = bits(call->sprite.size.x);
                actual[5] = bits(call->sprite.size.y);
            }
            for (size_t field = 0; field < 10; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "UI image draw case %zu call %zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            item, index, field, actual[field], expected[field]);
                    return false;
                }
            }
            ++call_counts[call->kind];
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI image draw reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "All 4096 original Image draws, %zu state calls, %zu sprite calls, %zu early "
        "culls and %zu nonfinite state arguments match.\n",
        call_counts[0], call_counts[1], culled, nonfinite);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
