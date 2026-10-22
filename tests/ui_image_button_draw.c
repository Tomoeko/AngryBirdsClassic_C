#include "ab/ui_image_button_draw.h"

#include "ui_widget_draw_fixture.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI button draw failed at %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t calls[11][10];
    size_t count;
    bool fail;
} Boundaries;

typedef struct {
    Boundaries *boundaries;
    uint32_t index;
} Child;

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

static bool image_sink(void *context, const AbUiImageDrawPlan *plan, AbError *error) {
    (void)error;
    Boundaries *boundaries = context;
    CHECK(plan->count <= 3 && boundaries->count + plan->count <= 11);
    for (size_t index = 0; index < plan->count; ++index) {
        const AbUiImageDrawCall *call = plan->calls + index;
        uint32_t *record = boundaries->calls[boundaries->count++];
        record[0] = (uint32_t)call->kind;
        if (call->kind == AB_UI_IMAGE_RENDER_STATE) {
            CHECK(call->state.count >= 7 && call->state.count <= 8);
            record[1] = (uint32_t)call->state.count;
            for (size_t field = 0; field < call->state.count; ++field) {
                record[2 + field] = bits(call->state.arguments[field]);
            }
        } else {
            CHECK(call->kind == AB_UI_IMAGE_SPRITE &&
                  call->sprite.image.size == sprite_name().size &&
                  memcmp(call->sprite.image.data, sprite_name().data,
                         sprite_name().size) == 0);
            record[1] = 8;
            record[4] = bits(call->sprite.size.x);
            record[5] = bits(call->sprite.size.y);
        }
    }
    return !boundaries->fail;
}

static bool child_draw(void *context, AbSpan method, const AbUiDrawTransform *transform,
                       AbError *error) {
    (void)error;
    Child *child = context;
    Boundaries *boundaries = child->boundaries;
    CHECK(boundaries->count < 11 && method.size == 4 &&
          memcmp(method.data, "draw", 4) == 0);
    uint32_t *record = boundaries->calls[boundaries->count++];
    record[0] = 2;
    record[1] = 6;
    record[2] = bits(transform->position.x);
    record[3] = bits(transform->position.y);
    record[4] = bits(transform->scale.x);
    record[5] = bits(transform->scale.y);
    record[6] = bits(transform->angle);
    record[7] = bits(transform->local_factor);
    record[8] = child->index;
    return !boundaries->fail;
}

static bool edge_checks(void) {
    AbUiImageDraw image = {.frame = {{10, 20}, {2, 3}, 0},
                           .size = {10, 10},
                           .image = sprite_name(),
                           .image_present = true,
                           .visible = true};
    AbUiImageButtonAnimation animation = {.hover_enabled = true, .hover_scale = 1};
    Boundaries calls = {0};
    Child context = {&calls, 7};
    AbUiDrawChild child = {child_draw, &context, {0}, true};
    AbUiImageButtonDraw button = {&image, &animation, &child, 1};
    AbUiDrawInput input = {.transform = {{1, 2}, {4, 5}, 0.5f, NAN}, .present = 63};
    CHECK(ab_ui_image_button_draw(&button, &input, (AbVec2){480, 320}, image_sink,
                                  &calls, NULL) &&
          calls.count == 4 && calls.calls[0][0] == 0 && calls.calls[1][0] == 1 &&
          calls.calls[2][0] == 0 && calls.calls[3][0] == 2 &&
          calls.calls[0][2] == bits(11.0f / ((4.0f * 1.1f) * 2.0f)) &&
          calls.calls[3][2] == bits(11) && calls.calls[3][3] == bits(22) &&
          calls.calls[3][4] == bits(8) && calls.calls[3][5] == bits(15) &&
          calls.calls[3][7] == bits(1.1f));
    calls = (Boundaries){0};
    button.children = NULL;
    button.child_count = SIZE_MAX;
    CHECK(ab_ui_image_button_draw_self(&button, &input, (AbVec2){480, 320}, image_sink,
                                       &calls, NULL) &&
          calls.count == 3);
    button.children = &child;
    button.child_count = 1;
    calls = (Boundaries){0};
    image.frame.position.x = -1000;
    input.transform.angle = 0;
    CHECK(ab_ui_image_button_draw(&button, &input, (AbVec2){480, 320}, image_sink,
                                  &calls, NULL) &&
          calls.count == 1 && calls.calls[0][0] == 2);
    calls = (Boundaries){.fail = true};
    image.frame.position.x = 10;
    CHECK(!ab_ui_image_button_draw(&button, &input, (AbVec2){480, 320}, image_sink,
                                   &calls, NULL) &&
          calls.count == 3 &&
          !ab_ui_image_button_draw(NULL, &input, (AbVec2){480, 320}, image_sink, &calls,
                                   NULL));
    puts(
        "ImageButton self-before-children, distinct hover transforms, ignored incoming "
        "factor, culled-self traversal and sink failure checks pass.");
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
          strcmp(header, "AB_ANDROID_UI_BUTTON_DRAW_V1 4096\n") == 0);
    size_t counts[3] = {0}, children_without_self = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[25], extra[3], flags[8];
        CHECK(words(file, setup, 25) && setup[0] < 4 && setup[1] < 4 && setup[2] < 64 &&
              setup[3] < 16 && setup[4] < 2 && words(file, extra, 3) && extra[0] <= 1 &&
              extra[2] <= 8);
        if (extra[2]) {
            CHECK(words(file, flags, extra[2]));
        }
        AbUiImageDraw image = {.frame = {{number(setup[13]), number(setup[14])},
                                         {number(setup[15]), number(setup[16])},
                                         number(setup[17])},
                               .size = {number(setup[18]), number(setup[19])},
                               .pivot = {number(setup[20]), number(setup[21])},
                               .rotation_pivot = {number(setup[22]), number(setup[23])},
                               .pivot_present = setup[3],
                               .image = sprite_name(),
                               .alpha = number(setup[24]),
                               .image_present = (setup[0] & 1) != 0,
                               .visible = setup[1] == 2,
                               .floor_coordinates = (setup[0] & 2) != 0,
                               .alpha_present = setup[4] != 0};
        AbUiDrawInput input = {.transform = {{number(setup[7]), number(setup[8])},
                                             {number(setup[9]), number(setup[10])},
                                             number(setup[11]),
                                             number(setup[12])},
                               .present = setup[2]};
        AbUiImageButtonAnimation animation = {.hover_enabled = extra[0] != 0,
                                              .hover_scale = number(extra[1])};
        Boundaries calls = {0};
        Child contexts[8];
        AbUiDrawChild children[8];
        for (uint32_t index = 0; index < extra[2]; ++index) {
            CHECK(flags[index] < 4);
            contexts[index] = (Child){&calls, index};
            children[index] = (AbUiDrawChild){
                child_draw, contexts + index, {0}, (flags[index] & 1) != 0};
        }
        AbUiImageButtonDraw button = {&image, &animation, children, extra[2]};
        CHECK(ab_ui_image_button_draw(&button, &input,
                                      (AbVec2){number(setup[5]), number(setup[6])},
                                      image_sink, &calls, NULL));
        AbUiImageButtonState state = {
            .image = widget_image(setup, sprite_name()),
            .hover_enabled = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = extra[0] != 0},
            .hover_scale = widget_number(extra[1])};
        Boundaries typed_calls = {0};
        for (uint32_t index = 0; index < extra[2]; ++index) {
            contexts[index].boundaries = &typed_calls;
        }
        CHECK(
            ab_ui_image_button_state_draw(&state, children, extra[2], &input,
                                          (AbVec2){number(setup[5]), number(setup[6])},
                                          image_sink, &typed_calls, NULL));
        CHECK(typed_calls.count == calls.count &&
              memcmp(typed_calls.calls, calls.calls, sizeof(calls.calls)) == 0);
        uint32_t count;
        CHECK(words(file, &count, 1) && count <= 11);
        if (count != calls.count) {
            fprintf(stderr, "UI button draw case %zu count %zu != %u\n", item,
                    calls.count, count);
            return false;
        }
        bool child_seen = false, self_seen = false;
        for (size_t index = 0; index < count; ++index) {
            uint32_t expected[10];
            CHECK(words(file, expected, 10) && expected[0] < 3);
            for (size_t field = 0; field < 10; ++field) {
                if (calls.calls[index][field] != expected[field]) {
                    fprintf(stderr,
                            "UI button draw case %zu call %zu field %zu: %08" PRIx32
                            " != %08" PRIx32 "\n",
                            item, index, field, calls.calls[index][field],
                            expected[field]);
                    return false;
                }
            }
            ++counts[expected[0]];
            child_seen |= expected[0] == 2;
            self_seen |= expected[0] != 2;
        }
        children_without_self += child_seen && !self_seen;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI button draw reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original button draw graphs, %zu state, %zu sprite and %zu child "
           "calls match, including %zu traversals after a culled self.\n",
           counts[0], counts[1], counts[2], children_without_self);
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
