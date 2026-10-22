#ifndef AB_ORACLE_ASSET_GROUP_H
#define AB_ORACLE_ASSET_GROUP_H

#include "../../tests/asset_group_cases.h"

static uint32_t asset_input[7], asset_events[8][5], asset_event_count;
static void (*asset_get_index)(void *, int, int);
static void (*asset_set_index)(void *, int, int);

static uint32_t asset_name_id(void *lua, int argument) {
    for (uint32_t index = 0; index < 5; ++index) {
        if (damage_named(lua, argument, asset_group_names[index])) {
            return index;
        }
    }
    fail("Unexpected original loaded asset group name\n");
    return UINT32_MAX;
}

static uint32_t asset_loaded(void *lua, uint32_t output[7], uint32_t *packed) {
    int top = damage_lua.get_top(lua), table = top + 1;
    damage_lua.get_field(lua, -10002, "loadedSpriteGroups");
    if (damage_lua.type(lua, table) != 5) {
        fail("Original loadedSpriteGroups is not a table\n");
    }
    uint32_t count = 0;
    *packed = 0;
    for (int index = 1; index <= 8; ++index) {
        asset_get_index(lua, table, index);
        if (damage_lua.type(lua, -1) == 0) {
            break;
        }
        if (index == 8) {
            fail("Original loaded asset groups exceeded controlled capacity\n");
        }
        uint32_t id = asset_name_id(lua, -1);
        if (output) {
            output[count] = id;
        }
        *packed |= (id + 1) << (count * 3);
        ++count;
        damage_lua.set_top(lua, table);
    }
    damage_lua.set_top(lua, top);
    return count;
}

static void asset_record(void *lua, uint32_t operation) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 4 ||
        asset_event_count == 8) {
        fail("Unexpected original asset resource boundary\n");
    }
    unsigned length = 0;
    const char *argument = damage_lua.to_string(lua, 1, &length);
    uint32_t *event = asset_events[asset_event_count++];
    event[0] = operation;
    event[1] = UINT32_C(2166136261);
    event[2] = length;
    for (size_t index = 0; index < length; ++index) {
        event[1] = (event[1] ^ (uint8_t)argument[index]) * UINT32_C(16777619);
    }
    event[3] = asset_loaded(lua, NULL, event + 4);
}

static int asset_select(void *lua) {
    asset_record(lua, 1);
    damage_lua.push_string(lua, asset_group_profiles[asset_input[3]]);
    return 1;
}

static int asset_load_list(void *lua) {
    asset_record(lua, 2);
    return 0;
}

static int asset_create_sheet(void *lua) {
    asset_record(lua, 3);
    return 0;
}

static int asset_release_sheet(void *lua) {
    asset_record(lua, 4);
    return 0;
}

static int asset_create_composites(void *lua) {
    asset_record(lua, 5);
    return 0;
}

static int asset_release_composites(void *lua) {
    asset_record(lua, 6);
    return 0;
}

static int asset_contains(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.type(lua, 2) != 4) {
        fail("Unexpected original group-list contains boundary\n");
    }
    uint32_t wanted = asset_name_id(lua, 2), found = 0;
    for (int index = 1; index <= 8; ++index) {
        asset_get_index(lua, 1, index);
        if (damage_lua.type(lua, -1) == 0) {
            break;
        }
        if (index == 8) {
            fail("Original group-list contains exceeded its bound\n");
        }
        found = asset_name_id(lua, -1) == wanted;
        damage_lua.set_top(lua, 2);
        if (found) {
            break;
        }
    }
    damage_lua.push_boolean(lua, (int)found);
    return 1;
}

static int asset_copy(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original group-list copy boundary\n");
    }
    damage_lua.create_table(lua, 0, 0);
    for (int index = 1; index <= 8; ++index) {
        asset_get_index(lua, 1, index);
        if (damage_lua.type(lua, -1) == 0) {
            damage_lua.set_top(lua, 2);
            break;
        }
        if (index == 8) {
            fail("Original group-list copy exceeded its bound\n");
        }
        (void)asset_name_id(lua, -1);
        asset_set_index(lua, 2, index);
    }
    return 1;
}

static void asset_prepare(void *lua, const uint32_t loaded[6],
                          const uint32_t kinds[4]) {
    damage_lua.set_top(lua, 0);
    if (asset_input[4] || !(asset_input[0] & 512u)) {
        damage_lua.create_table(lua, 0, 0);
        for (uint32_t index = 0; index < asset_input[4]; ++index) {
            damage_lua.push_string(lua, asset_group_names[loaded[index]]);
            asset_set_index(lua, 1, (int)index + 1);
        }
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "loadedSpriteGroups");
    damage_lua.push_string(lua, asset_input[0] & 256u ? "/data/images" : "images");
    damage_lua.set_field(lua, -10002, "imagePath");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    if (asset_input[6]) {
        damage_lua.create_table(lua, 0, 0);
        for (uint32_t index = 0; index < asset_input[5]; ++index) {
            damage_lua.create_table(lua, 0, 0);
            damage_lua.push_string(lua, asset_group_files[index]);
            asset_set_index(lua, 4, 1);
            damage_lua.push_number(lua, (float)kinds[index]);
            asset_set_index(lua, 4, 2);
            asset_set_index(lua, 3, (int)index + 1);
        }
        damage_lua.set_field(lua, 2, asset_group_names[asset_input[2]]);
    }
    damage_lua.set_field(lua, 1, asset_group_profiles[asset_input[3]]);
    damage_lua.set_field(lua, -10002, "loadlist");
}

static void probe_asset_group(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original asset-group Lua state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    open_table(lua);
    damage_lua.set_top(lua, 0);
    asset_get_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    asset_set_index =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_function(lua, -10002, "contains", asset_contains);
    damage_function(lua, -10002, "deepCopy", asset_copy);
    damage_function(lua, -10002, "loadLoadList", asset_load_list);
    damage_function(lua, -10002, "print", damage_require);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "selectAssetProfile", asset_select);
    damage_lua.set_field(lua, 1, "Profiles");
    damage_lua.set_field(lua, -10002, "platform");
    damage_lua.create_table(lua, 0, 4);
    damage_function(lua, 1, "createSpriteSheet", asset_create_sheet);
    damage_function(lua, 1, "releaseSpriteSheet", asset_release_sheet);
    damage_function(lua, 1, "createCompoSpriteSet", asset_create_composites);
    damage_function(lua, 1, "releaseCompoSpriteSet", asset_release_composites);
    damage_lua.set_field(lua, -10002, "res");
    const char *path = "/data/local/tmp/ab520/manage-assets.lua";
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    static char bytes[1029];
    size_t offset = 0;
    if (file < 0) {
        fail("Cannot open private original manageAssets prototype\n");
    }
    while (offset < sizeof(bytes)) {
        int count = system_call(3, file, bytes + offset, sizeof(bytes) - offset);
        if (count <= 0 || (size_t)count > sizeof(bytes) - offset) {
            fail("Incomplete private original manageAssets prototype\n");
        }
        offset += (size_t)count;
    }
    char extra;
    if (system_call(3, file, &extra, 1) != 0) {
        fail("Private manageAssets prototype exceeds its pinned extent\n");
    }
    (void)system_call(6, file, NULL, 0);
    if (damage_lua.load(lua, bytes, sizeof(bytes), "original-manageAssets")) {
        fail("Cannot load original manageAssets prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_manage_assets");
    semihost(4, "AB_ANDROID_ASSET_GROUP_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t loaded[6], kinds[4];
        asset_group_case(sample, asset_input, loaded, kinds);
        print_words(asset_input, 7);
        print_words(loaded, 6);
        print_words(kinds, 4);
        asset_prepare(lua, loaded, kinds);
        asset_event_count = 0;
        damage_lua.get_field(lua, -10002, "_ab_manage_assets");
        damage_lua.push_string(lua, asset_group_names[asset_input[2]]);
        damage_lua.push_boolean(lua, (int)asset_input[1]);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original asset-group management failed\n");
        }
        damage_lua.set_top(lua, 0);
        uint32_t final[7], packed;
        for (size_t index = 0; index < 7; ++index) {
            final[index] = UINT32_MAX;
        }
        uint32_t count = asset_loaded(lua, final, &packed);
        const uint32_t output[] = {asset_event_count, count};
        print_words(output, 2);
        print_words(final, 7);
        for (size_t index = 0; index < asset_event_count; ++index) {
            print_words(asset_events[index], 5);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB asset group reference complete\n");
}

#endif
