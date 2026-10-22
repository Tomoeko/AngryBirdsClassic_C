#ifndef AB_ORACLE_SLING_DRAW_H
#define AB_ORACLE_SLING_DRAW_H

static uint32_t sling_draw_input[12], sling_draw_events[5][9];
static uint32_t sling_draw_count, sling_draw_camera_events[3], sling_draw_camera_count;
static uint32_t *sling_draw_disabled;

static uint32_t *sling_draw_event(uint32_t kind) {
    if (sling_draw_count >= 5) {
        fail("Unexpected original sling draw event count\n");
    }
    uint32_t *event = sling_draw_events[sling_draw_count++];
    for (size_t field = 0; field < 9; ++field) {
        event[field] = 0;
    }
    event[0] = kind;
    return event;
}

static void sling_draw_numbers(void *lua, int first, size_t count, uint32_t *output) {
    for (size_t field = 0; field < count; ++field) {
        int index = first + (int)field;
        if (damage_lua.type(lua, index) != 3) {
            fail("Unexpected original sling draw numeric argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[field] = value.bits;
    }
}

static int sling_draw_state(void *lua) {
    if (damage_lua.get_top(lua) != 7) {
        fail("Unexpected original sling render-state arguments\n");
    }
    uint32_t *event = sling_draw_event(1);
    sling_draw_numbers(lua, 1, 7, event + 1);
    return 0;
}

static int sling_draw_sprite(void *lua) {
    if (damage_lua.get_top(lua) != 3) {
        fail("Unexpected original sling sprite arguments\n");
    }
    uint32_t *event = sling_draw_event(2);
    const char *names[] = {"SLING_SHOT_01_BACK", "SLING_HOLDER", "SLING_SHOT_01_FRONT",
                           "SLINGSHOT_POWERUP_BASIC"};
    unsigned index = 0;
    while (index < 4 && !damage_named(lua, 1, names[index])) {
        ++index;
    }
    if (index == 4) {
        fail("Unexpected original base sling sprite selection\n");
    }
    event[1] = index;
    sling_draw_numbers(lua, 2, 2, event + 2);
    return 0;
}

static int sling_draw_band(void *lua) {
    if (damage_lua.get_top(lua) != 6 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original sling band arguments\n");
    }
    uint32_t *event = sling_draw_event(3);
    sling_draw_numbers(lua, 2, 5, event + 1);
    return 0;
}

static int sling_draw_extras(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original sling extras arguments\n");
    }
    (void)sling_draw_event(4);
    return 0;
}

static int sling_draw_camera(void *lua, unsigned field) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5 ||
        sling_draw_camera_count >= 3) {
        fail("Unexpected original sling rendering camera boundary\n");
    }
    sling_draw_camera_events[sling_draw_camera_count++] = field + 1;
    FloatBits value = {.bits = sling_draw_input[6 + field]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int sling_draw_left(void *lua) {
    return sling_draw_camera(lua, 0);
}

static int sling_draw_top(void *lua) {
    return sling_draw_camera(lua, 1);
}

static int sling_draw_zoom(void *lua) {
    return sling_draw_camera(lua, 2);
}

static int sling_draw_dragged(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original sling dragged query\n");
    }
    damage_lua.push_boolean(lua, (sling_draw_input[11] & 2) != 0);
    return 1;
}

static int sling_draw_pivot(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "SLING_HOLDER")) {
        fail("Unexpected original sling holder pivot query\n");
    }
    for (size_t field = 9; field < 11; ++field) {
        FloatBits value = {.bits = sling_draw_input[field]};
        damage_lua.push_number(lua, value.number);
    }
    return 2;
}

static void sling_draw_call(void *lua, int arguments) {
    if (damage_lua.call(lua, arguments, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original sling draw call failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void sling_draw_prepare(void *lua) {
    damage_function(lua, -10002, "setRenderState", sling_draw_state);
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "drawSprite", sling_draw_sprite);
    damage_function(lua, 1, "getSpritePivot", sling_draw_pivot);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "getScreenLeftForRendering", sling_draw_left);
    damage_function(lua, 1, "getScreenTopForRendering", sling_draw_top);
    damage_function(lua, 1, "getZoomScale", sling_draw_zoom);
    damage_lua.set_field(lua, -10002, "cameraSystem");

    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "disableSlingshotForCurrentLevel");
    const uint32_t *closure = damage_lua.to_pointer(lua, 2);
    const uint8_t *header = (const uint8_t *)closure;
    if (!closure || damage_lua.type(lua, 2) != 6 || header[6] || header[7] != 1) {
        fail("Unexpected original sling disabled closure layout\n");
    }
    uint32_t *upvalue = (uint32_t *)(uintptr_t)closure[5];
    sling_draw_disabled = (uint32_t *)(uintptr_t)upvalue[2];
    if (sling_draw_disabled[1] != 1) {
        fail("Unexpected original sling disabled value type\n");
    }
    damage_lua.set_top(lua, 0);
}

static void sling_draw_self(void *lua) {
    sling_draw_disabled[0] = sling_draw_input[11] & 1;
    damage_lua.create_table(lua, 0, 8);
    damage_function(lua, 1, "isBirdDragged", sling_draw_dragged);
    damage_function(lua, 1, "drawRubberBandLine", sling_draw_band);
    const char *vectors[] = {"pos", "stretchPos"};
    for (size_t index = 0; index < 2; ++index) {
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, 2, "x", sling_draw_input[index * 2]);
        damage_number(lua, 2, "y", sling_draw_input[index * 2 + 1]);
        damage_lua.set_field(lua, 1, vectors[index]);
    }
    damage_number(lua, 1, "rubberBandWidth", sling_draw_input[4]);
    damage_number(lua, 1, "rubberBandAngle", sling_draw_input[5]);
    damage_lua.create_table(lua, 0, 6);
    damage_function(lua, 2, "drawExtras", sling_draw_extras);
    damage_lua.push_boolean(lua, (sling_draw_input[11] & 4) != 0);
    damage_lua.set_field(lua, 2, "hasSlingScope");
    damage_lua.get_field(lua, -10002, "Slingshot");
    const char *getters[] = {"getBackSprite", "getSlingHolderSprite", "getFrontSprite"};
    for (size_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, 3, getters[index]);
        damage_lua.set_field(lua, 2, getters[index]);
    }
    damage_lua.get_field(lua, 3, "refreshSprites");
    damage_lua.set_field(lua, 2, "refreshSprites");
    damage_lua.set_top(lua, 2);
    damage_lua.set_field(lua, 1, "slingshot");
    damage_lua.set_field(lua, -10002, "_ab_sling_draw");
    damage_lua.get_field(lua, -10002, "_ab_sling_draw");
    damage_lua.get_field(lua, 1, "slingshot");
    damage_lua.get_field(lua, 2, "refreshSprites");
    damage_lua.get_field(lua, 1, "slingshot");
    sling_draw_call(lua, 1);
}

static void probe_sling_draw(void) {
    void *lua = open_sling_reference();
    sling_draw_prepare(lua);
    uint32_t random = UINT32_C(0x520511d2);
    const char *parts[] = {"drawBackPart", "drawSlingHolder", "drawFrontPart"};
    semihost(4, "AB_ANDROID_SLING_DRAW_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        for (size_t field = 0; field < 11; ++field) {
            sling_draw_input[field] = random_float(&random, 122, 11, 1);
        }
        sling_draw_input[8] = random_float(&random, 125, 4, 1);
        sling_draw_input[11] = (uint32_t)item & 7;
        if (item % 17 == 0) {
            sling_draw_input[8] &= UINT32_C(0x80000000);
        }
        if (item % 23 == 0) {
            for (size_t field = 0; field < 11; ++field) {
                sling_draw_input[field] &= UINT32_C(0x80000000);
            }
        }
        sling_draw_self(lua);
        print_words(sling_draw_input, 12);
        for (size_t part = 0; part < 3; ++part) {
            sling_draw_count = sling_draw_camera_count = 0;
            for (size_t field = 0; field < 3; ++field) {
                sling_draw_camera_events[field] = 0;
            }
            damage_lua.get_field(lua, -10002, "SlingshotSystem");
            damage_lua.get_field(lua, 1, parts[part]);
            damage_lua.get_field(lua, -10002, "_ab_sling_draw");
            sling_draw_call(lua, 1);
            if (sling_draw_camera_count != ((sling_draw_input[11] & 1) ? 0u : 3u)) {
                fail("Incomplete original sling camera events\n");
            }
            print_words(&sling_draw_count, 1);
            print_words(sling_draw_camera_events, 3);
            for (size_t event = 0; event < sling_draw_count; ++event) {
                print_words(sling_draw_events[event], 9);
            }
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling draw reference complete\n");
}

#endif
