#ifndef AB_ORACLE_CAMERA_SHAKE_H
#define AB_ORACLE_CAMERA_SHAKE_H

static void shake_snapshot(void *lua, uint32_t output[8]) {
    damage_lua.get_field(lua, -10002, "_ab_shake");
    const char *fields[] = {"explosionCameraShakeDuration",
                            "explosionCameraShakeAmount", "cameraShake", "cameraShakeX",
                            "cameraShakeY"};
    for (size_t field = 0; field < 5; ++field) {
        damage_lua.get_field(lua, field < 2 ? 1 : -10002, fields[field]);
        int type = damage_lua.type(lua, 2);
        if (type != 0 && type != 3) {
            fail("Unexpected original camera shake state type\n");
        }
        if (type == 3) {
            if (field < 3) {
                output[0] |= 1u << field;
            }
            FloatBits value = {.number = damage_lua.to_number(lua, 2)};
            output[1 + field] = value.bits;
        }
        damage_lua.set_top(lua, 1);
    }
    damage_lua.set_top(lua, 0);
    unsigned short *(*seed48)(const unsigned short *) =
        (unsigned short *(*)(const unsigned short *))find_function("seed48");
    const unsigned short zero[3] = {0};
    const unsigned short *previous = seed48(zero);
    unsigned short copy[3] = {previous[0], previous[1], previous[2]};
    output[6] = (uint32_t)copy[0] | (uint32_t)copy[1] << 16;
    output[7] = copy[2];
    seed48(copy);
}

static void shake_command(void *lua, const uint32_t input[3]) {
    if (input[0] == 3) {
        if (input[2]) {
            damage_number(lua, -10002, "cameraShake", input[1]);
        } else {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -10002, "cameraShake");
        }
    } else {
        const char *name = input[0] == 0   ? "shake-add"
                           : input[0] == 1 ? "shake-remove"
                                           : "shake-update";
        damage_lua.get_field(lua, -10002, name);
        damage_lua.get_field(lua, -10002, "_ab_shake");
        int arguments = 1;
        if (input[0] != 1) {
            FloatBits value = {.bits = input[1]};
            damage_lua.push_number(lua, value.number);
            ++arguments;
        }
        if (input[0] == 0) {
            FloatBits amount = {.bits = input[2]};
            damage_lua.push_number(lua, amount.number);
            ++arguments;
        }
        if (damage_lua.call(lua, arguments, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original shake method failed\n");
        }
    }
    damage_lua.set_top(lua, 0);
    uint32_t output[8] = {0};
    shake_snapshot(lua, output);
    print_words(output, 8);
}

static void probe_camera_shake(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original camera shake oracle state\n");
    }
    (void)damage_lua.open_base(lua);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"shake-add", "shake-remove", "shake-update"};
    const char *paths[] = {"/data/local/tmp/ab520/camera/shake-add-oracle.lua",
                           "/data/local/tmp/ab520/camera/shake-remove-oracle.lua",
                           "/data/local/tmp/ab520/camera/shake-update-oracle.lua"};
    for (size_t index = 0; index < 3; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    void (*seed)(long) = (void (*)(long))find_function("srand48");
    static const float amounts[] = {0, -0.0f, 0.25f, -0.5f, 1, -4, 2.75f, 127};
    static const float durations[] = {0, -0.0f, -1, 0.125f, 0.5f, 1, 2, 8};
    static const float seconds[] = {0, -0.125f, 0x1.111112p-5f, 0.125f, 0.5f, 2};
    uint32_t random = UINT32_C(0x520ca5a1);
    semihost(4, "AB_ANDROID_CAMERA_SHAKE_V1 128 64\n");
    for (size_t item = 0; item < 128; ++item) {
        uint32_t seed_word = random_word(&random);
        print_words(&seed_word, 1);
        seed((long)(int32_t)seed_word);
        damage_lua.create_table(lua, 0, 2);
        damage_lua.set_field(lua, -10002, "_ab_shake");
        damage_number(lua, -10002, "cameraShake", 0);
        damage_number(lua, -10002, "cameraShakeX", 0);
        damage_number(lua, -10002, "cameraShakeY", 0);
        for (size_t step = 0; step < 64; ++step) {
            uint32_t operation = random_word(&random) % 8;
            FloatBits duration = {.number = durations[random_word(&random) % 8]};
            FloatBits amount = {.number = amounts[random_word(&random) % 8]};
            FloatBits elapsed = {.number = seconds[random_word(&random) % 6]};
            uint32_t input[3] = {operation < 4 ? operation : 2u, duration.bits,
                                 amount.bits};
            if (input[0] == 2) {
                input[1] = elapsed.bits;
            } else if (input[0] == 3) {
                input[1] = amount.bits;
                input[2] = (uint32_t)(step % 3 != 0);
            }
            if (step < 12) {
                input[0] = step == 0 ? 3u : step == 1 ? 0u : step == 10 ? 1u : 2u;
                amount.number = amounts[item % 8];
                duration.number = 0.125f;
                elapsed.number = step == 2 ? 0.125f : 0.0625f;
                input[1] = step == 0   ? amount.bits
                           : step == 1 ? duration.bits
                                       : elapsed.bits;
                input[2] = step == 0 ? (uint32_t)(item % 2) : amount.bits;
            }
            print_words(input, 3);
            shake_command(lua, input);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB camera shake reference complete\n");
}

#endif
