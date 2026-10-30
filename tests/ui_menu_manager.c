#include "ab/ui_menu_navigation.h"
#include "ab/ui_menu_keys.h"
#include "ab/ui_key.h"

#include "../tools/oracle/ui_menu_key_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Menu manager failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

enum {
    REQUIREMENTS = 7,
    LOAD,
    RELEASE,
    RECTANGLE,
    NOTIFY,
    CLIP,
    BACKGROUND,
    RESET_INPUT,
    CLICKS,
    KEYS,
    FRAME_PUSHED,
    POP_REQUESTED,
};

typedef struct {
    AbUiMenuManager *manager;
    uint32_t calls[256][159];
    size_t count;
    uint32_t fail_kind;
    float color[4];
    AbVec2 clip_size;
    AbUiMenuNode *root_at_callback;
    AbUiMenuNode *nodes;
    AbUiPointerValue requirement_value;
    bool native_mode, collect_on_callback, extended_links, extended_navigation;
    bool extended_input, extended_keys, extended_frame_keys;
    AbSpan working_order[6];
    AbSpan working_input_order[6];
    uint32_t current_command[16];
    uint8_t key_down[3][72];
    AbUiMenuKeyLayout key_layout;
    AbUiMenuNavigation navigation;
} Context;

static bool key_down(void *context, AbUiKeyPhase phase, AbSpan key, bool *output,
                     AbError *error);
static bool key_event(void *context, AbUiMenuNode *menu, AbSpan phase, AbSpan key,
                      AbUiPointerReply *output, AbError *error);

static const char *const channels[] = {"root", "overlay", "dialog", ""};
static const char *const texts[] = {"", "root", "overlay", "dialog", "flag"};
static const char *const frame_names[] = {"_ab_menu_one", "_ab_menu_two",
                                          "_ab_menu_three", "_ab_menu_four"};
static const char *const pointer_events[] = {"LPRESS", "LHOLD", "LRELEASE", "WHEEL",
                                             "HOVER",  "",      "custom"};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
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

static bool named(AbSpan text, const char *name) {
    return text.size == strlen(name) &&
           (!text.size || memcmp(text.data, name, text.size) == 0);
}

static uint32_t channel_id(AbSpan channel) {
    for (uint32_t index = 0; index < 4; ++index) {
        if (named(channel, channels[index])) {
            return index;
        }
    }
    return UINT32_MAX;
}

static bool encode_value(AbUiPointerValue value, uint32_t output[2]) {
    CHECK(ab_ui_pointer_value_valid(&value));
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    switch (value.kind) {
        case AB_UI_POINTER_BOOLEAN:
            output[1] = value.boolean;
            break;
        case AB_UI_POINTER_NUMBER:
            output[1] = value.number_bits;
            break;
        case AB_UI_POINTER_OBJECT:
            output[1] = (uint32_t)value.object;
            break;
        case AB_UI_POINTER_TEXT:
            output[1] = UINT32_MAX;
            for (uint32_t index = 0; index < 5; ++index) {
                if (named(value.text, texts[index])) {
                    output[1] = index;
                    break;
                }
            }
            CHECK(output[1] != UINT32_MAX);
            break;
        case AB_UI_POINTER_NONE:
        case AB_UI_POINTER_FUNCTION:
            break;
    }
    return true;
}

static bool encode_map(const AbStringMap *map, uint32_t output[23], bool links) {
    memset(output, 0, 23 * sizeof(*output));
    AbStringMapLayout layout;
    CHECK(ab_string_map_layout(map, &layout) && layout.capacity <= 4);
    output[0] = (uint32_t)layout.count;
    output[1] = (uint32_t)layout.capacity;
    output[2] = (uint32_t)layout.last_free;
    for (size_t index = 0; index < layout.capacity; ++index) {
        AbStringMapSlot slot;
        CHECK(ab_string_map_slot(map, index, &slot));
        uint32_t *row = output + 3 + index * 5;
        if (slot.key_present) {
            uint32_t key = channel_id(slot.entry.key);
            CHECK(key != UINT32_MAX);
            row[0] = key + 1;
            row[4] = slot.dead_key ? 11u : 4u;
        }
        row[1] = slot.entry.present;
        if (slot.entry.present) {
            const AbUiMenuNode *node = (const AbUiMenuNode *)slot.entry.value;
            CHECK((node || links) &&
                  (!node || (node->identity >= 1 && node->identity <= 4)));
            row[2] = node ? (uint32_t)node->identity : 0;
        }
        row[3] = slot.next == SIZE_MAX ? 0 : (uint32_t)slot.next + 1;
    }
    return true;
}

static bool capture_state(const Context *context, uint32_t output[151]) {
    AbUiMenuManagerView view;
    CHECK(ab_ui_menu_manager_view(context->manager, &view) && view.menu_count <= 6);
    memset(output, 0, 151 * sizeof(*output));
    CHECK(encode_value(view.allow_input, output) &&
          encode_value(view.update_enabled, output + 2) &&
          encode_value(view.delegate_back_clicks, output + 4));
    output[6] = bits(view.screen_size.x);
    output[7] = bits(view.screen_size.y);
    output[8] = (uint32_t)view.menu_count;
    for (size_t index = 0; index < view.menu_count; ++index) {
        uint32_t key = channel_id(view.menu_order[index]);
        CHECK(key != UINT32_MAX);
        output[9 + index] = key + 1;
    }
    if (context->nodes) {
        for (size_t index = 0; index < 4; ++index) {
            CHECK(encode_value(context->nodes[index].frame->fields.visible,
                               output + 15 + index * 2));
        }
    }
    CHECK(encode_map(view.menus, output + 23, false) &&
          encode_map(view.queue, output + 46, false) &&
          encode_map(view.links, output + 69, true));
    if (context->extended_navigation) {
        for (size_t node = 0; node < 4; ++node) {
            const AbUiMenuFrame *frame = context->navigation.frames + node;
            const AbUiFrameNode *tree = frame->tree;
            uint32_t *row = output + 92 + node * 13;
            CHECK(encode_value(tree->entered, row) &&
                  tree->state->child_count == tree->state->lookup_count &&
                  tree->state->child_count <= 4);
            row[3] = (uint32_t)tree->state->child_count;
            row[12] = frame->remove_from_frame;
            for (size_t candidate = 0; candidate < 4; ++candidate) {
                const AbUiFrameNode *other = context->navigation.frames[candidate].tree;
                if (tree->parent == other) {
                    row[2] = (uint32_t)candidate + 1;
                }
                for (size_t child = 0; child < tree->state->child_count; ++child) {
                    if (tree->children[child] == other) {
                        row[4 + child] = (uint32_t)candidate + 1;
                        row[8 + candidate] = (uint32_t)child + 1;
                    }
                }
            }
        }
    }
    if (context->extended_input) {
        CHECK(view.input_count <= 6);
        output[144] = (uint32_t)view.input_count;
        for (size_t index = 0; index < view.input_count; ++index) {
            uint32_t id = channel_id(view.input_order[index]);
            CHECK(id < 4);
            output[145 + index] = id + 1;
        }
    }
    return true;
}

static bool record(Context *context, uint32_t kind, uint32_t identity) {
    CHECK(context->count < 256);
    if (context->native_mode && context->collect_on_callback) {
        CHECK(ab_ui_menu_manager_collect_nil_keys(context->manager));
    }
    uint32_t *row = context->calls[context->count++];
    memset(row, 0, 159 * sizeof(*row));
    row[0] = kind;
    row[1] = identity;
    CHECK(capture_state(context, row + 8));
    AbUiMenuManagerView view;
    CHECK(ab_ui_menu_manager_view(context->manager, &view));
    uintptr_t value;
    bool present;
    CHECK(ab_string_map_get(view.menus, span("root"), &value, &present, NULL));
    context->root_at_callback = present ? (AbUiMenuNode *)value : NULL;
    return context->fail_kind != kind;
}

static bool menu_method(void *context, AbUiMenuNode *node, AbUiMenuMethod method,
                        float first, float second, AbError *error) {
    (void)error;
    if (method != AB_UI_MENU_UPDATE) {
        CHECK(first == 0 && second == 0);
    }
    Context *provider = context;
    bool okay = record(provider, (uint32_t)method, (uint32_t)node->identity);
    if (provider->native_mode &&
        (method == AB_UI_MENU_UPDATE || method == AB_UI_MENU_DRAW)) {
        provider->calls[provider->count - 1][4] = bits(first);
        provider->calls[provider->count - 1][5] = bits(second);
    }
    return okay;
}

static bool tree_method(void *context, AbUiFrameNode *node, AbUiFrameMethod method,
                        AbError *error) {
    Context *provider = context;
    for (size_t index = 0; index < 4; ++index) {
        if (provider->navigation.frames[index].tree == node) {
            return menu_method(context, provider->nodes + index,
                               (AbUiMenuMethod)((int)method + (int)AB_UI_MENU_ENTRY), 0,
                               0, error);
        }
    }
    return false;
}

static bool frame_pushed(void *context, AbUiMenuNode *node, AbSpan channel,
                         AbError *error) {
    (void)error;
    Context *provider = context;
    bool okay = record(provider, FRAME_PUSHED, node ? (uint32_t)node->identity : 0);
    provider->calls[provider->count - 1][2] = channel_id(channel);
    return okay;
}

static bool pop_requested(void *context, AbError *error) {
    (void)error;
    return record(context, POP_REQUESTED, 0);
}

static bool requirements(void *context, AbUiMenuNode *menu, AbUiPointerValue *output,
                         AbError *error) {
    (void)error;
    Context *provider = context;
    *output = provider->native_mode ? provider->requirement_value
                                    : (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT,
                                                         .object = menu->identity};
    bool okay = record(provider, REQUIREMENTS, (uint32_t)menu->identity);
    if (provider->native_mode) {
        CHECK(encode_value(*output, provider->calls[provider->count - 1] + 4));
    }
    return okay;
}

static bool load(void *context, AbUiPointerValue value, AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK(provider->native_mode || value.kind == AB_UI_POINTER_OBJECT);
    bool okay =
        record(provider, LOAD, provider->native_mode ? 0 : (uint32_t)value.object);
    if (provider->native_mode) {
        CHECK(encode_value(value, provider->calls[provider->count - 1] + 4));
    }
    return okay;
}

static bool release(void *context, AbUiPointerValue value, AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK(provider->native_mode || value.kind == AB_UI_POINTER_OBJECT);
    bool okay =
        record(provider, RELEASE, provider->native_mode ? 0 : (uint32_t)value.object);
    if (provider->native_mode) {
        CHECK(encode_value(value, provider->calls[provider->count - 1] + 4));
    }
    return okay;
}

static bool rectangle(void *context, const float color[4], AbError *error) {
    (void)error;
    Context *provider = context;
    memcpy(provider->color, color, sizeof(provider->color));
    bool okay = record(provider, RECTANGLE, 0);
    if (provider->native_mode) {
        for (size_t channel = 0; channel < 4; ++channel) {
            provider->calls[provider->count - 1][4 + channel] = bits(color[channel]);
        }
    }
    return okay;
}

static bool notify(void *context, AbUiMenuNode *menu, AbSpan channel, AbError *error) {
    (void)error;
    Context *provider = context;
    bool okay = record(provider, NOTIFY, (uint32_t)menu->identity);
    if (provider->native_mode) {
        uint32_t id = channel_id(channel);
        CHECK(id != UINT32_MAX);
        provider->calls[provider->count - 1][2] = id;
    }
    return okay;
}

static bool clip(void *context, AbVec2 origin, AbVec2 size, AbError *error) {
    (void)error;
    CHECK(origin.x == 0 && origin.y == 0);
    Context *provider = context;
    provider->clip_size = size;
    bool okay = record(context, CLIP, 0);
    if (provider->native_mode) {
        uint32_t *row = provider->calls[provider->count - 1];
        row[4] = bits(origin.x);
        row[5] = bits(origin.y);
        row[6] = bits(size.x);
        row[7] = bits(size.y);
    }
    return okay;
}

static bool background(void *context, const float color[3], AbError *error) {
    (void)error;
    Context *provider = context;
    memcpy(provider->color, color, 3 * sizeof(*color));
    bool okay = record(context, BACKGROUND, 0);
    if (provider->native_mode) {
        for (size_t channel = 0; channel < 3; ++channel) {
            provider->calls[provider->count - 1][4 + channel] = bits(color[channel]);
        }
    }
    return okay;
}

static bool reset_input(void *context, AbUiMenuManager *manager, AbError *error) {
    (void)error;
    Context *provider = context;
    provider->manager = manager;
    provider->navigation.manager = manager;
    return record(context, RESET_INPUT, 0);
}

static bool clicks(void *context, AbError *error) {
    (void)error;
    return record(context, CLICKS, 0);
}

static bool keys(void *context, AbError *error) {
    Context *provider = context;
    if (!record(context, KEYS, 0)) {
        return false;
    }
    if (!provider->extended_keys) {
        return true;
    }
    const AbUiKeyDevice devices[] = {AB_UI_KEY_DEVICE_OTHER, AB_UI_KEY_DEVICE_WINDOWS,
                                     AB_UI_KEY_DEVICE_OSX, AB_UI_KEY_DEVICE_ROKU,
                                     AB_UI_KEY_DEVICE_OTHER};
    AbUiMenuKeyBoundaries boundaries = {
        provider, devices[provider->current_command[12] % 5],
        (provider->current_command[13] & UINT32_C(0x10000)) != 0, key_down, key_event};
    return ab_ui_menu_keys(provider->manager, &provider->key_layout, &boundaries,
                           error);
}

static bool failing_pointer(void *context, AbUiMenuNode *menu, AbSpan event,
                            AbVec2 point, AbUiPointerReply *reply, AbError *error) {
    (void)event;
    (void)point;
    (void)error;
    Context *provider = context;
    reply->values[0] = provider->requirement_value;
    return record(provider, 19, (uint32_t)menu->identity);
}

static bool exercise(Context *context, AbUiMenuManager *manager,
                     AbUiMenuBoundaries *boundaries) {
    AbUiFrameState frames[4] = {0};
    AbUiMenuNode menus[4];
    for (size_t index = 0; index < 4; ++index) {
        menus[index] = (AbUiMenuNode){.frame = frames + index,
                                      .identity = index + 1,
                                      .background_present = true,
                                      .background = {51, 102, 153, 204},
                                      .context = context,
                                      .method = menu_method};
    }
    CHECK(context->count == 1 && context->calls[0][0] == RESET_INPUT);
    context->count = 0;
    boundaries->use_dynamic_assets = true;
    CHECK(ab_ui_menu_set_root(manager, menus, NULL, boundaries, NULL) &&
          context->count == 7 && context->root_at_callback == menus &&
          context->calls[0][0] == AB_UI_MENU_PREPARE_DYNAMIC &&
          context->calls[1][0] == REQUIREMENTS && context->calls[2][0] == LOAD &&
          context->calls[3][0] == AB_UI_MENU_ENTRY &&
          context->calls[4][0] == RECTANGLE &&
          context->calls[5][0] == AB_UI_MENU_LAYOUT && context->calls[6][0] == NOTIFY &&
          context->color[0] == 51.0f / 255 && context->color[3] == 204.0f / 255);
    boundaries->use_dynamic_assets = false;
    context->count = 0;
    CHECK(ab_ui_menu_change_root(manager, menus + 1, NULL, boundaries, NULL) &&
          !context->count);
    AbUiMenuNode *selected;
    bool active;
    CHECK(ab_ui_menu_get_root(manager, NULL, &selected, NULL) &&
          selected == menus + 1 && ab_ui_menu_is_active(manager, NULL, &active, NULL) &&
          active &&
          ab_ui_menu_set_root_visible(manager, NULL, (AbUiPointerValue){0}, NULL) &&
          frames[1].fields.visible.kind == AB_UI_POINTER_BOOLEAN &&
          frames[1].fields.visible.boolean &&
          frames[0].fields.visible.kind == AB_UI_POINTER_NONE);
    AbSpan order[] = {span("root"), span("overlay")};
    CHECK(ab_ui_menu_manager_order(manager, order, 2, NULL) &&
          ab_ui_menu_change_root(manager, menus + 2, order + 1, boundaries, NULL) &&
          ab_ui_menu_update(manager, 0.5f, 2, boundaries, NULL) &&
          context->count == 10 && context->calls[8][0] == AB_UI_MENU_UPDATE &&
          context->calls[8][1] == 2 && context->calls[9][0] == AB_UI_MENU_UPDATE &&
          context->calls[9][1] == 3);
    context->count = 0;
    CHECK(ab_ui_menu_update(manager, 0.5f, 2, boundaries, NULL) &&
          context->count == 5 && context->calls[2][0] == RESET_INPUT &&
          context->calls[3][0] == CLICKS && context->calls[4][0] == KEYS);
    context->count = 0;
    boundaries->screen_size.x = 496;
    CHECK(ab_ui_menu_update(manager, 0.5f, 2, boundaries, NULL) &&
          context->count == 7 && context->calls[0][0] == AB_UI_MENU_LAYOUT &&
          context->calls[1][0] == AB_UI_MENU_UPDATE &&
          context->calls[2][0] == AB_UI_MENU_LAYOUT);
    context->count = 0;
    CHECK(ab_ui_menu_draw(manager, boundaries, NULL) && context->count == 4 &&
          context->calls[0][0] == CLIP && context->calls[1][0] == BACKGROUND &&
          context->calls[2][0] == AB_UI_MENU_DRAW && context->calls[2][1] == 2 &&
          context->calls[3][0] == AB_UI_MENU_DRAW && context->calls[3][1] == 3 &&
          context->clip_size.x == 496 && context->color[0] == 51);
    context->count = 0;
    CHECK(ab_ui_menu_pause(manager, NULL) && context->count == 2 &&
          context->calls[0][0] == AB_UI_MENU_CLEAR_POINTER_FLAGS &&
          ab_ui_menu_resume(manager, NULL) && context->count == 4 &&
          context->calls[2][0] == AB_UI_MENU_LAYOUT);
    context->count = 0;
    context->fail_kind = AB_UI_MENU_ENTRY;
    CHECK(!ab_ui_menu_set_root(manager, menus + 3, NULL, boundaries, NULL) &&
          context->count == 1 && context->root_at_callback == menus + 3 &&
          ab_ui_menu_get_root(manager, NULL, &selected, NULL) && selected == menus + 3);
    context->fail_kind = AB_UI_MENU_EXIT;
    CHECK(!ab_ui_menu_exit_root(manager, NULL, boundaries, NULL) &&
          ab_ui_menu_get_root(manager, NULL, &selected, NULL) && selected == menus + 3);
    CHECK(ab_ui_menu_manager_update_enabled(
              manager, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN}) &&
          ab_ui_menu_update(manager, NAN, NAN, NULL, NULL));
    context->fail_kind = UINT32_MAX;
    CHECK(ab_ui_menu_manager_reset(manager, boundaries, NULL) &&
          ab_ui_menu_is_active(manager, NULL, &active, NULL) && !active &&
          !ab_ui_menu_set_root_visible(manager, NULL, (AbUiPointerValue){0}, NULL));
    CHECK(ab_ui_menu_keys(manager, NULL, NULL, NULL));
    CHECK(ab_ui_menu_set_root(manager, menus, NULL, boundaries, NULL));
    CHECK(ab_ui_menu_manager_delegate_back(
              manager, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN}) &&
          ab_ui_menu_keys(manager, NULL, NULL, NULL));
    context->count = 0;
    context->fail_kind = 19;
    bool accepted = true;
    CHECK(!ab_ui_menu_pointer(manager, span("LPRESS"), (AbVec2){0}, failing_pointer,
                              context, &accepted, NULL) &&
          accepted && context->count == 1);
    context->fail_kind = UINT32_MAX;
    context->requirement_value =
        (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = {NULL, 1}};
    accepted = false;
    CHECK(!ab_ui_menu_pointer(manager, span("LPRESS"), (AbVec2){0}, failing_pointer,
                              context, &accepted, NULL) &&
          !accepted && context->count == 2);
    puts("Menu root lifecycle, queued selection, dynamic asset order, resize, "
         "input suppression, draw order and failure prefixes pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool decode_value(const uint32_t input[2], AbUiPointerValue *output) {
    CHECK(input[0] <= AB_UI_POINTER_FUNCTION);
    *output = (AbUiPointerValue){.kind = (AbUiPointerKind)input[0]};
    switch (output->kind) {
        case AB_UI_POINTER_NONE:
            CHECK(input[1] == 0);
            break;
        case AB_UI_POINTER_BOOLEAN:
            CHECK(input[1] <= 1);
            output->boolean = input[1] != 0;
            break;
        case AB_UI_POINTER_NUMBER:
            output->number_bits = input[1];
            break;
        case AB_UI_POINTER_TEXT:
            CHECK(input[1] < 5);
            output->text = span(texts[input[1]]);
            break;
        case AB_UI_POINTER_OBJECT:
            output->object = input[1];
            break;
        case AB_UI_POINTER_FUNCTION:
            CHECK(input[1] == 0);
            output->function = 0;
            break;
    }
    return true;
}

static bool pointer_method(void *context, AbUiMenuNode *menu, AbSpan event,
                           AbVec2 point, AbUiPointerReply *output, AbError *error) {
    (void)error;
    Context *provider = context;
    const uint32_t *input = provider->current_command;
    CHECK(input[12] < 7 && named(event, pointer_events[input[12]]) &&
          bits(point.x) == input[6] && bits(point.y) == input[7]);
    CHECK(record(provider, 19, (uint32_t)menu->identity));
    uint32_t *row = provider->calls[provider->count - 1];
    row[4] = input[12];
    row[5] = bits(point.x);
    row[6] = bits(point.y);
    uint32_t value[] = {(input[13] % 6 + (uint32_t)menu->identity) % 6, input[14]};
    if (value[0] == 0 || value[0] == 5) {
        value[1] = 0;
    } else if (value[0] == 1) {
        value[1] &= 1;
    } else if (value[0] == 3) {
        value[1] %= 5;
    } else if (value[0] == 4) {
        value[1] %= 32;
    }
    *output = (AbUiPointerReply){0};
    CHECK(decode_value(value, output->values));
    output->values[1] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    output->values[2] = (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 4};
    return true;
}

static bool key_down(void *context, AbUiKeyPhase phase, AbSpan key, bool *output,
                     AbError *error) {
    (void)error;
    Context *provider = context;
    CHECK(phase <= AB_UI_KEY_RELEASE);
    for (size_t index = 0; index < UI_KEY_INPUT_COUNT; ++index) {
        if (named(key, ui_key_names[index])) {
            *output = provider->key_down[phase][index] != 0;
            return true;
        }
    }
    CHECK(false);
}

static bool key_child(void *context, AbSpan phase, AbSpan key, AbUiPointerReply *output,
                      AbError *error) {
    AbUiMenuNode *menu = context;
    return key_event(menu->context, menu, phase, key, output, error);
}

static bool key_event(void *context, AbUiMenuNode *menu, AbSpan phase, AbSpan key,
                      AbUiPointerReply *output, AbError *error) {
    (void)error;
    Context *provider = context;
    const char *phases[] = {"PRESS", "HOLD", "RELEASE"};
    uint32_t event_id = 3, label = UI_KEY_LABEL_COUNT;
    for (uint32_t index = 0; index < 3; ++index) {
        if (named(phase, phases[index])) {
            event_id = index;
        }
    }
    for (uint32_t index = 0; index < UI_KEY_LABEL_COUNT; ++index) {
        if (named(key, ui_key_names[index])) {
            label = index;
        }
    }
    CHECK(event_id < 3 && label < UI_KEY_LABEL_COUNT);
    const uint32_t *input = provider->current_command;
    uint32_t value[] = {(input[14] % 6 + (uint32_t)menu->identity) % 6, input[15]};
    if (value[0] == 0 || value[0] == 5) {
        value[1] = 0;
    } else if (value[0] == 1) {
        value[1] &= 1;
    } else if (value[0] == 3) {
        value[1] %= 5;
    } else if (value[0] == 4) {
        value[1] %= 32;
    }
    CHECK(record(provider, 20, (uint32_t)menu->identity));
    uint32_t *row = provider->calls[provider->count - 1];
    row[4] = event_id;
    row[5] = label;
    row[6] = value[0];
    row[7] = value[1];
    *output = (AbUiPointerReply){0};
    CHECK(decode_value(value, output->values));
    output->values[1] =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    if (provider->extended_frame_keys && menu->identity <= 2) {
        const AbUiFrameNode *tree =
            provider->navigation.frames[menu->identity - 1].tree;
        AbUiKeyChild children[4];
        CHECK(tree->state->child_count <= 4);
        for (size_t index = 0; index < tree->state->child_count; ++index) {
            AbUiMenuNode *child = NULL;
            for (size_t candidate = 0; candidate < 4; ++candidate) {
                if (provider->navigation.frames[candidate].tree ==
                    tree->children[index]) {
                    child = provider->nodes + candidate;
                }
            }
            CHECK(child && child->identity >= 3);
            children[index] = (AbUiKeyChild){
                key_child, child,
                ab_ui_pointer_value_truthy(&child->frame->fields.visible),
                ab_ui_pointer_value_truthy(&child->frame->fields.active)};
        }
        CHECK(ab_ui_frame_key(children, tree->state->child_count, phase, key, output,
                              error) &&
              encode_value(output->values[0], row + 6));
        CHECK(record(provider, 21, (uint32_t)menu->identity));
        uint32_t *reply_row = provider->calls[provider->count - 1];
        for (size_t index = 0; index < 3; ++index) {
            CHECK(encode_value(output->values[index], reply_row + 2 + index * 2));
        }
    }
    return true;
}

static bool compare_callbacks(FILE *file, const Context *context, size_t *total) {
    uint32_t count;
    CHECK(words(file, &count, 1) && count == context->count && count <= 256);
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected[159];
        size_t word_count = context->extended_input        ? 159
                            : context->extended_navigation ? 152
                            : context->extended_links      ? 100
                                                           : 80;
        CHECK(words(file, expected, word_count));
        for (size_t field = 0; field < word_count; ++field) {
            if (context->calls[index][field] != expected[field]) {
                fprintf(stderr,
                        "Menu callback %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        index, field, context->calls[index][field], expected[field]);
                return false;
            }
        }
    }
    *total += count;
    return true;
}

static bool compare_state(FILE *file, const Context *context) {
    uint32_t expected[151], actual[151];
    size_t word_count = context->extended_input        ? 151
                        : context->extended_navigation ? 144
                        : context->extended_links      ? 92
                                                       : 72;
    CHECK(words(file, expected, word_count) && capture_state(context, actual));
    for (size_t field = 0; field < word_count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr, "Menu state field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool run_command(Context *context, AbUiMenuBoundaries *boundaries,
                        const uint32_t input[16], const uint32_t input_order[6],
                        AbSpan working_order[6], uint32_t reply[3]) {
    CHECK(input[0] < (context->extended_keys         ? 27
                      : context->extended_input      ? 26
                      : context->extended_navigation ? 24
                      : context->extended_links      ? 19
                                                     : 16) &&
          input[1] <= 4 && input[2] < 4 && input[3] < 3 && input[10] < 4 &&
          input[11] <= 6);
    CHECK(decode_value(input + 4, &context->requirement_value));
    memcpy(context->current_command, input, sizeof(context->current_command));
    if (context->extended_keys) {
        ui_key_fixture(input, context->key_down);
    }
    context->count = 0;
    context->collect_on_callback = (input[10] & 2) != 0;
    boundaries->screen_size = (AbVec2){number(input[8]), number(input[9])};
    boundaries->use_dynamic_assets = (input[10] & 1) != 0;
    AbUiMenuManager *manager = context->manager;
    AbUiMenuNode *node = input[1] ? context->nodes + input[1] - 1 : NULL;
    AbSpan selected = span(channels[input[2]]);
    const AbSpan *channel = input[3] == 0 ? &selected : NULL;
    bool okay = false;
    reply[0] = 1;
    reply[1] = reply[2] = 0;
    switch (input[0]) {
        case 0:
            okay = ab_ui_menu_manager_reset(manager, boundaries, NULL);
            break;
        case 1:
            okay = ab_ui_menu_set_root(manager, node, channel, boundaries, NULL);
            break;
        case 2:
            okay = ab_ui_menu_exit_root(manager, channel, boundaries, NULL);
            break;
        case 3:
            okay = ab_ui_menu_change_root(manager, node, channel, boundaries, NULL);
            break;
        case 4:
            okay = ab_ui_menu_get_root(manager, channel, &node, NULL);
            if (okay && node) {
                reply[1] = 1;
                reply[2] = (uint32_t)node->identity;
            }
            break;
        case 5: {
            bool active;
            okay = ab_ui_menu_is_active(manager, channel, &active, NULL);
            if (okay) {
                reply[1] = 2;
                reply[2] = active;
            }
            break;
        }
        case 6:
            okay = ab_ui_menu_set_root_visible(manager, channel,
                                               context->requirement_value, NULL);
            break;
        case 7:
            okay = ab_ui_menu_update(manager, number(input[6]), number(input[7]),
                                     boundaries, NULL);
            break;
        case 8:
            okay = ab_ui_menu_draw(manager, boundaries, NULL);
            break;
        case 9:
            okay = ab_ui_menu_resume(manager, NULL);
            break;
        case 10:
            okay = ab_ui_menu_pause(manager, NULL);
            break;
        case 11:
            okay = ab_ui_menu_manager_allow_input(manager, context->requirement_value);
            break;
        case 12:
            okay =
                ab_ui_menu_manager_update_enabled(manager, context->requirement_value);
            break;
        case 13:
            for (size_t index = 0; index < input[11]; ++index) {
                CHECK(input_order[index] < 4);
                working_order[index] = span(channels[input_order[index]]);
            }
            okay = ab_ui_menu_manager_order(manager, working_order, input[11], NULL);
            break;
        case 14:
            CHECK(node);
            node->frame->fields.visible = context->requirement_value;
            node->background_present = (input[2] & 1) != 0;
            for (size_t index = 0; index < 4; ++index) {
                node->background[index] = number(input[12 + index]);
            }
            okay = true;
            break;
        case 15:
            okay = ab_ui_menu_change_root(manager, NULL, NULL, boundaries, NULL);
            break;
        case 16:
            CHECK(input_order[0] < 4);
            okay = ab_ui_menu_add_link(manager, span(channels[input_order[0]]), node,
                                       NULL);
            break;
        case 17:
            CHECK(input_order[0] < 4);
            okay = ab_ui_menu_get_link(manager, span(channels[input_order[0]]), &node,
                                       NULL);
            if (okay && node) {
                reply[1] = 1;
                reply[2] = (uint32_t)node->identity;
            }
            break;
        case 18:
            CHECK(input_order[0] < 4);
            okay = ab_ui_menu_handle_result(manager, span(channels[input_order[0]]),
                                            channel, boundaries, NULL);
            break;
        case 19: {
            CHECK(input[11] <= AB_UI_MENU_EVENT_POP_FRAME && input[12] <= 2 &&
                  input_order[0] < 4);
            AbUiMenuTarget target = {.kind = (AbUiMenuTargetKind)input[12],
                                     .frame = node,
                                     .link = span(channels[input_order[0]])};
            okay = ab_ui_menu_event(&context->navigation, (AbUiMenuEventKind)input[11],
                                    target, channel, boundaries, NULL);
            break;
        }
        case 20: {
            AbUiFrameNode *popped;
            okay = ab_ui_menu_pop_frame(&context->navigation, channel, &popped, NULL);
            if (okay) {
                for (size_t index = 0; index < 4; ++index) {
                    if (context->navigation.frames[index].tree == popped) {
                        reply[1] = 1;
                        reply[2] = (uint32_t)index + 1;
                    }
                }
                CHECK(reply[1] == 1);
            }
            break;
        }
        case 21:
            CHECK(node);
            okay = ab_ui_menu_frame_remove(
                &context->navigation, context->navigation.frames + input[1] - 1, NULL);
            break;
        case 22:
            okay =
                ab_ui_menu_manager_delegate_back(manager, context->requirement_value);
            break;
        case 23:
            CHECK(node);
            context->navigation.frames[input[1] - 1].tree->entered =
                context->requirement_value;
            okay = true;
            break;
        case 24:
            for (size_t index = 0; index < input[11]; ++index) {
                CHECK(input_order[index] < 4);
                context->working_input_order[index] =
                    span(channels[input_order[index]]);
            }
            okay = ab_ui_menu_manager_input_order(manager, context->working_input_order,
                                                  input[11], NULL);
            break;
        case 25: {
            CHECK(input[12] < 7);
            bool accepted;
            okay = ab_ui_menu_pointer(manager, span(pointer_events[input[12]]),
                                      (AbVec2){number(input[6]), number(input[7])},
                                      pointer_method, context, &accepted, NULL);
            if (okay) {
                reply[1] = 2;
                reply[2] = accepted;
            }
            break;
        }
        case 26:
            okay = keys(context, NULL);
            break;
    }
    reply[0] = okay;
    return true;
}

static bool compare_instance(FILE *file, Context *context,
                             AbUiMenuBoundaries *boundaries, size_t instance,
                             size_t *callbacks, size_t *errors) {
    CHECK(compare_callbacks(file, context, callbacks) && compare_state(file, context));
    AbSpan *working_order = context->working_order;
    for (size_t step = 0; step < 128; ++step) {
        uint32_t input[16], input_order[6], reply[3], expected_reply[3];
        CHECK(words(file, input, 16) && words(file, input_order, 6) &&
              run_command(context, boundaries, input, input_order, working_order,
                          reply) &&
              words(file, expected_reply, 3));
        if (memcmp(reply, expected_reply, sizeof(reply)) != 0 ||
            !compare_callbacks(file, context, callbacks) ||
            !compare_state(file, context)) {
            fprintf(stderr,
                    "Menu manager instance %zu step %zu operation %u differs; "
                    "reply %u/%u/%u expected %u/%u/%u\n",
                    instance, step, input[0], reply[0], reply[1], reply[2],
                    expected_reply[0], expected_reply[1], expected_reply[2]);
            return false;
        }
        *errors += !reply[0];
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    bool extended_links = strcmp(header, "AB_ANDROID_UI_MENU_MANAGER_V3 32 128\n") == 0;
    bool extended_navigation =
        strcmp(header, "AB_ANDROID_UI_MENU_MANAGER_V4 32 128\n") == 0;
    bool extended_input = strcmp(header, "AB_ANDROID_UI_MENU_MANAGER_V5 32 128\n") == 0;
    bool extended_keys = strcmp(header, "AB_ANDROID_UI_MENU_MANAGER_V6 32 128\n") == 0;
    bool extended_frame_keys =
        strcmp(header, "AB_ANDROID_UI_MENU_MANAGER_V7 32 128\n") == 0;
    extended_keys |= extended_frame_keys;
    extended_input |= extended_keys;
    extended_navigation |= extended_input;
    extended_links |= extended_navigation;
    CHECK(extended_links ||
          strcmp(header, "AB_ANDROID_UI_MENU_MANAGER_V1 32 128\n") == 0 ||
          strcmp(header, "AB_ANDROID_UI_MENU_MANAGER_V2 32 128\n") == 0);
    size_t callbacks = 0, errors = 0;
    for (size_t instance = 0; instance < 32; ++instance) {
        uint32_t id;
        CHECK(words(file, &id, 1) && id == instance);
        Context context = {.fail_kind = UINT32_MAX,
                           .native_mode = true,
                           .extended_links = extended_links,
                           .extended_navigation = extended_navigation,
                           .extended_input = extended_input,
                           .extended_keys = extended_keys,
                           .extended_frame_keys = extended_frame_keys};
        CHECK(ab_ui_menu_key_layout(&context.key_layout, NULL));
        AbUiFrameState frames[4] = {0};
        AbUiMenuNode nodes[4];
        AbUiFrameNode trees[4] = {0};
        AbUiFrameNode *children[4][4] = {0};
        AbUiMenuFrame bindings[4] = {0};
        for (size_t index = 0; index < 4; ++index) {
            nodes[index] = (AbUiMenuNode){.frame = frames + index,
                                          .identity = index + 1,
                                          .background_present = true,
                                          .background = {51, 102, 153, 204},
                                          .context = &context,
                                          .method = menu_method};
            frames[index].fields.name = (AbUiPointerValue){
                .kind = AB_UI_POINTER_TEXT, .text = span(frame_names[index])};
            if (extended_frame_keys) {
                frames[index].fields.visible = frames[index].fields.active =
                    (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
            }
            trees[index] = (AbUiFrameNode){.state = frames + index,
                                           .children = children[index],
                                           .capacity = 4,
                                           .context = &context,
                                           .method = tree_method};
            bindings[index] = (AbUiMenuFrame){nodes + index, trees + index, false};
        }
        context.nodes = nodes;
        context.navigation = (AbUiMenuNavigation){.frames = bindings,
                                                  .frame_count = 4,
                                                  .context = &context,
                                                  .frame_pushed = frame_pushed,
                                                  .pop_requested = pop_requested};
        AbUiMenuBoundaries boundaries = {
            &context, {480, 320}, false,      requirements, load,   release, rectangle,
            notify,   clip,       background, reset_input,  clicks, keys};
        AbUiMenuManager *manager = NULL;
        CHECK(ab_ui_menu_manager_open(16, 65536, &boundaries, &manager, NULL));
        bool okay = compare_instance(file, &context, &boundaries, instance, &callbacks,
                                     &errors);
        ab_ui_menu_manager_close(manager);
        if (!okay) {
            return false;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI menu manager reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "All 4096 original menu commands, 32 constructors, %zu full callback "
        "snapshots, %zu rejected operations and final dictionary/flag states match.\n",
        callbacks, errors);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        if (!file) {
            return 1;
        }
        bool okay = compare_native(file);
        return fclose(file) == 0 && okay ? 0 : 1;
    }
    if (argc != 1) {
        return 2;
    }
    Context context = {.fail_kind = UINT32_MAX};
    AbUiMenuBoundaries boundaries = {
        &context, {480, 320}, false,      requirements, load,   release, rectangle,
        notify,   clip,       background, reset_input,  clicks, keys};
    AbUiMenuManager *manager = NULL;
    if (!ab_ui_menu_manager_open(16, 65536, &boundaries, &manager, NULL)) {
        return 1;
    }
    bool okay = exercise(&context, manager, &boundaries);
    ab_ui_menu_manager_close(manager);
    return okay ? 0 : 1;
}
