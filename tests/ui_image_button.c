#include "ab/ui_image_button.h"
#include "ab/ui_widget_pointer.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "UI image button failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static const char *const events[] = {"LPRESS", "LHOLD", "LRELEASE",
                                     "WHEEL",  "HOVER", "CUSTOM"};
static const char *const texts[] = {"", "CLICK", "HOVER", "reply", "menu_confirm"};

typedef struct {
    uint32_t hit_mask, hits, calls[8][10];
    size_t count;
    bool fail;
    AbUiImageButtonState *state;
} Boundaries;

typedef struct {
    Boundaries *boundaries;
    uint32_t index;
    AbUiPointerReply reply;
} Child;

static AbSpan text(const char *string) {
    return (AbSpan){(const uint8_t *)string, strlen(string)};
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
    } else if (kind == AB_UI_POINTER_FUNCTION) {
        result.function = payload;
    }
    return result;
}

static uint32_t payload(AbUiPointerValue value) {
    if (value.kind == AB_UI_POINTER_BOOLEAN) {
        return value.boolean;
    }
    if (value.kind == AB_UI_POINTER_NUMBER) {
        return value.number_bits;
    }
    if (value.kind == AB_UI_POINTER_OBJECT) {
        return (uint32_t)value.object;
    }
    if (value.kind == AB_UI_POINTER_TEXT) {
        for (uint32_t index = 0; index < 5; ++index) {
            AbSpan candidate = text(texts[index]);
            if (candidate.size == value.text.size &&
                (!candidate.size ||
                 memcmp(candidate.data, value.text.data, candidate.size) == 0)) {
                return index;
            }
        }
        return UINT32_MAX;
    }
    return 0;
}

static bool boundary(Boundaries *boundaries, uint32_t kind, uint32_t index, uint32_t a,
                     uint32_t b, uint32_t x, uint32_t y) {
    CHECK(boundaries->count < 8);
    uint32_t call[] = {kind, index, a, b, x, y};
    uint32_t *output = boundaries->calls[boundaries->count++];
    memcpy(output, call, sizeof(call));
    if (boundaries->state) {
        output[6] = (uint32_t)boundaries->state->click_started.kind;
        output[7] = payload(boundaries->state->click_started);
        output[8] = (uint32_t)boundaries->state->clear_pointer_flags.kind;
        output[9] = payload(boundaries->state->clear_pointer_flags);
    }
    return !boundaries->fail;
}

static bool hit_test(void *context, AbVec2 point, bool *hit, AbError *error) {
    (void)error;
    Boundaries *boundaries = context;
    uint32_t index = boundaries->hits++;
    CHECK(index < 3);
    *hit = ((boundaries->hit_mask >> index) & 1) != 0;
    return boundary(boundaries, 1, index, *hit, 0, bits(point.x), bits(point.y));
}

static bool audio(void *context, AbUiPointerValue sound, float volume, bool loop,
                  AbError *error) {
    (void)error;
    return boundary(context, 2, 0, (uint32_t)sound.kind, payload(sound), bits(volume),
                    loop);
}

static bool child(void *context, AbSpan event, AbVec2 point, AbUiPointerReply *reply,
                  AbError *error) {
    (void)event;
    (void)error;
    Child *item = context;
    *reply = item->reply;
    return boundary(item->boundaries, 3, item->index, 0, 0, bits(point.x),
                    bits(point.y));
}

static bool canonical_checks(void) {
    AbUiImageButtonState state = {
        .image = {.frame = {.fields = {.visible = value(2, 0)}}, .image = value(3, 0)},
        .activate_on_release = value(2, 0),
        .clear_pointer_flags = value(5, 0),
        .enabled = value(1, 1),
        .return_value = value(3, 3)};
    Boundaries calls = {.hit_mask = 7, .state = &state};
    AbUiImageButtonBoundaries boundaries = {
        .context = &calls, .hit_test = hit_test, .play_audio = audio};
    AbUiPointerReply reply;
    CHECK(ab_ui_image_button_state_handle(&state, 17, &boundaries, text("CUSTOM"),
                                          (AbVec2){0}, &reply, NULL) &&
          state.click_started.kind == AB_UI_POINTER_NONE &&
          state.clear_pointer_flags.kind == AB_UI_POINTER_FUNCTION && !calls.count);
    CHECK(ab_ui_image_button_state_pointer(&state, 17, &boundaries, text("LPRESS"),
                                           (AbVec2){0}, &reply, NULL) &&
          state.click_started.kind == AB_UI_POINTER_BOOLEAN &&
          state.click_started.boolean &&
          state.clear_pointer_flags.kind == AB_UI_POINTER_FUNCTION &&
          calls.count == 2 && calls.calls[0][6] == AB_UI_POINTER_NONE &&
          calls.calls[1][6] == AB_UI_POINTER_BOOLEAN && calls.calls[1][7] == 1);
    calls = (Boundaries){.hit_mask = 7, .state = &state};
    CHECK(ab_ui_image_button_state_pointer(&state, 17, &boundaries, text("LRELEASE"),
                                           (AbVec2){0}, &reply, NULL) &&
          state.clear_pointer_flags.kind == AB_UI_POINTER_BOOLEAN &&
          state.clear_pointer_flags.boolean && reply.values[2].object == 17);
    state.click_started = value(4, 12);
    calls = (Boundaries){.hit_mask = 7, .state = &state, .fail = true};
    AbUiPointerReply saved = reply;
    CHECK(!ab_ui_image_button_state_handle(&state, 17, &boundaries, text("HOVER"),
                                           (AbVec2){0}, &reply, NULL) &&
          state.click_started.kind == AB_UI_POINTER_OBJECT &&
          memcmp(&reply, &saved, sizeof(reply)) == 0);
    calls = (Boundaries){.hit_mask = 2, .state = &state, .fail = true};
    boundaries.device_ipad = true;
    boundaries.touch_count = 4;
    CHECK(!ab_ui_image_button_state_handle(&state, 17, &boundaries, text("LRELEASE"),
                                           (AbVec2){0}, &reply, NULL) &&
          state.click_started.kind == AB_UI_POINTER_BOOLEAN &&
          !state.click_started.boolean && calls.calls[0][6] == AB_UI_POINTER_BOOLEAN &&
          calls.calls[0][7] == 0 && memcmp(&reply, &saved, sizeof(reply)) == 0);
    state.clear_pointer_flags = value(5, 0);
    ab_ui_image_button_state_clear_events(&state);
    CHECK(state.click_started.kind == AB_UI_POINTER_BOOLEAN &&
          !state.click_started.boolean &&
          state.clear_pointer_flags.kind == AB_UI_POINTER_FUNCTION);
    ab_ui_image_button_state_clear_flags(&state);
    CHECK(state.clear_pointer_flags.kind == AB_UI_POINTER_BOOLEAN &&
          !state.clear_pointer_flags.boolean);
    ab_ui_image_button_state_clear_flags(NULL);
    ab_ui_image_button_state_clear_events(NULL);
    return true;
}

static bool edge_checks(void) {
    Boundaries calls = {.hit_mask = 7};
    AbUiImageButtonBoundaries boundaries = {
        .context = &calls, .hit_test = hit_test, .play_audio = audio};
    AbUiImageButton button = {
        .visible = true,
        .image_present = true,
        .activate_on_release = true,
        .enabled = {.kind = AB_UI_POINTER_BOOLEAN, .boolean = true},
        .return_value = {.kind = AB_UI_POINTER_BOOLEAN},
        .object = 17};
    AbUiPointerReply reply;
    CHECK(ab_ui_image_button_pointer(&button, &boundaries, text("LPRESS"), (AbVec2){0},
                                     &reply, NULL) &&
          button.click_started && !button.should_clear_pointer_flags &&
          reply.values[0].kind == AB_UI_POINTER_TEXT && payload(reply.values[0]) == 1 &&
          calls.count == 2);
    calls = (Boundaries){.hit_mask = 7};
    button.visible = false;
    CHECK(ab_ui_image_button_pointer(&button, &boundaries, text("LRELEASE"),
                                     (AbVec2){0}, &reply, NULL) &&
          button.click_started && button.should_clear_pointer_flags &&
          reply.values[0].kind == AB_UI_POINTER_BOOLEAN && !reply.values[0].boolean &&
          reply.values[2].object == 17);
    ab_ui_image_button_clear_events(&button);
    CHECK(!button.click_started && button.should_clear_pointer_flags);
    ab_ui_image_button_clear_flags(&button);
    CHECK(!button.click_started && !button.should_clear_pointer_flags);
    calls = (Boundaries){.hit_mask = 7};
    button.visible = true;
    boundaries.device_ipad = true;
    boundaries.touch_count = 4;
    CHECK(ab_ui_image_button_handle(&button, &boundaries, text("LPRESS"), (AbVec2){0},
                                    &reply, NULL) &&
          !button.click_started && reply.values[0].kind == AB_UI_POINTER_NONE &&
          calls.count == 1);
    button.click_started = true;
    calls = (Boundaries){.hit_mask = 7};
    CHECK(ab_ui_image_button_handle(&button, &boundaries, text("HOVER"), (AbVec2){0},
                                    &reply, NULL) &&
          button.click_started && payload(reply.values[0]) == 2);
    boundaries.device_ipad = false;
    button.image_present = false;
    calls = (Boundaries){.hit_mask = 7};
    CHECK(ab_ui_image_button_handle(&button, &boundaries, text("LRELEASE"), (AbVec2){0},
                                    &reply, NULL) &&
          button.click_started && reply.values[0].kind == AB_UI_POINTER_NONE &&
          calls.count == 1);
    button.image_present = true;
    button.enabled = value(1, 0);
    calls = (Boundaries){.hit_mask = 7};
    CHECK(ab_ui_image_button_handle(&button, &boundaries, text("LRELEASE"), (AbVec2){0},
                                    &reply, NULL) &&
          button.click_started && payload(reply.values[0]) == 1 && calls.count == 1);
    button.enabled = value(0, 0);
    button.activate_on_release = false;
    button.disabled_return_value = value(1, 0);
    calls = (Boundaries){.hit_mask = 7};
    CHECK(ab_ui_image_button_handle(&button, &boundaries, text("LPRESS"), (AbVec2){0},
                                    &reply, NULL) &&
          reply.values[0].kind == AB_UI_POINTER_BOOLEAN && !reply.values[0].boolean &&
          reply.values[2].object == 17);
    calls = (Boundaries){.hit_mask = 7, .fail = true};
    reply.values[0] = value(AB_UI_POINTER_OBJECT, 99);
    CHECK(!ab_ui_image_button_handle(&button, &boundaries, text("HOVER"), (AbVec2){0},
                                     &reply, NULL) &&
          reply.values[0].object == 99);
    CHECK(!ab_ui_image_button_pointer(&button, &boundaries, text("HOVER"), (AbVec2){0},
                                      NULL, NULL));
    ab_ui_image_button_clear_events(NULL);
    ab_ui_image_button_clear_flags(NULL);
    puts(
        "ImageButton press/release, hidden activation, false reply, iPad cancellation, "
        "hover priority, distinct resets and callback failure checks pass.");
    return canonical_checks();
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool read_value(uint32_t kind, uint32_t payload) {
    return kind <= 4 && (kind != 1 || payload <= 1) && (kind != 3 || payload < 5);
}

static bool compare_typed_native(FILE *file);

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    if (strcmp(header, "AB_ANDROID_UI_BUTTON_STATE_V1 64 128\n") == 0) {
        return compare_typed_native(file);
    }
    CHECK(strcmp(header, "AB_ANDROID_UI_IMAGE_BUTTON_V1 64 128\n") == 0);
    size_t call_counts[4] = {0}, false_replies = 0, nil_meta_replies = 0;
    for (uint32_t configuration = 0; configuration < 64; ++configuration) {
        uint32_t id;
        CHECK(words(file, &id, 1) && id == configuration);
        AbUiImageButton button = {.click_started = (id & 1) != 0,
                                  .should_clear_pointer_flags = (id & 2) != 0,
                                  .object = id + 128};
        for (size_t step = 0; step < 128; ++step) {
            uint32_t command[19];
            CHECK(words(file, command, 19) && command[0] < 4 && command[1] < 16 &&
                  command[3] < 6 && command[4] < 8);
            Boundaries calls = {.hit_mask = command[4]};
            button.visible = (command[1] & 1) != 0;
            button.image_present = (command[1] & 2) != 0;
            button.activate_on_release = (command[1] & 4) != 0;
            AbUiPointerValue *values[] = {&button.enabled, &button.return_value,
                                          &button.disabled_return_value, &button.meta,
                                          &button.click_sound};
            for (size_t field = 0; field < 5; ++field) {
                CHECK(read_value(command[9 + field * 2], command[10 + field * 2]));
                *values[field] = value(command[9 + field * 2], command[10 + field * 2]);
            }
            Child contexts[2];
            AbUiPointerChild children[2];
            for (uint32_t index = 0; index < 2; ++index) {
                uint32_t input[7];
                CHECK(words(file, input, 7) && input[0] < 4);
                contexts[index] = (Child){.boundaries = &calls, .index = index};
                for (size_t field = 0; field < 3; ++field) {
                    CHECK(read_value(input[1 + field * 2], input[2 + field * 2]));
                    contexts[index].reply.values[field] =
                        value(input[1 + field * 2], input[2 + field * 2]);
                }
                children[index] = (AbUiPointerChild){
                    child, contexts + index, (input[0] & 1) != 0, (input[0] & 2) != 0};
            }
            AbUiImageButtonBoundaries boundaries = {
                .context = &calls,
                .hit_test = hit_test,
                .play_audio = audio,
                .device_ipad = (command[1] & 8) != 0,
                .touch_count = number(command[2]),
                .frame_position = {number(command[5]), number(command[6])},
                .children = children,
                .child_count = 2};
            AbUiPointerReply reply = {0};
            if (command[0] < 2) {
                AbSpan event = text(events[command[3]]);
                AbVec2 point = {number(command[7]), number(command[8])};
                bool okay = command[0] == 0
                                ? ab_ui_image_button_handle(&button, &boundaries, event,
                                                            point, &reply, NULL)
                                : ab_ui_image_button_pointer(
                                      &button, &boundaries, event, point, &reply, NULL);
                CHECK(okay);
            } else if (command[0] == 2) {
                ab_ui_image_button_clear_flags(&button);
            } else {
                ab_ui_image_button_clear_events(&button);
            }
            uint32_t expected[9], actual[9];
            CHECK(words(file, expected, 9) && expected[8] <= 8);
            for (size_t field = 0; field < 3; ++field) {
                actual[field * 2] = (uint32_t)reply.values[field].kind;
                actual[1 + field * 2] = payload(reply.values[field]);
            }
            actual[6] = button.click_started;
            actual[7] = button.should_clear_pointer_flags;
            actual[8] = (uint32_t)calls.count;
            for (size_t field = 0; field < 9; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "UI button %u/%zu field %zu: %08" PRIx32 " != %08" PRIx32
                            "\n",
                            configuration, step, field, actual[field], expected[field]);
                    return false;
                }
            }
            for (size_t index = 0; index < calls.count; ++index) {
                uint32_t call[6];
                CHECK(words(file, call, 6));
                if (memcmp(call, calls.calls[index], sizeof(call)) != 0) {
                    fprintf(stderr, "UI button %u/%zu callback %zu differs\n",
                            configuration, step, index);
                    return false;
                }
                CHECK(call[0] >= 1 && call[0] <= 3);
                ++call_counts[call[0]];
            }
            false_replies += reply.values[0].kind == AB_UI_POINTER_BOOLEAN &&
                             !reply.values[0].boolean;
            nil_meta_replies += reply.values[0].kind == AB_UI_POINTER_NONE &&
                                reply.values[2].kind == AB_UI_POINTER_OBJECT;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI image button reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 original ImageButton commands, %zu hit queries, %zu audio calls, "
           "%zu child calls, %zu false-primary and %zu nil-primary object replies "
           "match.\n",
           call_counts[1], call_counts[2], call_counts[3], false_replies,
           nil_meta_replies);
    return true;
}

static bool typed_value(const uint32_t input[2], AbUiPointerValue *output) {
    CHECK(input[0] <= AB_UI_POINTER_FUNCTION &&
          (input[0] != AB_UI_POINTER_BOOLEAN || input[1] <= 1) &&
          (input[0] != AB_UI_POINTER_TEXT || input[1] < 5) &&
          (input[0] != AB_UI_POINTER_FUNCTION || input[1] == 0));
    *output = value(input[0], input[1]);
    return true;
}

static bool compare_typed_native(FILE *file) {
    size_t call_counts[4] = {0}, retained_click = 0, retained_clear = 0;
    for (uint32_t configuration = 0; configuration < 64; ++configuration) {
        uint32_t id;
        CHECK(words(file, &id, 1) && id == configuration);
        AbUiImageButtonState state = {0};
        for (size_t step = 0; step < 128; ++step) {
            uint32_t command[19], flags[10];
            CHECK(words(file, command, 19) && command[0] < 4 && command[1] < 16 &&
                  command[3] < 6 && command[4] < 8 && words(file, flags, 10));
            Boundaries calls = {.hit_mask = command[4], .state = &state};
            AbUiPointerValue *raw_flags[] = {
                &state.image.frame.fields.visible, &state.image.image,
                &state.activate_on_release, &state.click_started,
                &state.clear_pointer_flags};
            size_t flag_count = step % 7 == 0 ? 5 : 3;
            for (size_t field = 0; field < flag_count; ++field) {
                CHECK(typed_value(flags + field * 2, raw_flags[field]));
            }
            AbUiPointerValue *values[] = {&state.enabled, &state.return_value,
                                          &state.disabled_return_value, &state.meta,
                                          &state.click_sound};
            for (size_t field = 0; field < 5; ++field) {
                CHECK(typed_value(command + 9 + field * 2, values[field]));
            }
            Child contexts[2];
            AbUiPointerChild children[2];
            for (uint32_t index = 0; index < 2; ++index) {
                uint32_t input[7];
                CHECK(words(file, input, 7) && input[0] < 4);
                contexts[index] = (Child){.boundaries = &calls, .index = index};
                for (size_t field = 0; field < 3; ++field) {
                    CHECK(typed_value(input + 1 + field * 2,
                                      contexts[index].reply.values + field));
                }
                children[index] = (AbUiPointerChild){
                    child, contexts + index, (input[0] & 1) != 0, (input[0] & 2) != 0};
            }
            AbUiImageButtonBoundaries boundaries = {
                .context = &calls,
                .hit_test = hit_test,
                .play_audio = audio,
                .device_ipad = (command[1] & 8) != 0,
                .touch_count = number(command[2]),
                .frame_position = {number(command[5]), number(command[6])},
                .children = children,
                .child_count = 2};
            AbUiPointerReply reply = {0};
            if (command[0] < 2) {
                AbSpan event = text(events[command[3]]);
                AbVec2 point = {number(command[7]), number(command[8])};
                bool okay =
                    command[0] == 0
                        ? ab_ui_image_button_state_handle(&state, id + 128, &boundaries,
                                                          event, point, &reply, NULL)
                        : ab_ui_image_button_state_pointer(&state, id + 128,
                                                           &boundaries, event, point,
                                                           &reply, NULL);
                CHECK(okay);
            } else if (command[0] == 2) {
                ab_ui_image_button_state_clear_flags(&state);
            } else {
                ab_ui_image_button_state_clear_events(&state);
            }
            uint32_t expected[11], actual[11];
            CHECK(words(file, expected, 11) && expected[10] <= 8);
            for (size_t field = 0; field < 3; ++field) {
                actual[field * 2] = (uint32_t)reply.values[field].kind;
                actual[1 + field * 2] = payload(reply.values[field]);
            }
            actual[6] = (uint32_t)state.click_started.kind;
            actual[7] = payload(state.click_started);
            actual[8] = (uint32_t)state.clear_pointer_flags.kind;
            actual[9] = payload(state.clear_pointer_flags);
            actual[10] = (uint32_t)calls.count;
            for (size_t field = 0; field < 11; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Typed button %u/%zu field %zu: %08" PRIx32 " != %08" PRIx32
                            "\n",
                            configuration, step, field, actual[field], expected[field]);
                    return false;
                }
            }
            for (size_t index = 0; index < calls.count; ++index) {
                uint32_t call[10];
                CHECK(words(file, call, 10) && call[0] >= 1 && call[0] <= 3);
                if (memcmp(call, calls.calls[index], sizeof(call)) != 0) {
                    fprintf(stderr, "Typed button %u/%zu callback %zu differs\n",
                            configuration, step, index);
                    return false;
                }
                ++call_counts[call[0]];
            }
            retained_click += state.click_started.kind != AB_UI_POINTER_BOOLEAN;
            retained_clear += state.clear_pointer_flags.kind != AB_UI_POINTER_BOOLEAN;
        }
    }
    char ending[80];
    CHECK(fgets(ending, sizeof(ending), file) &&
          strcmp(ending, "AB UI button state reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 typed button commands, %zu hit, %zu audio and %zu child "
           "snapshots match; %zu non-Boolean click and %zu deferred flags survive.\n",
           call_counts[1], call_counts[2], call_counts[3], retained_click,
           retained_clear);
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
