#ifndef AB_ORACLE_LUA_ASSET_SPRITE_H
#define AB_ORACLE_LUA_ASSET_SPRITE_H

#include "../../tests/lua_asset_sprite_cases.h"

static uint32_t asset_probe_state[24], asset_probe_rectangle[4], asset_probe_calls;

static uint32_t *asset_probe_get_state(void *context) {
    (void)context;
    return asset_probe_state;
}

static void asset_probe_image(void *image, void *context, float x, float y, float width,
                              float height, const void *uv, void *shader,
                              const void *color) {
    (void)image;
    (void)context;
    if (++asset_probe_calls != 1 || !uv || shader || color)
        fail("Unexpected Lua asset sprite image boundary\n");
    FloatBits values[] = {
        {.number = x}, {.number = y}, {.number = width}, {.number = height}};
    for (uint32_t index = 0; index < 4; ++index)
        asset_probe_rectangle[index] = values[index].bits;
}

static void asset_probe_vector(void *lua, const char *name, float x, float y) {
    damage_lua.create_table(lua, 0, 2);
    FloatBits horizontal = {.number = x}, vertical = {.number = y};
    damage_number(lua, 2, "x", horizontal.bits);
    damage_number(lua, 2, "y", vertical.bits);
    damage_lua.set_field(lua, 1, name);
}

/* Bootstrap only: complete unchanged draw method, real original LuaObject and
 * LuaState wrappers, controlled ordinary SpriteAsset/Sprite/image/context data.
 * Asset lookup, constructor defaults and native scene ownership are separate.
 */
static void probe_lua_asset_sprite(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct_state)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*construct_object)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua9LuaObjectC1EPNS_8LuaStateE");
    void (*destroy_object)(void *) =
        (void (*)(void *))find_function("_ZN3lua9LuaObjectD1Ev");
    void (*get_reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    void (*draw)(void *, void *, void *) =
        (void (*)(void *, void *, void *))find_function(
            "_ZN3gfx16LuaAssetRenderer4drawEPN2gr7ContextEPNS_6CameraE");
    void *wrapper = allocate(20);
    if (!wrapper)
        fail("Cannot allocate Lua asset observer wrapper\n");
    construct_state(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)((uint32_t *)wrapper)[2];
    uint32_t renderer[31] = {0}, asset[6] = {0}, sprite[16] = {0};
    uint32_t sheet[4] = {0}, image[1], image_vtable[7] = {0};
    uint32_t context[1], context_vtable[25] = {0}, camera[30] = {0};
    renderer[2] = (uint32_t)(uintptr_t)asset;
    asset[3] = (uint32_t)(uintptr_t)sprite;
    sprite[2] = (uint32_t)(uintptr_t)sheet;
    sprite[5] = 10u | (12u << 16);
    FloatBits pivot_x = {.number = 5}, pivot_y = {.number = 6};
    sprite[6] = pivot_x.bits;
    sprite[7] = pivot_y.bits;
    image_vtable[6] = (uint32_t)(uintptr_t)asset_probe_image;
    image[0] = (uint32_t)(uintptr_t)image_vtable;
    sheet[3] = (uint32_t)(uintptr_t)image;
    context_vtable[24] = (uint32_t)(uintptr_t)asset_probe_get_state;
    context[0] = (uint32_t)(uintptr_t)context_vtable;
    const float camera_values[] = {-20, -10, 0.5f, 0.75f};
    for (uint32_t index = 0; index < 4; ++index) {
        FloatBits value = {.number = camera_values[index]};
        camera[26 + index] = value.bits;
    }
    construct_object(renderer + 19, wrapper);
    damage_lua.set_top(lua, 0);
    get_reference(renderer + 19);
    if (damage_lua.type(lua, 1) != 5)
        fail("Missing original Lua asset property table\n");
    asset_probe_vector(lua, "position", 80.125f, -40.25f);
    asset_probe_vector(lua, "scale", 2, 0.5f);
    damage_number(lua, 1, "angle", 0);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "visible");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 1, "useAlpha");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 1, "drawString");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 2, "use");
    damage_lua.set_field(lua, 1, "imageSize");
    damage_lua.set_top(lua, 0);
#if defined(AB_ORACLE_LUA_ASSET_SPRITE_SMOKE)
    semihost(4, "AB_ANDROID_LUA_ASSET_SPRITE_SMOKE_V1 1\n");
    draw(renderer, context, camera);
    if (asset_probe_calls != 1 || damage_lua.get_top(lua))
        fail("Lua asset draw did not preserve its boundary/stack\n");
    print_words(asset_probe_rectangle, 4);
    print_words(asset_probe_state, 24);
#else
    semihost(4, "AB_ANDROID_LUA_ASSET_SPRITE_V1 4096\n");
    for (uint32_t sample = 0; sample < LUA_ASSET_SPRITE_CASES; ++sample) {
        uint32_t input[16];
        asset_sprite_case(sample, input);
        sprite[5] = input[0];
        sprite[6] = input[1];
        sprite[7] = input[2];
        asset[3] = input[13] ? (uint32_t)(uintptr_t)sprite : 0;
        camera[26] = input[8];
        camera[27] = input[9];
        camera[28] = input[10];
        camera[29] = input[11];
        get_reference(renderer + 19);
        asset_probe_vector(lua, "position", asset_sprite_number(input[3]),
                           asset_sprite_number(input[4]));
        asset_probe_vector(lua, "scale", asset_sprite_number(input[5]),
                           asset_sprite_number(input[6]));
        damage_number(lua, 1, "angle", input[7]);
        damage_number(lua, 1, "alpha", input[15]);
        damage_lua.push_boolean(lua, input[12] != 0);
        damage_lua.set_field(lua, 1, "visible");
        damage_lua.push_boolean(lua, input[14] != 0);
        damage_lua.set_field(lua, 1, "useAlpha");
        damage_lua.set_top(lua, 0);
        for (uint32_t index = 0; index < 24; ++index)
            asset_probe_state[index] = 0;
        asset_probe_state[16] = UINT32_C(0x3e800000);
        for (uint32_t index = 0; index < 4; ++index)
            asset_probe_rectangle[index] = UINT32_C(0x7fc00000);
        asset_probe_calls = 0;
        draw(renderer, context, camera);
        if (asset_probe_calls != (input[12] && input[13]) || damage_lua.get_top(lua))
            fail("Lua asset ordinary draw/stack boundary mismatch\n");
        print_words(input, 16);
        print_words(&asset_probe_calls, 1);
        print_words(asset_probe_rectangle, 4);
        print_words(asset_probe_state, 24);
    }
#endif
    destroy_object(renderer + 19);
    release(wrapper);
#if defined(AB_ORACLE_LUA_ASSET_SPRITE_SMOKE)
    semihost(4, "AB Lua asset sprite smoke reference complete\n");
#else
    semihost(4, "AB Lua asset sprite reference complete\n");
#endif
}

#endif
