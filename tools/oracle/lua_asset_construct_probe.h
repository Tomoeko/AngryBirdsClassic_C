#ifndef AB_ORACLE_LUA_ASSET_CONSTRUCT_H
#define AB_ORACLE_LUA_ASSET_CONSTRUCT_H

#include "../../tests/lua_asset_construct_cases.h"

static uint32_t construct_sprite[16], construct_resource_calls;

static void *construct_sprite_lookup(void *resources, const void *name) {
    (void)resources;
    (void)name;
    ++construct_resource_calls;
    return construct_sprite;
}

static void construct_observe_field(void *lua, int table, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    int type = damage_lua.type(lua, -1);
    uint32_t payload = 0;
    if (type == 1)
        payload = (uint32_t)damage_lua.to_boolean(lua, -1);
    else if (type == 3) {
        FloatBits number = {.number = damage_lua.to_number(lua, -1)};
        payload = number.bits;
    } else if (type == 4) {
        unsigned length = 0;
        damage_lua.to_string(lua, -1, &length);
        payload = length;
    } else if (type == 5 || type == 6)
        payload = 1;
    uint32_t words[] = {(uint32_t)type, payload};
#if !defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_FULL)
    semihost(4, name);
    semihost(4, "\n");
#endif
    print_words(words, 2);
    damage_lua.set_top(lua, top);
}

static void construct_observe_object(void *lua, int object) {
    const char *fields[] = {"position", "scale",       "imageSize", "angle",
                            "visible",  "useAlpha",    "alpha",     "drawString",
                            "string",   "stringScale", "setSprite"};
    for (unsigned index = 0; index < sizeof(fields) / sizeof(*fields); ++index)
        construct_observe_field(lua, object, fields[index]);
    const char *vectors[] = {"position", "scale", "imageSize"};
    const void *identities[3];
    for (unsigned index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, object, vectors[index]);
        identities[index] = damage_lua.to_pointer(lua, -1);
#if !defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_FULL)
        semihost(4, vectors[index]);
        semihost(4, "\n");
#endif
        int table = damage_lua.get_top(lua);
        if (index < 2) {
            construct_observe_field(lua, table, "x");
            construct_observe_field(lua, table, "y");
        } else {
            const char *sizes[] = {"use", "w", "h", "width", "height"};
            for (unsigned field = 0; field < 5; ++field)
                construct_observe_field(lua, table, sizes[field]);
        }
        damage_lua.set_top(lua, object);
    }
#if defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_FULL)
    uint32_t distinct = identities[0] != identities[1] &&
                        identities[1] != identities[2] &&
                        identities[0] != identities[2];
    print_words(&distinct, 1);
    construct_observe_field(lua, 1, "sentinel");
    construct_observe_field(lua, 1, "position");
#else
    (void)identities;
#endif
}

static void probe_lua_asset_construct(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct_state)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*construct_scene)(void *, void *, void *) =
        (void (*)(void *, void *, void *))find_function(
            "_ZN3gfx5SceneC1ERN4game9ResourcesERN2gr7ContextE");
    int (*create)(void *, void *) = (int (*)(void *, void *))find_function(
        "_ZN7GameLua22createLuaAssetRendererEPN3lua8LuaStateE");
    void (*clear)(void *) = (void (*)(void *))find_function("_ZN3gfx5Scene5clearEv");
    const uint32_t *next =
        (const uint32_t *)(uintptr_t)find_function("_ZN3gfx5Scene5newIdEv");
    uint32_t *counter = (uint32_t *)((uintptr_t)next + 12 + next[6]);
    uint32_t saved_counter = *counter;
    void *wrapper = allocate(20);
    if (!wrapper)
        fail("Cannot create original renderer constructor Lua state\n");
    construct_state(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)((uint32_t *)wrapper)[2];
    uint32_t resources[1], resource_methods[3] = {0};
    resource_methods[2] = (uint32_t)(uintptr_t)construct_sprite_lookup;
    resources[0] = (uint32_t)(uintptr_t)resource_methods;
#if defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_FULL)
    semihost(4, "AB_ANDROID_LUA_ASSET_CONSTRUCT_V1 4096\n");
    const uint32_t cases = 4096;
#else
    const uint32_t cases = 1;
#endif
    for (uint32_t sample = 0; sample < cases; ++sample) {
        uint32_t scene[8], game[1637] = {0}, input[3];
        asset_construct_case(sample, input);
        construct_scene(scene, resources, 0);
        game[1636] = (uint32_t)(uintptr_t)scene;
        damage_lua.create_table(lua, 0, 4);
#if defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_FULL)
        *counter = input[0];
        damage_number(lua, 1, "sentinel", UINT32_C(0x42280000));
        damage_number(lua, 1, "position", UINT32_C(0x40e80000));
        damage_lua.push_string(lua, input[2] ? "TRAIL_WHITE_1" : "");
        FloatBits layer = {.bits = input[1]};
        damage_lua.push_number(lua, layer.number);
#else
        damage_lua.push_string(lua, "TRAIL_WHITE_1");
        damage_lua.push_number(lua, 2);
#endif
        construct_resource_calls = 0;
        int returns = create(game, wrapper);
#if defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_FULL)
        const uint32_t *node = (const uint32_t *)(uintptr_t)scene[5];
        const uint32_t *members = (const uint32_t *)(uintptr_t)node[5];
        const uint32_t *renderer = (const uint32_t *)(uintptr_t)members[0];
        const uint32_t *asset = (const uint32_t *)(uintptr_t)renderer[2];
        FloatBits id = {.number = damage_lua.to_number(lua, 4)};
        uint32_t initial[] = {(uint32_t)returns,
                              (uint32_t)damage_lua.get_top(lua),
                              construct_resource_calls,
                              *counter,
                              renderer[18],
                              id.bits,
                              renderer[17],
                              scene[7],
                              asset[3] != 0,
                              asset[4] != 0};
        print_words(input, 3);
        print_words(initial, 10);
#else
        semihost(4, "AB_ANDROID_LUA_ASSET_CONSTRUCT_SMOKE_V1 1\n");
        uint32_t initial[] = {(uint32_t)returns, (uint32_t)damage_lua.get_top(lua),
                              construct_resource_calls};
        print_words(initial, 3);
#endif
        damage_lua.get_field(lua, 1, "luaAssetRenderer");
        if (damage_lua.type(lua, -1) != 5)
            fail("Original renderer did not publish its property table\n");
        construct_observe_object(lua, damage_lua.get_top(lua));
        clear(scene);
        damage_lua.set_top(lua, 0);
    }
    *counter = saved_counter;
    release(wrapper);
#if defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_FULL)
    semihost(4, "AB Lua asset constructor reference complete\n");
#else
    semihost(4, "AB Lua asset constructor smoke reference complete\n");
#endif
}

#endif
