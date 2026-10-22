#ifndef AB_ORACLE_INITIAL_SPRITE_MATERIAL_H
#define AB_ORACLE_INITIAL_SPRITE_MATERIAL_H

#include "../../tests/initial_sprite_material_cases.h"

static uint32_t material_boundary;

static int material_bounds_stop(void *lua) {
    (void)lua;
    material_boundary = 1;
    return 0;
}

static int material_geometry_stop(void *lua) {
    (void)lua;
    material_boundary = 2;
    return 0;
}

static void material_source(void *lua, const char *source, size_t size,
                            const char *name, int results) {
    if (damage_lua.load(lua, source, (unsigned)size, name) ||
        damage_lua.call(lua, 0, results, 0)) {
        fail("Cannot prepare initial-sprite material oracle namespace\n");
    }
}

static void material_file(void *lua, const char *path, const char *name, int execute) {
    static char bytes[1024 * 1024];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        fail("Cannot open original material-sprite input\n");
    }
    size_t size = 0;
    while (size < sizeof(bytes)) {
        int count = system_call(3, file, bytes + size, sizeof(bytes) - size);
        if (count < 0) {
            fail("Cannot read original material-sprite input\n");
        }
        if (!count) {
            break;
        }
        size += (size_t)count;
    }
    if (size == sizeof(bytes) || system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, bytes, (unsigned)size, name)) {
        fail("Cannot load original material-sprite input\n");
    }
    if (execute) {
        if (damage_lua.call(lua, 0, 0, 0)) {
            fail("Cannot execute original material catalog\n");
        }
    } else {
        damage_lua.set_field(lua, -10002, name);
    }
}

static void material_scalar(void *lua, int index, uint32_t output[2]) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] != 0) {
        fail("Unexpected initial-sprite material scalar\n");
    }
}

static void material_definition(void *lua, const char *name) {
    damage_lua.get_field(lua, -10002, "blocks");
    damage_lua.get_field(lua, 1, name);
}

static void material_value(void *lua, const char *field) {
    damage_lua.get_field(lua, 1, field);
    uint32_t output[130] = {(uint32_t)damage_lua.type(lua, 2), 0};
    if (output[0] == 4) {
        unsigned size;
        const char *value = damage_lua.to_string(lua, 2, &size);
        if (!value || size > 128) {
            fail("Invalid original material-sprite name length\n");
        }
        output[1] = size;
        for (unsigned index = 0; index < size; ++index) {
            output[index + 2] = (uint8_t)value[index];
        }
    } else if (output[0] != 0) {
        fail("Unexpected original material-sprite selected type\n");
    }
    print_words(output, 2 + output[1]);
    damage_lua.set_top(lua, 1);
}

static void probe_initial_sprite_material(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original material-sprite state\n");
    }
    damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "_ab_material_bounds", material_bounds_stop);
    damage_function(lua, -10002, "_ab_material_geometry", material_geometry_stop);
    /* Controlled failures stay inside the original VM's C++ unwind boundary. */
    static const char namespace_source[] =
        "local function stop_bounds(...)\n"
        "    _ab_material_bounds()\n"
        "    error('Controlled resource boundary')\n"
        "end\n"
        "local function stop_geometry(...)\n"
        "    _ab_material_geometry()\n"
        "    error('Controlled geometry boundary')\n"
        "end\n"
        "res = {getSpriteBounds = stop_bounds, getSpritePivot = stop_bounds}\n"
        "gamelua = {blockTable = _G, objects = {world = {}}, physicsScale = 0.05,\n"
        "    createBox = stop_geometry, createCircle = stop_geometry,\n"
        "    createPolygon = stop_geometry, createChain = stop_geometry}\n";
    material_source(lua, namespace_source, sizeof(namespace_source) - 1,
                    "material-boundaries", 0);
    material_file(lua, "/data/local/tmp/ab520/material-create.lua",
                  "_ab_material_create", 0);
    material_file(lua, "/data/local/tmp/ab520/material-names.lua", "_ab_material_names",
                  0);
    material_file(lua, "/data/local/tmp/ab520/material-groups.lua",
                  "_ab_material_groups", 0);
    semihost(4, "AB_ANDROID_INITIAL_SPRITE_MATERIAL_V1 15\n");
    for (uint32_t sample = 0; sample < 15; ++sample) {
        material_file(lua, "/data/local/tmp/ab520/material-blocks.lua",
                      "material-catalog", 1);
        damage_lua.get_field(lua, -10002, "_ab_material_groups");
        damage_lua.get_field(lua, -10002, "_G");
        if (damage_lua.call(lua, 1, 0, 0)) {
            fail("Cannot prepare original catalog groups\n");
        }
        const char *name = initial_material_names[sample];
        uint32_t output[9] = {sample};
        material_definition(lua, name);
        damage_lua.get_field(lua, 2, "strength");
        material_scalar(lua, 3, output + 3);
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "materialName");
        const char *material = damage_lua.to_string(lua, 3, NULL);
        if (!material) {
            fail("Selected definition lacks its material name\n");
        }
        damage_lua.get_field(lua, -10002, "materials");
        damage_lua.get_field(lua, 4, material);
        damage_lua.get_field(lua, 5, "strength");
        material_scalar(lua, 6, output + 5);
        damage_lua.set_top(lua, 0);
        material_boundary = 0;
        damage_lua.get_field(lua, -10002, "_ab_material_create");
        damage_lua.get_field(lua, -10002, "_G");
        damage_lua.push_string(lua, name);
        damage_lua.push_number(lua, 1);
        damage_lua.push_number(lua, 2);
        damage_lua.push_number(lua, 0);
        damage_lua.push_nil(lua);
        output[1] = damage_lua.call(lua, 6, 1, 0) != 0;
        output[2] = material_boundary;
        if (!output[1] || !material_boundary) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error
                       : "Original constructor missed the controlled boundary\n");
        }
        damage_lua.set_top(lua, 0);
        material_definition(lua, name);
        damage_lua.get_field(lua, 2, "strength");
        material_scalar(lua, 3, output + 7);
        damage_lua.set_top(lua, 0);
        print_words(output, 9);
        damage_lua.get_field(lua, -10002, "_ab_material_names");
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_string(lua, name);
        damage_lua.set_field(lua, 2, "definition");
        damage_lua.get_field(lua, -10002, "blocks");
        damage_lua.get_field(lua, 3, name);
        damage_lua.get_field(lua, 4, "sprite");
        damage_lua.set_field(lua, 2, "sprite");
        damage_lua.get_field(lua, 4, "strength");
        damage_lua.set_field(lua, 2, "strength");
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, -10002, "blocks");
        if (damage_lua.call(lua, 2, 1, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error
                       : "Original normalized damage-sprite selection failed\n");
        }
        material_value(lua, "sprite");
        material_value(lua, "blink");
        material_value(lua, "smile");
        material_value(lua, "frozenSprite");
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB initial sprite material reference complete\n");
}

#endif
