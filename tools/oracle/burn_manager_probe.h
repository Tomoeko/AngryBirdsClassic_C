#ifndef AB_ORACLE_BURN_MANAGER_H
#define AB_ORACLE_BURN_MANAGER_H

#include "../../tests/burn_manager_cases.h"

static void burn_manager_snapshot(void *lua, uint32_t output[4]) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "BurnManager");
    damage_lua.get_field(lua, 1, "update");
    const uint8_t *closure = damage_lua.to_pointer(lua, 2);
    if (damage_lua.type(lua, 2) != 6 || !closure || closure[6] || closure[7] != 3)
        fail("Unexpected original burn update closure\n");
    /* Stripped debug names prevent lua_getupvalue from returning these slots.
     * Rechecked target ABI at Binary Ninja VA 0x83b2c4: LClosure.upvals at 0x14,
     * UpVal.v at 8, TValue value/type at 0/4. Read checked closed values only;
     * do not modify the original closure, state, prototype or instructions. */
    output[0] = 0;
    output[1] = 0;
    output[2] = 0;
    for (size_t index = 0; index < 3; ++index) {
        const uint32_t *upvalue =
            (const uint32_t *)(uintptr_t)((const uint32_t *)closure)[5 + index];
        if (!upvalue || upvalue[2] != (uint32_t)(uintptr_t)(upvalue + 3))
            fail("Expected a closed original burn upvalue\n");
        const uint32_t *value = (const uint32_t *)(uintptr_t)upvalue[2];
        uint32_t kind = value[1];
        if (index == 0) {
            if (kind != 1)
                fail("Unexpected original burn pending flag\n");
            output[0] = value[0] != 0;
        } else if (index == 1) {
            if (kind != 0 && kind != 3)
                fail("Unexpected original burn deadline\n");
            if (kind == 3) {
                output[0] |= 2;
                output[1] = value[0];
            }
        } else {
            if (kind != 3)
                fail("Unexpected original burn duration\n");
            output[2] = value[0];
        }
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "isBurningObjects");
    damage_lua.push_nil(lua);
    if (damage_lua.call(lua, 1, 1, 0) || damage_lua.type(lua, 2) != 1)
        fail("Original burn-status query failed\n");
    output[3] = (uint32_t)(damage_lua.to_boolean(lua, 2) != 0);
    damage_lua.set_top(lua, 0);
}

static void probe_burn_manager(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original burn manager state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    load_damage_chunk(lua, "/data/local/tmp/ab520/burn-manager.lua");
    static const char *const methods[] = {"addBurnTime", "update", "reset",
                                          "isBurningObjects"};
    semihost(4, "AB_ANDROID_BURN_MANAGER_V1 24\n");
    for (size_t index = 0; index < AB_BURN_MANAGER_CASES; ++index) {
        const uint32_t *input = burn_manager_cases[index];
        print_words(input, 3);
        damage_lua.get_field(lua, -10002, "BurnManager");
        damage_lua.get_field(lua, 1, methods[input[0]]);
        damage_lua.push_nil(lua);
        int arguments = 1;
        if (input[0] == 0 || input[0] == 1) {
            FloatBits value = {.bits = input[input[0] == 1 ? 2 : 1]};
            damage_lua.push_number(lua, value.number);
            ++arguments;
        }
        if (input[0] == 1) {
            FloatBits time = {.bits = input[1]};
            damage_lua.push_number(lua, time.number);
            ++arguments;
        }
        if (damage_lua.call(lua, arguments, 0, 0))
            fail("Original burn manager operation failed\n");
        uint32_t output[4];
        burn_manager_snapshot(lua, output);
        print_words(output, 4);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB burn manager reference complete\n");
}

#endif
