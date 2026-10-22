#ifndef AB_ORACLE_UI_TEXT_METRICS_H
#define AB_ORACLE_UI_TEXT_METRICS_H

static int ui_text_metrics_localize(void *lua) {
    if (damage_lua.get_top(lua) != 2) {
        fail("Unexpected original width localization argument count\n");
    }
    UiTextLineEvent *event = ui_text_lines_event(5);
    ui_text_lines_argument(event, 0, lua, 1);
    ui_text_lines_argument(event, 1, lua, 2);
    ui_text_lines_context.push_value(lua, 2);
    return 1;
}

static int ui_text_metrics_unused_getter(void *lua) {
    (void)lua;
    fail("Original text metric unexpectedly invoked stored font getter\n");
}

static void ui_text_metrics_scale(void *lua, const char *field, uint32_t kind,
                                  uint32_t value) {
    if (!kind) {
        damage_lua.push_nil(lua);
    } else if (kind == 1) {
        damage_lua.push_boolean(lua, 0);
    } else {
        FloatBits number = {.bits = value};
        damage_lua.push_number(lua, number.number);
    }
    damage_lua.set_field(lua, 1, field);
}

static void ui_text_metrics_prepare(void *lua, const uint32_t input[12]) {
    damage_lua.create_table(lua, 0, 12);
    ui_text_metrics_scale(lua, "scaleX", input[2], input[3]);
    ui_text_metrics_scale(lua, "scaleY", input[4], input[5]);
    damage_number(lua, 1, "fontLeadingScale", input[6]);
    damage_lua.push_string(lua, "FONT_BASIC");
    damage_lua.set_field(lua, 1, "font");
    damage_lua.push_string(lua, ui_text_lines_groups[input[10]]);
    damage_lua.set_field(lua, 1, "group");
    if (input[0] == 0) {
        damage_lua.push_nil(lua);
    } else if (input[0] == 1) {
        damage_lua.push_boolean(lua, (int)input[1]);
    } else if (input[0] == 2) {
        FloatBits number = {.bits = input[1]};
        damage_lua.push_number(lua, number.number);
    } else {
        const UiTextWrapCase *source = ui_text_wrap_cases + input[1];
        ui_text_lines_context.push_bytes(lua, source->bytes, (unsigned)source->size);
    }
    damage_lua.set_field(lua, 1, "text");
    if (input[7] == 0) {
        damage_lua.push_nil(lua);
    } else if (input[7] < 3) {
        damage_lua.push_boolean(lua, input[7] == 2);
    } else if (input[7] == 3) {
        damage_lua.push_number(lua, 0);
    } else if (input[7] == 4) {
        damage_lua.push_string(lua, "");
    } else {
        damage_lua.create_table(lua, 0, 0);
    }
    damage_lua.set_field(lua, 1, "clipped");
    damage_lua.create_table(lua, (int)input[8], 0);
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    for (uint32_t line = 0; line < input[8]; ++line) {
        damage_lua.push_boolean(lua, 1);
        raw_set(lua, 2, (int)line + 1);
    }
    damage_lua.set_field(lua, 1, "lines");
    damage_lua.get_field(lua, -10002, "text-leading");
    damage_lua.set_field(lua, 1, "getFontLeading");
    damage_function(lua, 1, "getFontFunction", ui_text_metrics_unused_getter);
    damage_number(lua, 1, "width", 0x7fc12345);
    damage_number(lua, 1, "widestLine", 0x7fc12345);
    damage_number(lua, 1, "textBlockHeight", 0x7fc12345);
    damage_lua.set_field(lua, -10002, "_ab_text_metrics_root");
    ui_text_lines_context.font[13] = input[9];
    ui_text_lines_context.font[10] = input[11];
}

static void probe_ui_text_metrics(void) {
    ui_text_lines_initialize();
    void *lua = ui_text_lines_context.lua;
    damage_lua.get_field(lua, -10002, "_ab_text_builtin_tostring");
    damage_lua.set_field(lua, -10002, "tostring");
    const char *names[] = {"text-width", "text-height", "text-leading"};
    const char *paths[] = {"/data/local/tmp/ab520/ui/text-width-oracle.lua",
                           "/data/local/tmp/ab520/ui/text-height-oracle.lua",
                           "/data/local/tmp/ab520/ui/text-leading-oracle.lua"};
    for (size_t method = 0; method < 3; ++method) {
        load_selected_function(lua, paths[method], names[method]);
    }
    damage_lua.get_field(lua, -10002, "res");
    damage_function(lua, 1, "getString", ui_text_metrics_localize);
    damage_lua.set_top(lua, 0);
    static const float scales[] = {-2, -1, -0.5f, -0.0f, 0, 0.5f, 1, 2};
    uint32_t random = 0x5207e671;
    semihost(4, "AB_ANDROID_UI_TEXT_METRICS_V1 4096 3\n");
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t input[12] = {(uint32_t)(test % 4),
                              random_word(&random),
                              (uint32_t)(test % 3),
                              0,
                              (uint32_t)((test / 3) % 3),
                              0,
                              0,
                              (uint32_t)(test % 6),
                              (uint32_t)(test % 9)};
        input[1] = input[0] == 0   ? 0
                   : input[0] == 1 ? input[1] & 1
                   : input[0] == 3 ? (uint32_t)(test / 4) % 36
                                   : input[1];
        FloatBits value = {.number = scales[random_word(&random) % 8]};
        input[3] = value.bits;
        value.number = scales[random_word(&random) % 8];
        input[5] = value.bits;
        value.number = scales[random_word(&random) % 8];
        input[6] = value.bits;
        input[9] = (uint32_t)((int32_t)(random_word(&random) % 121) - 60);
        input[10] = random_word(&random) % 3;
        input[11] = (uint32_t)((int32_t)(random_word(&random) % 9) - 4);
        print_words(input, 12);
        ui_text_metrics_prepare(lua, input);
        for (size_t method = 0; method < 3; ++method) {
            ui_text_lines_event_count = 0;
            damage_lua.get_field(lua, -10002, names[method]);
            damage_lua.get_field(lua, -10002, "_ab_text_metrics_root");
            if (damage_lua.call(lua, 1, 1, 0) || damage_lua.type(lua, 1) != 3) {
                const char *message = damage_lua.to_string(lua, -1, NULL);
                fail(message ? message : "Original text metric failed\n");
            }
            FloatBits result = {.number = damage_lua.to_number(lua, 1)};
            uint32_t output[] = {result.bits, (uint32_t)ui_text_lines_event_count};
            print_words(output, 2);
            ui_text_lines_emit_events();
            damage_lua.set_top(lua, 0);
        }
    }
    semihost(4, "AB UI text metrics reference complete\n");
}

#endif
