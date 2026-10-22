#ifndef AB_ORACLE_UI_TEXT_WRAP_CONTEXT_H
#define AB_ORACLE_UI_TEXT_WRAP_CONTEXT_H

typedef struct {
    _Alignas(8) uint32_t wrapper[8], game[0x1984 / 4];
    uint32_t resources[0x468 / 4], font[18];
    void *lua;
    void (*wrap)(void *, const void *, const void *, float);
    void (*get_table)(const void *);
    void (*raw_get)(void *, int, int);
    void (*push_value)(void *, int);
    void (*push_bytes)(void *, const char *, unsigned);
    unsigned (*length)(void *, int);
    int32_t (*width)(const void *, const void *, int32_t, int32_t);
    int32_t (*leading)(const void *);
} UiTextWrapContext;

static void ui_text_wrap_context_init(UiTextWrapContext *context) {
    initialize_damage_lua();
    void *parent = damage_lua.new_state();
    if (!parent) {
        fail("Cannot allocate native wrapped-text state\n");
    }
    void (*construct_state)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaStateC1EP9lua_State");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*construct_table)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    context->get_table =
        (void (*)(const void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    context->raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    context->push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    context->push_bytes = (void (*)(void *, const char *, unsigned))find_function(
        "_Z15lua_pushlstringP9lua_StatePKcj");
    context->length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    context->wrap = (void (*)(void *, const void *, const void *, float))find_function(
        "_ZN7GameLua8clipTextESsSsf");
    context->width =
        (int32_t (*)(const void *, const void *, int32_t, int32_t))find_function(
            "_ZNK4game9Resources14getStringWidthERKSsii");
    context->leading = (int32_t (*)(const void *))find_function(
        "_ZNK4game9Resources14getFontLeadingEv");
    construct_state(context->wrapper, parent);
    claim(context->wrapper);
    context->lua = (void *)(uintptr_t)context->wrapper[2];
    context->game[0x10 / 4] = (uint32_t)(uintptr_t)context->wrapper;
    construct_table(context->game + 0x1840 / 4, context->wrapper);
    context->game[0x50 / 4] = (uint32_t)(uintptr_t)context->resources;
    context->resources[0x28 / 4] = (uint32_t)(uintptr_t)context->font;
    uint32_t *font = context->font;
    font[0] = (uint32_t)find_symbol("_ZTVN4game10BitmapFontE", 1) + 8;
    font[5] = metric_tree(0, 7, (uint32_t)(uintptr_t)(font + 4));
    font[6] = (uint32_t)(uintptr_t)metric_nodes[0];
    font[7] = (uint32_t)(uintptr_t)metric_nodes[6];
    font[8] = 7;
    font[10] = 1;
    font[11] = 20;
    font[12] = 5;
    font[13] = 30;
    for (size_t glyph = 0; glyph < 7; ++glyph) {
        metric_sprites[glyph][5] =
            (uint32_t)(8 + glyph) | ((uint32_t)(12 + glyph) << 16);
    }
}

static void ui_text_wrap_emit_bytes(const char *bytes, uint32_t length) {
    if (length > 4096) {
        fail("Native wrapped text exceeds bounded output\n");
    }
    print_words(&length, 1);
    static const char digits[] = "0123456789abcdef";
    for (size_t position = 0; position < length;) {
        size_t count = length - position;
        if (count > 64) {
            count = 64;
        }
        char output[129];
        for (size_t index = 0; index < count; ++index) {
            uint8_t value = (uint8_t)bytes[position + index];
            output[index * 2] = digits[value >> 4];
            output[index * 2 + 1] = digits[value & 15];
        }
        output[count * 2] = 0;
        semihost(4, output);
        position += count;
    }
    semihost(4, "\n");
}

#endif
