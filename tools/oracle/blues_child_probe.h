#ifndef AB_ORACLE_BLUES_CHILD_H
#define AB_ORACLE_BLUES_CHILD_H

#include "reference_blues_child.h"
#include "selected_function.h"

#if defined(AB_ORACLE_BLUES_SPLIT_ONLY)
static void bs_trace(void *lua, uint32_t stage);
static void bs_execute(void *lua, void *wrapper);
static void bs_cleanup(void (*remove_object)(void *, const char **));
#endif

typedef struct {
    uint32_t length, capacity, references;
    char bytes[96];
} BcNativeString;

static struct {
    uint32_t game[0x1a00 / 4], birds[64];
    uint64_t world[16384];
    BcNativeString name;
    const void *payload, *physical, *descriptor;
    const char *parent_name;
    uint32_t events[BC_MAX_EVENTS][BC_EVENT_WORDS], count, follows;
    void (*reference)(void *);
    void (*push)(void *, int);
    int (*next)(void *, int);
    unsigned (*length)(void *, int);
    uint32_t *(*object)(void *, const char **);
    void (*circle)(void *, const char **, const char **, float, float, float, float,
                   float, float, unsigned, float);
    void (*sprite)(void *, const char **, const char **);
    void (*rotation)(void *, const char **, float);
    void (*velocity)(void *, const char **, float, float);
    void (*collider)(void *, const char **, int);
} bc_native;

static unsigned bc_string_length(const char *value) {
    unsigned size = 0;
    while (value[size])
        ++size;
    return size;
}

static void bc_native_string(void *lua, int index, BcNativeString *output) {
    unsigned size = 0;
    const char *bytes = damage_lua.to_string(lua, index, &size);
    if (!bytes || size >= sizeof(output->bytes))
        fail("Unsupported original Blues native string argument\n");
    output->length = output->capacity = size;
    output->references = UINT32_MAX;
    for (unsigned offset = 0; offset < size; ++offset)
        output->bytes[offset] = bytes[offset];
    output->bytes[size] = 0;
}

static BcValue bc_encode(void *lua, int index) {
    uint32_t kind = (uint32_t)damage_lua.type(lua, index);
    if (!kind)
        return (BcValue){0};
    if (kind == 1)
        return (BcValue){1, damage_lua.to_boolean(lua, index) != 0};
    if (kind == 3) {
        FloatBits number = {.number = damage_lua.to_number(lua, index)};
        return (BcValue){3, number.bits};
    }
    if (kind == 4) {
        if (bc_native.parent_name && damage_named(lua, index, bc_native.parent_name))
            return (BcValue){4, 1};
        if (damage_named(lua, index, bc_native.name.bytes))
            return (BcValue){4, 6};
        for (uint32_t role = 0; role < 9; ++role)
            if (role != 6 && damage_named(lua, index, bc_strings[role]))
                return (BcValue){4, role};
    }
    if (kind == 5) {
        const void *value = damage_lua.to_pointer(lua, index);
        if (value == bc_native.payload)
            return (BcValue){5, 1};
        if (value == bc_native.physical)
            return (BcValue){5, 2};
        if (value == bc_native.descriptor)
            return (BcValue){5, 3};
    }
    fail("Unexpected original Blues constructor value\n");
    return (BcValue){0};
}

static void bc_push(void *lua, BcValue value) {
    if (!value.kind)
        damage_lua.push_nil(lua);
    else if (value.kind == 1)
        damage_lua.push_boolean(lua, value.bits != 0);
    else if (value.kind == 3) {
        FloatBits number = {.bits = value.bits};
        damage_lua.push_number(lua, number.number);
    } else if (value.kind == 4 && value.bits < 9)
        damage_lua.push_string(lua, bc_strings[value.bits]);
    else if (value.kind == 5 && value.bits == 1)
        damage_lua.get_field(lua, -10002, "_ab_bc_payload");
    else
        fail("Unsupported prepared Blues parent value\n");
}

static uint32_t bc_table_count(void *lua, int index) {
    int top = damage_lua.get_top(lua);
    uint32_t count = 0;
    damage_lua.push_nil(lua);
    while (bc_native.next(lua, index)) {
        if (++count > 96)
            fail("Unexpected original Blues table growth\n");
        damage_lua.set_top(lua, top + 1);
    }
    damage_lua.set_top(lua, top);
    return count;
}

static uint32_t bc_same_field(void *lua, int owner, const char *name,
                              const void *pointer) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, owner, name);
    int same = damage_lua.type(lua, top + 1) == 5 &&
               damage_lua.to_pointer(lua, top + 1) == pointer;
    damage_lua.set_top(lua, top);
    return same ? 1u : 0u;
}

static void bc_snapshot(void *lua, uint32_t output[BC_SNAPSHOT_WORDS]) {
    for (unsigned index = 0; index < BC_SNAPSHOT_WORDS; ++index)
        output[index] = 0;
    int top = damage_lua.get_top(lua);
    bc_native.reference((uint8_t *)bc_native.game + 0x1810);
    damage_lua.get_field(lua, top + 1, bc_native.name.bytes);
    int physical = top + 2;
    if (damage_lua.type(lua, physical) == 0) {
        damage_lua.set_top(lua, top);
        return;
    }
    if (damage_lua.type(lua, physical) != 5)
        fail("Original Blues child world entry is not physical\n");
    bc_native.physical = damage_lua.to_pointer(lua, physical);
    output[0] = 1;
    for (unsigned index = 0; index < BC_FIELD_COUNT; ++index) {
        damage_lua.get_field(lua, physical, bc_fields[index]);
        BcValue value = bc_encode(lua, physical + 1);
        output[1 + index * 2] = value.kind;
        output[2 + index * 2] = value.bits;
        damage_lua.set_top(lua, physical);
    }
    unsigned start = 1 + (unsigned)BC_FIELD_COUNT * 2;
    const char *name = bc_native.name.bytes;
    const uint32_t *object = bc_native.object(bc_native.game, &name);
    const uint32_t *body = object ? (const void *)(uintptr_t)object[11] : NULL;
    if (!body)
        fail("Original Blues child lost its native body\n");
    const unsigned cache[] = {0x44, 0x48, 0x4c, 0x50, 0x54, 0x58, 0x90, 0x94, 0x98};
    for (unsigned index = 0; index < 9; ++index)
        output[start + index] = object[cache[index] / 4];
    output[start + 9] = body[0];
    for (unsigned index = 0; index < 4; ++index)
        output[start + 10 + index] = body[29 + index];
    output[start + 14] = body[16];
    output[start + 15] = body[17];
    output[start + 16] = object[0];
    start += BC_CORE_WORDS;
    damage_lua.get_field(lua, -10002, "birds");
    damage_lua.get_field(lua, physical + 1, bc_native.name.bytes);
    output[start] = damage_lua.type(lua, physical + 2) == 5 &&
                    damage_lua.to_pointer(lua, physical + 2) == bc_native.physical;
    damage_lua.set_top(lua, physical);
    damage_lua.get_field(lua, physical, "gameObjectScript");
    int script = physical + 1;
    if (damage_lua.type(lua, script) == 5) {
        const void *identity = damage_lua.to_pointer(lua, script);
        output[start + 1] = 1;
        output[start + 2] =
            bc_same_field(lua, script, "physicsObject", bc_native.physical);
        damage_lua.get_field(lua, script, "name");
        output[start + 3] = damage_named(lua, script + 1, bc_native.name.bytes) != 0;
        damage_lua.set_top(lua, script);
        damage_lua.get_field(lua, script, "behaviors");
        int groups = script + 1;
        output[start + 4] = bc_table_count(lua, groups);
        damage_lua.get_field(lua, groups, "MuseumBluesTrajectoryRenderer");
        int group = groups + 1;
        output[start + 5] = bc_native.length(lua, group);
        void (*get_index)(void *, int, int) =
            (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
        get_index(lua, group, 1);
        int behavior = group + 1;
        if (damage_lua.type(lua, behavior) != 5)
            fail("Original Blues child did not construct its trajectory behavior\n");
        const char *const flags[] = {"isCluster", "flying"};
        for (unsigned flag = 0; flag < 2; ++flag) {
            damage_lua.get_field(lua, behavior, flags[flag]);
            BcValue value = bc_encode(lua, behavior + 1);
            output[start + 6 + flag * 2] = value.kind;
            output[start + 7 + flag * 2] = value.bits;
            damage_lua.set_top(lua, behavior);
        }
        output[start + 10] = bc_same_field(lua, behavior, "gameObject", identity);
        output[start + 11] =
            bc_same_field(lua, behavior, "physicsObject", bc_native.physical);
    }
    damage_lua.set_top(lua, top);
}

static void bc_trace(void *lua, uint32_t stage) {
#if defined(AB_ORACLE_BLUES_SPLIT_ONLY)
    bs_trace(lua, stage);
    return;
#endif
    int count = damage_lua.get_top(lua);
    if (count < 0 || count > 10 || bc_native.count == BC_MAX_EVENTS)
        fail("Unexpected original Blues constructor boundary\n");
    uint32_t *record = bc_native.events[bc_native.count++];
    for (unsigned index = 0; index < BC_EVENT_WORDS; ++index)
        record[index] = 0;
    record[0] = stage;
    record[1] = (uint32_t)count;
    for (int index = 0; index < count; ++index) {
        BcValue value = bc_encode(lua, index + 1);
        record[2 + (unsigned)index * 2] = value.kind;
        record[3 + (unsigned)index * 2] = value.bits;
    }
    bc_snapshot(lua, record + BC_ARGUMENT_WORDS);
}

static int bc_circle(void *lua) {
    bc_trace(lua, 1);
    BcNativeString name, sprite;
    bc_native_string(lua, 1, &name);
    bc_native_string(lua, 2, &sprite);
    const char *first = name.bytes, *second = sprite.bytes;
    bc_native.circle(bc_native.game, &first, &second, damage_lua.to_number(lua, 3),
                     damage_lua.to_number(lua, 4), damage_lua.to_number(lua, 5),
                     damage_lua.to_number(lua, 6), damage_lua.to_number(lua, 7),
                     damage_lua.to_number(lua, 8), damage_lua.to_boolean(lua, 9) != 0,
                     damage_lua.to_number(lua, 10));
    return 0;
}

static void bc_set_sprite(void *lua) {
    BcNativeString name, sprite;
    bc_native_string(lua, 1, &name);
    bc_native_string(lua, 2, &sprite);
    const char *first = name.bytes, *second = sprite.bytes;
    bc_native.sprite(bc_native.game, &first, &second);
}

static int bc_renderer(void *lua) {
    bc_trace(lua, 2);
    /* Controlled renderer boundary; the original native setSprite still writes
     * the real object's raw table. No attached native/GPU renderer is invented. */
    bc_set_sprite(lua);
    return 0;
}

static int bc_sprite(void *lua) {
    bc_trace(lua, 3);
    bc_set_sprite(lua);
    return 0;
}

static int bc_rotation(void *lua) {
    bc_trace(lua, 4);
    BcNativeString storage;
    bc_native_string(lua, 1, &storage);
    const char *name = storage.bytes;
    bc_native.rotation(bc_native.game, &name, damage_lua.to_number(lua, 2));
    return 0;
}

static int bc_velocity(void *lua) {
    bc_trace(lua, 5);
    BcNativeString storage;
    bc_native_string(lua, 1, &storage);
    const char *name = storage.bytes;
    bc_native.velocity(bc_native.game, &name, damage_lua.to_number(lua, 2),
                       damage_lua.to_number(lua, 3));
    return 0;
}

static int bc_collider(void *lua) {
    bc_trace(lua, 6);
    BcNativeString storage;
    bc_native_string(lua, 1, &storage);
    const char *name = storage.bytes;
    bc_native.collider(bc_native.game, &name, (int)damage_lua.to_number(lua, 2));
    return 0;
}

static int bc_game_object(void *lua) {
    bc_native.descriptor = damage_lua.to_pointer(lua, 2);
    bc_trace(lua, 7);
    damage_lua.get_field(lua, -10002, "_ab_bc_original_create");
    bc_native.push(lua, 1);
    bc_native.push(lua, 2);
    if (damage_lua.call(lua, 2, 0, 0))
        fail("Original Blues GameObject/class creation failed\n");
    return 0;
}

static int bc_scale(void *lua) {
    /* The native renderable is outside this capture; retain exact arguments. */
    bc_trace(lua, 8);
    return 0;
}

static int bc_follow(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5 ||
        damage_lua.to_pointer(lua, 1) != bc_native.physical)
        fail("Original Blues constructor followed a different physical identity\n");
    ++bc_native.follows;
    return 0;
}

static void bc_graph(void *lua) {
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "lua");
    damage_lua.get_field(lua, -10002, "_G");
    damage_lua.set_field(lua, -10002, "gamelua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/bc-class.lua");
    load_selected_function(lua, "/data/local/tmp/ab520/bc-copy.lua", "shallowCopy");
    load_selected_function(lua, "/data/local/tmp/ab520/bc-merge.lua",
                           "mergeTablesRecursive");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "addObjectToFollow", bc_follow);
    damage_lua.set_field(lua, -10002, "TrajectoryRenderer");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, -10002, "g_in_editor");
    load_damage_chunk(lua, "/data/local/tmp/ab520/bc-base.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/bc-utils.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/bc-blue.lua");
    damage_lua.get_field(lua, -10002, "BehaviorList");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.get_field(lua, -10002, "MuseumBluesTrajectoryRenderer");
    damage_lua.set_field(lua, 2, "class");
    damage_lua.set_field(lua, 1, "MuseumBluesTrajectoryRenderer");
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/bc-game-object.lua");
    damage_lua.get_field(lua, -10002, "GameObject");
    damage_lua.get_field(lua, 1, "createGameObject");
    damage_lua.set_field(lua, -10002, "_ab_bc_original_create");
    damage_function(lua, 1, "createGameObject", bc_game_object);
    damage_lua.set_top(lua, 0);
    const char *const names[] = {"createCircle", "createRendererForGameObject",
                                 "setSprite",    "setRotation",
                                 "setVelocity",  "setColliderType",
                                 "setScale"};
    int (*functions[])(void *) = {bc_circle,   bc_renderer, bc_sprite, bc_rotation,
                                  bc_velocity, bc_collider, bc_scale};
    for (unsigned index = 0; index < 7; ++index)
        damage_function(lua, -10002, names[index], functions[index]);
    damage_lua.create_table(lua, 0, 64);
    damage_lua.set_field(lua, -10002, "birds");
    damage_lua.create_table(lua, 0, 1);
    bc_native.payload = damage_lua.to_pointer(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_bc_payload");
    damage_lua.create_table(lua, 0, BC_PARENT_FIELDS);
    const void *parent = damage_lua.to_pointer(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_bc_parent");
    load_selected_function(lua, "/data/local/tmp/ab520/bc-child.lua", "_ab_bc_child");
    damage_lua.get_field(lua, -10002, "_ab_bc_child");
    const uint8_t *closure = damage_lua.to_pointer(lua, 1);
    const uint32_t *upvalue =
        closure ? (const void *)(uintptr_t)((const uint32_t *)closure)[5] : NULL;
    if (!closure || closure[6] || closure[7] != 1 || !upvalue ||
        upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3) ||
        (((const uint8_t *)upvalue)[5] & 4u))
        fail("Expected a fresh closed original Blues parent upvalue\n");
    uint32_t *value = (void *)(uintptr_t)upvalue[2];
    if (value[1])
        fail("Unexpected original initial Blues parent value\n");
    /* The source closure/prototype/instructions remain unchanged. Prepare its
     * one captured parent as an isolated fixture input, rooted globally. */
    value[0] = (uint32_t)(uintptr_t)parent;
    value[1] = 5;
    damage_lua.set_top(lua, 0);
}

static void probe_blues_child(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*state_construct)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*table_construct)(void *, void *) =
        (void (*)(void *, void *))find_function("_ZN3lua8LuaTableC1EPNS_8LuaStateE");
    void (*table_destroy)(void *) =
        (void (*)(void *))find_function("_ZN3lua8LuaTableD1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    void (*world_construct)(void *, const void *) =
        (void (*)(void *, const void *))find_function("_ZN7b2WorldC1ERK6b2Vec2");
    void (*world_destroy)(void *) = (void (*)(void *))find_function("_ZN7b2WorldD1Ev");
    void (*remove_object)(void *, const char **) =
        (void (*)(void *, const char **))find_function("_ZN7GameLua12removeObjectESs");
    bc_native.reference =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    bc_native.push =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    bc_native.next = (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    bc_native.length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    bc_native.object = (uint32_t *(*)(void *, const char **))find_function(
        "_ZN7GameLua13getGameObjectERKSs");
    bc_native.circle = (void (*)(
        void *, const char **, const char **, float, float, float, float, float, float,
        unsigned, float))find_function("_ZN7GameLua15createCircleLuaESsSsffffffbf");
    bc_native.sprite = (void (*)(void *, const char **, const char **))find_function(
        "_ZN7GameLua9setSpriteESsSs");
    bc_native.rotation = (void (*)(void *, const char **, float))find_function(
        "_ZN7GameLua11setRotationESsf");
    bc_native.velocity = (void (*)(void *, const char **, float, float))find_function(
        "_ZN7GameLua11setVelocityESsff");
    bc_native.collider = (void (*)(void *, const char **, int))find_function(
        "_ZN7GameLua15setColliderTypeESsi");
    uint32_t *wrapper = allocate(20);
    if (!wrapper)
        fail("Cannot allocate original Blues constructor Lua state\n");
    state_construct(wrapper);
    claim(wrapper);
    void *lua = (void *)(uintptr_t)wrapper[2];
    const uint32_t gravity[2] = {0};
    world_construct(bc_native.world, gravity);
    bc_native.game[0x1784 / 4] = (uint32_t)(uintptr_t)bc_native.world;
    const unsigned maps[] = {0x1790, 0x17a8, 0x17c0};
    for (unsigned index = 0; index < 3; ++index) {
        uint32_t *header = bc_native.game + maps[index] / 4;
        header[2] = header[3] = (uint32_t)(uintptr_t)header;
    }
    bc_native.game[0x17d4 / 4] = bc_native.game[0x17d8 / 4] =
        (uint32_t)(uintptr_t)bc_native.birds;
    bc_native.game[0x17dc / 4] = (uint32_t)(uintptr_t)(bc_native.birds + 64);
    table_construct((uint8_t *)bc_native.game + 0x17f8, wrapper);
    table_construct((uint8_t *)bc_native.game + 0x1810, wrapper);
    bc_native.reference((uint8_t *)bc_native.game + 0x17f8);
    bc_native.reference((uint8_t *)bc_native.game + 0x1810);
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    bc_graph(lua);
#if defined(AB_ORACLE_BLUES_SPLIT_ONLY)
    bs_execute(lua, wrapper);
    bs_cleanup(remove_object);
#else
    semihost(4, "AB_ANDROID_BLUES_CHILD_V1 16 3\n");
    for (unsigned profile = 0; profile < BC_PROFILES; ++profile) {
        for (unsigned child = 0; child < BC_CHILDREN; ++child) {
            BcValue parent[BC_PARENT_FIELDS];
            uint32_t offset[2];
            char suffix[4];
            bc_case(profile, child, parent, offset, suffix);
            unsigned length = bc_string_length(bc_strings[1]);
            for (unsigned index = 0; index < length; ++index)
                bc_native.name.bytes[index] = bc_strings[1][index];
            for (unsigned index = 0; index < 4; ++index)
                bc_native.name.bytes[length + index] = suffix[index];
            bc_native.name.length = bc_native.name.capacity = length + 3;
            bc_native.name.references = UINT32_MAX;
            bc_native.physical = bc_native.descriptor = NULL;
            bc_native.count = bc_native.follows = 0;
            damage_lua.get_field(lua, -10002, "_ab_bc_parent");
            for (unsigned index = 0; index < BC_PARENT_FIELDS; ++index) {
                bc_push(lua, parent[index]);
                damage_lua.set_field(lua, 1, bc_parent_fields[index]);
            }
            damage_lua.set_top(lua, 0);
            damage_lua.get_field(lua, -10002, "_ab_bc_child");
            damage_lua.push_string(lua, suffix);
            damage_lua.create_table(lua, 0, 2);
            damage_number(lua, 3, "x", offset[0]);
            damage_number(lua, 3, "y", offset[1]);
            if (damage_lua.call(lua, 2, 1, 0)) {
                const char *error = damage_lua.to_string(lua, -1, NULL);
                fail(error ? error : "Original Blues child constructor failed\n");
            }
            uint32_t summary[] = {profile,
                                  child,
                                  offset[0],
                                  offset[1],
                                  bc_native.count,
                                  bc_native.follows,
                                  damage_named(lua, 1, bc_native.name.bytes) != 0};
            print_words(summary, 7);
            damage_lua.set_top(lua, 0);
            for (unsigned event = 0; event < bc_native.count; ++event)
                print_words(bc_native.events[event], BC_EVENT_WORDS);
            uint32_t final[BC_SNAPSHOT_WORDS];
            bc_snapshot(lua, final);
            print_words(final, BC_SNAPSHOT_WORDS);
        }
    }
    for (unsigned profile = 0; profile < BC_PROFILES; ++profile)
        for (unsigned child = 0; child < BC_CHILDREN; ++child) {
            BcValue unused_parent[BC_PARENT_FIELDS];
            uint32_t unused_offset[2];
            char suffix[4];
            bc_case(profile, child, unused_parent, unused_offset, suffix);
            unsigned length = bc_string_length(bc_strings[1]);
            for (unsigned index = 0; index < 4; ++index)
                bc_native.name.bytes[length + index] = suffix[index];
            const char *name = bc_native.name.bytes;
            remove_object(bc_native.game, &name);
        }
#endif
    if (bc_native.game[0x17b8 / 4] || ((uint32_t *)bc_native.world)[0x19230 / 4])
        fail("Original Blues constructor fixture retained native objects\n");
    table_destroy((uint8_t *)bc_native.game + 0x1810);
    table_destroy((uint8_t *)bc_native.game + 0x17f8);
    world_destroy(bc_native.world);
    release(wrapper);
#if defined(AB_ORACLE_BLUES_SPLIT_ONLY)
    semihost(4, "AB Blues split reference complete\n");
#else
    semihost(4, "AB Blues child reference complete\n");
#endif
}

#endif
