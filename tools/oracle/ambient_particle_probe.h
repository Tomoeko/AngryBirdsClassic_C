#ifndef AB_ORACLE_AMBIENT_PARTICLE_H
#define AB_ORACLE_AMBIENT_PARTICLE_H

#include "selected_function.h"

static uint32_t ap_input[39], ap_events[3][6], ap_transforms, ap_emissions, ap_angles;
static uint32_t ap_point[2];
static const char *const ap_names[] = {"particle1", "particle2", "particle3"};
static void (*ap_raw_set)(void *, int, int);
static void (*ap_raw_get)(void *, int, int);
static const char *(*ap_upvalue)(void *, int, int);
#if defined(AB_ORACLE_AMBIENT_PARTICLE_COORDINATES_ONLY)
static void (*ap_to_world)(FloatBits[2], const void *, float, float);
#endif

static int ap_enabled(void *lua) {
    if (damage_lua.get_top(lua)) {
        fail("Unexpected ambient-particle enabled query\n");
    }
    damage_lua.push_boolean(lua, 1);
    return 1;
}

static int ap_angle(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "first")) {
        fail("Unexpected ambient-particle angle arguments\n");
    }
    ++ap_angles;
    FloatBits angle = {.bits = ap_input[2]};
    damage_lua.push_number(lua, angle.number);
    return 1;
}

static int ap_transform(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_lua.type(lua, 1) != 3 ||
        damage_lua.type(lua, 2) != 3) {
        fail("Unexpected ambient-particle conversion arguments\n");
    }
    ++ap_transforms;
#if defined(AB_ORACLE_AMBIENT_PARTICLE_COORDINATES_ONLY)
    FloatBits x = {.number = damage_lua.to_number(lua, 1)},
              y = {.number = damage_lua.to_number(lua, 2)}, output[2];
    uint32_t coordinates[4] = {0};
    ap_point[0] = x.bits;
    ap_point[1] = y.bits;
    ap_to_world(output, coordinates, x.number, y.number);
    damage_lua.push_number(lua, output[0].number);
    damage_lua.push_number(lua, output[1].number);
#else
    FloatBits scale = {.bits = ap_input[5]};
    for (int index = 0; index < 2; ++index) {
        FloatBits point = {.number = damage_lua.to_number(lua, index + 1)};
        FloatBits offset = {.bits = ap_input[6 + index]};
        ap_point[index] = point.bits;
        damage_lua.push_number(lua, point.number * scale.number + offset.number);
    }
#endif
    return 2;
}

static int ap_emit(void *lua) {
    if (damage_lua.get_top(lua) != 9 || ap_emissions >= 3) {
        fail("Unexpected ambient-particle emission arguments\n");
    }
    uint32_t *event = ap_events[ap_emissions++];
    for (size_t index = 0; index < 3; ++index) {
        if (damage_named(lua, 1, ap_names[index])) {
            event[0] = (uint32_t)index + 1;
        }
    }
    if (!event[0]) {
        fail("Unexpected ambient-particle effect name\n");
    }
    for (int index = 2; index <= 4; ++index) {
        if (damage_lua.type(lua, index) != 3) {
            fail("Expected numeric ambient-particle emission field\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        event[index == 2 ? 1 : index + 1] = value.bits;
    }
    event[2] = ap_point[0];
    event[3] = ap_point[1];
    for (int index = 5; index <= 7; ++index) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        if (damage_lua.type(lua, index) != 3 || value.bits) {
            fail("Expected positive-zero ambient-particle emission field\n");
        }
    }
    for (int index = 8; index <= 9; ++index) {
        if (damage_lua.type(lua, index) != 1 || damage_lua.to_boolean(lua, index)) {
            fail("Expected false ambient-particle emission flag\n");
        }
    }
    return 0;
}

static void ap_optional(void *lua, int table, const char *field, uint32_t kind,
                        uint32_t bits) {
    if (kind == 2) {
        damage_number(lua, table, field, bits);
    } else if (kind == 1) {
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, table, field);
    }
}

static void ap_prepare(void *lua) {
    damage_lua.create_table(lua, 0, 3);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, 1, "name");
    damage_number(lua, 1, "x", ap_input[3]);
    damage_number(lua, 1, "y", ap_input[4]);
    damage_lua.set_field(lua, -10002, "_ab_ap_object");
    damage_lua.create_table(lua, 0, 0);
    for (uint32_t index = 0; index < ap_input[0]; ++index) {
        size_t base = 12 + index * 9;
        damage_lua.create_table(lua, 0, 8);
        ap_optional(lua, 2, "timer", ap_input[base], ap_input[base + 1]);
        ap_optional(lua, 2, "delay", ap_input[base + 2], ap_input[base + 3]);
        damage_number(lua, 2, "minDelay", ap_input[base + 4]);
        damage_number(lua, 2, "maxDelay", ap_input[base + 5]);
        damage_number(lua, 2, "x", ap_input[base + 6]);
        damage_number(lua, 2, "y", ap_input[base + 7]);
        damage_number(lua, 2, "amount", ap_input[base + 8]);
        damage_lua.push_string(lua, ap_names[index]);
        damage_lua.set_field(lua, 2, "particles");
        ap_raw_set(lua, 1, (int)index + 1);
    }
    damage_lua.set_field(lua, -10002, "_ab_ap_list");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.get_field(lua, -10002, "_ab_ap_object");
    damage_lua.get_field(lua, -10002, "_ab_ap_list");
    void (*raw_set)(void *, int) =
        (void (*)(void *, int))find_function("_Z10lua_rawsetP9lua_Statei");
    raw_set(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_ap_map");
    damage_lua.get_field(lua, -10002, "_ab_ap_update");
    damage_lua.get_field(lua, -10002, "_ab_ap_map");
    if (!ap_upvalue(lua, 1, 1)) {
        fail("Cannot bind original ambient-particle object map\n");
    }
    damage_lua.set_top(lua, 0);
}

static void ap_snapshot(void *lua) {
    uint32_t output[24] = {ap_transforms, ap_emissions, 0};
    damage_lua.get_field(lua, -10002, "_ab_ap_list");
    for (uint32_t index = 0; index < ap_input[0]; ++index) {
        ap_raw_get(lua, 1, (int)index + 1);
        damage_lua.get_field(lua, 2, "timer");
        if (damage_lua.type(lua, 3) == 3) {
            output[2] |= 1u << index;
            FloatBits timer = {.number = damage_lua.to_number(lua, 3)};
            output[3 + index] = timer.bits;
        } else if (damage_lua.type(lua, 3) != 0) {
            fail("Unexpected original ambient-particle timer\n");
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < ap_emissions; ++index) {
        for (size_t field = 0; field < 6; ++field) {
            output[6 + index * 6 + field] = ap_events[index][field];
        }
    }
    print_words(output, 24);
}

static void probe_ambient_particle(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original ambient-particle state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    ap_raw_set =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    ap_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    ap_upvalue = (const char *(*)(void *, int, int))find_function(
        "_Z14lua_setupvalueP9lua_Stateii");
    load_selected_function(
        lua, "/data/local/tmp/ab520/block-event/ambient-particle.lua", "_ab_ap_update");
    damage_lua.create_table(lua, 0, 4);
    damage_function(lua, 1, "isPhysicsEnabled", ap_enabled);
    damage_function(lua, 1, "getAngle", ap_angle);
    damage_function(lua, 1, "physicsToWorldTransform", ap_transform);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 3, "first");
    damage_lua.set_field(lua, 2, "world");
    damage_lua.set_field(lua, 1, "objects");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "addParticles", ap_emit);
    damage_lua.set_field(lua, -10002, "particles");
    void (*seed)(int32_t) = (void (*)(int32_t))find_function("srand48");
    int32_t (*next)(void) = (int32_t (*)(void))find_function("lrand48");
    uint32_t random = UINT32_C(0x520a4b18);
#if defined(AB_ORACLE_AMBIENT_PARTICLE_COORDINATES_ONLY)
    ap_to_world = (void (*)(FloatBits[2], const void *, float, float))find_function(
        "_ZNK20CoordinateTransforms23physicsToWorldTransformEff");
    semihost(4, "AB_ANDROID_AMBIENT_PARTICLE_COORDINATES_V1 4096\n");
#else
    semihost(4, "AB_ANDROID_AMBIENT_PARTICLE_V1 4096\n");
#endif
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 39; ++field) {
            ap_input[field] = random_float(&random, 118, 16, 1);
        }
        ap_input[0] = (uint32_t)(item % 4);
        ap_input[1] = random_word(&random);
        for (size_t step = 0; step < 4; ++step) {
            FloatBits seconds = {.number = (float)(random_word(&random) % 129) / 64};
            if (item % 17 == 0) {
                seconds.bits ^= UINT32_C(0x80000000);
            }
            if (item < 256) {
                seconds.bits = item & 4 ? UINT32_C(0x80000000) : 0;
            }
            ap_input[8 + step] = seconds.bits;
        }
        for (size_t index = 0; index < 3; ++index) {
            size_t base = 12 + index * 9;
            ap_input[base] = (uint32_t)((item / 4 + index) % 3);
            ap_input[base + 2] = (uint32_t)((item / 12 + index) % 3);
            if (item < 256) {
                ap_input[base + 1] = item & 8 ? UINT32_C(0x80000000) : 0;
                ap_input[base + 3] = item & 16 ? UINT32_C(0x80000000) : 0;
            }
        }
        if (!ap_input[0]) {
            ap_input[2] = ap_input[3] = ap_input[4] = UINT32_C(0x7fc12345);
        }
        ap_prepare(lua);
        seed((int32_t)ap_input[1]);
        print_words(ap_input, 39);
        for (size_t step = 0; step < 4; ++step) {
            ap_transforms = ap_emissions = ap_angles = 0;
            for (size_t index = 0; index < 3; ++index) {
                for (size_t field = 0; field < 6; ++field) {
                    ap_events[index][field] = 0;
                }
            }
            damage_lua.get_field(lua, -10002, "_ab_ap_update");
            damage_lua.push_nil(lua);
            FloatBits seconds = {.bits = ap_input[8 + step]};
            damage_lua.push_number(lua, seconds.number);
            damage_lua.push_nil(lua);
            if (damage_lua.call(lua, 3, 0, 0)) {
                const char *message = damage_lua.to_string(lua, -1, NULL);
                fail(message ? message : "Original ambient-particle update failed\n");
            }
            if (ap_angles != 1 || ap_transforms != ap_emissions) {
                fail("Unexpected original ambient-particle query/conversion order\n");
            }
            ap_snapshot(lua);
        }
        uint32_t following = (uint32_t)next();
        print_words(&following, 1);
    }
    damage_lua.close_state(lua);
#if defined(AB_ORACLE_AMBIENT_PARTICLE_COORDINATES_ONLY)
    semihost(4, "AB ambient particle coordinates reference complete\n");
#else
    semihost(4, "AB ambient particle reference complete\n");
#endif
}

#endif
