#ifndef AB_ORACLE_UI_TEXT_LINES_H
#define AB_ORACLE_UI_TEXT_LINES_H

#include "selected_function.h"
#include "ui_text_lines_cases.h"
#include "ui_text_wrap_cases.h"
#include "ui_text_wrap_context.h"

typedef struct {
    uint32_t kind, value;
    unsigned lengths[2];
    char bytes[2][128];
} UiTextLineEvent;

typedef struct {
    uint32_t length, capacity;
    int32_t references;
    char bytes[128];
} UiTextLineString;

static UiTextWrapContext ui_text_lines_context;
static UiTextLineEvent ui_text_lines_events[256];
static size_t ui_text_lines_event_count;

static UiTextLineEvent *ui_text_lines_event(uint32_t kind) {
    if (ui_text_lines_event_count == 256) {
        fail("Original text line event capacity exceeded\n");
    }
    UiTextLineEvent *event = ui_text_lines_events + ui_text_lines_event_count++;
    event->kind = kind;
    event->value = event->lengths[0] = event->lengths[1] = 0;
    return event;
}

static const char *ui_text_lines_string(void *lua, int index, unsigned *size) {
    if (damage_lua.type(lua, index) != 4) {
        fail("Unexpected original text line argument type\n");
    }
    const char *bytes = damage_lua.to_string(lua, index, size);
    if (*size >= 128 || !bytes) {
        fail("Original text line string exceeds fixture bounds\n");
    }
    return bytes;
}

static void ui_text_lines_argument(UiTextLineEvent *event, size_t argument, void *lua,
                                   int index) {
    const char *bytes = ui_text_lines_string(lua, index, event->lengths + argument);
    for (size_t offset = 0; offset < event->lengths[argument]; ++offset) {
        event->bytes[argument][offset] = bytes[offset];
    }
}

static void ui_text_lines_native_string(UiTextLineString *output, const char *bytes,
                                        unsigned size) {
    if (size >= sizeof(output->bytes)) {
        fail("Original native text fixture exceeds storage\n");
    }
    output->length = size;
    output->capacity = sizeof(output->bytes) - 1;
    output->references = -1;
    for (size_t offset = 0; offset < size; ++offset) {
        output->bytes[offset] = bytes[offset];
    }
    output->bytes[size] = 0;
}

static int ui_text_lines_font(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "FONT_BASIC")) {
        fail("Unexpected original text line font selection\n");
    }
    ui_text_lines_argument(ui_text_lines_event(1), 0, lua, 1);
    return 0;
}

static int ui_text_lines_wrap(void *lua) {
    if (damage_lua.get_top(lua) != 3 || damage_lua.type(lua, 3) != 3) {
        fail("Unexpected original text line wrapping arguments\n");
    }
    UiTextLineEvent *event = ui_text_lines_event(2);
    ui_text_lines_argument(event, 0, lua, 1);
    ui_text_lines_argument(event, 1, lua, 2);
    FloatBits width = {.number = damage_lua.to_number(lua, 3)};
    event->value = width.bits;
    UiTextLineString group, text;
    ui_text_lines_native_string(&group, event->bytes[0], event->lengths[0]);
    ui_text_lines_native_string(&text, event->bytes[1], event->lengths[1]);
    const char *group_value = group.bytes, *text_value = text.bytes;
    UiTextWrapContext *context = &ui_text_lines_context;
    context->wrap(context->game, &group_value, &text_value, width.number);
    if (damage_lua.get_top(lua) != 3) {
        fail("Original clipText did not preserve callback stack\n");
    }
    context->get_table(context->game + 0x1840 / 4);
    damage_lua.get_field(lua, -10002, "gamelua");
    context->push_value(lua, 4);
    damage_lua.set_field(lua, 5, "clippedText");
    return 0;
}

static int ui_text_lines_leading(void *lua) {
    if (damage_lua.get_top(lua)) {
        fail("Unexpected original font leading arguments\n");
    }
    FloatBits value = {.number = (float)ui_text_lines_context.leading(
                           ui_text_lines_context.resources)};
    ui_text_lines_event(3)->value = value.bits;
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int ui_text_lines_name(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original text line default name arguments\n");
    }
    UiTextLineEvent *event = ui_text_lines_event(4);
    damage_lua.push_string(lua, "generated-name");
    ui_text_lines_argument(event, 0, lua, 2);
    return 1;
}

static int ui_text_lines_localize(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "TEXTS_BASIC")) {
        fail("Unexpected original line constructor localization\n");
    }
    UiTextLineEvent *event = ui_text_lines_event(5);
    ui_text_lines_argument(event, 0, lua, 1);
    ui_text_lines_argument(event, 1, lua, 2);
    /* The prepared native resource group map is empty. Its missing-group
     * result is the exact requested identifier; no localization is invented. */
    ui_text_lines_context.push_value(lua, 2);
    return 1;
}

static int ui_text_lines_measure(void *lua) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Unexpected original line constructor measurement\n");
    }
    UiTextLineEvent *event = ui_text_lines_event(6);
    ui_text_lines_argument(event, 0, lua, 1);
    UiTextLineString text;
    ui_text_lines_native_string(&text, event->bytes[0], event->lengths[0]);
    const char *value = text.bytes;
    FloatBits width = {.number = (float)ui_text_lines_context.width(
                           ui_text_lines_context.resources, &value, 0, INT32_MAX)};
    event->value = width.bits;
    damage_lua.push_number(lua, width.number);
    return 1;
}

static void ui_text_lines_prepare(void *lua, const uint32_t input[19]) {
    damage_lua.create_table(lua, 0, 20);
    const char *fields[] = {"scaleX", "scaleY", "textBoxSize", "fontLeadingScale"};
    const size_t offsets[] = {9, 10, 5, 6};
    for (size_t field = 0; field < 4; ++field) {
        damage_number(lua, 1, fields[field], input[offsets[field]]);
    }
    damage_lua.push_string(lua, "FONT_BASIC");
    damage_lua.set_field(lua, 1, "font");
    damage_lua.push_string(lua, ui_text_lines_groups[input[18]]);
    damage_lua.set_field(lua, 1, "group");
    const UiTextWrapCase *source = ui_text_wrap_cases + input[0];
    ui_text_lines_context.push_bytes(lua, source->bytes, (unsigned)source->size);
    damage_lua.set_field(lua, 1, "text");
    damage_lua.push_string(lua, ui_text_lines_vertical[input[7]]);
    damage_lua.set_field(lua, 1, "vanchor");
    damage_lua.push_string(lua, ui_text_lines_horizontal[input[8]]);
    damage_lua.set_field(lua, 1, "hanchor");
    int table = 1;
    for (uint32_t parent = 0; parent < input[1]; ++parent) {
        damage_lua.create_table(lua, 0, 3);
        damage_number(lua, table + 1, "scaleX", input[11 + parent * 2]);
        damage_number(lua, table + 1, "scaleY", input[12 + parent * 2]);
        ui_text_lines_context.push_value(lua, table + 1);
        damage_lua.set_field(lua, table, "_parent");
        ++table;
    }
    damage_lua.set_top(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_text_lines_root");
}

static void ui_text_lines_emit_line(void *lua, int table) {
    static const char *const numeric[] = {
        "x", "y", "scaleX", "scaleY",      "angle",
        "w", "h", "width",  "textBoxSize", "fontLeadingScale"};
    static const char *const boolean[] = {"visible", "active", "floorCoordinates"};
    static const char *const strings[] = {"font", "group", "hanchor", "vanchor",
                                          "text"};
    int top = damage_lua.get_top(lua);
    uint32_t output[18];
    for (size_t field = 0; field < 10; ++field) {
        damage_lua.get_field(lua, table, numeric[field]);
        if (damage_lua.type(lua, -1) != 3) {
            fail("Original created line lacks numeric field\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[field] = value.bits;
        damage_lua.set_top(lua, top);
    }
    for (size_t field = 0; field < 3; ++field) {
        damage_lua.get_field(lua, table, boolean[field]);
        if (damage_lua.type(lua, -1) != 1) {
            fail("Original created line lacks Boolean field\n");
        }
        output[10 + field] = (uint32_t)damage_lua.to_boolean(lua, -1);
        damage_lua.set_top(lua, top);
    }
    damage_lua.get_field(lua, table, "height");
    output[13] = (uint32_t)damage_lua.type(lua, -1);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, table, "getFontFunction");
    output[14] = (uint32_t)damage_lua.type(lua, -1);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, table, "children");
    output[15] = ui_text_lines_context.length(lua, -1);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, table, "_lookup");
    output[16] = ui_text_lines_context.length(lua, -1);
    damage_lua.set_top(lua, top);
    damage_lua.get_field(lua, table, "name");
    output[17] = (uint32_t)damage_named(lua, -1, "generated-name");
    damage_lua.set_top(lua, top);
    print_words(output, 18);
    for (size_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, table, strings[field]);
        unsigned size;
        const char *bytes = ui_text_lines_string(lua, -1, &size);
        ui_text_wrap_emit_bytes(bytes, size);
        damage_lua.set_top(lua, top);
    }
}

static void ui_text_lines_emit_events(void) {
    for (size_t index = 0; index < ui_text_lines_event_count; ++index) {
        const UiTextLineEvent *event = ui_text_lines_events + index;
        uint32_t metadata[] = {event->kind, event->value};
        print_words(metadata, 2);
        for (size_t argument = 0; argument < 2; ++argument) {
            ui_text_wrap_emit_bytes(event->bytes[argument], event->lengths[argument]);
        }
    }
}

static void ui_text_lines_run(void *lua, const uint32_t input[19]) {
    ui_text_lines_prepare(lua, input);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_number(lua, 1, "screenWidth", input[17]);
    damage_lua.set_top(lua, 0);
    ui_text_lines_event_count = 0;
    damage_lua.get_field(lua, -10002, "text-clip");
    damage_lua.get_field(lua, -10002, "_ab_text_lines_root");
    if (input[2] < 3) {
        if (!input[2]) {
            damage_lua.push_nil(lua);
        } else {
            damage_lua.push_boolean(lua, input[2] == 2);
        }
    } else if (input[2] < 5) {
        damage_lua.push_number(lua, (float)(input[2] - 3));
    } else {
        damage_lua.push_string(lua, input[2] == 5 ? "" : "truthy");
    }
    if (input[3] == 2) {
        FloatBits spacing = {.bits = input[4]};
        damage_lua.push_number(lua, spacing.number);
    } else if (input[3] == 1) {
        damage_lua.push_boolean(lua, 0);
    } else {
        damage_lua.push_nil(lua);
    }
    if (damage_lua.call(lua, 3, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original Text.clip construction failed\n");
    }
    damage_lua.get_field(lua, -10002, "_ab_text_lines_root");
    damage_lua.get_field(lua, 1, "textBlockHeight");
    FloatBits height = {.number = damage_lua.to_number(lua, 2)};
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "widestLine");
    FloatBits widest = {.number = damage_lua.to_number(lua, 2)};
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "clipped");
    uint32_t clipped = (uint32_t)damage_lua.to_boolean(lua, 2);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "lines");
    uint32_t count = ui_text_lines_context.length(lua, 2);
    if (count > 128) {
        fail("Original Text.clip produced too many lines\n");
    }
    uint32_t output[] = {height.bits, widest.bits, count, clipped,
                         (uint32_t)ui_text_lines_event_count};
    print_words(output, 5);
    for (uint32_t line = 0; line < count; ++line) {
        ui_text_lines_context.raw_get(lua, 2, (int)line + 1);
        ui_text_lines_emit_line(lua, 3);
        damage_lua.set_top(lua, 2);
    }
    damage_lua.set_top(lua, 0);
    ui_text_lines_emit_events();
}

static void ui_text_lines_initialize(void) {
    ui_text_wrap_context_init(&ui_text_lines_context);
    void *lua = ui_text_lines_context.lua;
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "tostring");
    damage_lua.set_field(lua, -10002, "_ab_text_builtin_tostring");
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"class-new", "frame-init", "text-init", "frame-scale",
                           "text-clip"};
    const char *paths[] = {"/data/local/tmp/ab520/ui/class-new-oracle.lua",
                           "/data/local/tmp/ab520/ui/frame-init-oracle.lua",
                           "/data/local/tmp/ab520/ui/text-init-oracle.lua",
                           "/data/local/tmp/ab520/ui/frame-scale-oracle.lua",
                           "/data/local/tmp/ab520/ui/text-clip-oracle.lua"};
    for (size_t index = 0; index < 5; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_function(lua, -10002, "tostring", ui_text_lines_name);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "frame-init");
    damage_lua.set_field(lua, 1, "init");
    damage_lua.get_field(lua, -10002, "frame-scale");
    damage_lua.set_field(lua, 1, "getCumulativeScale");
    ui_text_lines_context.push_value(lua, 1);
    damage_lua.set_field(lua, -10002, "Frame");
    damage_lua.create_table(lua, 0, 1);
    ui_text_lines_context.push_value(lua, 1);
    damage_lua.set_field(lua, 2, "Frame");
    damage_lua.set_field(lua, -10002, "ui");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "class-new");
    damage_lua.set_field(lua, 1, "new");
    damage_lua.get_field(lua, -10002, "text-init");
    damage_lua.set_field(lua, 1, "init");
    damage_lua.set_field(lua, -10002, "Text");
    damage_lua.create_table(lua, 0, 4);
    damage_lua.push_string(lua, "FONT_BASIC");
    damage_lua.set_field(lua, 1, "fontBasic");
    damage_function(lua, 1, "setFont", ui_text_lines_font);
    damage_function(lua, 1, "clipText", ui_text_lines_wrap);
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "getFontLeading", ui_text_lines_leading);
    damage_function(lua, 1, "getString", ui_text_lines_localize);
    damage_function(lua, 1, "getStringWidth", ui_text_lines_measure);
    damage_lua.set_field(lua, -10002, "res");
}

static void probe_ui_text_lines(void) {
    ui_text_lines_initialize();
    void *lua = ui_text_lines_context.lua;
    static const float scales[] = {0.25f, -0.25f, 0.5f, -0.5f, 1, -1, 2, -2};
    static const float spacing[] = {-2, -1, -0.5f, -0.0f, 0, 0.5f, 1, 2};
    static const float widths[] = {-1, 0, 16, 20, 33, 64, 128, 1000000};
    uint32_t random = 0x52011e71;
    semihost(4, "AB_ANDROID_UI_TEXT_LINES_V1 4096\n");
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t input[19] = {(uint32_t)(test % 36), (uint32_t)(test % 4),
                              (uint32_t)(test % 7), (uint32_t)(test % 3)};
        FloatBits factor = {.number = spacing[random_word(&random) % 8]};
        input[4] = factor.bits;
        factor.number = spacing[random_word(&random) % 8];
        input[6] = factor.bits;
        input[7] = random_word(&random) % 5;
        input[8] = random_word(&random) % 5;
        float scale_x = 1;
        for (size_t axis = 0; axis < 8; ++axis) {
            FloatBits scale = {.number = scales[random_word(&random) % 8]};
            input[9 + axis] = scale.bits;
            if (!(axis & 1) && axis <= input[1] * 2) {
                scale_x = scale_x * scale.number;
            }
        }
        if (input[2] >= 2) {
            FloatBits own = {.bits = input[9]};
            scale_x = own.number;
        }
        FloatBits box = {.number = widths[random_word(&random) % 8] * scale_x};
        input[5] = box.bits;
        FloatBits screen = {.number = test & 1 ? 480.0f : 320.0f};
        input[17] = screen.bits;
        input[18] = random_word(&random) % 3;
        print_words(input, 19);
        ui_text_lines_run(lua, input);
    }
    semihost(4, "AB UI text lines reference complete\n");
}

#endif
