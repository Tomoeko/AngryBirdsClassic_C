#ifndef AB_ORACLE_SLING_SCOPE_H
#define AB_ORACLE_SLING_SCOPE_H

#include "rubberband_probe.h"
#include "../../tests/sling_scope_cases.h"

static uint32_t scope_rows[64][9], scope_count;
static uint32_t scope_game[0x2000 / 4], scope_world[0x20000 / 4];
static uint32_t scope_context[3], scope_state[39];

static uint32_t *scope_render_state(void *context) {
    (void)context;
    return scope_state;
}

static void scope_image(void *image, void *context, float x, float y, float width,
                        float height, const void *uv, void *shader, void *color) {
    (void)image;
    (void)context;
    (void)uv;
    if (scope_count == 64 || shader || color)
        fail("Unexpected original Sling Scope image boundary\n");
    uint32_t *row = scope_rows[scope_count++];
    for (unsigned index = 0; index < 4; ++index)
        row[index] = scope_state[8 + index];
    row[4] = scope_state[16];
    FloatBits rectangle[] = {
        {.number = x}, {.number = y}, {.number = width}, {.number = height}};
    for (unsigned index = 0; index < 4; ++index)
        row[5 + index] = rectangle[index].bits;
}

static void scope_number(void *lua, const char *key, float value, unsigned kind) {
    if (kind == 0)
        damage_lua.push_nil(lua);
    else if (kind == 2)
        damage_lua.push_boolean(lua, 0);
    else
        damage_lua.push_number(lua, value);
    damage_lua.set_field(lua, 1, key);
}

static void probe_sling_scope(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct_state)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*construct_table)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    void (*destroy_table)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaTableD1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    void (*draw)(void *, void *) = (void (*)(void *, void *))find_function(
        "_ZN7GameLua14drawSlingScopeEN3lua8LuaTableE");
    uint32_t *wrapper = allocate(20);
    if (!wrapper)
        fail("Cannot allocate original Sling Scope Lua state\n");
    construct_state(wrapper);
    claim(wrapper);
    uint32_t table[6] = {0};
    construct_table(table, wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    RubberbandFixture fixture;
    rubberband_prepare(&fixture);
    uint32_t context_vtable[25] = {0};
    uint32_t camera[30] = {0}, map_node[6] = {0}, object[16] = {0}, body[64] = {0};
    RubberbandName bird = {10, 10, 1000, "scope_bird"};
    scope_context[0] = (uint32_t)(uintptr_t)context_vtable;
    context_vtable[24] = (uint32_t)(uintptr_t)scope_render_state;
    scope_game[0x44 / 4] = (uint32_t)(uintptr_t)scope_context;
    scope_game[0x50 / 4] = (uint32_t)(uintptr_t)fixture.resources;
    scope_game[0x1784 / 4] = (uint32_t)(uintptr_t)scope_world;
    scope_game[0x198c / 4] = (uint32_t)(uintptr_t)camera;
    uint32_t *header = scope_game + 0x17a8 / 4;
    header[1] = header[2] = header[3] = (uint32_t)(uintptr_t)map_node;
    header[4] = 1;
    map_node[0] = 1;
    map_node[1] = (uint32_t)(uintptr_t)header;
    map_node[4] = (uint32_t)(uintptr_t)bird.bytes;
    object[0x2c / 4] = (uint32_t)(uintptr_t)body;
    fixture.resources[3] = (uint32_t)(uintptr_t)scope_context;
    fixture.image_vtable[6] = (uint32_t)(uintptr_t)scope_image;
    semihost(4, "AB_ANDROID_SLING_SCOPE_V1 256\n");
    for (size_t item = 0; item < AB_SLING_SCOPE_CASES; ++item) {
        AbSlingScopeCase input = ab_sling_scope_case(item);
        scope_count = 0;
        FloatBits mass = {.number = input.mass}, gravity = {.number = input.gravity};
        body[0x74 / 4] = mass.bits;
        scope_world[0x1923c / 4] = gravity.bits;
        map_node[5] = input.body ? (uint32_t)(uintptr_t)object : 0;
        const float state[] = {input.tx, input.ty, input.sx, input.sy};
        for (size_t field = 0; field < 4; ++field) {
            FloatBits value = {.number = state[field]};
            scope_state[8 + field] = value.bits;
        }
        FloatBits pivot_x = {.number = (float)input.pivot_x};
        FloatBits pivot_y = {.number = (float)input.pivot_y};
        fixture.sprite[6] = pivot_x.bits;
        fixture.sprite[7] = pivot_y.bits;
        reference(table);
        scope_number(lua, "vx", input.vx, 1);
        scope_number(lua, "vy", input.vy, 1);
        scope_number(lua, "x", input.x, 1);
        scope_number(lua, "y", input.y, 1);
        scope_number(lua, "verticalForce", input.force, 1);
        scope_number(lua, "timer", input.timer, 1);
        scope_number(lua, "scopeLength", input.length, input.length_kind);
        scope_number(lua, "alphaFactor", input.alpha, input.alpha_kind);
        if (input.decay_present)
            damage_lua.push_boolean(lua, (int)input.decay);
        else
            damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 1, "hasAlphaDecay");
        damage_lua.push_string(lua, bird.bytes);
        damage_lua.set_field(lua, 1, "birdName");
        damage_lua.push_string(lua, fixture.sprite_name.bytes);
        damage_lua.set_field(lua, 1, "dotSpriteName");
        damage_lua.set_top(lua, 0);
        scope_state[16] = UINT32_C(0x3ef00000);
        draw(scope_game, table);
        if (scope_state[16] != UINT32_C(0x3f800000) ||
            scope_state[8] != scope_state[10] || scope_state[9] != scope_state[11])
            fail("Original Sling Scope did not restore the controlled render state\n");
        uint32_t row_header[] = {(uint32_t)item, scope_count};
        print_words(row_header, 2);
        for (uint32_t dot = 0; dot < scope_count; ++dot)
            print_words(scope_rows[dot], 9);
        uint32_t final[5];
        for (unsigned field = 0; field < 4; ++field)
            final[field] = scope_state[8 + field];
        final[4] = scope_state[16];
        print_words(final, 5);
    }
    destroy_table(table);
    release(wrapper);
    semihost(4, "AB Sling Scope reference complete\n");
}

#endif
