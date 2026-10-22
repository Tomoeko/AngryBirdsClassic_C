#ifndef AB_ORACLE_DAMAGE_PARTICLES_H
#define AB_ORACLE_DAMAGE_PARTICLES_H

#include "../../tests/damage_particles_cases.h"

static uint32_t damage_particles_input[10], damage_particles_events[2][6];
static uint32_t damage_particles_count;

static int damage_particles_transform(void *lua) {
    if (damage_lua.get_top(lua) != 2 || damage_particles_count) {
        fail("Unexpected damage-particle transform callback\n");
    }
    uint32_t *event = damage_particles_events[damage_particles_count++];
    event[0] = 1;
    for (int argument = 1; argument <= 2; ++argument) {
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[argument] = value.bits;
        event[argument + 2] = damage_particles_input[3 + argument];
    }
    event[5] = 0;
    uint32_t mutations = damage_particles_input[8];
    if (damage_particles_input[1] == 1) {
        damage_lua.get_field(lua, -10002, "_ab_particle_descriptor");
        if (mutations & 1u) {
            damage_lua.push_string(lua, "second");
            damage_lua.set_field(lua, 3, "name");
        }
        if (mutations & 2u) {
            damage_number(lua, 3, "amount", damage_particles_input[7]);
        }
        damage_lua.set_top(lua, 2);
        if (mutations & 4u) {
            damage_lua.get_field(lua, -10002, "_ab_particle_raw_definition");
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_string(lua, "alternate");
            damage_lua.set_field(lua, 4, "name");
            damage_lua.push_number(lua, 7);
            damage_lua.set_field(lua, 4, "amount");
            damage_lua.set_field(lua, 3, "damageStateChangedParticles");
            damage_lua.set_top(lua, 2);
        }
    }
    for (size_t field = 4; field <= 5; ++field) {
        FloatBits value = {.bits = damage_particles_input[field]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static int damage_particles_emit(void *lua) {
    if (damage_lua.get_top(lua) != 9 || damage_particles_count != 1 ||
        damage_lua.type(lua, 1) != 4) {
        fail("Unexpected damage-particle emission callback\n");
    }
    uint32_t *event = damage_particles_events[damage_particles_count++];
    event[0] = 2;
    if (damage_named(lua, 1, "first")) {
        event[1] = 1;
    } else if (damage_named(lua, 1, "second")) {
        event[1] = 2;
    } else {
        fail("Unexpected damage-particle descriptor selection\n");
    }
    for (int argument = 2; argument <= 7; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Damage-particle emission argument is not numeric\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        if (argument <= 4) {
            event[argument] = value.bits;
        } else if (value.bits) {
            fail("Damage-particle trailing numeric argument is not positive zero\n");
        }
    }
    for (int argument = 8; argument <= 9; ++argument) {
        if (damage_lua.type(lua, argument) != 1 ||
            damage_lua.to_boolean(lua, argument)) {
            fail("Damage-particle trailing Boolean argument is not false\n");
        }
    }
    event[5] = 0;
    return 0;
}

static void probe_damage_particles(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original damage-particle oracle state\n");
    }
    int file = system_call(
        5, (int)(uintptr_t)"/data/local/tmp/ab520/collision/damage-particles.lua", NULL,
        0);
    char chunk[350];
    if (file < 0 || system_call(3, file, chunk, sizeof(chunk)) != sizeof(chunk)) {
        fail("Cannot read original damage-particle prototype\n");
    }
    system_call(6, file, NULL, 0);
    if (damage_lua.load(lua, chunk, sizeof(chunk), "damage-particles-reference")) {
        fail("Cannot load original damage-particle prototype\n");
    }
    damage_lua.set_field(lua, -10002, "_ab_damage_particles");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "addParticles", damage_particles_emit);
    damage_lua.set_field(lua, -10002, "particles");
    semihost(4, "AB_ANDROID_DAMAGE_PARTICLES_V1 4096\n");
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        damage_particles_case(sample, damage_particles_input);
        damage_particles_count = 0;
        print_words(damage_particles_input, 10);
        uint32_t kind = damage_particles_input[1];
        if (kind == 1) {
            damage_lua.create_table(lua, 0, 2);
            damage_lua.push_string(lua, damage_particles_input[9] ? "second" : "first");
            damage_lua.set_field(lua, 1, "name");
            damage_number(lua, 1, "amount", damage_particles_input[6]);
        } else if (kind == 2 || kind == 3) {
            damage_lua.push_boolean(lua, kind == 3);
        } else if (kind == 4) {
            damage_lua.push_number(lua, 0);
        } else if (kind == 5) {
            damage_lua.push_string(lua, "invalid");
        } else {
            damage_lua.push_nil(lua);
        }
        damage_lua.set_field(lua, -10002, "_ab_particle_descriptor");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, "_ab_particle_descriptor");
        damage_lua.set_field(lua, 1, "damageStateChangedParticles");
        damage_lua.set_field(lua, -10002, "_ab_particle_raw_definition");
        damage_lua.create_table(lua, 0, 2);
        damage_function(lua, 1, "physicsToWorldTransform", damage_particles_transform);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.get_field(lua, -10002, "_ab_particle_raw_definition");
        damage_lua.set_field(lua, 3, "Object");
        damage_lua.set_field(lua, 2, "blocks");
        damage_lua.set_field(lua, 1, "blockTable");
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, -10002, "_ab_damage_particles");
        damage_lua.create_table(lua, 0, 3);
        damage_lua.push_string(lua, "Object");
        damage_lua.set_field(lua, 2, "definition");
        damage_number(lua, 2, "x", damage_particles_input[2]);
        damage_number(lua, 2, "y", damage_particles_input[3]);
        int status = damage_lua.call(lua, 1, 0, 0);
        uint32_t result[] = {status != 0, damage_particles_count};
        print_words(result, 2);
        damage_lua.set_top(lua, 0);
        for (uint32_t event = 0; event < damage_particles_count; ++event) {
            print_words(damage_particles_events[event], 6);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB damage particles reference complete\n");
}

#endif
