#ifndef AB_ORACLE_BLOCK_DAMAGE_H
#define AB_ORACLE_BLOCK_DAMAGE_H

/* Original Lua executes only in this isolated Android oracle. These bindings
 * provide controlled object metadata and record boundary calls; no original
 * bytecode, interpreter, or reference implementation enters ab_game. */
typedef struct {
    void *(*new_state)(void);
    void (*close_state)(void *);
    int (*open_base)(void *);
    int (*open_math)(void *);
    int (*load)(void *, const char *, unsigned, const char *);
    int (*call)(void *, int, int, int);
    void (*set_top)(void *, int);
    int (*get_top)(void *);
    void (*create_table)(void *, int, int);
    void (*get_field)(void *, int, const char *);
    void (*set_field)(void *, int, const char *);
    void (*push_number)(void *, float);
    void (*push_boolean)(void *, int);
    void (*push_string)(void *, const char *);
    void (*push_closure)(void *, int (*)(void *), int);
    float (*to_number)(void *, int);
    int (*to_boolean)(void *, int);
    const char *(*to_string)(void *, int, unsigned *);
    int (*type)(void *, int);
    const void *(*to_pointer)(void *, int);
    void (*push_nil)(void *);
} DamageLua;

static DamageLua damage_lua;
static uint32_t damage_flags[2];
static uint32_t damage_events[3][9];
static size_t damage_event_count;

static int damage_named(void *lua, int index, const char *wanted) {
    unsigned length = 0;
    const char *value = damage_lua.to_string(lua, index, &length);
    if (!value) {
        return 0;
    }
    unsigned offset = 0;
    while (wanted[offset]) {
        if (offset >= length || value[offset] != wanted[offset]) {
            return 0;
        }
        ++offset;
    }
    return offset == length;
}

static unsigned damage_object_id(void *lua, int index) {
    return damage_named(lua, index, "second") ? 2u : 1u;
}

static void damage_number(void *lua, int table, const char *field, uint32_t bits) {
    FloatBits value = {.bits = bits};
    damage_lua.push_number(lua, value.number);
    damage_lua.set_field(lua, table, field);
}

static void damage_function(void *lua, int table, const char *field,
                            int (*function)(void *)) {
    damage_lua.push_closure(lua, function, 0);
    damage_lua.set_field(lua, table, field);
}

static int damage_require(void *lua) {
    (void)lua;
    return 0;
}

static int damage_property(void *lua) {
    damage_lua.get_field(lua, 1, "_ab_flags");
    uint32_t flags = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, 2);
    uint32_t mask = damage_named(lua, 2, "ignoreGroundDamage")  ? 1u
                    : damage_named(lua, 2, "ignoreAllDamage")   ? 2u
                    : damage_named(lua, 2, "noCollisionDamage") ? 4u
                                                                : 0;
    damage_lua.push_boolean(lua, (flags & mask) != 0);
    return 1;
}

static int damage_definition(void *lua) {
    unsigned id = damage_object_id(lua, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, damage_flags[id - 1] & 32 ? 2.0f : 0.0f);
    damage_lua.set_field(lua, -2, "collider");
    return 1;
}

static int damage_break_joints(void *lua) {
    if (damage_event_count >= 3) {
        fail("Legacy collision boundary event overflow\n");
    }
    uint32_t *event = damage_events[damage_event_count++];
    event[0] = 1;
    event[1] = damage_object_id(lua, 1);
    FloatBits force = {.number = damage_lua.to_number(lua, 2)};
    event[3] = force.bits;
    return 0;
}

static int damage_block_callback(void *lua) {
    if (damage_event_count >= 3) {
        fail("Legacy block callback overflow\n");
    }
    uint32_t *event = damage_events[damage_event_count++];
    event[0] = 2;
    event[1] = damage_object_id(lua, 1);
    event[2] = damage_object_id(lua, 2);
    for (int argument = 3; argument <= 8; ++argument) {
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[(size_t)argument] = value.bits;
    }
    return 0;
}

static void load_damage_chunk(void *lua, const char *path) {
    static char bytes[65536];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        fail("Cannot open target collision-reference chunk\n");
    }
    size_t size = 0;
    while (size < sizeof(bytes)) {
        int count = system_call(3, file, bytes + size, sizeof(bytes) - size);
        if (count < 0) {
            fail("Cannot read target collision-reference chunk\n");
        }
        if (!count) {
            break;
        }
        size += (size_t)count;
    }
    if (size == sizeof(bytes) || system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, bytes, (unsigned)size, "collision-reference") ||
        damage_lua.call(lua, 0, 0, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original collision-reference chunk failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void initialize_damage_lua(void) {
    damage_lua = (DamageLua){
        .new_state = (void *(*)(void))find_function("_Z13luaL_newstatev"),
        .close_state = (void (*)(void *))find_function("_Z9lua_closeP9lua_State"),
        .open_base = (int (*)(void *))find_function("_Z12luaopen_baseP9lua_State"),
        .open_math = (int (*)(void *))find_function("_Z12luaopen_mathP9lua_State"),
        .load = (int (*)(void *, const char *, unsigned, const char *))find_function(
            "_Z15luaL_loadbufferP9lua_StatePKcjS2_"),
        .call =
            (int (*)(void *, int, int, int))find_function("_Z9lua_pcallP9lua_Stateiii"),
        .set_top = (void (*)(void *, int))find_function("_Z10lua_settopP9lua_Statei"),
        .get_top = (int (*)(void *))find_function("_Z10lua_gettopP9lua_State"),
        .create_table = (void (*)(void *, int, int))find_function(
            "_Z15lua_createtableP9lua_Stateii"),
        .get_field = (void (*)(void *, int, const char *))find_function(
            "_Z12lua_getfieldP9lua_StateiPKc"),
        .set_field = (void (*)(void *, int, const char *))find_function(
            "_Z12lua_setfieldP9lua_StateiPKc"),
        .push_number =
            (void (*)(void *, float))find_function("_Z14lua_pushnumberP9lua_Statef"),
        .push_boolean =
            (void (*)(void *, int))find_function("_Z15lua_pushbooleanP9lua_Statei"),
        .push_string = (void (*)(void *, const char *))find_function(
            "_Z14lua_pushstringP9lua_StatePKc"),
        .push_closure = (void (*)(void *, int (*)(void *), int))find_function(
            "_Z16lua_pushcclosureP9lua_StatePFiS0_Ei"),
        .to_number =
            (float (*)(void *, int))find_function("_Z12lua_tonumberP9lua_Statei"),
        .to_boolean =
            (int (*)(void *, int))find_function("_Z13lua_tobooleanP9lua_Statei"),
        .to_string = (const char *(*)(void *, int, unsigned *))find_function(
            "_Z13lua_tolstringP9lua_StateiPj"),
        .type = (int (*)(void *, int))find_function("_Z8lua_typeP9lua_Statei"),
        .to_pointer = (const void *(*)(void *, int))find_function(
            "_Z13lua_topointerP9lua_Statei"),
        .push_nil = (void (*)(void *))find_function("_Z11lua_pushnilP9lua_State"),
    };
}

static void prepare_damage_object(void *lua, const uint32_t input[7], unsigned id) {
    damage_lua.create_table(lua, 0, 7);
    int object = damage_lua.get_top(lua);
    damage_number(lua, object, "xVel", input[0]);
    damage_number(lua, object, "yVel", input[1]);
    damage_number(lua, object, "mass", input[2]);
    if (!(input[5] & 8)) {
        damage_number(lua, object, "strength", input[3]);
    }
    if (!(input[5] & 16)) {
        damage_number(lua, object, "defence", input[4]);
    }
    damage_lua.push_string(lua, id == 2         ? "second"
                                : input[5] & 64 ? "ground"
                                                : "first");
    damage_lua.set_field(lua, object, "name");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, -2, "get", damage_property);
    damage_lua.push_number(lua, (float)input[5]);
    damage_lua.set_field(lua, -2, "_ab_flags");
    damage_lua.set_field(lua, object, "gameObjectScript");
    damage_lua.set_field(lua, -10002, id == 1 ? "first" : "second");
}

static void prepare_damage_game(void *lua) {
    damage_lua.create_table(lua, 0, 4);
    damage_function(lua, -2, "getObjectDefinition", damage_definition);
    damage_function(lua, -2, "destroyBreakableJoints", damage_break_joints);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -2, "deadBlocks");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, 2);
    damage_lua.set_field(lua, -2, "static");
    damage_lua.set_field(lua, -2, "collider_types");
    damage_lua.set_field(lua, -2, "blockTable");
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void print_damage_results(void *lua) {
    uint32_t result[6] = {0};
    for (unsigned id = 1; id <= 2; ++id) {
        damage_lua.get_field(lua, -10002, id == 1 ? "first" : "second");
        damage_lua.get_field(lua, -1, "strength");
        result[(id - 1) * 2] = (uint32_t)damage_lua.type(lua, -1);
        FloatBits strength = {.number = damage_lua.to_number(lua, -1)};
        result[(id - 1) * 2 + 1] = strength.bits;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -1, "deadBlocks");
        damage_lua.get_field(lua, -1,
                             id == 2                ? "second"
                             : damage_flags[0] & 64 ? "ground"
                                                    : "first");
        result[3 + id] = damage_lua.type(lua, -1) == 5;
        damage_lua.set_top(lua, 0);
    }
    print_words(result, 6);
    uint32_t count = (uint32_t)damage_event_count;
    print_words(&count, 1);
    for (size_t event = 0; event < damage_event_count; ++event) {
        print_words(damage_events[event], 9);
    }
}

#if defined(AB_ORACLE_BIRD_DAMAGE_ONLY)
#include "bird_damage_probe.h"
#endif
#if defined(AB_ORACLE_BODY_VELOCITY_ONLY)
#include "body_velocity_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_FRICTION_ONLY)
#include "object_friction_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_ONLY)
#include "object_forces_probe.h"
#endif
#if defined(AB_ORACLE_FORCE_QUEUE_ONLY)
#include "force_queue_probe.h"
#endif
#if defined(AB_ORACLE_FORCE_POINT_ONLY)
#include "force_point_probe.h"
#endif
#if defined(AB_ORACLE_WATER_EFFECT_ONLY)
#include "water_effect_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_EVENT_ONLY)
#include "block_event_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_SCRIPT_ONLY)
#include "block_script_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_COMPONENT_ONLY)
#include "block_component_probe.h"
#endif
#if defined(AB_ORACLE_FLAME_EFFECT_ONLY)
#include "flame_effect_probe.h"
#endif
#if defined(AB_ORACLE_FLAME_DRAW_ONLY)
#include "flame_draw_probe.h"
#endif
#if defined(AB_ORACLE_FLAME_APPLY_ONLY)
#include "flame_apply_probe.h"
#endif
#if defined(AB_ORACLE_AMBIENT_ANIMATION_ONLY)
#include "ambient_animation_probe.h"
#endif
#if defined(AB_ORACLE_AMBIENT_PARTICLE_ONLY)
#include "ambient_particle_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_UPDATE_ONLY)
#include "block_update_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_UPDATE_ONLY)
#include "object_update_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_VISUAL_ONLY)
#include "object_visual_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_TRANSFORM_ONLY)
#include "object_transform_probe.h"
#endif
#if defined(AB_ORACLE_GAME_JOINT_ONLY)
#include "game_joint_probe.h"
#endif
#if defined(AB_ORACLE_GAME_JOINT_BREAK_ONLY)
#include "game_joint_break_probe.h"
#endif
#if defined(AB_ORACLE_GAME_JOINT_NAMED_ONLY)
#include "game_joint_named_probe.h"
#endif
#if defined(AB_ORACLE_GAME_POLYGON_ONLY)
#include "game_polygon_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_MISSING_ONLY)
#include "object_missing_probe.h"
#endif
#if defined(AB_ORACLE_ASSET_PROFILE_ONLY)
#include "asset_profile_probe.h"
#endif
#if defined(AB_ORACLE_ASSET_GROUP_ONLY)
#include "asset_group_probe.h"
#endif
#if defined(AB_ORACLE_ASSET_BUNDLE_ONLY)
#include "asset_bundle_probe.h"
#endif
#if defined(AB_ORACLE_THEME_ASSET_ONLY)
#include "theme_asset_probe.h"
#endif
#if defined(AB_ORACLE_INITIAL_SPRITE_MATERIAL_ONLY)
#include "initial_sprite_material_probe.h"
#endif
#if defined(AB_ORACLE_ASSET_SPLASH_ONLY)
#include "asset_splash_probe.h"
#endif
#if defined(AB_ORACLE_ASSET_PROFILE_BUNDLES_ONLY)
#include "asset_profile_bundles_probe.h"
#endif

#if defined(AB_ORACLE_GAME_CONTACT_ONLY)
#include "game_contact_probe.h"
#endif
#if defined(AB_ORACLE_GAME_CONTACT_WORLD_ONLY)
#include "game_contact_world_probe.h"
#endif
#if defined(AB_ORACLE_MUSEUM_GLUE_ONLY)
#include "museum_glue_probe.h"
#endif
#if defined(AB_ORACLE_BUBBLE_CONTACT_ONLY)
#include "bubble_contact_probe.h"
#endif
#if defined(AB_ORACLE_DAMAGE_PARTICLES_ONLY)
#include "damage_particles_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_COLLISION_ONLY)
#include "block_collision_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_NOTIFY_ONLY)
#include "bird_notify_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_PRELUDE_ONLY)
#include "bird_prelude_probe.h"
#endif
#if defined(AB_ORACLE_GAME_BEHAVIOR_ONLY)
#include "game_behavior_probe.h"
#endif
#if defined(AB_ORACLE_GAME_BEHAVIOR_ADD_ONLY)
#include "game_behavior_add_probe.h"
#endif
#if defined(AB_ORACLE_MUSEUM_BIRD_BEHAVIOR_ONLY)
#include "museum_bird_behavior_probe.h"
#endif
#if defined(AB_ORACLE_BLUES_TRAJECTORY_ONLY)
#include "blues_trajectory_probe.h"
#endif
#if defined(AB_ORACLE_EVENT_MANAGER_ONLY)
#include "event_manager_probe.h"
#endif
#if defined(AB_ORACLE_SETTINGS_STATISTICS_ONLY)
#include "settings_statistics_probe.h"
#endif
#if defined(AB_ORACLE_CLASSIC_SHOT_LISTENERS_ONLY)
#include "classic_shot_listeners_probe.h"
#endif
#if defined(AB_ORACLE_POWERUP_USES_ONLY)
#include "powerup_uses_probe.h"
#endif
#if defined(AB_ORACLE_TRAJECTORY_FOLLOW_ONLY)
#include "trajectory_follow_probe.h"
#endif
#if defined(AB_ORACLE_LUA_ASSET_SPRITE_ONLY)
#include "lua_asset_sprite_probe.h"
#endif
#if defined(AB_ORACLE_SCENE_ID_ONLY)
#include "scene_id_probe.h"
#endif
#if defined(AB_ORACLE_SCENE_MEMBERSHIP_ONLY)
#include "scene_membership_probe.h"
#endif
#if defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_ONLY)
#include "lua_asset_construct_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_PROPERTY_ACCESS_ONLY)
#include "object_property_access_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_DEFINITION_LOOKUP_ONLY)
#include "object_definition_lookup_probe.h"
#endif
#if defined(AB_ORACLE_LEVEL_PROPERTIES_ONLY)
#include "level_properties_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_MIGHTY_ONLY)
#include "bird_mighty_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_SPRITES_ONLY)
#include "bird_sprites_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_SPECIALTY_ONLY)
#include "bird_specialty_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_PROJECTILES_ONLY)
#include "bird_projectiles_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_COLLISION_DAMAGE_ONLY)
#include "bird_collision_damage_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_EFFECTS_ONLY)
#include "bird_effects_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_COLLISION_ONLY)
#include "bird_collision_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_REMOVAL_NOTIFY_ONLY)
#include "object_removal_notify_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_REMOVED_ONLY)
#include "object_removed_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_REMOVE_ONLY)
#include "bird_remove_probe.h"
#endif
#if defined(AB_ORACLE_LEVEL_EFFECT_BASE_ONLY)
#include "level_effect_base_probe.h"
#endif
#if defined(AB_ORACLE_BURN_MANAGER_ONLY)
#include "burn_manager_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_IMPACT_PREDICTION_ONLY)
#include "bird_impact_prediction_probe.h"
#endif
#if defined(AB_ORACLE_GAME_RAYCAST_ONLY)
#include "game_raycast_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_SMOOTHSTEP_ONLY)
#include "camera_smoothstep_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_FOLLOW_ONLY)
#include "camera_follow_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_ZOOM_FIT_ONLY)
#include "camera_zoom_fit_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY)
#include "camera_follow_system_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_CURSOR_ONLY)
#include "block_removal_cursor_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_SCORE_ONLY)
#include "block_removal_score_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_EFFECTS_ONLY)
#include "block_removal_effects_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_PARTICLES_ONLY)
#include "block_removal_particles_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_DETACH_ONLY)
#include "block_removal_detach_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_CLEANUP_ONLY)
#include "block_removal_cleanup_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_SPAWN_ONLY)
#include "block_removal_spawn_probe.h"
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_ONLY)
#include "game_object_remove_probe.h"
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_POLICY_ONLY)
#include "game_object_remove_policy_probe.h"
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_JOINTS_ONLY)
#include "game_object_remove_joints_probe.h"
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_TELEPORTERS_ONLY)
#include "game_object_remove_teleporters_probe.h"
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_PASS_ONLY)
#include "block_removal_pass_probe.h"
#endif
#if defined(AB_ORACLE_INTEGER_ORDER_ONLY) || defined(AB_ORACLE_MENU_POINTER_ONLY) ||   \
    defined(AB_ORACLE_SCORE_ONLY) || defined(AB_ORACLE_BIRD_JUMP_ONLY) ||              \
    defined(AB_ORACLE_LEVEL_FLOW_ONLY) || defined(AB_ORACLE_LEVEL_BONUS_ONLY) ||       \
    defined(AB_ORACLE_BIRD_RETIREMENT_ONLY) || defined(AB_ORACLE_LEVEL_ENDING_ONLY) || \
    defined(AB_ORACLE_LEVEL_COMPLETE_ONLY) ||                                          \
    defined(AB_ORACLE_CAMERA_VIEWPORT_ONLY) || defined(AB_ORACLE_CAMERA_MOVER_ONLY) || \
    defined(AB_ORACLE_CAMERA_LIMITS_ONLY) || defined(AB_ORACLE_CAMERA_SHAKE_ONLY) ||   \
    defined(AB_ORACLE_CAMERA_TWEENS_ONLY) ||                                           \
    defined(AB_ORACLE_CAMERA_SEQUENCE_ONLY) || defined(AB_ORACLE_CAMERA_DRAG_ONLY) ||  \
    defined(AB_ORACLE_CAMERA_SYSTEM_ONLY) || defined(AB_ORACLE_BIRD_ANIMATION_ONLY) || \
    defined(AB_ORACLE_GOAL_ANIMATION_ONLY) ||                                          \
    defined(AB_ORACLE_CHARACTER_ANIMATION_ONLY) ||                                     \
    defined(AB_ORACLE_DAMAGE_SPRITE_ONLY) || defined(AB_ORACLE_AUDIO_GROUP_ONLY) ||    \
    defined(AB_ORACLE_AUDIO_DEFINITION_ONLY) ||                                        \
    defined(AB_ORACLE_AUDIO_RESOURCES_ONLY) || defined(AB_ORACLE_AUDIO_PLAY_ONLY)
#include "selected_function.h"
#endif
#if defined(AB_ORACLE_MENU_POINTER_ONLY)
#include "menu_pointer_probe.h"
#endif
#if defined(AB_ORACLE_INTEGER_ORDER_ONLY)
#include "integer_order_probe.h"
#endif
#if defined(AB_ORACLE_UI_FRAME_GEOMETRY_ONLY)
#include "selected_function.h"
#include "ui_frame_geometry_probe.h"
#endif
#if defined(AB_ORACLE_UI_POINTER_ONLY)
#include "selected_function.h"
#include "ui_pointer_probe.h"
#endif
#if defined(AB_ORACLE_MAIN_MENU_THEME_ONLY)
#include "main_menu_theme_probe.h"
#endif
#if defined(AB_ORACLE_MAIN_MENU_SETTINGS_ONLY)
#include "main_menu_settings_probe.h"
#endif
#if defined(AB_ORACLE_THEME_LAYER_ONLY)
#if defined(AB_ORACLE_THEME_LAYER_CONSTRUCT)
#include "theme_layer_construct_probe.h"
#else
#include "theme_layer_probe.h"
#endif
#endif
#if defined(AB_ORACLE_COMPOSITE_BOUNDS_ONLY)
#if defined(AB_ORACLE_COMPOSITE_DRAW_ONLY)
#include "composite_draw_probe.h"
#else
#include "composite_bounds_probe.h"
#endif
#endif
#if defined(AB_ORACLE_THEME_BACKGROUND_ONLY)
#include "theme_background_probe.h"
#endif
#if defined(AB_ORACLE_GAME_RECTANGLE_ONLY)
#include "game_rectangle_probe.h"
#endif
#if defined(AB_ORACLE_RUBBERBAND_ONLY) || defined(AB_ORACLE_RUBBERBAND_WORLD_ONLY) ||  \
    defined(AB_ORACLE_SLING_RECTANGLE_ONLY)
#include "rubberband_probe.h"
#endif
#if defined(AB_ORACLE_SLING_RECTANGLE_ONLY)
#include "sling_rectangle_probe.h"
#endif
#if defined(AB_ORACLE_GAME_HUD_DRAW_ONLY)
#include "game_hud_draw_probe.h"
#endif
#if defined(AB_ORACLE_RUBBERBAND_WORLD_ONLY)
#include "selected_function.h"
#include "rubberband_world_probe.h"
#endif
#if defined(AB_ORACLE_THEME_FOREGROUND_ONLY)
#include "theme_foreground_probe.h"
#endif
#if defined(AB_ORACLE_THEME_COLORS_ONLY)
#include "theme_colors_probe.h"
#endif
#if defined(AB_ORACLE_THEME_SCENE_ONLY)
#include "theme_scene_probe.h"
#endif
#if defined(AB_ORACLE_IMAGE_MATERIAL_ONLY)
#include "image_material_probe.h"
#endif
#if defined(AB_ORACLE_FILL_RECTANGLE_ONLY)
#include "fill_rectangle_probe.h"
#endif
#if defined(AB_ORACLE_UI_IMAGE_HIT_ONLY)
#include "selected_function.h"
#include "ui_image_hit_probe.h"
#endif
#if defined(AB_ORACLE_UI_IMAGE_BUTTON_ONLY)
#include "selected_function.h"
#include "ui_image_button_probe.h"
#endif
#if defined(AB_ORACLE_UI_IMAGE_ANIMATION_ONLY)
#include "selected_function.h"
#include "ui_image_animation_probe.h"
#endif
#if defined(AB_ORACLE_UI_FRAME_DRAW_ONLY)
#include "selected_function.h"
#include "ui_frame_draw_probe.h"
#endif
#if defined(AB_ORACLE_UI_IMAGE_DRAW_ONLY)
#include "selected_function.h"
#include "ui_image_draw_probe.h"
#endif
#if defined(AB_ORACLE_UI_IMAGE_RESOURCES_ONLY)
#include "selected_function.h"
#include "ui_image_resources_probe.h"
#endif
#if defined(AB_ORACLE_UI_LOCALIZATION_ONLY)
#include "selected_function.h"
#include "ui_localization_probe.h"
#endif
#if defined(AB_ORACLE_UI_INIT_ONLY)
#include "selected_function.h"
#include "ui_init_probe.h"
#endif
#if defined(AB_ORACLE_UI_FONT_ONLY)
#include "selected_function.h"
#include "ui_font_probe.h"
#endif
#if defined(AB_ORACLE_UI_VALUE_TEXT_ONLY)
#include "ui_value_text_probe.h"
#endif
#if defined(AB_ORACLE_UI_FRAME_TREE_ONLY)
#include "selected_function.h"
#include "ui_frame_tree_probe.h"
#endif
#if defined(AB_ORACLE_UI_IMAGE_STATE_ONLY)
#include "ui_image_state_probe.h"
#endif
#if defined(AB_ORACLE_UI_TEXT_ONLY)
#include "ui_text_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_JUMP_ONLY)
#include "bird_jump_probe.h"
#endif
#if defined(AB_ORACLE_SCORE_ONLY)
#include "score_probe.h"
#endif
#if defined(AB_ORACLE_LEVEL_FLOW_ONLY)
#include "level_flow_probe.h"
#endif
#if defined(AB_ORACLE_LEVEL_BONUS_ONLY)
#include "level_bonus_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_RETIREMENT_ONLY)
#include "bird_retirement_probe.h"
#endif
#if defined(AB_ORACLE_LEVEL_ENDING_ONLY)
#include "level_ending_probe.h"
#endif
#if defined(AB_ORACLE_LEVEL_COMPLETE_ONLY)
#include "level_complete_probe.h"
#endif
#if defined(AB_ORACLE_COORDINATES_ONLY)
#include "coordinates_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_VIEWPORT_ONLY)
#include "camera_viewport_probe.h"
#endif
#if defined(AB_ORACLE_FONT_METRICS_ONLY) || defined(AB_ORACLE_FONT_LAYOUT_ONLY)
#include "font_metrics_probe.h"
#endif
#if defined(AB_ORACLE_FONT_LAYOUT_ONLY)
#include "font_layout_probe.h"
#endif
#if defined(AB_ORACLE_UTF8_ONLY)
#include "utf8_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_SPRITE_ONLY)
#include "object_sprite_probe.h"
#endif
#if defined(AB_ORACLE_IMAGE_RECTANGLE_ONLY)
#include "image_rectangle_probe.h"
#endif
#if defined(AB_ORACLE_SPRITE_SIZED_ONLY)
#include "sprite_sized_probe.h"
#endif
#if defined(AB_ORACLE_STRING_MAP_ONLY)
#include "string_map_probe.h"
#endif
#if defined(AB_ORACLE_UI_MENU_MANAGER_ONLY)
#include "ui_menu_manager_probe.h"
#endif
#if defined(AB_ORACLE_OBJECT_RENDER_ONLY)
#include "object_render_probe.h"
#endif
#if defined(AB_ORACLE_RANDOM_ONLY)
#include "random_probe.h"
#endif
#if defined(AB_ORACLE_PARTICLE_BOX_ONLY)
#include "particle_box_probe.h"
#endif
#if defined(AB_ORACLE_BIRD_ANIMATION_ONLY)
#include "bird_animation_probe.h"
#endif
#if defined(AB_ORACLE_GOAL_ANIMATION_ONLY)
#include "goal_animation_probe.h"
#endif
#if defined(AB_ORACLE_CHARACTER_ANIMATION_ONLY)
#include "character_animation_probe.h"
#endif
#if defined(AB_ORACLE_DAMAGE_SPRITE_ONLY)
#include "damage_sprite_probe.h"
#endif
#if defined(AB_ORACLE_AUDIO_GROUP_ONLY)
#include "audio_group_probe.h"
#endif
#if defined(AB_ORACLE_AUDIO_DEFINITION_ONLY)
#include "audio_definition_probe.h"
#endif
#if defined(AB_ORACLE_AUDIO_RESOURCES_ONLY)
#include "audio_resources_probe.h"
#endif
#if defined(AB_ORACLE_AUDIO_PLAY_ONLY)
#include "audio_play_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_MOVER_ONLY) || defined(AB_ORACLE_CAMERA_TWEENS_ONLY)
#include "camera_mover_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_TWEENS_ONLY)
#include "camera_tweens_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_LIMITS_ONLY)
#include "camera_limits_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_SEQUENCE_ONLY)
#include "camera_sequence_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_SYSTEM_ONLY)
#include "camera_system_probe.h"
#if defined(AB_ORACLE_CAMERA_TOUCH_ONLY)
#include "camera_touch_probe.h"
#endif
#endif
#if defined(AB_ORACLE_CAMERA_SHAKE_ONLY)
#include "camera_shake_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_DRAG_ONLY)
#include "camera_drag_probe.h"
#endif
#if defined(AB_ORACLE_SCRIPT_TRIG_ONLY)
#include "script_trig_probe.h"
#endif
#if defined(AB_ORACLE_CAMERA_SELECTION_ONLY)
#include "selected_function.h"
#include "camera_selection_probe.h"
#endif
#if defined(AB_ORACLE_AUDIO_CATALOG_ONLY) || defined(AB_ORACLE_AUDIO_ASSETS_ONLY)
#include "audio_catalog_probe.h"
#endif

#if defined(AB_ORACLE_GAME_DRAW_ONLY)
#include "selected_function.h"
#include "game_draw_probe.h"
#endif
#if defined(AB_ORACLE_PROCESS_MANAGER_ONLY)
#include "process_manager_probe.h"
#endif
#if defined(AB_ORACLE_LOADER_ONLY)
#include "selected_function.h"
#include "loader_probe.h"
#endif
#if defined(AB_ORACLE_ROOT_ONLY)
#include "selected_function.h"
#include "root_probe.h"
#endif
#if defined(AB_ORACLE_AUDIO_RAMP_ONLY)
#include "selected_function.h"
#include "audio_ramp_probe.h"
#endif
#if defined(AB_ORACLE_URL_REQUESTS_ONLY)
#include "url_requests_probe.h"
#endif
#if defined(AB_ORACLE_CALENDAR_TIME_ONLY)
#include "calendar_time_probe.h"
#endif
#if defined(AB_ORACLE_UI_SLIDER_ONLY)
#include "selected_function.h"
#include "ui_slider_probe.h"
#endif
#if defined(AB_ORACLE_SESSION_TIMER_ONLY)
#include "selected_function.h"
#include "session_timer_probe.h"
#endif
#if defined(AB_ORACLE_ANGER_ONLY)
#include "selected_function.h"
#include "anger_probe.h"
#endif
#if defined(AB_ORACLE_ROOT_QUERIES_ONLY)
#include "selected_function.h"
#include "root_queries_probe.h"
#endif
#if defined(AB_ORACLE_GUI_QUERIES_ONLY)
#include "selected_function.h"
#include "gui_queries_probe.h"
#endif
#if defined(AB_ORACLE_LEVEL_START_FLOW_ONLY)
#include "selected_function.h"
#include "level_start_flow_probe.h"
#endif
#if defined(AB_ORACLE_MUSEUM_PAUSE_ONLY)
#include "selected_function.h"
#include "museum_pause_probe.h"
#endif
#if defined(AB_ORACLE_GAME_MODE_ONLY)
#include "selected_function.h"
#include "game_mode_probe.h"
#endif
#if defined(AB_ORACLE_CREATE_CIRCLE_ONLY)
#include "create_circle_probe.h"
#endif
#if defined(AB_ORACLE_BLUES_CHILD_ONLY) || defined(AB_ORACLE_BLUES_SPLIT_ONLY)
#include "blues_child_probe.h"
#endif
#if defined(AB_ORACLE_BLUES_SPLIT_ONLY)
#include "blues_split_probe.h"
#endif
#if defined(AB_ORACLE_WORLD_ITERATION_ONLY)
#include "world_iteration_probe.h"
#endif
#if defined(AB_ORACLE_RELOAD_GEOMETRY_ONLY)
#include "selected_function.h"
#include "reload_geometry_probe.h"
#endif

static void probe_block_damage(void) {
#if defined(AB_ORACLE_WORLD_ITERATION_ONLY)
    probe_world_iteration();
    return;
#endif
#if defined(AB_ORACLE_BLUES_CHILD_ONLY) || defined(AB_ORACLE_BLUES_SPLIT_ONLY)
    probe_blues_child();
    return;
#endif
#if defined(AB_ORACLE_CREATE_CIRCLE_ONLY)
    probe_create_circle();
    return;
#endif
#if defined(AB_ORACLE_RELOAD_GEOMETRY_ONLY)
    probe_reload_geometry();
    return;
#endif
#if defined(AB_ORACLE_GAME_MODE_ONLY)
    probe_game_mode();
    return;
#endif
#if defined(AB_ORACLE_MUSEUM_PAUSE_ONLY)
    probe_museum_pause();
    return;
#endif
#if defined(AB_ORACLE_LEVEL_START_FLOW_ONLY)
    probe_level_start_flow();
    return;
#endif
#if defined(AB_ORACLE_GUI_QUERIES_ONLY)
    probe_gui_queries();
    return;
#endif
#if defined(AB_ORACLE_ROOT_QUERIES_ONLY)
    probe_root_queries();
    return;
#endif
#if defined(AB_ORACLE_ANGER_ONLY)
    probe_anger();
    return;
#endif
#if defined(AB_ORACLE_SESSION_TIMER_ONLY)
    probe_session_timer();
    return;
#endif
#if defined(AB_ORACLE_CALENDAR_TIME_ONLY)
    probe_calendar_time();
    return;
#endif
#if defined(AB_ORACLE_UI_SLIDER_ONLY)
    probe_ui_slider();
    return;
#endif
#if defined(AB_ORACLE_URL_REQUESTS_ONLY)
    probe_url_requests();
    return;
#endif
#if defined(AB_ORACLE_AUDIO_RAMP_ONLY)
    probe_audio_ramp();
    return;
#endif
#if defined(AB_ORACLE_ROOT_ONLY)
    probe_root();
    return;
#endif
#if defined(AB_ORACLE_LOADER_ONLY)
    probe_loader();
    return;
#endif
#if defined(AB_ORACLE_PROCESS_MANAGER_ONLY)
    probe_process_manager();
    return;
#endif
#if defined(AB_ORACLE_GAME_DRAW_ONLY)
    probe_game_draw();
    return;
#endif
#if defined(AB_ORACLE_ASSET_PROFILE_BUNDLES_ONLY)
    probe_asset_profile_bundles();
    return;
#endif
#if defined(AB_ORACLE_ASSET_SPLASH_ONLY)
    probe_asset_splash();
    return;
#endif
#if defined(AB_ORACLE_ASSET_BUNDLE_ONLY)
    probe_asset_bundle();
    return;
#endif
#if defined(AB_ORACLE_THEME_ASSET_ONLY)
    probe_theme_asset();
    return;
#endif
#if defined(AB_ORACLE_INITIAL_SPRITE_MATERIAL_ONLY)
    probe_initial_sprite_material();
    return;
#endif
#if defined(AB_ORACLE_ASSET_GROUP_ONLY)
    probe_asset_group();
    return;
#endif
#if defined(AB_ORACLE_ASSET_PROFILE_ONLY)
    probe_asset_profile();
    return;
#endif
#if defined(AB_ORACLE_GAME_JOINT_NAMED_ONLY)
    probe_game_joint_named();
    return;
#endif
#if defined(AB_ORACLE_GAME_JOINT_BREAK_ONLY)
    probe_game_joint_break();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_MISSING_ONLY)
    probe_object_missing();
    return;
#endif
#if defined(AB_ORACLE_GAME_POLYGON_ONLY)
    probe_game_polygon();
    return;
#endif
#if defined(AB_ORACLE_GAME_JOINT_ONLY)
    probe_game_joint();
    return;
#endif
#if defined(AB_ORACLE_GAME_HUD_DRAW_ONLY)
    probe_game_hud_draw();
    return;
#endif
#if defined(AB_ORACLE_SLING_RECTANGLE_ONLY)
    probe_sling_rectangle();
    return;
#endif
#if defined(AB_ORACLE_UI_IMAGE_STATE_ONLY)
    probe_ui_image_state();
    return;
#endif
#if defined(AB_ORACLE_UI_FRAME_TREE_ONLY)
    probe_ui_frame_tree();
    return;
#endif
#if defined(AB_ORACLE_UI_VALUE_TEXT_ONLY)
    probe_ui_value_text();
    return;
#endif
#if defined(AB_ORACLE_UI_FONT_ONLY)
    probe_ui_font();
    return;
#endif
#if defined(AB_ORACLE_UI_INIT_ONLY)
    probe_ui_init();
    return;
#endif
#if defined(AB_ORACLE_UI_TEXT_ONLY)
    probe_ui_text();
    return;
#endif
#if defined(AB_ORACLE_UI_LOCALIZATION_ONLY)
    probe_ui_localization();
    return;
#endif
#if defined(AB_ORACLE_UI_IMAGE_RESOURCES_ONLY)
    probe_ui_image_resources();
    return;
#endif
#if defined(AB_ORACLE_UI_IMAGE_DRAW_ONLY)
    probe_ui_image_draw();
    return;
#endif
#if defined(AB_ORACLE_UI_FRAME_DRAW_ONLY)
    probe_ui_frame_draw();
    return;
#endif
#if defined(AB_ORACLE_UI_IMAGE_ANIMATION_ONLY)
    probe_ui_image_animation();
    return;
#endif
#if defined(AB_ORACLE_UI_IMAGE_BUTTON_ONLY)
    probe_ui_image_button();
    return;
#endif
#if defined(AB_ORACLE_UI_IMAGE_HIT_ONLY)
    probe_ui_image_hit();
    return;
#endif
#if defined(AB_ORACLE_UI_POINTER_ONLY)
    probe_ui_pointer();
    return;
#endif
#if defined(AB_ORACLE_MAIN_MENU_THEME_ONLY)
    probe_main_menu_theme();
    return;
#endif
#if defined(AB_ORACLE_MAIN_MENU_SETTINGS_ONLY)
    probe_main_menu_settings();
    return;
#endif
#if defined(AB_ORACLE_THEME_LAYER_ONLY)
    probe_theme_layer();
    return;
#endif
#if defined(AB_ORACLE_COMPOSITE_BOUNDS_ONLY)
#if defined(AB_ORACLE_COMPOSITE_DRAW_ONLY)
    probe_composite_draw();
#else
    probe_composite_bounds();
#endif
    return;
#endif
#if defined(AB_ORACLE_THEME_BACKGROUND_ONLY)
    probe_theme_background();
    return;
#endif
#if defined(AB_ORACLE_GAME_RECTANGLE_ONLY)
    probe_game_rectangle();
    return;
#endif
#if defined(AB_ORACLE_RUBBERBAND_ONLY)
    probe_rubberband();
    return;
#endif
#if defined(AB_ORACLE_RUBBERBAND_WORLD_ONLY)
    probe_rubberband_world();
    return;
#endif
#if defined(AB_ORACLE_THEME_FOREGROUND_ONLY)
    probe_theme_foreground();
    return;
#endif
#if defined(AB_ORACLE_THEME_COLORS_ONLY)
    probe_theme_colors();
    return;
#endif
#if defined(AB_ORACLE_THEME_SCENE_ONLY)
    probe_theme_scene();
    return;
#endif
#if defined(AB_ORACLE_IMAGE_MATERIAL_ONLY)
    probe_image_material();
    return;
#endif
#if defined(AB_ORACLE_FILL_RECTANGLE_ONLY)
    probe_fill_rectangle();
    return;
#endif
#if defined(AB_ORACLE_UI_FRAME_GEOMETRY_ONLY)
    probe_ui_frame_geometry();
    return;
#endif
#if defined(AB_ORACLE_INTEGER_ORDER_ONLY)
    probe_integer_order();
    return;
#endif
#if defined(AB_ORACLE_MENU_POINTER_ONLY)
    probe_menu_pointer();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_SELECTION_ONLY)
    probe_camera_selection();
    return;
#endif
#if defined(AB_ORACLE_SCRIPT_TRIG_ONLY)
    probe_script_trig();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_TWEENS_ONLY)
    probe_camera_tweens();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_DRAG_ONLY)
    probe_camera_drag();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_SHAKE_ONLY)
    probe_camera_shake();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_SYSTEM_ONLY)
#if defined(AB_ORACLE_CAMERA_TOUCH_ONLY)
    probe_camera_touch();
#else
    probe_camera_system();
#endif
    return;
#endif
#if defined(AB_ORACLE_CAMERA_SEQUENCE_ONLY)
    probe_camera_sequence();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_LIMITS_ONLY)
    probe_camera_limits();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_MOVER_ONLY)
    probe_camera_mover();
    return;
#endif
#if defined(AB_ORACLE_AUDIO_PLAY_ONLY)
    probe_audio_play();
    return;
#endif
#if defined(AB_ORACLE_AUDIO_RESOURCES_ONLY)
    probe_audio_resources();
    return;
#endif
#if defined(AB_ORACLE_AUDIO_DEFINITION_ONLY)
    probe_audio_definition();
    return;
#endif
#if defined(AB_ORACLE_AUDIO_ASSETS_ONLY)
    probe_audio_assets();
    return;
#endif
#if defined(AB_ORACLE_AUDIO_CATALOG_ONLY)
    probe_audio_catalog();
    return;
#endif
#if defined(AB_ORACLE_AUDIO_GROUP_ONLY)
    probe_audio_group();
    return;
#endif
#if defined(AB_ORACLE_DAMAGE_SPRITE_ONLY)
    probe_damage_sprite();
    return;
#endif
#if defined(AB_ORACLE_CHARACTER_ANIMATION_ONLY)
    probe_character_animation();
    return;
#endif
#if defined(AB_ORACLE_GOAL_ANIMATION_ONLY)
    probe_goal_animation();
    return;
#endif
#if defined(AB_ORACLE_BIRD_ANIMATION_ONLY)
    probe_bird_animation();
    return;
#endif
#if defined(AB_ORACLE_RANDOM_ONLY)
    probe_random();
    return;
#endif
#if defined(AB_ORACLE_PARTICLE_BOX_ONLY)
    probe_particle_box();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_RENDER_ONLY)
    probe_object_render();
    return;
#endif
#if defined(AB_ORACLE_IMAGE_RECTANGLE_ONLY)
    probe_image_rectangle();
    return;
#endif
#if defined(AB_ORACLE_SPRITE_SIZED_ONLY)
    probe_sprite_sized();
    return;
#endif
#if defined(AB_ORACLE_STRING_MAP_ONLY)
    probe_string_map();
    return;
#endif
#if defined(AB_ORACLE_UI_MENU_MANAGER_ONLY)
    probe_ui_menu_manager();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_SPRITE_ONLY)
    probe_object_sprite();
    return;
#endif
#if defined(AB_ORACLE_UTF8_ONLY)
    probe_utf8();
    return;
#endif
#if defined(AB_ORACLE_FONT_LAYOUT_ONLY)
    probe_font_layout();
    return;
#endif
#if defined(AB_ORACLE_FONT_METRICS_ONLY)
    probe_font_metrics();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_VIEWPORT_ONLY)
    probe_camera_viewport();
    return;
#endif
#if defined(AB_ORACLE_COORDINATES_ONLY)
    probe_coordinates();
    return;
#endif
#if defined(AB_ORACLE_LEVEL_COMPLETE_ONLY)
    probe_level_complete();
    return;
#endif
#if defined(AB_ORACLE_LEVEL_ENDING_ONLY)
    probe_level_ending();
    return;
#endif
#if defined(AB_ORACLE_BIRD_RETIREMENT_ONLY)
    probe_bird_retirement();
    return;
#endif
#if defined(AB_ORACLE_LEVEL_BONUS_ONLY)
    probe_level_bonus();
    return;
#endif
#if defined(AB_ORACLE_LEVEL_FLOW_ONLY)
    probe_level_flow();
    return;
#endif
#if defined(AB_ORACLE_BIRD_JUMP_ONLY)
    probe_bird_jump();
    return;
#endif
#if defined(AB_ORACLE_SCORE_ONLY)
    probe_score();
    return;
#endif
#if defined(AB_ORACLE_GAME_CONTACT_ONLY)
    probe_game_contact();
    return;
#endif
#if defined(AB_ORACLE_GAME_CONTACT_WORLD_ONLY)
    probe_game_contact_world();
    return;
#endif
#if defined(AB_ORACLE_MUSEUM_GLUE_ONLY)
    probe_museum_glue();
    return;
#endif
#if defined(AB_ORACLE_BUBBLE_CONTACT_ONLY)
    probe_bubble_contact();
    return;
#endif
#if defined(AB_ORACLE_DAMAGE_PARTICLES_ONLY)
    probe_damage_particles();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_COLLISION_ONLY)
    probe_block_collision();
    return;
#endif
#if defined(AB_ORACLE_BIRD_NOTIFY_ONLY)
    probe_bird_notify();
    return;
#endif
#if defined(AB_ORACLE_BIRD_PRELUDE_ONLY)
    probe_bird_prelude();
    return;
#endif
#if defined(AB_ORACLE_GAME_BEHAVIOR_ONLY)
    probe_game_behavior();
    return;
#endif
#if defined(AB_ORACLE_GAME_BEHAVIOR_ADD_ONLY)
    probe_game_behavior_add();
    return;
#endif
#if defined(AB_ORACLE_MUSEUM_BIRD_BEHAVIOR_ONLY)
    probe_museum_bird_behavior();
    return;
#endif
#if defined(AB_ORACLE_BLUES_TRAJECTORY_ONLY)
    probe_blues_trajectory();
    return;
#endif
#if defined(AB_ORACLE_EVENT_MANAGER_ONLY)
    probe_event_manager();
#elif defined(AB_ORACLE_SETTINGS_STATISTICS_ONLY)
    probe_settings_statistics();
#elif defined(AB_ORACLE_CLASSIC_SHOT_LISTENERS_ONLY)
    probe_classic_shot_listeners();
#elif defined(AB_ORACLE_POWERUP_USES_ONLY)
    probe_powerup_uses();
    return;
#endif
#if defined(AB_ORACLE_TRAJECTORY_FOLLOW_ONLY)
    probe_trajectory_follow();
    return;
#endif
#if defined(AB_ORACLE_LUA_ASSET_SPRITE_ONLY)
    probe_lua_asset_sprite();
    return;
#endif
#if defined(AB_ORACLE_SCENE_ID_ONLY)
    probe_scene_id();
    return;
#endif
#if defined(AB_ORACLE_SCENE_MEMBERSHIP_ONLY)
    probe_scene_membership();
    return;
#endif
#if defined(AB_ORACLE_LUA_ASSET_CONSTRUCT_ONLY)
    probe_lua_asset_construct();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_PROPERTY_ACCESS_ONLY)
    probe_object_property_access();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_DEFINITION_LOOKUP_ONLY)
    probe_object_definition_lookup();
    return;
#endif
#if defined(AB_ORACLE_LEVEL_PROPERTIES_ONLY)
    probe_level_properties();
    return;
#endif
#if defined(AB_ORACLE_BIRD_MIGHTY_ONLY)
    probe_bird_mighty();
    return;
#endif
#if defined(AB_ORACLE_BIRD_SPRITES_ONLY)
    probe_bird_sprites();
    return;
#endif
#if defined(AB_ORACLE_BIRD_SPECIALTY_ONLY)
    probe_bird_specialty();
    return;
#endif
#if defined(AB_ORACLE_BIRD_PROJECTILES_ONLY)
    probe_bird_projectiles();
    return;
#endif
#if defined(AB_ORACLE_BIRD_COLLISION_DAMAGE_ONLY)
    probe_bird_collision_damage();
    return;
#endif
#if defined(AB_ORACLE_BIRD_EFFECTS_ONLY)
    probe_bird_effects();
    return;
#endif
#if defined(AB_ORACLE_BIRD_COLLISION_ONLY)
    probe_bird_collision();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_REMOVAL_NOTIFY_ONLY)
    probe_object_removal_notify();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_REMOVED_ONLY)
    probe_object_removed();
    return;
#endif
#if defined(AB_ORACLE_BIRD_REMOVE_ONLY)
    probe_bird_remove();
    return;
#endif
#if defined(AB_ORACLE_LEVEL_EFFECT_BASE_ONLY)
    probe_level_effect_base();
    return;
#endif
#if defined(AB_ORACLE_BURN_MANAGER_ONLY)
    probe_burn_manager();
    return;
#endif
#if defined(AB_ORACLE_BIRD_IMPACT_PREDICTION_ONLY)
    probe_bird_impact_prediction();
    return;
#endif
#if defined(AB_ORACLE_GAME_RAYCAST_ONLY)
    probe_game_raycast();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_SMOOTHSTEP_ONLY)
    probe_camera_smoothstep();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_FOLLOW_ONLY)
    probe_camera_follow();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_ZOOM_FIT_ONLY)
    probe_camera_zoom_fit();
    return;
#endif
#if defined(AB_ORACLE_CAMERA_FOLLOW_SYSTEM_ONLY)
    probe_camera_follow_system();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_CURSOR_ONLY)
    probe_block_removal_cursor();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_SCORE_ONLY)
    probe_block_removal_score();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_EFFECTS_ONLY)
    probe_block_removal_effects();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_PARTICLES_ONLY)
    probe_block_removal_particles();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_DETACH_ONLY)
    probe_block_removal_detach();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_CLEANUP_ONLY)
    probe_block_removal_cleanup();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_SPAWN_ONLY)
    probe_block_removal_spawn();
    return;
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_ONLY)
    probe_game_object_remove();
    return;
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_POLICY_ONLY)
    probe_game_object_remove_policy();
    return;
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_JOINTS_ONLY)
    probe_game_object_remove_joints();
    return;
#endif
#if defined(AB_ORACLE_GAME_OBJECT_REMOVE_TELEPORTERS_ONLY)
    probe_game_object_remove_teleporters();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_REMOVAL_PASS_ONLY)
    probe_block_removal_pass();
    return;
#endif
#if defined(AB_ORACLE_BODY_VELOCITY_ONLY)
    probe_body_velocity();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_FRICTION_ONLY)
    probe_object_friction();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_FORCES_ONLY)
    probe_object_forces();
    return;
#endif
#if defined(AB_ORACLE_FORCE_QUEUE_ONLY)
    probe_force_queue();
    return;
#endif
#if defined(AB_ORACLE_FORCE_POINT_ONLY)
    probe_force_point();
    return;
#endif
#if defined(AB_ORACLE_WATER_EFFECT_ONLY)
    probe_water_effect();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_EVENT_ONLY)
    probe_block_event();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_SCRIPT_ONLY)
    probe_block_script();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_COMPONENT_ONLY)
    probe_block_component();
    return;
#endif
#if defined(AB_ORACLE_FLAME_EFFECT_ONLY)
    probe_flame_effect();
    return;
#endif
#if defined(AB_ORACLE_FLAME_DRAW_ONLY)
    probe_flame_draw();
    return;
#endif
#if defined(AB_ORACLE_FLAME_APPLY_ONLY)
    probe_flame_apply();
    return;
#endif
#if defined(AB_ORACLE_AMBIENT_ANIMATION_ONLY)
    probe_ambient_animation();
    return;
#endif
#if defined(AB_ORACLE_AMBIENT_PARTICLE_ONLY)
    probe_ambient_particle();
    return;
#endif
#if defined(AB_ORACLE_BLOCK_UPDATE_ONLY)
    probe_block_update();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_UPDATE_ONLY)
    probe_object_update();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_VISUAL_ONLY)
    probe_object_visual();
    return;
#endif
#if defined(AB_ORACLE_OBJECT_TRANSFORM_ONLY)
    probe_object_transform();
    return;
#endif
#if defined(AB_ORACLE_BIRD_DAMAGE_ONLY)
    probe_bird_damage();
    return;
#endif
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate legacy collision reference state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "lua");
    damage_function(lua, -10002, "requireFile", damage_require);
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/vector2.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/collision/museum-collisions.lua");
    damage_lua.get_field(lua, -10002, "MuseumCollisions");
    damage_lua.get_field(lua, -1, "onCollision");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || damage_lua.type(lua, -1) != 6 || header[6] != 0 || header[7] != 4) {
        fail("Missing original legacy block collision closure\n");
    }
    /* Stripped chunks have no debug upvalue names, so lua_getupvalue rejects
     * even existing closure upvalues. Read the measured third UpVal's TValue
     * and place it in a native pushnil-reserved stack slot. The prototype and
     * closure remain unchanged. ABI comes from target lua_getupvalue at ELF
     * 0x82b2c4: LClosure.upvals[2] at 0x1c, UpVal.v at 8, lua_State.top at 8.
     */
    const uint32_t *upvalue = (const uint32_t *)(uintptr_t)closure[7];
    const uint32_t *closure_value = (const uint32_t *)(uintptr_t)upvalue[2];
    if (closure_value[1] != 6) {
        fail("Original legacy collision upvalue is not a function\n");
    }
    damage_lua.push_nil(lua);
    uint32_t *top = (uint32_t *)(uintptr_t)((const uint32_t *)lua)[2];
    top[-2] = closure_value[0];
    top[-1] = closure_value[1];
    damage_lua.set_field(lua, -10002, "_ab_legacy_collision");
    damage_lua.set_top(lua, 0);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "blockCollision", damage_block_callback);
    damage_lua.set_field(lua, -10002, "CollisionCallbacks");
    uint32_t state = UINT32_C(0x520da6a9);
    semihost(4, "AB_ANDROID_BLOCK_DAMAGE_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t input[18] = {0};
        for (size_t body = 0; body < 2; ++body) {
            size_t offset = body * 7;
            input[offset] = random_float(&state, 122, 12, 1);
            input[offset + 1] = random_float(&state, 122, 12, 1);
            input[offset + 2] = index % 7 ? random_float(&state, 120, 12, 0) : 0;
            input[offset + 3] = random_float(&state, 120, 15, index % 13 == 0);
            input[offset + 4] = random_float(&state, 115, 14, index % 11 == 0);
            input[offset + 5] = (uint32_t)((index >> (body * 3)) & (body ? 63u : 127u));
            damage_flags[body] = input[offset + 5];
            prepare_damage_object(lua, input + offset, (unsigned)body + 1);
        }
        if (index < 16) {
            input[3] = index % 2 ? UINT32_C(0x80000000) : 0;
            damage_lua.get_field(lua, -10002, "first");
            if (!(damage_flags[0] & 8)) {
                damage_number(lua, -2, "strength", input[3]);
            }
            damage_lua.set_top(lua, 0);
        }
        for (size_t field = 14; field < 18; ++field) {
            input[field] = random_float(&state, 120, 10, 1);
        }
        prepare_damage_game(lua);
        damage_event_count = 0;
        for (size_t event = 0; event < 3; ++event) {
            for (size_t field = 0; field < 9; ++field) {
                damage_events[event][field] = 0;
            }
        }
        print_words(input, 18);
        damage_lua.get_field(lua, -10002, "_ab_legacy_collision");
        damage_lua.get_field(lua, -10002, "first");
        damage_lua.get_field(lua, -10002, "second");
        for (size_t field = 14; field < 18; ++field) {
            FloatBits value = {.bits = input[field]};
            damage_lua.push_number(lua, value.number);
        }
        if (damage_lua.call(lua, 6, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original legacy collision failed\n");
        }
        print_damage_results(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB block damage reference complete\n");
}

#endif
