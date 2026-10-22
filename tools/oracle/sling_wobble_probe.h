#ifndef AB_ORACLE_SLING_WOBBLE_H
#define AB_ORACLE_SLING_WOBBLE_H

#include "../../tests/sling_wobble_cases.h"

static const char *wobble_current_method;

static void wobble_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        if (wobble_current_method)
            semihost(4, wobble_current_method);
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original sling spring fixture failed\n");
    }
}

static void wobble_method(void *lua, const char *name) {
    wobble_current_method = name;
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, -1, name);
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
}

static int wobble_audio_query(void *lua) {
    damage_lua.push_boolean(lua, 0);
    return 1;
}

static int wobble_audio_name(void *lua) {
    damage_lua.push_string(lua, "fixture");
    return 1;
}

static int wobble_definition(void *lua) {
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "fixture");
    damage_lua.set_field(lua, -2, "selectionSound");
    return 1;
}

static void wobble_select(void *lua) {
    wobble_method(lua, "setDraggedBird");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -2, "name");
    wobble_call(lua, 2, 0);
    damage_lua.set_top(lua, 0);
}

static void wobble_snapshot(void *lua) {
    uint32_t output[6];
    wobble_method(lua, "getRubberBandPosition");
    wobble_call(lua, 1, 1);
    damage_lua.get_field(lua, 2, "x");
    FloatBits x = {.number = damage_lua.to_number(lua, -1)};
    damage_lua.get_field(lua, 2, "y");
    FloatBits y = {.number = damage_lua.to_number(lua, -1)};
    output[0] = x.bits;
    output[1] = y.bits;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, 1, "resetRubberBand");
    const uint32_t *closure = damage_lua.to_pointer(lua, -1);
    if (!closure || ((const uint8_t *)closure)[6] || ((const uint8_t *)closure)[7] != 7)
        fail("Original sling reset closure layout mismatch\n");
    static const size_t scalar_upvalues[] = {4, 3, 2, 5};
    for (size_t index = 0; index < 4; ++index) {
        const uint32_t *value = (const uint32_t *)(uintptr_t)((
            uint32_t *)(uintptr_t)closure[5 + scalar_upvalues[index]])[2];
        if (value[1] != 3)
            fail("Original sling spring scalar changed type\n");
        output[2 + index] = value[0];
    }
    damage_lua.set_top(lua, 0);
    print_words(output, 6);
}

static void probe_sling_wobble(void) {
    void *lua = open_sling_reference();
    damage_function(lua, -10002, "getObjectDefinition", wobble_definition);
    damage_function(lua, -10002, "getAudioName", wobble_audio_name);
    damage_function(lua, -10002, "playAudio", damage_require);
    damage_function(lua, -10002, "setPosition", damage_require);
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "isAudioPlaying", wobble_audio_query);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, 1, "stopMovement", damage_require);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_number(lua, -10002, "physicsScale", UINT32_C(0x3d4ccccd));
    damage_lua.get_field(lua, -10002, "SlingshotSystem");
    damage_lua.get_field(lua, -10002, "Slingshot");
    damage_lua.set_field(lua, 1, "slingshot");
    damage_lua.set_top(lua, 0);
    wobble_method(lua, "getLevelStartPosition");
    wobble_call(lua, 1, 1);
    damage_number(lua, 2, "x", UINT32_C(0x41400000));
    damage_number(lua, 2, "y", UINT32_C(0x40c66666));
    damage_lua.set_top(lua, 0);
    semihost(4, "AB_ANDROID_SLING_WOBBLE_V1 64\n");
    for (unsigned sample = 0; sample < SLING_WOBBLE_CASES; ++sample) {
        float input[3];
        sling_wobble_input(sample, input);
        print_words(&sample, 1);
        uint32_t words[3];
        for (size_t index = 0; index < 3; ++index) {
            FloatBits value = {.number = input[index]};
            words[index] = value.bits;
        }
        print_words(words, 3);
        wobble_select(lua);
        wobble_method(lua, "cancelBirdDrag");
        wobble_call(lua, 1, 0);
        damage_lua.set_top(lua, 0);
        wobble_select(lua);
        wobble_method(lua, "stretchRubberBand");
        damage_lua.create_table(lua, 0, 2);
        damage_number(lua, -2, "x", words[0]);
        damage_number(lua, -2, "y", words[1]);
        wobble_call(lua, 2, 0);
        damage_lua.set_top(lua, 0);
        damage_lua.push_boolean(lua, 1);
        damage_lua.set_field(lua, -10002, "birdFired");
        wobble_method(lua, "updateRubberBandWobble");
        damage_lua.push_number(lua, input[2]);
        wobble_call(lua, 2, 0);
        damage_lua.set_top(lua, 0);
        wobble_snapshot(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling wobble reference complete\n");
}

#endif
