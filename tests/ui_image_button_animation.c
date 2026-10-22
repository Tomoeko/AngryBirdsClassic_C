#include "ab/ui_image_button_animation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI image animation failed at %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiImageButton *button;
    AbUiImageButtonAnimation *animation;
    AbVec2 screen;
    uint32_t calls[3][6];
    size_t count;
    bool hit;
    bool fail_hit;
} Boundaries;

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

static bool boundary(Boundaries *context, uint32_t kind, float a, float b) {
    CHECK(context->count < 3);
    uint32_t *call = context->calls[context->count++];
    call[0] = kind;
    call[1] = bits(context->animation->position.x);
    call[2] = bits(context->animation->position.y);
    call[3] = bits(a);
    call[4] = bits(b);
    call[5] = (context->button->click_started ? 1u : 0u) |
              (context->button->should_clear_pointer_flags ? 2u : 0u);
    return true;
}

static bool image_update(void *context, float seconds, float time, AbError *error) {
    (void)error;
    return boundary(context, 1, seconds, time);
}

static bool position(void *context, AbVec2 *output, AbError *error) {
    (void)error;
    Boundaries *input = context;
    *output = input->screen;
    return boundary(input, 2, output->x, output->y);
}

static bool hit_test(void *context, AbVec2 point, bool *hit, AbError *error) {
    (void)error;
    Boundaries *input = context;
    *hit = input->hit;
    return boundary(input, 3, point.x, point.y) && !input->fail_hit;
}

static bool edge_checks(void) {
    AbUiImageButtonAnimation animation = {.hover_enabled = true,
                                          .scale_on_touch = true};
    CHECK(ab_ui_image_button_mouse_leave(&animation) && animation.hover_count == -1 &&
          animation.hover_state == AB_UI_HOVER_NONE);
    CHECK(ab_ui_image_button_mouse_enter(&animation) && animation.hover_count == 0 &&
          animation.hover_state == AB_UI_HOVER_NONE);
    CHECK(ab_ui_image_button_mouse_enter(&animation) && animation.hover_count == 1 &&
          animation.hover_state == AB_UI_HOVER_ENTER);
    animation.hover_enabled = false;
    CHECK(ab_ui_image_button_mouse_leave(&animation) && animation.hover_count == 0 &&
          animation.hover_state == AB_UI_HOVER_ENTER);
    float factor = -17;
    CHECK(ab_ui_image_button_hover_scale(&animation, &factor) && factor == 1);
    animation.hover_enabled = true;
    AbUiImageButton button = {
        .enabled = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true}};
    Boundaries calls = {.button = &button, .animation = &animation, .screen = {10, 20}};
    AbUiImageButtonAnimationBoundaries boundaries = {.context = &calls,
                                                     .image_update = image_update,
                                                     .screen_position = position,
                                                     .hit_test = hit_test};
    CHECK(ab_ui_image_button_update(&button, &animation, &boundaries, 0.2f, 1, NULL) &&
          animation.hover_scale == 1 && animation.hover_state == AB_UI_HOVER_HOVER &&
          calls.count == 1);
    CHECK(ab_ui_image_button_hover_scale(&animation, &factor) && factor == 1.1f);
    calls.count = 0;
    calls.hit = true;
    animation.hover_state = AB_UI_HOVER_ENTER;
    boundaries.scale_buttons_on_touch = true;
    boundaries.left_button_held = true;
    button.click_started = true;
    CHECK(ab_ui_image_button_update(&button, &animation, &boundaries, 0.01f, 2, NULL) &&
          bits(animation.hover_scale) == bits(0.8f) &&
          animation.hover_state == AB_UI_HOVER_ENTER && calls.count == 3 &&
          calls.calls[2][1] == bits(10) && calls.calls[2][2] == bits(20) &&
          animation.position.x == 0 && animation.position.y == 0);
    calls.count = 0;
    button.should_clear_pointer_flags = true;
    CHECK(ab_ui_image_button_update(&button, &animation, &boundaries, 0.2f, 3, NULL) &&
          !button.click_started && !button.should_clear_pointer_flags &&
          animation.hover_state == AB_UI_HOVER_HOVER && calls.count == 2 &&
          calls.calls[0][5] == 3 && calls.calls[1][5] == 0);
    animation.hover_state = AB_UI_HOVER_NONE;
    animation.hover_scale = -1;
    boundaries.scale_buttons_on_touch = false;
    calls.count = 0;
    CHECK(ab_ui_image_button_update(&button, &animation, &boundaries, 0.5f, 4, NULL) &&
          animation.hover_scale == -1);
    boundaries.scale_buttons_on_touch = true;
    calls.count = 0;
    CHECK(ab_ui_image_button_update(&button, &animation, &boundaries, 0.5f, 4, NULL) &&
          bits(animation.hover_scale) == 0);
    calls.count = 0;
    calls.fail_hit = true;
    button.click_started = true;
    CHECK(!ab_ui_image_button_update(&button, &animation, &boundaries, 0.5f, 4, NULL) &&
          animation.position.x == 0 && animation.position.y == 0);
    animation.hover_scale = NAN;
    factor = -17;
    CHECK(!ab_ui_image_button_hover_scale(&animation, &factor) && factor == -17 &&
          !ab_ui_image_button_mouse_enter(NULL) &&
          !ab_ui_image_button_mouse_leave(NULL));
    puts("ImageButton hover count, disabled hover, pressed scaling, deferred clear, "
         "screen substitution/restoration and conditional scale recovery pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static AbUiPointerValue enabled(uint32_t fixture) {
    switch (fixture) {
        case 0:
            return (AbUiPointerValue){0};
        case 1:
        case 2:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                      .boolean = fixture == 2};
        case 3:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER};
        case 4:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT};
        default:
            return (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT};
    }
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_IMAGE_ANIMATION_V1 64 128\n") == 0);
    size_t callback_counts[4] = {0}, operations[3] = {0};
    for (size_t configuration = 0; configuration < 64; ++configuration) {
        uint32_t setup[4];
        CHECK(words(file, setup, 4) && setup[2] < 5 && setup[3] == configuration);
        AbUiImageButtonAnimation animation = {.hover_scale = number(setup[0]),
                                              .hover_count = number(setup[1]),
                                              .hover_state = (AbUiHoverState)setup[2]};
        AbUiImageButton button = {0};
        for (size_t step = 0; step < 128; ++step) {
            uint32_t command[14];
            CHECK(words(file, command, 14) && command[0] < 3 && command[1] < 16 &&
                  command[2] < 6 && command[3] == 0 && command[12] < 2 &&
                  command[13] < 4);
            animation.position = (AbVec2){number(command[10]), number(command[11])};
            animation.hover_enabled = (command[1] & 1) != 0;
            animation.scale_on_touch = (command[1] & 2) != 0;
            button.enabled = enabled(command[2]);
            button.click_started = (command[13] & 1) != 0;
            button.should_clear_pointer_flags = (command[13] & 2) != 0;
            Boundaries calls = {.button = &button,
                                .animation = &animation,
                                .screen = {number(command[6]), number(command[7])},
                                .hit = command[12] != 0};
            AbUiImageButtonAnimationBoundaries boundaries = {
                .context = &calls,
                .image_update = image_update,
                .screen_position = position,
                .hit_test = hit_test,
                .cursor = {number(command[8]), number(command[9])},
                .scale_buttons_on_touch = (command[1] & 4) != 0,
                .left_button_held = (command[1] & 8) != 0};
            if (!command[0]) {
                CHECK(ab_ui_image_button_update(&button, &animation, &boundaries,
                                                number(command[4]), number(command[5]),
                                                NULL));
            } else if (command[0] == 1) {
                CHECK(ab_ui_image_button_mouse_enter(&animation));
            } else {
                CHECK(ab_ui_image_button_mouse_leave(&animation));
            }
            ++operations[command[0]];
            float factor;
            CHECK(ab_ui_image_button_hover_scale(&animation, &factor));
            uint32_t actual[] = {bits(animation.hover_scale),
                                 bits(animation.hover_count),
                                 (uint32_t)animation.hover_state,
                                 button.click_started,
                                 button.should_clear_pointer_flags,
                                 bits(animation.position.x),
                                 bits(animation.position.y),
                                 bits(factor),
                                 (uint32_t)calls.count};
            uint32_t expected[9];
            CHECK(words(file, expected, 9) && expected[8] <= 3);
            for (size_t field = 0; field < 9; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "UI animation %zu/%zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            configuration, step, field, actual[field], expected[field]);
                    return false;
                }
            }
            for (size_t index = 0; index < calls.count; ++index) {
                uint32_t call[6];
                CHECK(words(file, call, 6) && call[0] >= 1 && call[0] <= 3);
                if (memcmp(call, calls.calls[index], sizeof(call)) != 0) {
                    fprintf(stderr, "UI animation %zu/%zu callback %zu differs\n",
                            configuration, step, index);
                    return false;
                }
                ++callback_counts[call[0]];
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI image animation reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 original ImageButton animation commands (%zu updates, %zu enters, "
           "%zu leaves), 8192 scale factors and %zu/%zu/%zu image/position/hit "
           "callbacks match.\n",
           operations[0], operations[1], operations[2], callback_counts[1],
           callback_counts[2], callback_counts[3]);
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
