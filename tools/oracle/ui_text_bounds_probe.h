#ifndef AB_ORACLE_UI_TEXT_BOUNDS_H
#define AB_ORACLE_UI_TEXT_BOUNDS_H

static uint32_t ui_text_bounds_input[24], ui_text_bounds_events[8][8];
static size_t ui_text_bounds_event_count, ui_text_bounds_leading_count;

static uint32_t *ui_text_bounds_event(uint32_t kind) {
    if (ui_text_bounds_event_count == 8) {
        fail("Original Text bounds event capacity exceeded\n");
    }
    uint32_t *event = ui_text_bounds_events[ui_text_bounds_event_count++];
    for (size_t index = 0; index < 8; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    return event;
}

static int ui_text_bounds_never_called(void *lua) {
    (void)lua;
    fail("Original Text bounds invoked an unused font/tostring boundary\n");
}

static void ui_text_bounds_source(void *lua, int index) {
    unsigned size;
    const char *bytes = ui_text_lines_string(lua, index, &size);
    const UiTextWrapCase *source = ui_text_wrap_cases + ui_text_bounds_input[0];
    if (size != source->size) {
        fail("Original bounds changed raw text size\n");
    }
    for (size_t offset = 0; offset < size; ++offset) {
        if (bytes[offset] != source->bytes[offset]) {
            fail("Original bounds changed raw text bytes\n");
        }
    }
}

static int ui_text_bounds_localize(void *lua) {
    if (damage_lua.get_top(lua) != 2 ||
        !damage_named(lua, 1, ui_text_lines_groups[ui_text_bounds_input[1]])) {
        fail("Unexpected original bounds localization arguments\n");
    }
    ui_text_bounds_source(lua, 2);
    uint32_t *event = ui_text_bounds_event(1);
    event[1] = ui_text_bounds_input[1];
    event[2] = ui_text_bounds_input[0];
    ui_text_lines_context.push_value(lua, 2);
    return 1;
}

static int ui_text_bounds_measure(void *lua) {
    if (damage_lua.get_top(lua) != 1) {
        fail("Unexpected original bounds measurement arguments\n");
    }
    ui_text_bounds_source(lua, 1);
    const UiTextWrapCase *source = ui_text_wrap_cases + ui_text_bounds_input[0];
    UiTextLineString text;
    ui_text_lines_native_string(&text, source->bytes, (unsigned)source->size);
    const char *bytes = text.bytes;
    FloatBits width = {.number = (float)ui_text_lines_context.width(
                           ui_text_lines_context.resources, &bytes, 0, INT32_MAX)};
    uint32_t *event = ui_text_bounds_event(2);
    event[1] = ui_text_bounds_input[0];
    event[2] = width.bits;
    damage_lua.push_number(lua, width.number);
    return 1;
}

static int ui_text_bounds_leading(void *lua) {
    if (damage_lua.get_top(lua)) {
        fail("Unexpected original bounds leading arguments\n");
    }
    FloatBits value = {.number = (float)ui_text_lines_context.leading(
                           ui_text_lines_context.resources)};
    ui_text_bounds_event(3)[1] = value.bits;
    ++ui_text_bounds_leading_count;
    ui_text_lines_context.font[13] = ui_text_bounds_input[12];
    damage_lua.push_number(lua, value.number);
    return 1;
}

static void ui_text_bounds_reply(void *lua, uint32_t kind, uint32_t payload) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind == 1) {
        damage_lua.push_boolean(lua, (int)payload);
    } else if (kind == 2) {
        FloatBits value = {.bits = payload};
        damage_lua.push_number(lua, value.number);
    } else if (kind == 3) {
        damage_lua.push_string(lua, payload ? "truthy" : "");
    } else if (kind == 4) {
        damage_lua.create_table(lua, 0, 0);
    } else {
        damage_lua.push_closure(lua, ui_text_bounds_never_called, 0);
    }
}

static int ui_text_bounds_child(void *lua) {
    if (damage_lua.get_top(lua) != 3 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 3 || damage_lua.type(lua, 3) != 3) {
        fail("Unexpected original bounds child query\n");
    }
    damage_lua.get_field(lua, 1, "_ab_child");
    uint32_t index = (uint32_t)damage_lua.to_number(lua, 4);
    damage_lua.set_top(lua, 3);
    if (index >= ui_text_bounds_input[3] || index >= 5) {
        fail("Original bounds child index exceeds prepared array\n");
    }
    uint32_t *event = ui_text_bounds_event(4);
    event[1] = index;
    FloatBits x = {.number = damage_lua.to_number(lua, 2)};
    FloatBits y = {.number = damage_lua.to_number(lua, 3)};
    event[2] = x.bits;
    event[3] = y.bits;
    damage_lua.get_field(lua, -10002, "_ab_bounds_root");
    damage_lua.get_field(lua, 4, "width");
    FloatBits width = {.number = damage_lua.to_number(lua, 5)};
    event[4] = width.bits;
    event[5] = ui_text_bounds_input[14 + index * 2];
    event[6] = ui_text_bounds_input[15 + index * 2];
    damage_lua.set_top(lua, 3);
    ui_text_bounds_reply(lua, event[5], event[6]);
    return 1;
}

static void ui_text_bounds_prepare(void *lua) {
    const uint32_t *input = ui_text_bounds_input;
    damage_lua.create_table(lua, 0, 15);
    damage_number(lua, 1, "x", input[7]);
    damage_number(lua, 1, "y", input[8]);
    damage_number(lua, 1, "width", input[6]);
    damage_lua.push_string(lua, ui_text_lines_horizontal[input[4]]);
    damage_lua.set_field(lua, 1, "hanchor");
    damage_lua.push_string(lua, ui_text_lines_vertical[input[5]]);
    damage_lua.set_field(lua, 1, "vanchor");
    damage_lua.push_string(lua, ui_text_lines_groups[input[1]]);
    damage_lua.set_field(lua, 1, "group");
    const UiTextWrapCase *source = ui_text_wrap_cases + input[0];
    ui_text_lines_context.push_bytes(lua, source->bytes, (unsigned)source->size);
    damage_lua.set_field(lua, 1, "text");
    ui_text_bounds_reply(lua, input[2] < 3 ? (uint32_t)(input[2] != 0) : input[2] - 1,
                         input[2] == 2);
    damage_lua.set_field(lua, 1, "clipped");
    damage_lua.get_field(lua, -10002, "text-correction");
    damage_lua.set_field(lua, 1, "getCorrectionParams");
    damage_function(lua, 1, "getFontFunction", ui_text_bounds_never_called);
    damage_number(lua, 1, "scaleX", 0x7fc12345);
    damage_number(lua, 1, "scaleY", 0x7fc12345);
    damage_number(lua, 1, "fontLeadingScale", 0x7fc12345);
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 1, "visible");
    damage_lua.create_table(lua, (int)input[3], 0);
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    for (uint32_t index = 0; index < input[3]; ++index) {
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_number(lua, (float)index);
        damage_lua.set_field(lua, 3, "_ab_child");
        damage_function(lua, 3, "checkBounds", ui_text_bounds_child);
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 3, "visible");
        raw_set(lua, 2, (int)index + 1);
    }
    damage_lua.set_field(lua, 1, "lines");
    damage_lua.set_field(lua, -10002, "_ab_bounds_root");
}

static void ui_text_bounds_emit_events(void) {
    for (size_t index = 0; index < ui_text_bounds_event_count; ++index) {
        print_words(ui_text_bounds_events[index], 8);
    }
}

static void ui_text_bounds_run(void *lua) {
    ui_text_bounds_prepare(lua);
    ui_text_lines_context.font[13] = ui_text_bounds_input[11];
    ui_text_lines_context.font[10] = ui_text_bounds_input[13];
    ui_text_bounds_event_count = ui_text_bounds_leading_count = 0;
    damage_lua.get_field(lua, -10002, "text-correction");
    damage_lua.get_field(lua, -10002, "_ab_bounds_root");
    if (damage_lua.call(lua, 1, 2, 0) || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Original text correction query failed\n");
    }
    FloatBits x = {.number = damage_lua.to_number(lua, 1)};
    FloatBits y = {.number = damage_lua.to_number(lua, 2)};
    uint32_t correction[] = {x.bits, y.bits, (uint32_t)ui_text_bounds_event_count};
    print_words(correction, 3);
    ui_text_bounds_emit_events();
    damage_lua.set_top(lua, 0);
    ui_text_lines_context.font[13] = ui_text_bounds_input[11];
    ui_text_bounds_event_count = ui_text_bounds_leading_count = 0;
    damage_lua.get_field(lua, -10002, "text-bounds");
    damage_lua.get_field(lua, -10002, "_ab_bounds_root");
    x.bits = ui_text_bounds_input[9];
    y.bits = ui_text_bounds_input[10];
    damage_lua.push_number(lua, x.number);
    damage_lua.push_number(lua, y.number);
    if (damage_lua.call(lua, 3, 1, 0) || damage_lua.type(lua, 1) > 1) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original text bounds query failed\n");
    }
    uint32_t kind = (uint32_t)damage_lua.type(lua, 1);
    uint32_t value = kind ? (uint32_t)damage_lua.to_boolean(lua, 1) : 0;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_bounds_root");
    damage_lua.get_field(lua, 1, "width");
    FloatBits width = {.number = damage_lua.to_number(lua, 2)};
    uint32_t output[] = {kind, value, width.bits, (uint32_t)ui_text_bounds_event_count};
    print_words(output, 4);
    ui_text_bounds_emit_events();
    damage_lua.set_top(lua, 0);
}

static void ui_text_bounds_fixture_point(size_t test) {
    uint32_t *input = ui_text_bounds_input;
    ui_text_lines_context.font[13] = input[11];
    ui_text_lines_context.font[10] = input[13];
    const UiTextWrapCase *source = ui_text_wrap_cases + input[0];
    UiTextLineString text;
    ui_text_lines_native_string(&text, source->bytes, (unsigned)source->size);
    const char *bytes = text.bytes;
    float measured = (float)ui_text_lines_context.width(ui_text_lines_context.resources,
                                                        &bytes, 0, INT32_MAX);
    FloatBits old_width = {.bits = input[6]}, x = {.bits = input[7]},
              y = {.bits = input[8]};
    float width = old_width.number < measured ? measured : old_width.number;
    float height = (float)(int32_t)input[11];
    float correction_leading = (float)(int32_t)input[12];
    float dx = input[4] == 1 ? -width / 2.0f : input[4] == 2 ? -width : 0;
    float dy = input[5] == 1   ? -correction_leading / 2.0f
               : input[5] == 2 ? -correction_leading
                               : 0;
    float left = x.number + dx, right = left + width;
    float top = y.number + dy, bottom = top + height;
    size_t mode = test % 9;
    if (mode < 4) {
        x.number = mode & 1 ? right : left;
        y.number = mode & 2 ? bottom : top;
    } else if (mode == 4) {
        x.number = (left + right) / 2.0f;
        y.number = (top + bottom) / 2.0f;
    } else if (mode == 5) {
        x.number = right;
        x.bits = right == 0 ? 1 : right > 0 ? x.bits + 1 : x.bits - 1;
        y.number = (top + bottom) / 2.0f;
    } else if (mode == 6) {
        x.number = (left + right) / 2.0f;
        y.number = top;
        y.bits = top == 0 ? 0x80000001 : top > 0 ? y.bits - 1 : y.bits + 1;
    } else if (mode == 7) {
        return;
    }
    input[9] = x.bits;
    input[10] = y.bits;
}

static void probe_ui_text_bounds(void) {
    ui_text_lines_initialize();
    void *lua = ui_text_lines_context.lua;
    load_selected_function(lua, "/data/local/tmp/ab520/ui/text-bounds-oracle.lua",
                           "text-bounds");
    load_selected_function(lua, "/data/local/tmp/ab520/ui/text-correction-oracle.lua",
                           "text-correction");
    damage_function(lua, -10002, "tostring", ui_text_bounds_never_called);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "setFont", ui_text_bounds_never_called);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "res");
    damage_function(lua, 1, "getString", ui_text_bounds_localize);
    damage_function(lua, 1, "getStringWidth", ui_text_bounds_measure);
    damage_function(lua, 1, "getFontLeading", ui_text_bounds_leading);
    damage_lua.set_top(lua, 0);
    static const uint32_t reply_numbers[] = {0, 0x80000000, 0x7fc12345, 0x3f800000};
    uint32_t random = 0x520b00d5;
    semihost(4, "AB_ANDROID_UI_TEXT_BOUNDS_V2 4096\n");
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t *input = ui_text_bounds_input;
        input[0] = (uint32_t)(test % 36);
        input[1] = random_word(&random) % 3;
        input[2] = (uint32_t)(test % 7);
        input[3] = (uint32_t)((test / 7) % 6);
        input[4] = random_word(&random) % 5;
        input[5] = random_word(&random) % 5;
        for (size_t field = 6; field < 11; ++field) {
            input[field] = random_float(&random, 130, 6, 1);
        }
        input[11] = (uint32_t)((int32_t)(random_word(&random) % 121) - 60);
        input[12] = (uint32_t)((int32_t)(random_word(&random) % 121) - 60);
        input[13] = (uint32_t)((int32_t)(random_word(&random) % 9) - 4);
        for (size_t child = 0; child < 5; ++child) {
            uint32_t kind = random_word(&random) % 6;
            uint32_t value = random_word(&random);
            input[14 + child * 2] = kind;
            input[15 + child * 2] = kind == 1   ? value & 1
                                    : kind == 2 ? reply_numbers[value % 4]
                                    : kind == 3 ? value & 1
                                    : kind >= 4 ? value % 17 + 1
                                                : 0;
        }
        if (test % 17 == 0) {
            input[6] = test & 1 ? 0x80000000 : 0;
        }
        ui_text_bounds_fixture_point(test);
        print_words(input, 24);
        ui_text_bounds_run(lua);
    }
    semihost(4, "AB UI text bounds reference complete\n");
}

#endif
