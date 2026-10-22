#ifndef AB_ORACLE_OBJECT_VISUAL_H
#define AB_ORACLE_OBJECT_VISUAL_H

typedef struct {
    uint32_t length, capacity, references;
    char bytes[16];
} VisualName;

static VisualName visual_names[] = {{0, 0, 1000, ""},          {3, 3, 1000, "raw"},
                                    {9, 9, 1000, "composite"}, {7, 7, 1000, "missing"},
                                    {7, 7, 1000, "bin\0ary"},  {3, 3, 1000, "old"}};
static uint32_t visual_input[7], visual_calls, visual_snapshot[3], visual_composite;
static uint32_t visual_game[0x1860 / 4], visual_asset[5], visual_renderer[3];
static uint32_t visual_resources[0x4e0 / 4], visual_vtable[3], visual_node[7],
    visual_entry[3], visual_regular[16], visual_compo[1];
static void *visual_lua;
static void (*visual_reference)(void *);
static int visual_recording;

static uint32_t visual_name_code(const char *bytes, unsigned length) {
    for (size_t index = 0; index < 6; ++index) {
        if (length != visual_names[index].length) {
            continue;
        }
        unsigned offset = 0;
        while (offset < length && bytes[offset] == visual_names[index].bytes[offset]) {
            ++offset;
        }
        if (offset == length) {
            return (uint32_t)index;
        }
    }
    fail("Unknown original visual sprite name\n");
}

static uint32_t visual_script(void) {
    visual_reference((uint8_t *)visual_game + 0x1810);
    damage_lua.get_field(visual_lua, 1, "first");
    damage_lua.get_field(visual_lua, 2, "sprite");
    if (damage_lua.type(visual_lua, 3) != 4) {
        fail("Expected string original object sprite field\n");
    }
    unsigned length;
    const char *bytes = damage_lua.to_string(visual_lua, 3, &length);
    uint32_t code = visual_name_code(bytes, length);
    damage_lua.set_top(visual_lua, 0);
    return code;
}

static uint32_t visual_asset_name(void) {
    const char *bytes = (const char *)(uintptr_t)visual_asset[2];
    const uint32_t *header = (const uint32_t *)(bytes - 12);
    return visual_name_code(bytes, header[0]);
}

static void visual_cache(void) {
    visual_resources[0x4cc / 4] =
        visual_composite ? (uint32_t)(uintptr_t)visual_node : 0;
    visual_node[4] = (uint32_t)(uintptr_t)visual_names[visual_input[3]].bytes;
    visual_node[5] = (uint32_t)(uintptr_t)visual_entry;
    visual_node[6] = (uint32_t)(uintptr_t)(visual_entry + 3);
    visual_entry[0] = 2;
    visual_entry[2] = (uint32_t)(uintptr_t)visual_compo;
}

static void *visual_get_sprite(void *resources, const char **name) {
    if (resources != visual_resources) {
        fail("Unexpected original visual resource owner\n");
    }
    if (!visual_recording) {
        return visual_regular;
    }
    const uint32_t *header = (const uint32_t *)(*name - 12);
    if (visual_name_code(*name, header[0]) != visual_input[3] || ++visual_calls != 1) {
        fail("Unexpected original visual resource lookup\n");
    }
    visual_snapshot[0] = visual_script();
    visual_snapshot[1] = visual_asset_name();
    visual_snapshot[2] = (visual_asset[3] ? 1u : 0u) | (visual_asset[4] ? 2u : 0u);
    if (visual_input[6]) {
        visual_composite ^= 1;
        visual_cache();
    }
    return visual_input[1] ? visual_regular : NULL;
}

static void probe_object_visual(void) {
    typedef void (*Construct)(void *);
    typedef void (*TableConstruct)(void *, void *);
    typedef void (*AssetSet)(void *, void *, const char **);
    typedef void (*ObjectSet)(void *, const char **, const char **);
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    Construct state_construct = (Construct)find_function("_ZN3lua8LuaStateC1Ev");
    TableConstruct table_construct =
        (TableConstruct)find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    Construct table_destroy = (Construct)find_function("_ZN3lua8LuaTableD1Ev");
    Construct claim = (Construct)find_function("_ZN4lang6Object5claimEv");
    Construct release = (Construct)find_function("_ZN4lang6Object7releaseEv");
    Construct object_construct = (Construct)find_function("_ZN4lang6ObjectC1Ev");
    Construct asset_construct = (Construct)find_function("_ZN3gfx11SpriteAssetC1Ev");
    AssetSet asset_set = (AssetSet)find_function(
        "_ZN3gfx11SpriteAsset9setSpriteEPN4game9ResourcesERKSs");
    ObjectSet object_set = (ObjectSet)find_function("_ZN7GameLua9setSpriteESsSs");
    initialize_damage_lua();
    visual_reference = (Construct)find_function("_ZNK3lua6detail7RefBase6getRefEv");
    uint32_t *wrapper = allocate(20);
    if (!wrapper) {
        fail("Cannot allocate original visual Lua wrapper\n");
    }
    state_construct(wrapper);
    claim(wrapper);
    visual_lua = (void *)(uintptr_t)wrapper[2];
    table_construct((uint8_t *)visual_game + 0x17f8, wrapper);
    table_construct((uint8_t *)visual_game + 0x1810, wrapper);
    visual_reference((uint8_t *)visual_game + 0x17f8);
    visual_reference((uint8_t *)visual_game + 0x1810);
    damage_lua.set_field(visual_lua, 1, "world");
    damage_lua.set_top(visual_lua, 0);
    static uint32_t object[50], object_node[6];
    static const VisualName object_name = {5, 5, UINT32_MAX, "first"};
    const char *identifier = object_name.bytes;
    object_node[0] = 1;
    object_node[1] = (uint32_t)(uintptr_t)((uint8_t *)visual_game + 0x17a8);
    object_node[4] = (uint32_t)(uintptr_t)object_name.bytes;
    object_node[5] = (uint32_t)(uintptr_t)object;
    visual_game[0x17ac / 4] = visual_game[0x17b0 / 4] = visual_game[0x17b4 / 4] =
        (uint32_t)(uintptr_t)object_node;
    visual_game[0x17b8 / 4] = 1;
    object_construct(visual_renderer);
    claim(visual_renderer);
    asset_construct(visual_asset);
    visual_renderer[2] = (uint32_t)(uintptr_t)visual_asset;
    visual_vtable[2] = (uint32_t)(uintptr_t)visual_get_sprite;
    visual_resources[0] = (uint32_t)(uintptr_t)visual_vtable;
    visual_game[0x50 / 4] = (uint32_t)(uintptr_t)visual_resources;
    semihost(4, "AB_ANDROID_OBJECT_VISUAL_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        visual_input[0] = (uint32_t)(item & 1);
        visual_input[1] = (uint32_t)((item / 2) & 1);
        visual_input[2] = (uint32_t)((item / 4) & 1);
        visual_input[3] = (uint32_t)((item / 8) % 5);
        visual_input[4] = (uint32_t)((item / 40) % 3);
        visual_input[5] = (uint32_t)((item / 120) & 3);
        visual_input[6] = (uint32_t)((item / 480) & 1);
        visual_recording = 0;
        const char *old_name = visual_names[5].bytes;
        asset_set(visual_asset, visual_resources, &old_name);
        visual_asset[3] = visual_input[5] & 1 ? (uint32_t)(uintptr_t)visual_regular : 0;
        visual_asset[4] = visual_input[5] & 2 ? (uint32_t)(uintptr_t)visual_compo : 0;
        object[49] = visual_input[0] ? (uint32_t)(uintptr_t)visual_renderer : 0;
        visual_composite = visual_input[2];
        visual_cache();
        visual_reference((uint8_t *)visual_game + 0x1810);
        damage_lua.create_table(visual_lua, 0, 1);
        if (visual_input[4] == 1) {
            damage_lua.push_boolean(visual_lua, 0);
            damage_lua.set_field(visual_lua, 2, "sprite");
        } else if (visual_input[4] == 2) {
            damage_lua.push_string(visual_lua, "old");
            damage_lua.set_field(visual_lua, 2, "sprite");
        }
        damage_lua.set_field(visual_lua, 1, "first");
        damage_lua.set_top(visual_lua, 0);
        print_words(visual_input, 7);
        for (size_t step = 0; step < 2; ++step) {
            visual_calls = 0;
            visual_snapshot[0] = visual_snapshot[1] = visual_snapshot[2] = 0;
            visual_recording = 1;
            const char *name = visual_names[visual_input[3]].bytes;
            object_set(visual_game, &identifier, &name);
            visual_recording = 0;
            uint32_t output[] = {4,
                                 visual_script(),
                                 visual_asset_name(),
                                 visual_asset[3] == (uint32_t)(uintptr_t)visual_regular,
                                 visual_asset[4] == (uint32_t)(uintptr_t)visual_compo,
                                 visual_calls,
                                 visual_snapshot[0],
                                 visual_snapshot[1],
                                 visual_snapshot[2],
                                 visual_composite};
            if ((visual_asset[3] && !output[3]) || (visual_asset[4] && !output[4])) {
                fail("Unexpected original visual resource binding\n");
            }
            print_words(output, 10);
        }
    }
    table_destroy((uint8_t *)visual_game + 0x1810);
    table_destroy((uint8_t *)visual_game + 0x17f8);
    release(wrapper);
    semihost(4, "AB object visual reference complete\n");
}

#endif
