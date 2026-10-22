#include "ab/classic_menu_input.h"
#include "ab/ui_menu_keys.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Concrete menu input failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbClassicMenuInput input;
    AbClassicMenuRawInput raw;
    AbClassicTouch source[8], filtered[8], copied[8];
    int32_t keys[8];
    size_t indices[8], array[8];
    AbIntegerOrderNode hash[8];
    AbDataEntry buttons[3];
    AbDataTable tables[3];
    AbUiMenuManager *manager;
    AbUiMenuBoundaries boundaries;
    AbUiMenuKeyLayout key_layout;
    AbUiMenuNode menus[4];
    AbUiFrameState frames[4];
    AbSpan order[4];
    uint32_t calls[16][4], consume;
    size_t count, resets, key_calls;
    bool fail_hold;
} Fixture;

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1);
    return true;
}

static bool reset(void *context, AbUiMenuManager *manager, AbError *error) {
    Fixture *test = context;
    CHECK(manager);
    ++test->resets;
    return ab_classic_menu_input_reset(&test->input, error);
}

static bool node_method(void *context, AbUiMenuNode *node, AbUiMenuMethod method,
                        float first, float second, AbError *error) {
    (void)context;
    (void)node;
    (void)method;
    (void)first;
    (void)second;
    (void)error;
    return true;
}

static bool changed(void *context, AbUiMenuNode *node, AbSpan channel, AbError *error) {
    (void)context;
    (void)node;
    (void)channel;
    (void)error;
    return true;
}

static bool pointer(void *context, AbUiMenuNode *menu, AbSpan event, AbVec2 cursor,
                    AbUiPointerReply *output, AbError *error) {
    Fixture *test = context;
    const char *const events[] = {"LPRESS", "LHOLD", "LRELEASE", "WHEEL"};
    size_t phase = 0;
    while (phase < 4 && (event.size != strlen(events[phase]) ||
                         memcmp(event.data, events[phase], event.size)))
        ++phase;
    CHECK(phase < 4 && menu->identity >= 1 && menu->identity <= 4 && test->count < 16 &&
          !ab_classic_menu_input_reset(&test->input, NULL) &&
          !ab_classic_menu_input_clicks(&test->input, test->manager, NULL));
    uint32_t *record = test->calls[test->count++];
    record[0] = (uint32_t)phase;
    record[1] = (uint32_t)menu->identity;
    record[2] = bits(cursor.x);
    record[3] = bits(cursor.y);
    if (phase == 1 && test->fail_hold)
        return ab_fail(error, "Intentional live menu hold failure");
    *output = (AbUiPointerReply){0};
    output->values[0] = (AbUiPointerValue){
        .kind = AB_UI_POINTER_BOOLEAN,
        .boolean = (test->consume & (1u << (phase * 4 + menu->identity - 1))) != 0};
    return true;
}

static bool clicks(void *context, AbError *error) {
    Fixture *test = context;
    return ab_classic_menu_input_clicks(&test->input, test->manager, error);
}

static bool key_down(void *context, AbUiKeyPhase phase, AbSpan key, bool *output,
                     AbError *error) {
    Fixture *test = context;
    (void)error;
    const AbDataValue *value =
        ab_data_get(test->raw.pressed + phase,
                    (AbDataValue){.kind = AB_DATA_STRING, .string = key});
    *output = value && value->kind == AB_DATA_BOOLEAN && value->bits;
    return true;
}

static bool keys(void *context, AbError *error) {
    Fixture *test = context;
    ++test->key_calls;
    return ab_ui_menu_keys(test->manager, &test->key_layout,
                           &(AbUiMenuKeyBoundaries){.context = test,
                                                    .device = AB_UI_KEY_DEVICE_OTHER,
                                                    .down = key_down},
                           error);
}

static bool open_fixture(Fixture *test) {
    memset(test, 0, sizeof(*test));
    for (size_t phase = 0; phase < 3; ++phase) {
        test->buttons[phase] =
            (AbDataEntry){{.kind = AB_DATA_STRING, .string = span("LBUTTON")},
                          {.kind = AB_DATA_BOOLEAN}};
        test->tables[phase] = (AbDataTable){test->buttons + phase, 1, 1, NULL};
    }
    test->raw = (AbClassicMenuRawInput){.pressed = test->tables,
                                        .held = test->tables + 1,
                                        .released = test->tables + 2,
                                        .cursor = {12, 34},
                                        .touches = test->source};
    AbClassicMenuInputStorage storage = {
        test->filtered,
        test->copied,
        test->keys,
        8,
        {test->indices, 8, test->array, 8, test->hash, 8}};
    CHECK(
        ab_classic_menu_input_initialize((AbClassicMenuInputConfig){.raw = &test->raw,
                                                                    .storage = storage,
                                                                    .pointer = pointer,
                                                                    .context = test},
                                         &test->input, NULL) &&
        ab_ui_menu_key_layout(&test->key_layout, NULL));
    test->boundaries = (AbUiMenuBoundaries){.context = test,
                                            .screen_size = {480, 320},
                                            .root_changed = changed,
                                            .reset_filtered_input = reset,
                                            .delegate_clicks = clicks,
                                            .delegate_keys = keys};
    CHECK(ab_ui_menu_manager_open(32, 65536, &test->boundaries, &test->manager, NULL));
    const char *const channels[] = {"one", "two", "three", "four"};
    for (size_t index = 0; index < 4; ++index) {
        test->order[index] = span(channels[index]);
        test->menus[index] = (AbUiMenuNode){.frame = test->frames + index,
                                            .identity = index + 1,
                                            .method = node_method};
    }
    return true;
}

static bool setup(Fixture *test, uint32_t flags, uint32_t missing) {
    CHECK(ab_ui_menu_manager_reset(test->manager, &test->boundaries, NULL));
    test->count = 0;
    for (size_t phase = 0; phase < 3; ++phase)
        test->buttons[phase].value.bits = !!(flags & (1u << phase));
    test->raw.wheel = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = !!(flags & 8)};
    CHECK(ab_ui_menu_manager_allow_input(
        test->manager,
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = !!(flags & 16)}));
    const char *const channels[] = {"one", "two", "three", "four"};
    for (size_t index = 0; index < 4; ++index) {
        test->order[index] = span(channels[index]);
        if (!(missing & (1u << index))) {
            AbSpan channel = test->order[index];
            CHECK(ab_ui_menu_set_root(test->manager, test->menus + index, &channel,
                                      &test->boundaries, NULL));
        }
    }
    return ab_ui_menu_manager_input_order(test->manager, test->order, 4, NULL);
}

static bool edge_checks(void) {
    Fixture test;
    CHECK(open_fixture(&test));
    const int32_t ids[] = {17, 0, -3, 2};
    for (size_t index = 0; index < 4; ++index)
        test.source[index] = (AbClassicTouch){
            ids[index], {(float)index, NAN}, {.kind = AB_DATA_NUMBER, .bits = bits(0)}};
    test.raw.touch_count = 4;
    CHECK(setup(&test, 16, 0) &&
          ab_ui_menu_update(test.manager, 0, 4, &test.boundaries, NULL));
    AbClassicMenuInputView view = ab_classic_menu_input_view(&test.input);
    CHECK(view.count == 4 && !view.pressed->count && !view.held->count &&
          !view.released->count && view.wheel.kind == AB_DATA_BOOLEAN &&
          !view.wheel.bits && test.key_calls == 1);
    const AbClassicTouch *copied = NULL;
    size_t count = 0;
    CHECK(ab_classic_menu_input_copy_touches(&test.input, &copied, &count, NULL) &&
          count == 4 && copied != view.touches);
    for (size_t index = 0; index < count; ++index) {
        CHECK(isnan(copied[index].position.y) &&
              copied[index].consumed.kind == AB_DATA_NUMBER &&
              !copied[index].consumed.bits);
        bool found = false;
        for (size_t original = 0; original < 4; ++original)
            found |= copied[index].id == ids[original];
        CHECK(found);
    }
    test.source[0].position.x = 999;
    CHECK(view.touches[0].position.x != 999 && copied[0].position.x != 999);
    /* Falsy allowInput preserves an earlier cache without an implicit reset. */
    CHECK(ab_ui_menu_manager_allow_input(test.manager, (AbUiPointerValue){0}) &&
          ab_classic_menu_input_clicks(&test.input, test.manager, NULL) &&
          ab_classic_menu_input_view(&test.input).count == 4);
    CHECK(setup(&test, 31, 0));
    test.fail_hold = true;
    AbError late = {0};
    CHECK(!ab_classic_menu_input_clicks(&test.input, test.manager, &late) &&
          strstr(late.message, "Intentional live menu hold failure") &&
          test.input.failed && !test.input.calling);
    view = ab_classic_menu_input_view(&test.input);
    CHECK(view.pressed->count == 1 && view.pressed->entries[0].value.bits &&
          !view.held->count && !view.released->count && !view.count &&
          !ab_classic_menu_input_reset(&test.input, NULL));
    ab_ui_menu_manager_close(test.manager);
    CHECK(open_fixture(&test) && setup(&test, 17, 0));
    test.raw.touch_count = 1;
    test.source[0] = (AbClassicTouch){0, {10, 20}, {0}};
    test.raw.extra_mouse_events = (AbDataValue){.kind = AB_DATA_NUMBER};
    CHECK(!ab_classic_menu_input_clicks(&test.input, test.manager, &late) &&
          strstr(late.message, "Missing reached extra mouse") && test.input.failed &&
          ab_classic_menu_input_view(&test.input).count == 1 &&
          test.input.tables[0].entries[0].value.bits);
    ab_ui_menu_manager_close(test.manager);
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_MENU_POINTER_V1 4096\n"));
    Fixture test;
    CHECK(open_fixture(&test));
    size_t callbacks = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[10], touch[8][3], expected[5];
        CHECK(words(file, input, 10) && input[5] <= 8);
        for (size_t index = 0; index < input[5]; ++index)
            CHECK(words(file, touch[index], 3));
        CHECK(words(file, expected, 5) && expected[4] <= 16 &&
              setup(&test, input[0], input[1]));
        const AbSpan channels[] = {span("one"), span("two"), span("three"),
                                   span("four")};
        for (size_t index = 0; index < 4; ++index) {
            CHECK(input[6 + index] >= 1 && input[6 + index] <= 4);
            test.order[index] = channels[input[6 + index] - 1];
        }
        test.consume = input[2];
        test.raw.cursor = (AbVec2){number(input[3]), number(input[4])};
        test.raw.touch_count = input[5];
        for (size_t index = 0; index < input[5]; ++index)
            test.source[index] =
                (AbClassicTouch){(int32_t)number(touch[index][0]),
                                 {number(touch[index][1]), number(touch[index][2])},
                                 {0}};
        CHECK(ab_classic_menu_input_clicks(&test.input, test.manager, NULL));
        AbClassicMenuInputView view = ab_classic_menu_input_view(&test.input);
        uint32_t present = 8, accepted = view.wheel.bits ? 8 : 0;
        const AbDataTable *tables[] = {view.pressed, view.held, view.released};
        for (size_t phase = 0; phase < 3; ++phase)
            if (tables[phase]->count) {
                present |= 1u << phase;
                if (tables[phase]->entries[0].value.bits)
                    accepted |= 1u << phase;
            }
        uint32_t actual[] = {present, accepted,
                             !!(input[0] & 16) &&
                                 (accepted & (input[0] & 15)) == (input[0] & 15),
                             (uint32_t)view.count, (uint32_t)test.count};
        CHECK(!memcmp(actual, expected, sizeof(actual)));
        for (size_t index = 0; index < expected[4]; ++index) {
            uint32_t call[4];
            CHECK(words(file, call, 4) &&
                  !memcmp(call, test.calls[index], sizeof(call)));
        }
        callbacks += test.count;
        for (size_t index = 0; index < input[5]; ++index) {
            uint32_t member[3];
            CHECK(words(file, member, 3));
            const AbClassicTouch *found = NULL;
            for (size_t row = 0; row < view.count; ++row)
                if (view.touches[row].id == (int32_t)number(touch[index][0]))
                    found = view.touches + row;
            CHECK(member[0] == (found != NULL) &&
                  member[1] == (found ? bits(found->position.x) : 0) &&
                  member[2] == (found ? bits(found->position.y) : 0));
        }
    }
    CHECK(fscanf(file, "\n%79[^\n]", header) == 1 &&
          !strcmp(header, "AB menu pointer reference complete"));
    int trailing;
    while ((trailing = fgetc(file)) != EOF)
        CHECK(trailing == '\n' || trailing == '\r');
    CHECK(!ferror(file));
    ab_ui_menu_manager_close(test.manager);
    printf("Concrete MenuManager input matches 4096 native filters, %zu ordered "
           "callbacks and actual touch memberships/coordinates\n",
           callbacks);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 1 && argc != 2)
        return 2;
    bool okay = edge_checks();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        okay = file && compare_native(file);
        if (file)
            fclose(file);
    }
    return okay ? 0 : 1;
}
