#ifndef AB_ORACLE_GAME_GLUE_POLICY_H
#define AB_ORACLE_GAME_GLUE_POLICY_H

static void seed_glue_policy(uint32_t *game, void *wrapper, void *lua,
                             const uint32_t bodies[2], const uint32_t input[28]) {
    uint32_t *world = (uint32_t *)(uintptr_t)game[0x1784 / 4];
    if (input[0] & 1u) {
        uint32_t registered[28];
        for (size_t field = 0; field < 28; ++field) {
            registered[field] = input[field];
        }
        registered[1] = 2;
        game_joint_lua_input(lua, registered);
        damage_lua.push_string(lua, "registered");
        damage_lua.set_field(lua, -2, "name");
        int (*create)(void *, void *) = (int (*)(void *, void *))find_function(
            "_ZN7GameLua14createJointLuaEPN3lua8LuaStateE");
        if (input[0] & 128u) {
            world[0x191d4 / 4] |= 2u;
        }
        if (create(game, wrapper) != 0) {
            fail("Cannot create the registered glue-policy joint\n");
        }
        world[0x191d4 / 4] &= ~UINT32_C(2);
        uint32_t *joint = (uint32_t *)(uintptr_t)world[0x1922c / 4];
        if (joint) {
            joint[16] = 1;
        }
    }
    if (input[0] & 2u) {
        const uint32_t definition[12] = {8, 2, bodies[0], bodies[1]};
        void *(*create)(void *, const void *) =
            (void *(*)(void *, const void *))find_function(
                "_ZN7b2World11CreateJointEPK10b2JointDef");
        if (!create(world, definition)) {
            fail("Cannot create the unregistered glue-policy joint\n");
        }
    }
    for (size_t index = 0; index < 2; ++index) {
        uint32_t *body = (uint32_t *)(uintptr_t)bodies[index];
        body[1] = (body[1] & ~UINT32_C(2)) | (((input[0] >> (4 + index)) & 1u) << 1);
        body[36] = index ? UINT32_C(0x3e000000) : UINT32_C(0x3e800000);
    }
}

static void print_glue_policy(uint32_t *game, void *lua, const uint32_t bodies[2]) {
    void (*reference)(void *) =
        (void (*)(void *))find_function("_ZNK3lua6detail7RefBase6getRefEv");
    damage_lua.set_top(lua, 0);
    reference((uint8_t *)game + 0x17f8);
    damage_lua.get_field(lua, -1, "joints");
    damage_lua.get_field(lua, -1, "registered");
    uint32_t *world = (uint32_t *)(uintptr_t)game[0x1784 / 4];
    const uint32_t counts[] = {
        world[0x19234 / 4], (game[0x17e4 / 4] - game[0x17e0 / 4]) / 20,
        (game[0x17f0 / 4] - game[0x17ec / 4]) / 20, (uint32_t)damage_lua.type(lua, -1),
        (world[0x191d4 / 4] & 2u) ? 1u : 0u};
    print_words(counts, 5);
    damage_lua.set_top(lua, 0);
    for (size_t index = 0; index < 2; ++index) {
        const uint32_t *body = (const uint32_t *)(uintptr_t)bodies[index];
        const uint32_t state[] = {body[1] & UINT32_C(0xffff), body[36]};
        print_words(state, 2);
    }
    const uint32_t *joint = (const uint32_t *)(uintptr_t)world[0x1922c / 4];
    size_t count = 0;
    while (joint) {
        if (++count > 4) {
            fail("Native glue-policy joint list exceeds its bound\n");
        }
        const uint32_t metadata[] = {
            joint[16], joint[12] == bodies[0] ? 0u : 1u,
            joint[13] == bodies[0] ? 0u : 1u,
            joint[2] ? ((const uint32_t *)(uintptr_t)joint[2])[16] : UINT32_MAX,
            joint[3] ? ((const uint32_t *)(uintptr_t)joint[3])[16] : UINT32_MAX};
        print_words(metadata, 5);
        print_game_joint_definition(joint, 2);
        joint = (const uint32_t *)(uintptr_t)joint[3];
    }
    if (count != counts[0]) {
        fail("Native glue-policy joint count disagrees\n");
    }
}

#endif
