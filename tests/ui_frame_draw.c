#include "ab/ui_frame_draw.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI frame draw failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const char *const methods[] = {"draw", "paint", ""};
static const char *const texts[] = {"", "group", "alternate"};

typedef struct {
    uint32_t calls[8][8];
    size_t count;
    bool fail;
} Boundaries;

typedef struct {
    Boundaries *boundaries;
    uint32_t index;
} Child;

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

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

static bool callback(void *context, AbSpan method, const AbUiDrawTransform *transform,
                     AbError *error) {
    (void)error;
    Child *child = context;
    Boundaries *boundaries = child->boundaries;
    CHECK(boundaries->count < 8);
    uint32_t *call = boundaries->calls[boundaries->count++];
    call[0] = child->index;
    call[1] = 3;
    for (uint32_t index = 0; index < 3; ++index) {
        AbSpan candidate = text(methods[index]);
        if (candidate.size == method.size &&
            (!candidate.size ||
             memcmp(candidate.data, method.data, candidate.size) == 0)) {
            call[1] = index;
        }
    }
    CHECK(call[1] < 3);
    call[2] = bits(transform->position.x);
    call[3] = bits(transform->position.y);
    call[4] = bits(transform->scale.x);
    call[5] = bits(transform->scale.y);
    call[6] = bits(transform->angle);
    call[7] = bits(transform->local_factor);
    return !boundaries->fail;
}

static AbUiPointerValue value(uint32_t kind, uint32_t payload) {
    AbUiPointerValue result = {.kind = (AbUiPointerKind)kind};
    if (kind == AB_UI_POINTER_BOOLEAN) {
        result.boolean = payload != 0;
    } else if (kind == AB_UI_POINTER_NUMBER) {
        result.number_bits = payload;
    } else if (kind == AB_UI_POINTER_TEXT) {
        result.text = text(texts[payload]);
    } else if (kind == AB_UI_POINTER_OBJECT) {
        result.object = payload;
    }
    return result;
}

static bool edge_checks(void) {
    AbUiDrawFrame frame = {{10, 20}, {-2, 3}, 0.5f};
    Boundaries calls = {0};
    Child contexts[4];
    AbUiDrawChild children[4];
    for (uint32_t index = 0; index < 4; ++index) {
        contexts[index] = (Child){&calls, index};
        children[index] = (AbUiDrawChild){callback, contexts + index, {0}, true};
    }
    children[0].tag = value(1, 0);
    children[1].tag = value(2, UINT32_C(0x80000000));
    children[2].tag = value(2, UINT32_C(0x7fc00001));
    children[3].tag = value(4, 9);
    AbUiDrawInput input = {0};
    AbUiPointerValue filter = value(2, 0);
    CHECK(ab_ui_frame_draw(&frame, children, 4, &input, &filter, NULL, NULL) &&
          calls.count == 1 && calls.calls[0][0] == 1 && calls.calls[0][2] == bits(10) &&
          calls.calls[0][3] == bits(20) && calls.calls[0][4] == bits(-2) &&
          calls.calls[0][5] == bits(3) && calls.calls[0][6] == bits(0.5f) &&
          calls.calls[0][7] == bits(1));
    calls.count = 0;
    filter = children[2].tag;
    CHECK(ab_ui_frame_draw(&frame, children, 4, &input, &filter, NULL, NULL) &&
          !calls.count);
    filter = children[0].tag;
    CHECK(ab_ui_frame_draw(&frame, children, 4, &input, &filter, NULL, NULL) &&
          calls.count == 1 && calls.calls[0][0] == 0);
    calls.count = 0;
    filter = children[3].tag;
    AbSpan method = text("");
    input = (AbUiDrawInput){.transform = {{1, 2}, {2, 0}, -0.5f, 0}, .present = 63};
    CHECK(ab_ui_frame_draw(&frame, children, 4, &input, &filter, &method, NULL) &&
          calls.count == 1 && calls.calls[0][0] == 3 && calls.calls[0][1] == 2 &&
          calls.calls[0][2] == bits(11) && calls.calls[0][3] == bits(22) &&
          calls.calls[0][4] == bits(-4) && calls.calls[0][5] == 0 &&
          calls.calls[0][6] == 0 && calls.calls[0][7] == 0);
    calls.count = 0;
    CHECK(ab_ui_frame_draw(&frame, children, 4, &input, NULL, NULL, NULL) &&
          calls.count == 4 && calls.calls[0][0] == 0 && calls.calls[3][0] == 3);
    calls.count = 0;
    calls.fail = true;
    CHECK(!ab_ui_frame_draw(&frame, children, 4, &input, NULL, NULL, NULL) &&
          calls.count == 1);
    children[0].visible = false;
    children[1].callback = NULL;
    CHECK(!ab_ui_frame_draw(&frame, children, 4, &input, NULL, NULL, NULL) &&
          !ab_ui_frame_draw(NULL, children, 4, &input, NULL, NULL, NULL));
    puts("UI frame draw order, numeric defaults/zero, tag equality, method selection, "
         "separate composition and callback failure checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool valid_value(uint32_t kind, uint32_t payload) {
    return kind <= 4 && (kind != 1 || payload <= 1) && (kind < 3 || payload < 3);
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_FRAME_DRAW_V1 4096\n") == 0);
    size_t total = 0, tagged = 0, method_counts[3] = {0};
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[19];
        CHECK(words(file, setup, 19) && setup[0] < 2 && setup[1] <= 8 &&
              setup[2] < 64 && setup[3] < 4 && setup[4] < 2 && setup[5] < 3 &&
              valid_value(setup[6], setup[7]));
        AbUiDrawFrame frame = {{number(setup[14]), number(setup[15])},
                               {number(setup[16]), number(setup[17])},
                               number(setup[18])};
        AbUiDrawInput input = {.transform = {{number(setup[8]), number(setup[9])},
                                             {number(setup[10]), number(setup[11])},
                                             number(setup[12]),
                                             number(setup[13])},
                               .present = setup[2]};
        AbUiPointerValue filter = value(setup[6], setup[7]);
        Boundaries calls = {0};
        Child contexts[8];
        AbUiDrawChild children[8];
        for (uint32_t index = 0; index < setup[1]; ++index) {
            uint32_t child[3];
            CHECK(words(file, child, 3) && child[0] < 4 &&
                  valid_value(child[1], child[2]));
            contexts[index] = (Child){&calls, index};
            children[index] =
                (AbUiDrawChild){callback, contexts + index, value(child[1], child[2]),
                                (child[0] & 1) != 0};
        }
        AbSpan method = text(methods[setup[5]]);
        CHECK(ab_ui_frame_draw(&frame, children, setup[1], &input,
                               setup[0] ? NULL : &filter, setup[4] ? &method : NULL,
                               NULL));
        uint32_t count;
        CHECK(words(file, &count, 1) && count <= 8);
        if (calls.count != count) {
            fprintf(stderr, "UI frame draw case %zu count %zu != %u\n", item,
                    calls.count, count);
            return false;
        }
        for (size_t index = 0; index < count; ++index) {
            uint32_t expected[8];
            CHECK(words(file, expected, 8));
            if (memcmp(expected, calls.calls[index], sizeof(expected)) != 0) {
                fprintf(stderr, "UI frame draw case %zu callback %zu differs\n", item,
                        index);
                return false;
            }
            CHECK(expected[1] < 3);
            ++method_counts[expected[1]];
        }
        total += count;
        tagged += !setup[0] ? count : 0;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI frame draw reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original Frame draws, %zu callbacks (%zu tagged), and %zu/%zu/%zu "
           "default/custom/empty method calls match.\n",
           total, tagged, method_counts[0], method_counts[1], method_counts[2]);
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
