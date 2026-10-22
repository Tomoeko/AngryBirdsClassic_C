#ifndef AB_ORACLE_GAME_HUD_DRAW_H
#define AB_ORACLE_GAME_HUD_DRAW_H

static uint32_t hud_input[11], hud_events[32][12], hud_event_count;

static uint32_t *hud_event(uint32_t kind) {
    if (hud_event_count >= 32) {
        fail("Original HUD exceeds controlled event storage\n");
    }
    uint32_t *event = hud_events[hud_event_count++];
    for (size_t index = 0; index < 12; ++index) {
        event[index] = 0;
    }
    event[0] = kind;
    return event;
}

static int hud_numbers(void *lua, uint32_t kind, int first) {
    int count = damage_lua.get_top(lua) - first + 1;
    if (count < 0 || count > 8) {
        fail("Unexpected original HUD numeric arity\n");
    }
    uint32_t *event = hud_event(kind);
    event[1] = (uint32_t)count;
    for (int index = 0; index < count; ++index) {
        if (damage_lua.type(lua, first + index) != 3) {
            fail("Unexpected original HUD numeric argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, first + index)};
        event[2 + (size_t)index] = value.bits;
    }
    return 0;
}

static int hud_state(void *lua) {
    return hud_numbers(lua, 2, 1);
}

static int hud_frame(void *lua) {
    return hud_numbers(lua, 5, 2);
}

static int hud_game(void *lua) {
    return hud_numbers(lua, 3, 1);
}

static int hud_font(void *lua) {
    uint32_t *event = hud_event(1);
    event[1] = (uint32_t)damage_lua.get_top(lua);
    event[2] = damage_named(lua, 1, "FONT_BASIC")   ? 1u
               : damage_named(lua, 1, "FONT_SCORE") ? 2u
                                                    : 0u;
    if (event[1] != 1 || !event[2]) {
        fail("Unexpected original HUD font boundary\n");
    }
    return 0;
}

static int hud_false(void *lua) {
    damage_lua.push_boolean(lua, 0);
    return 1;
}

static int hud_child(void *lua) {
    damage_lua.push_nil(lua);
    return 1;
}

static int hud_retina(void *lua) {
    damage_lua.push_boolean(lua, (hud_input[9] & 1) != 0);
    return 1;
}

static int hud_scale(void *lua) {
    FloatBits value = {.bits = hud_input[2]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int hud_leading(void *lua) {
    FloatBits value = {.bits = hud_input[7]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int hud_width(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 4) {
        fail("Unexpected original HUD width query\n");
    }
    FloatBits value = {.bits = hud_input[8]};
    damage_lua.push_number(lua, value.number);
    return 1;
}

static int hud_localize(void *lua) {
    if (damage_lua.get_top(lua) != 1 || !damage_named(lua, 1, "MI_SCORE")) {
        fail("Unexpected original HUD score localization\n");
    }
    damage_lua.push_string(lua, "Score");
    return 1;
}

static int hud_string(void *lua) {
    if (damage_lua.get_top(lua) != 2 || !damage_named(lua, 1, "TEXTS_BASIC") ||
        !damage_named(lua, 2, "MI_HIGH_SCORE")) {
        fail("Unexpected original HUD highscore localization\n");
    }
    damage_lua.push_string(lua, "Highscore");
    return 1;
}

static int hud_text(void *lua) {
    if (damage_lua.get_top(lua) != 6 || !damage_named(lua, 1, "TEXTS_BASIC") ||
        !damage_named(lua, 5, "TOP") || !damage_named(lua, 6, "RIGHT")) {
        fail("Unexpected original HUD text arguments\n");
    }
    uint32_t *event = hud_event(4);
    if (damage_named(lua, 2, "Score")) {
        event[2] = 1;
    } else if (damage_named(lua, 2, "Highscore")) {
        event[2] = 2;
    } else {
        unsigned length = 0;
        const char *text = damage_lua.to_string(lua, 2, &length);
        if (!text || !length || length > 9) {
            fail("Unexpected original HUD numeric text length\n");
        }
        event[2] = 3;
        for (unsigned index = 0; index < length; ++index) {
            if (text[index] < '0' || text[index] > '9') {
                fail("Unexpected original HUD numeric text\n");
            }
            event[3] = event[3] * 10 + (uint32_t)(text[index] - '0');
        }
    }
    for (int index = 3; index < 5; ++index) {
        if (damage_lua.type(lua, index) != 3) {
            fail("Unexpected original HUD text position\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        event[1 + (size_t)index] = value.bits;
    }
    return 0;
}

static void hud_globals(void *lua) {
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "lua");
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "draw", hud_frame);
    damage_lua.set_field(lua, -10002, "Frame");
    damage_lua.create_table(lua, 0, 2);
    damage_lua.get_field(lua, -10002, "Frame");
    damage_lua.set_field(lua, 1, "Frame");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "ImageButton");
    damage_lua.set_field(lua, -10002, "ui");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "getScaleNum", hud_scale);
    damage_lua.set_field(lua, -10002, "Scaling");
    damage_lua.create_table(lua, 0, 4);
    damage_function(lua, 1, "getStringWidth", hud_width);
    damage_function(lua, 1, "getString", hud_string);
    damage_function(lua, 1, "getFontLeading", hud_leading);
    damage_function(lua, 1, "drawString", hud_text);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 16);
    damage_function(lua, 1, "setFont", hud_font);
    damage_function(lua, 1, "setRenderState", hud_state);
    damage_function(lua, 1, "drawGame", hud_game);
    damage_function(lua, 1, "updateGame", damage_require);
    damage_lua.get_field(lua, 1, "updateGame");
    damage_lua.set_field(lua, 1, "currentGameMode");
    damage_function(lua, 1, "isRetinaGraphicsEnabled", hud_retina);
    damage_function(lua, 1, "getLocalizationString", hud_localize);
    damage_lua.push_string(lua, "android");
    damage_lua.set_field(lua, 1, "deviceModel");
    damage_lua.push_string(lua, "FONT_BASIC");
    damage_lua.set_field(lua, 1, "fontBasic");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 2, "isEagleBaitLaunched", hud_false);
    damage_lua.set_field(lua, 1, "subsystemsapi");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "floatingScores");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, 1, "highscores");
    damage_lua.set_field(lua, -10002, "gamelua");
}

static void hud_case(void *lua) {
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_number(lua, 1, "screenWidth", hud_input[0]);
    damage_number(lua, 1, "screenHeight", hud_input[1]);
    damage_number(lua, 1, "score", hud_input[5]);
    damage_lua.push_boolean(lua, (hud_input[9] & 64) != 0);
    damage_lua.set_field(lua, 1, "g_tutorialActive");
    if (hud_input[9] & 32) {
        damage_lua.push_nil(lua);
    } else {
        damage_lua.get_field(lua, 1, "updateGame");
    }
    damage_lua.set_field(lua, 1, "currentGameMode");
    damage_lua.create_table(lua, 0, 2);
    damage_number(lua, 2, "x", hud_input[3]);
    damage_number(lua, 2, "y", hud_input[4]);
    damage_lua.set_field(lua, 1, "g_layoutMargin");
    damage_lua.set_top(lua, 0);
    if (hud_input[9] & 16) {
        FloatBits value = {.bits = hud_input[10]};
        damage_lua.push_number(lua, value.number);
    } else {
        damage_lua.push_nil(lua);
    }
    damage_lua.set_field(lua, -10002, "oldScoreLen");
    damage_lua.create_table(lua, 0, 6);
    damage_function(lua, 1, "getChild", hud_child);
    damage_lua.get_field(lua, -10002, "GameHud");
    damage_lua.get_field(lua, 2, "setTextRenderState");
    damage_lua.set_field(lua, 1, "setTextRenderState");
    damage_lua.set_top(lua, 1);
    damage_lua.push_string(lua, "MI_SCORE");
    damage_lua.set_field(lua, 1, "TextID");
    damage_lua.push_string(lua, "TEXTS_BASIC");
    damage_lua.set_field(lua, 1, "TextGroupToUse");
    damage_lua.push_boolean(lua, (hud_input[9] & 8) != 0);
    damage_lua.set_field(lua, 1, "showScore");
    damage_lua.push_boolean(lua, (hud_input[9] & 4) != 0);
    damage_lua.set_field(lua, 1, "hideScores");
    if (hud_input[9] & 2) {
        damage_number(lua, 1, "highscore", hud_input[6]);
    }
    damage_lua.set_field(lua, -10002, "_ab_hud");
}

#if defined(AB_ORACLE_GAME_HUD_QUERIES_ONLY)
#include "game_hud_queries_probe.h"
#endif

static void probe_game_hud_draw(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot create original HUD oracle state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    int (*open_string)(void *) =
        (int (*)(void *))find_function("_Z14luaopen_stringP9lua_State");
    (void)open_string(lua);
    damage_lua.set_top(lua, 0);
    hud_globals(lua);
    load_damage_chunk(lua, "/data/local/tmp/ab520/ui/hud-class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/ui/game-hud.lua");
#if defined(AB_ORACLE_GAME_HUD_QUERIES_ONLY)
    probe_game_hud_queries(lua);
    damage_lua.close_state(lua);
    return;
#endif
    uint32_t random = UINT32_C(0x5204a0d1);
    semihost(4, "AB_ANDROID_GAME_HUD_DRAW_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        const float values[] = {480,
                                320,
                                1,
                                3,
                                3,
                                (float)item * 173 + 0.25f,
                                (float)item * 293,
                                26,
                                20,
                                0,
                                (float)item * 0.125f};
        for (size_t field = 0; field < 11; ++field) {
            FloatBits value = {.number = values[field]};
            hud_input[field] = value.bits;
        }
        for (size_t field = 3; field < 11; ++field) {
            FloatBits value = {
                .number = (float)((int32_t)(random_word(&random) % 4097) - 2048) / 8};
            hud_input[field] = value.bits;
        }
        const float scales[] = {0.25f, 0.5f, 0.7f, 1, 1.25f, 1.5f, 2, 3};
        FloatBits scale = {.number = scales[item % 8]};
        hud_input[2] = scale.bits;
        for (size_t field = 5; field < 7; ++field) {
            FloatBits value = {.number =
                                   (float)(random_word(&random) % 1000001) + 0.25f};
            hud_input[field] = value.bits;
        }
        if (item % 19 == 0) {
            hud_input[0] = item & 1 ? UINT32_C(0x80000000) : 0;
        }
        if (item % 23 == 0) {
            hud_input[8] = hud_input[10] = item & 1 ? UINT32_C(0x80000000) : 0;
        }
        hud_input[9] = (uint32_t)item & 127;
        hud_case(lua);
        hud_event_count = 0;
        damage_lua.get_field(lua, -10002, "GameHud");
        damage_lua.get_field(lua, 1, "draw");
        damage_lua.get_field(lua, -10002, "_ab_hud");
        damage_lua.push_number(lua, 0);
        damage_lua.push_number(lua, 0);
        damage_lua.push_number(lua, 1);
        damage_lua.push_number(lua, 1);
        damage_lua.push_number(lua, 0);
        if (damage_lua.call(lua, 6, 0, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original HUD draw failed\n");
        }
        damage_lua.set_top(lua, 0);
        print_words(hud_input, 11);
        print_words(&hud_event_count, 1);
        for (size_t event = 0; event < hud_event_count; ++event) {
            print_words(hud_events[event], 12);
        }
        damage_lua.get_field(lua, -10002, "oldScoreLen");
        if (damage_lua.type(lua, 1) != 3) {
            fail("Unexpected original HUD width history type\n");
        }
        FloatBits retained_width = {.number = damage_lua.to_number(lua, 1)};
        print_words(&retained_width.bits, 1);
        damage_lua.set_top(lua, 0);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB game HUD draw reference complete\n");
}

#endif
