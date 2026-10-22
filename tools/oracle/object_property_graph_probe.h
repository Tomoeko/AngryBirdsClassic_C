#ifndef AB_ORACLE_OBJECT_PROPERTY_GRAPH_H
#define AB_ORACLE_OBJECT_PROPERTY_GRAPH_H

static void property_graph_load_function(void *lua, const char *path,
                                         const char *global) {
    char chunk[512];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    int size = file < 0 ? -1 : system_call(3, file, chunk, sizeof(chunk));
    if (size <= 0 || size == sizeof(chunk) || system_call(6, file, NULL, 0) ||
        damage_lua.load(lua, chunk, (unsigned)size, "original-level-property"))
        fail("Cannot load original level property method\n");
    damage_lua.set_field(lua, -10002, global);
}

static void property_graph_bind_method(void *lua, int table_index, const char *global,
                                       const char *method) {
    damage_lua.get_field(lua, -10002, global);
    damage_lua.set_field(lua, table_index, method);
}

static void load_object_property_graph(void *lua) {
    property_graph_load_function(lua, "/data/local/tmp/ab520/camera/get-object.lua",
                                 "_ab_level_object_lookup");
    load_damage_chunk(lua,
                      "/data/local/tmp/ab520/collision/object-property-functions.lua");
    property_graph_load_function(
        lua, "/data/local/tmp/ab520/camera/get-object-definition.lua",
        "getObjectDefinition");
    property_graph_load_function(
        lua, "/data/local/tmp/ab520/camera/game-object-init.lua", "_ab_level_init");
    property_graph_load_function(
        lua, "/data/local/tmp/ab520/camera/game-object-get.lua", "_ab_level_get");
    property_graph_load_function(
        lua, "/data/local/tmp/ab520/camera/mode-object-get.lua", "_ab_level_mode_get");
    property_graph_load_function(lua,
                                 "/data/local/tmp/ab520/camera/museum-object-get.lua",
                                 "_ab_level_museum_get");
    damage_lua.create_table(lua, 0, 1);
    property_graph_bind_method(lua, 1, "_ab_level_museum_get", "getObjectProperty");
    damage_lua.set_field(lua, -10002, "_ab_level_museum");
    damage_lua.create_table(lua, 0, 2);
    property_graph_bind_method(lua, 1, "_ab_level_mode_get", "getObjectProperty");
    damage_lua.get_field(lua, -10002, "_ab_level_museum");
    damage_lua.set_field(lua, 1, "playMode");
    damage_lua.set_field(lua, -10002, "_ab_level_mode");
    damage_lua.get_field(lua, -10002, "gamelua");
    if (damage_lua.type(lua, 1) != 5) {
        damage_lua.set_top(lua, 0);
        damage_lua.create_table(lua, 0, 3);
    }
    damage_lua.get_field(lua, -10002, "_ab_level_mode");
    damage_lua.set_field(lua, 1, "modeSystem");
    property_graph_bind_method(lua, 1, "getObjectDefinition", "getObjectDefinition");
    damage_lua.set_field(lua, -10002, "gamelua");
}

#endif
