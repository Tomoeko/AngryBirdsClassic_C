#ifndef AB_ORACLE_BIRD_HAT_PROBE_H
#define AB_ORACLE_BIRD_HAT_PROBE_H

#include "selected_function.h"
#include "../../tests/bird_hat_cases.h"

static BirdHatCase hat_input;
static uint32_t hat_output[AB_BIRD_HAT_WORDS], hat_events, hat_prints;
static char hat_object_name[64], hat_joint_name[64];

static uint32_t *hat_event(uint32_t code) {
    if (hat_events >= 11)
        fail("Hat reference event overflow\n");
    uint32_t *output = hat_output + 49 + hat_events++ * 7;
    output[0] = code;
    return output;
}

static uint32_t hat_number(void *lua, int index) {
    if (damage_lua.type(lua, index) != 3)
        fail("Expected original numeric hat boundary argument\n");
    FloatBits value = {.number = damage_lua.to_number(lua, index)};
    return value.bits;
}

static void hat_field_number(void *lua, int index, const char *name, float value) {
    damage_lua.push_number(lua, value);
    damage_lua.set_field(lua, index, name);
}

static void hat_copy_name(void *lua, int index, char bytes[64], uint32_t *output) {
    unsigned length = 0;
    const char *value = damage_lua.to_string(lua, index, &length);
    if (!value || length >= 64)
        fail("Original hat name exceeds bounded trace storage\n");
    for (unsigned offset = 0; offset < length; ++offset) {
        bytes[offset] = value[offset];
        output[offset / 4] |= (uint32_t)(uint8_t)value[offset] << ((offset % 4) * 8);
    }
    bytes[length] = 0;
}

static int hat_create_object(void *lua) {
    if (damage_lua.get_top(lua) != 6 || !damage_named(lua, 2, "block_a"))
        fail("Unexpected original hat object construction\n");
    uint32_t *output = hat_event(1);
    output[1] = 1;
    output[2] = hat_number(lua, 4);
    output[3] = hat_number(lua, 5);
    damage_lua.get_field(lua, 6, "density");
    output[4] = hat_number(lua, 7);
    damage_lua.set_top(lua, 6);
    hat_copy_name(lua, 3, hat_object_name, hat_output + 16);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, 7, "world");
    damage_lua.create_table(lua, 0, 4);
    damage_number(lua, 9, "x", output[2]);
    damage_number(lua, 9, "y", output[3]);
    hat_field_number(lua, 9, "height", hat_input.height);
    hat_field_number(lua, 9, "defence", 0);
    damage_lua.set_field(lua, 8, hat_object_name);
    damage_lua.set_top(lua, 6);
    return 0;
}

static int hat_static(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, hat_object_name))
        fail("Unexpected original static-hat transition\n");
    (void)hat_event(2);
    return 0;
}

static int hat_joint(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected original hat joint arity\n");
    uint32_t *output = hat_event(3);
    damage_lua.get_field(lua, 1, "name");
    hat_copy_name(lua, 2, hat_joint_name, hat_output + 32);
    damage_lua.set_top(lua, 1);
    const char *fields[] = {"type", "coordType", "x1", "y1", "x2", "y2"};
    for (unsigned index = 0; index < 6; ++index) {
        damage_lua.get_field(lua, 1, fields[index]);
        float value = damage_lua.to_number(lua, 2);
        if (value != (index < 2 ? 2.0f : 0.0f))
            fail("Unexpected original hat joint numeric field\n");
        damage_lua.set_top(lua, 1);
    }
    output[1] = 2;
    output[2] = 2;
    damage_lua.get_field(lua, 1, "end1");
    if (!damage_named(lua, 2, "bird"))
        fail("Unexpected original hat first endpoint\n");
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "end2");
    if (!damage_named(lua, 2, hat_object_name))
        fail("Unexpected original hat second endpoint\n");
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "collideConnected");
    if (damage_lua.type(lua, 2) != 1 || damage_lua.to_boolean(lua, 2))
        fail("Unexpected original hat connected-collision flag\n");
    damage_lua.set_top(lua, 1);
    if (hat_input.change_counter) {
        damage_lua.push_number(lua, 25.5f);
        damage_lua.set_field(lua, -10002, "hatcounter");
    }
    if (hat_input.change_parameters) {
        damage_lua.get_field(lua, -10002, "blockTable");
        damage_lua.get_field(lua, 2, "hats");
        damage_lua.get_field(lua, 3, "selected");
        damage_lua.push_string(lua, "block_b");
        damage_lua.set_field(lua, 4, "block");
        damage_lua.get_field(lua, 4, "params");
        damage_lua.get_field(lua, 5, "birddef");
        hat_field_number(lua, 6, "x", hat_input.dx + 0.03125f);
        hat_field_number(lua, 6, "y", hat_input.dy - 0.0625f);
        damage_lua.set_top(lua, 1);
    }
    return 0;
}

static int hat_print(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "releasing hat"))
        fail("Unexpected original hat release print\n");
    (void)hat_event(4);
    if (!hat_prints++ && hat_input.remove_on_print) {
        damage_lua.get_field(lua, -10002, "g_birdHats");
        damage_lua.push_nil(lua);
        damage_lua.set_field(lua, 2, "bird");
        damage_lua.set_top(lua, 1);
    }
    return 0;
}

static int hat_destroy_joint(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, hat_joint_name))
        fail("Unexpected original deferred hat joint removal\n");
    (void)hat_event(5);
    return 0;
}

static int hat_damping(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, hat_object_name))
        fail("Unexpected original hat damping call\n");
    hat_event(6)[1] = hat_number(lua, 2);
    return 0;
}

static int hat_dynamic(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, hat_object_name))
        fail("Unexpected original dynamic-hat transition\n");
    (void)hat_event(7);
    if (hat_input.change_physics) {
        damage_lua.get_field(lua, -10002, "_ab_bird");
        hat_field_number(lua, 2, "xVel", hat_input.velocity * -2);
        damage_lua.get_field(lua, -10002, "objects");
        damage_lua.get_field(lua, 3, "world");
        damage_lua.get_field(lua, 4, hat_object_name);
        hat_field_number(lua, 5, "x", hat_input.x + 3.25f);
        hat_field_number(lua, 5, "y", hat_input.y - 5.5f);
        damage_lua.set_top(lua, 1);
    }
    return 0;
}

static int hat_impulse(void *lua) {
    if (damage_lua.get_top(lua) != 5 || !damage_named(lua, 1, hat_object_name))
        fail("Unexpected original hat impulse call\n");
    uint32_t *output = hat_event(8);
    for (int index = 2; index <= 5; ++index)
        output[index - 1] = hat_number(lua, index);
    return 0;
}

static void hat_destroyed_value(void *lua, uint32_t kind) {
    if (!kind)
        damage_lua.push_nil(lua);
    else if (kind == 1 || kind == 2)
        damage_lua.push_boolean(lua, kind == 2);
    else if (kind == 3)
        damage_lua.push_number(lua, 0);
    else
        damage_lua.push_string(lua, "");
}

static void hat_prepare(void *lua, uint32_t sample) {
    void (*copy)(void *, int) =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    for (unsigned index = 0; index < AB_BIRD_HAT_WORDS; ++index)
        hat_output[index] = 0;
    hat_events = hat_prints = 0;
    hat_object_name[0] = hat_joint_name[0] = 0;
    if (hat_input.counter_kind < 2)
        hat_destroyed_value(lua, hat_input.counter_kind);
    else
        damage_lua.push_number(lua, hat_input.counter);
    damage_lua.set_field(lua, -10002, "hatcounter");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 2);
    damage_lua.push_string(lua, "block_a");
    damage_lua.set_field(lua, 3, "block");
    damage_lua.create_table(lua, 0, 1);
    if (hat_input.absent)
        hat_destroyed_value(lua, sample & 1);
    else {
        damage_lua.create_table(lua, 0, 2);
        hat_field_number(lua, 5, "x", hat_input.dx);
        hat_field_number(lua, 5, "y", hat_input.dy);
    }
    damage_lua.set_field(lua, 4, "birddef");
    damage_lua.set_field(lua, 3, "params");
    damage_lua.set_field(lua, 2, "selected");
    damage_lua.set_field(lua, 1, "hats");
    damage_lua.set_field(lua, -10002, "blockTable");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 5);
    damage_lua.push_string(lua, "bird");
    damage_lua.set_field(lua, 3, "name");
    damage_lua.push_string(lua, "birddef");
    damage_lua.set_field(lua, 3, "definition");
    hat_field_number(lua, 3, "x", hat_input.x);
    hat_field_number(lua, 3, "y", hat_input.y);
    hat_field_number(lua, 3, "xVel", hat_input.velocity);
    copy(lua, 3);
    damage_lua.set_field(lua, -10002, "_ab_bird");
    damage_lua.set_field(lua, 2, "bird");
    damage_lua.set_field(lua, 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "g_birdHats");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, -10002, "_ab_held_hat");
}

static void hat_scalar(void *lua, int index, uint32_t *output) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = output[0] == 3   ? hat_number(lua, index)
                : output[0] == 1 ? (uint32_t)damage_lua.to_boolean(lua, index)
                                 : 0;
}

static void probe_bird_hat(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot create original hat state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "createObject", hat_create_object);
    damage_function(lua, -10002, "setObjectBodyStatic", hat_static);
    damage_function(lua, -10002, "createJoint", hat_joint);
    damage_function(lua, -10002, "print", hat_print);
    damage_function(lua, -10002, "destroyJointDeferred", hat_destroy_joint);
    damage_function(lua, -10002, "setLinearDamping", hat_damping);
    damage_function(lua, -10002, "setObjectBodyDynamic", hat_dynamic);
    damage_function(lua, -10002, "applyImpulse", hat_impulse);
    load_selected_function(lua, "/data/local/tmp/ab520/create-hat.lua",
                           "_ab_create_hat");
    load_selected_function(lua, "/data/local/tmp/ab520/release-hat.lua",
                           "_ab_release_hat");
    semihost(4, "AB_ANDROID_BIRD_HAT_V1 1024\n");
    for (uint32_t sample = 0; sample < AB_BIRD_HAT_CASES; ++sample) {
        hat_input = bird_hat_case(sample);
        hat_prepare(lua, sample);
        damage_lua.get_field(lua, -10002, "_ab_create_hat");
        damage_lua.get_field(lua, -10002, "_ab_bird");
        damage_lua.push_string(lua, "selected");
        if (damage_lua.call(lua, 2, 0, 0))
            fail("Original hat creation failed\n");
        damage_lua.get_field(lua, -10002, "g_birdHats");
        damage_lua.get_field(lua, 1, "bird");
        if (damage_lua.type(lua, 2) == 5) {
            hat_destroyed_value(lua, hat_input.destroyed_kind);
            damage_lua.set_field(lua, 2, "jointDestroyed");
            damage_lua.set_field(lua, -10002, "_ab_held_hat");
        }
        damage_lua.set_top(lua, 0);
        for (unsigned repeat = 0; repeat < 2; ++repeat) {
            damage_lua.get_field(lua, -10002, "_ab_release_hat");
            damage_lua.push_string(lua, "bird");
            if (damage_lua.call(lua, 1, 0, 0))
                fail("Original hat release failed\n");
        }
        damage_lua.get_field(lua, -10002, "hatcounter");
        hat_scalar(lua, 1, hat_output);
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "g_birdHats");
        damage_lua.get_field(lua, 1, "bird");
        hat_output[2] = damage_lua.type(lua, 2) == 5;
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, "_ab_held_hat");
        if (damage_lua.type(lua, 1) == 5) {
            damage_lua.get_field(lua, 1, "dx");
            hat_output[3] = hat_number(lua, 2);
            damage_lua.set_top(lua, 1);
            damage_lua.get_field(lua, 1, "dy");
            hat_output[4] = hat_number(lua, 2);
            damage_lua.set_top(lua, 1);
            damage_lua.get_field(lua, 1, "definition");
            hat_output[5] = damage_named(lua, 2, "block_a") ? 1u : 2u;
            damage_lua.set_top(lua, 1);
            damage_lua.get_field(lua, 1, "jointDestroyed");
            hat_scalar(lua, 2, hat_output + 6);
            damage_lua.set_top(lua, 0);
            damage_lua.get_field(lua, -10002, "objects");
            damage_lua.get_field(lua, 1, "world");
            damage_lua.get_field(lua, 2, hat_object_name);
            damage_lua.get_field(lua, 3, "defence");
            hat_output[8] = hat_number(lua, 4);
        }
        damage_lua.set_top(lua, 0);
        hat_output[48] = hat_events;
        print_words(hat_output, AB_BIRD_HAT_WORDS);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB bird-hat reference complete\n");
}

#endif
