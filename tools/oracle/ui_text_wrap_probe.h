#ifndef AB_ORACLE_UI_TEXT_WRAP_H
#define AB_ORACLE_UI_TEXT_WRAP_H

#include "ui_text_wrap_cases.h"

#include "ui_text_wrap_context.h"

static void probe_ui_text_wrap(void) {
    UiTextWrapContext context = {0};
    ui_text_wrap_context_init(&context);
    void *lua = context.lua;
    struct {
        uint32_t size, capacity;
        int32_t references;
        char bytes[128];
    } text = {.capacity = 127, .references = -1},
      group = {.capacity = 127, .references = -1};
    const char *text_value = text.bytes, *group_value = group.bytes;
    static const float widths[] = {-__builtin_inff(),
                                   -1,
                                   -0.0f,
                                   0,
                                   16,
                                   17,
                                   20,
                                   21,
                                   22,
                                   23,
                                   24,
                                   30,
                                   31,
                                   32,
                                   33,
                                   40,
                                   44,
                                   64,
                                   128,
                                   1000000,
                                   __builtin_inff()};
    uint32_t random = 0x520b1e01;
    semihost(4, "AB_ANDROID_UI_TEXT_WRAP_V1 4096\n");
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t input[2];
        input[0] = (uint32_t)(test % (sizeof(ui_text_wrap_cases) /
                                      sizeof(*ui_text_wrap_cases)));
        FloatBits width = {
            .number =
                widths[random_word(&random) % (sizeof(widths) / sizeof(*widths))]};
        input[1] = width.bits;
        const UiTextWrapCase *source = ui_text_wrap_cases + input[0];
        if (source->size >= sizeof(text.bytes)) {
            fail("Wrapped-text input exceeds fixture storage\n");
        }
        text.size = (uint32_t)source->size;
        for (size_t index = 0; index < source->size; ++index) {
            text.bytes[index] = source->bytes[index];
        }
        text.bytes[source->size] = 0;
        damage_lua.set_top(lua, 0);
        print_words(input, 2);
        context.wrap(context.game, &group_value, &text_value, width.number);
        context.get_table(context.game + 0x1840 / 4);
        damage_lua.get_field(lua, 1, "widestLine");
        if (damage_lua.type(lua, 2) != 3) {
            fail("Native wrapped-text width is not numeric\n");
        }
        FloatBits widest = {.number = damage_lua.to_number(lua, 2)};
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "lines");
        if (damage_lua.type(lua, 2) != 5) {
            fail("Native wrapped-text result is not a table\n");
        }
        uint32_t count = context.length(lua, 2);
        if (count > 128) {
            fail("Native wrapped-text line count exceeds bound\n");
        }
        uint32_t output[] = {count, widest.bits};
        print_words(output, 2);
        for (uint32_t line = 0; line < count; ++line) {
            context.raw_get(lua, 2, (int)line + 1);
            unsigned size = 0;
            const char *value = damage_lua.to_string(lua, 3, &size);
            if (!value) {
                fail("Native wrapped-text line is not a string\n");
            }
            ui_text_wrap_emit_bytes(value, size);
            damage_lua.set_top(lua, 2);
        }
    }
    semihost(4, "AB UI text wrap reference complete\n");
}

#endif
