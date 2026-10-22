#ifndef AB_ORACLE_UI_TEXT_CLIP_H
#define AB_ORACLE_UI_TEXT_CLIP_H

static uint32_t ui_text_clip_lines[4][15], ui_text_clip_count;
static uint32_t ui_text_clip_leaf_count;
static uintptr_t ui_text_clip_internal_start, ui_text_clip_internal_end;
static void (*ui_text_clip_raw_set)(void *, int, int);

static void ui_text_clip_context_access(uintptr_t caller) {
    /* Each uncolored native leaf sets its transform through one context getter.
     * Distinguish that callsite from Font and setAlpha getters, without patches
     * or instruction execution in the running app. Only counts are emitted. */
    if (caller >= ui_text_clip_internal_start && caller < ui_text_clip_internal_end) {
        ++ui_text_clip_leaf_count;
    }
}

static int32_t ui_text_clip_screen_height(void *context) {
    (void)context;
    return 320;
}

static void ui_text_clip_prepare(void *lua, uint32_t *random, size_t test) {
    static const char *const horizontal[] = {"LEFT", "HCENTER", "RIGHT", "HPIVOT"};
    static const char *const vertical[] = {"TOP", "VCENTER", "BOTTOM", "BASELINE",
                                           "VPIVOT"};
    static const char *const texts[] = {
        "",   "AB",  "a0 B", "A\nB", "missing", "\xe4\xb8\x80\xf0\x9f\x98\x80",
        "  ", "A0aB"};
    static const uint32_t scales[] = {0x3e000000, 0xbe000000, 0x3f000000, 0xbf000000,
                                      0x3f800000, 0xbf800000, 0x40000000, 0xc0000000};
    ui_text_clip_count = (uint32_t)test % 5;
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "clipped");
    damage_lua.create_table(lua, (int)ui_text_clip_count, 0);
    for (uint32_t line = 0; line < ui_text_clip_count; ++line) {
        uint32_t *input = ui_text_clip_lines[line];
        for (size_t field = 0; field < 4; ++field) {
            input[field] = random_word(random) % (field == 3 ? 4u : 2u);
        }
        input[4] = random_word(random) % 4;
        input[5] = random_word(random) % 5;
        input[6] = random_word(random) % 8;
        input[7] = random_word(random) % 3;
        for (size_t field = 8; field < 15; ++field) {
            input[field] = random_float(random, field < 10 ? 127 : 122, 7, 1);
        }
        input[10] = scales[random_word(random) % 8];
        input[11] = scales[random_word(random) % 8];
        damage_lua.create_table(lua, 0, 16);
        damage_lua.push_boolean(lua, (int)input[0]);
        damage_lua.set_field(lua, 3, "visible");
        damage_lua.push_boolean(lua, (int)input[1]);
        damage_lua.set_field(lua, 3, "floorCoordinates");
        const char *const fields[] = {"x", "y", "scaleX", "scaleY"};
        for (size_t field = 0; field < 4; ++field) {
            damage_number(lua, 3, fields[field], input[8 + field]);
        }
        if (input[2]) {
            damage_number(lua, 3, "alpha", input[12]);
        }
        if (input[3] & 1) {
            damage_number(lua, 3, "rotationPivotX", input[13]);
        }
        if (input[3] & 2) {
            damage_number(lua, 3, "rotationPivotY", input[14]);
        }
        damage_lua.push_string(lua, horizontal[input[4]]);
        damage_lua.set_field(lua, 3, "hanchor");
        damage_lua.push_string(lua, vertical[input[5]]);
        damage_lua.set_field(lua, 3, "vanchor");
        damage_lua.push_string(lua, texts[input[6]]);
        damage_lua.set_field(lua, 3, "text");
        damage_lua.push_string(lua, "child-group-ignored");
        damage_lua.set_field(lua, 3, "group");
        if (input[7]) {
            damage_lua.push_string(lua, input[7] == 1 ? "FONT_BASIC" : "MISSING_FONT");
            damage_lua.set_field(lua, 3, "font");
        }
        ui_text_clip_raw_set(lua, 2, (int)line + 1);
    }
    damage_lua.set_field(lua, 1, "lines");
}

static void ui_text_clip_emit(void) {
    print_words(&ui_text_clip_count, 1);
    for (uint32_t line = 0; line < ui_text_clip_count; ++line) {
        print_words(ui_text_clip_lines[line], 15);
    }
}

#endif
