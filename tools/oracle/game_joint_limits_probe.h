#ifndef AB_ORACLE_GAME_JOINT_LIMITS_H
#define AB_ORACLE_GAME_JOINT_LIMITS_H

static void print_joint_limit_bodies(const uint32_t bodies[2]) {
    for (size_t index = 0; index < 2; ++index) {
        const uint32_t *body = (const uint32_t *)(uintptr_t)bodies[index];
        const uint32_t output[] = {body[1] & UINT32_C(0xffff), body[36]};
        print_words(output, 2);
    }
}

static void probe_joint_limit_state(uint32_t *game, uint32_t *joint,
                                    const uint32_t bodies[2], const uint32_t record[5],
                                    const uint32_t input[28]) {
    typedef void (*SetTransform)(void *, const void *, float);
    SetTransform set_transform =
        (SetTransform)find_function("_ZN6b2Body12SetTransformERK6b2Vec2f");
    void (*check)(void *, const void *) = (void (*)(void *, const void *))find_function(
        "_ZN7GameLua16checkJointLimitsESs");
    float (*coordinate)(const void *) = (float (*)(const void *))find_function(
        input[1] == 3 ? "_ZNK15b2RevoluteJoint13GetJointAngleEv"
                      : "_ZNK16b2PrismaticJoint19GetJointTranslationEv");
    void (*set_speed)(void *, float) = (void (*)(void *, float))find_function(
        input[1] == 3 ? "_ZN15b2RevoluteJoint13SetMotorSpeedEf"
                      : "_ZN16b2PrismaticJoint13SetMotorSpeedEf");
    uint32_t before_joint[64], before_bodies[2][38];
    for (size_t index = 0; index < 2; ++index) {
        uint32_t *body = (uint32_t *)(uintptr_t)bodies[index];
        FloatBits x = {.bits = input[4 + index * 3]};
        FloatBits y = {.bits = input[5 + index * 3]};
        FloatBits angle = {.bits = input[6 + index * 3]};
        FloatBits delta = {.bits = input[24 + index]};
        angle.number += delta.number;
        if (index) {
            FloatBits dx = {.bits = input[26]}, dy = {.bits = input[27]};
            x.number += dx.number;
            y.number += dy.number;
        }
        const float position[] = {x.number, y.number};
        set_transform(body, position, angle.number);
        const uint32_t frame[] = {body[3], body[4], body[5], body[6], body[14]};
        print_words(frame, 5);
        body[1] = (body[1] & ~UINT32_C(2)) | (((input[0] >> (4 + index)) & 1u) << 1);
        body[36] = index ? UINT32_C(0x3e000000) : UINT32_C(0x3e800000);
        for (size_t field = 0; field < 38; ++field) {
            before_bodies[index][field] = body[field];
        }
    }
    size_t impulse = input[1] == 3 ? 21u : 26u;
    joint[impulse] = input[14];
    joint[impulse + 1] = input[15];
    joint[impulse + 2] = input[22];
    joint[input[1] == 3 ? 24 : 29] = input[16];
    joint[input[1] == 3 ? 56 : 35] = input[0] % 4;
    for (size_t field = 0; field < 64; ++field) {
        before_joint[field] = joint[field];
    }
    FloatBits current = {.number = coordinate(joint)};
    print_words(&current.bits, 1);
    print_joint_limit_bodies(bodies);
    uint32_t *world = (uint32_t *)(uintptr_t)game[0x1784 / 4];
    if (input[0] & 1024u) {
        world[0x191d4 / 4] |= 2u;
    }
    size_t speed = input[1] == 3 ? 27u : 33u;
    if (input[0] & 512u) {
        FloatBits value = {.bits = joint[speed]};
        set_speed(joint, value.number);
    }
    print_joint_limit_bodies(bodies);
    check(game, record);
    print_game_joint_definition(joint, input[1]);
    print_joint_limit_bodies(bodies);
    for (size_t field = 0; field < 64; ++field) {
        if (field != speed && joint[field] != before_joint[field]) {
            fail("Original limit check changed a retained joint field\n");
        }
    }
    for (size_t index = 0; index < 2; ++index) {
        const uint32_t *body = (const uint32_t *)(uintptr_t)bodies[index];
        for (size_t field = 0; field < 38; ++field) {
            if (field != 1 && field != 36 &&
                body[field] != before_bodies[index][field]) {
                fail("Original limit check changed a preserved body field\n");
            }
        }
        if ((body[1] & ~UINT32_C(2)) != (before_bodies[index][1] & ~UINT32_C(2))) {
            fail("Original limit check changed a non-awake body flag\n");
        }
    }
    world[0x191d4 / 4] &= ~UINT32_C(2);
}

#endif
