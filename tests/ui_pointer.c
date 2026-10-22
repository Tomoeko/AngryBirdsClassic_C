#include "ab/ui_pointer.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI pointer failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const char *const events[] = {"LPRESS", "LHOLD", "LRELEASE",
                                     "WHEEL",  "HOVER", "CUSTOM"};
static const char *const texts[] = {"", "CLICK", "HOVER", "reply"};

typedef struct {
    AbSpan event;
    uint32_t event_id;
    uint32_t calls[8][4];
    size_t count;
    bool fail;
} Boundaries;

typedef struct {
    Boundaries *boundaries;
    AbUiPointerReply reply;
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

static bool callback(void *context, AbSpan event, AbVec2 point, AbUiPointerReply *reply,
                     AbError *error) {
    (void)error;
    Child *child = context;
    Boundaries *boundaries = child->boundaries;
    CHECK(boundaries->count < 8 && event.size == boundaries->event.size &&
          (!event.size || memcmp(event.data, boundaries->event.data, event.size) == 0));
    uint32_t *call = boundaries->calls[boundaries->count++];
    call[0] = child->index;
    call[1] = boundaries->event_id;
    call[2] = bits(point.x);
    call[3] = bits(point.y);
    *reply = child->reply;
    return !boundaries->fail;
}

static bool edge_checks(void) {
    Boundaries boundaries = {.event = text("LPRESS")};
    Child contexts[2] = {{.boundaries = &boundaries, .index = 0},
                         {.boundaries = &boundaries, .index = 1}};
    contexts[0].reply.values[0] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = text("CLICK")};
    contexts[1].reply.values[0] = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    contexts[1].reply.values[1] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 17};
    AbUiPointerChild children[] = {{callback, contexts, true, true},
                                   {callback, contexts + 1, true, true}};
    AbUiPointerReply reply;
    CHECK(ab_ui_frame_pointer((AbVec2){10, 20}, children, 2, boundaries.event,
                              (AbVec2){11, 23}, &reply, NULL) &&
          boundaries.count == 1 && boundaries.calls[0][0] == 1 &&
          boundaries.calls[0][2] == bits(1) && boundaries.calls[0][3] == bits(3) &&
          reply.values[0].kind == AB_UI_POINTER_BOOLEAN &&
          !ab_ui_pointer_value_truthy(reply.values) && reply.values[1].object == 17);
    boundaries.count = 0;
    contexts[1].reply.values[0] = (AbUiPointerValue){0};
    CHECK(ab_ui_frame_pointer((AbVec2){0}, children, 2, boundaries.event, (AbVec2){0},
                              &reply, NULL) &&
          boundaries.count == 2 && boundaries.calls[1][0] == 0 &&
          reply.values[0].kind == AB_UI_POINTER_TEXT);
    children[0].visible = false;
    children[1].active = false;
    boundaries.count = 0;
    CHECK(ab_ui_frame_pointer((AbVec2){0}, children, 2, boundaries.event, (AbVec2){0},
                              &reply, NULL) &&
          !boundaries.count && reply.values[0].kind == AB_UI_POINTER_NONE &&
          reply.values[1].kind == AB_UI_POINTER_NONE);
    children[1].active = true;
    boundaries.fail = true;
    reply.values[0] = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 99};
    CHECK(!ab_ui_frame_pointer((AbVec2){0}, children, 2, boundaries.event, (AbVec2){0},
                               &reply, NULL) &&
          reply.values[0].object == 99);
    boundaries.fail = false;
    children[1].callback = NULL;
    CHECK(!ab_ui_frame_pointer((AbVec2){0}, children, 2, boundaries.event, (AbVec2){0},
                               &reply, NULL) &&
          !ab_ui_frame_pointer((AbVec2){0}, NULL, 1, boundaries.event, (AbVec2){0},
                               &reply, NULL));
    CHECK(
        ab_ui_pointer_value_truthy(&(AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER}) &&
        ab_ui_pointer_value_truthy(&(AbUiPointerValue){.kind = AB_UI_POINTER_TEXT}) &&
        ab_ui_pointer_value_truthy(&(AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT}) &&
        ab_ui_pointer_value_truthy(
            &(AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION}) &&
        !ab_ui_pointer_value_truthy(NULL));
    puts("UI reverse child order, nil/false distinction, reply identity, coordinate "
         "subtraction, visibility gates and failed-output preservation pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
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

static uint32_t payload(const AbUiPointerValue *value) {
    switch (value->kind) {
        case AB_UI_POINTER_NONE:
            return 0;
        case AB_UI_POINTER_BOOLEAN:
            return value->boolean;
        case AB_UI_POINTER_NUMBER:
            return value->number_bits;
        case AB_UI_POINTER_TEXT:
            for (uint32_t index = 0; index < 4; ++index) {
                AbSpan candidate = text(texts[index]);
                if (candidate.size == value->text.size &&
                    (!candidate.size ||
                     memcmp(candidate.data, value->text.data, candidate.size) == 0)) {
                    return index;
                }
            }
            return UINT32_MAX;
        case AB_UI_POINTER_OBJECT:
            return (uint32_t)value->object;
        case AB_UI_POINTER_FUNCTION:
            return (uint32_t)value->function;
    }
    return UINT32_MAX;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_POINTER_V1 4096\n") == 0);
    size_t callback_count = 0, false_stops = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[7];
        CHECK(words(file, setup, 7) && setup[0] <= 8 && setup[1] < 6);
        Boundaries boundaries = {.event = text(events[setup[1]]), .event_id = setup[1]};
        Child contexts[8];
        AbUiPointerChild children[8];
        for (size_t index = 0; index < setup[0]; ++index) {
            uint32_t input[7];
            CHECK(words(file, input, 7) && input[0] <= 3);
            contexts[index] =
                (Child){.boundaries = &boundaries, .index = (uint32_t)index};
            for (size_t field = 0; field < 3; ++field) {
                uint32_t kind = input[1 + field * 2], word = input[2 + field * 2];
                CHECK(kind <= 4 && (kind != 1 || word <= 1) && (kind != 3 || word < 4));
                contexts[index].reply.values[field] = value(kind, word);
            }
            children[index] = (AbUiPointerChild){
                callback, contexts + index, (input[0] & 1) != 0, (input[0] & 2) != 0};
        }
        uint32_t expected[8];
        CHECK(words(file, expected, 8) && expected[7] <= 8);
        AbUiPointerReply reply;
        CHECK(ab_ui_frame_pointer((AbVec2){number(setup[2]), number(setup[3])},
                                  children, setup[0], boundaries.event,
                                  (AbVec2){number(setup[4]), number(setup[5])}, &reply,
                                  NULL));
        uint32_t actual[8];
        for (size_t field = 0; field < 3; ++field) {
            actual[field * 2] = (uint32_t)reply.values[field].kind;
            actual[1 + field * 2] = payload(reply.values + field);
        }
        actual[6] = ab_ui_pointer_value_truthy(reply.values);
        actual[7] = (uint32_t)boundaries.count;
        for (size_t field = 0; field < 8; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "UI pointer case %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        for (size_t index = 0; index < expected[7]; ++index) {
            uint32_t call[4];
            CHECK(words(file, call, 4) &&
                  memcmp(call, boundaries.calls[index], sizeof(call)) == 0);
        }
        callback_count += boundaries.count;
        false_stops +=
            reply.values[0].kind == AB_UI_POINTER_BOOLEAN && !reply.values[0].boolean;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI pointer reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original frame dispatches, %zu callbacks and %zu false-primary "
           "stops match.\n",
           callback_count, false_stops);
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
