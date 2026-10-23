#ifndef AB_ORACLE_FLOATING_SCORE_DRAW_H
#define AB_ORACLE_FLOATING_SCORE_DRAW_H

/* Execute the complete original GameHud.draw with unrelated branches disabled.
 * The score loop receives one retained source record and controlled world/camera
 * services. Record actual resource-call coordinates and render-state calls. */
static uint32_t floating_draw_input[8];

static int floating_draw_transform(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected floating-score transform arity\n");
    for (int index = 1; index <= 2; ++index) {
        float value = damage_lua.to_number(lua, index);
        damage_lua.push_number(lua, value * 20.0f);
    }
    return 2;
}

static int floating_draw_camera_value(void *lua, size_t index) {
    FloatBits value = {.bits = floating_draw_input[index]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int floating_draw_zoom(void *lua) {
    return floating_draw_camera_value(lua, 4);
}

static int floating_draw_left(void *lua) {
    return floating_draw_camera_value(lua, 2);
}

static int floating_draw_top(void *lua) {
    return floating_draw_camera_value(lua, 3);
}

static int floating_draw_resource(void *lua, uint32_t kind, const char *group) {
    if (damage_lua.get_top(lua) != 6 || !damage_named(lua, 1, group) ||
        !damage_named(lua, 5, "BOTTOM") || !damage_named(lua, 6, "HCENTER"))
        fail("Unexpected original floating-score resource call\n");
    uint32_t *event = hud_event(kind);
    for (int index = 3; index <= 4; ++index) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        event[index - 1] = value.bits;
    }
    return 0;
}

static int floating_draw_text(void *lua) {
    return floating_draw_resource(lua, 6, "TEXTS_BASIC");
}

static int floating_draw_sprite(void *lua) {
    return floating_draw_resource(lua, 7, "MENU_ELEMENTS_1");
}

static void probe_floating_score_draw(void *lua) {
    void (*raw_set)(void *, int, int) =
        (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    damage_lua.get_field(lua, -10002, "res");
    damage_function(lua, 1, "drawString", floating_draw_text);
    damage_function(lua, 1, "drawSprite", floating_draw_sprite);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_function(lua, 1, "physicsToWorldTransform", floating_draw_transform);
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 2, "getZoomScale", floating_draw_zoom);
    damage_function(lua, 2, "getScreenLeft", floating_draw_left);
    damage_function(lua, 2, "getScreenTop", floating_draw_top);
    damage_lua.set_field(lua, 1, "cameraSystem");
    damage_lua.set_top(lua, 0);
    uint32_t random = UINT32_C(0x520f10a7);
    semihost(4, "AB_ANDROID_FLOATING_SCORE_DRAW_V1 1024\n");
    for (size_t sample = 0; sample < 1024; ++sample) {
        for (size_t field = 0; field < 4; ++field) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 8193) - 4096) / 17};
            floating_draw_input[field] = value.bits;
        }
        const float scales[] = {0.25f, 0.5f, 0.7f, 1, 1.25f, 1.5f, 2, 3};
        FloatBits zoom = {.number = scales[sample % 8]};
        FloatBits ui_scale = {.number = scales[(sample / 8) % 8]};
        FloatBits xs = {.number = (float)(1 + sample % 31) / 19};
        if (sample & 32)
            xs.number = -xs.number;
        floating_draw_input[4] = zoom.bits;
        floating_draw_input[5] = ui_scale.bits;
        floating_draw_input[6] = xs.bits;
        floating_draw_input[7] = (uint32_t)sample & 3;
        for (size_t field = 0; field < 11; ++field)
            hud_input[field] = 0;
        hud_input[0] = UINT32_C(0x43f00000);
        hud_input[1] = UINT32_C(0x43a00000);
        hud_input[2] = ui_scale.bits;
        hud_input[7] = UINT32_C(0x41d00000);
        hud_input[9] = 8 | 4; /* showScore, hide static scores only */
        hud_case(lua);
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.create_table(lua, 1, 0);
        damage_lua.create_table(lua, 0, 5);
        damage_number(lua, 3, "x", floating_draw_input[0]);
        damage_number(lua, 3, "y", floating_draw_input[1]);
        damage_number(lua, 3, "xs", xs.bits);
        if (sample & 1) {
            damage_lua.push_string(lua, "750");
            damage_lua.set_field(lua, 3, "text");
        }
        if (sample & 2) {
            damage_lua.push_string(lua, "PIG_SCORE_5000");
            damage_lua.set_field(lua, 3, "sprite");
        }
        raw_set(lua, 2, 1);
        damage_lua.set_field(lua, 1, "floatingScores");
        damage_lua.set_top(lua, 0);
        hud_event_count = 0;
        damage_lua.get_field(lua, -10002, "GameHud");
        damage_lua.get_field(lua, 1, "draw");
        damage_lua.get_field(lua, -10002, "_ab_hud");
        for (size_t index = 0; index < 5; ++index)
            damage_lua.push_number(lua, index == 2 || index == 3 ? 1 : 0);
        if (damage_lua.call(lua, 6, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original floating-score draw failed\n");
        }
        damage_lua.set_top(lua, 0);
        print_words(floating_draw_input, 8);
        print_words(&hud_event_count, 1);
        for (size_t event = 0; event < hud_event_count; ++event)
            print_words(hud_events[event], 12);
    }
    semihost(4, "AB floating score draw reference complete\n");
}

#endif
