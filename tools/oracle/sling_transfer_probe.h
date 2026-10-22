#ifndef AB_ORACLE_SLING_TRANSFER_H
#define AB_ORACLE_SLING_TRANSFER_H

static uint32_t transfer_input[9], transfer_events[5][3];
static size_t transfer_event_count;

static int transfer_event(void *lua, uint32_t kind) {
    if (transfer_event_count >= 5 || !damage_named(lua, 1, "first")) {
        fail("Unexpected original sling-transfer event\n");
    }
    uint32_t *event = transfer_events[transfer_event_count++];
    event[0] = kind;
    for (int field = 1; field < 3; ++field) {
        FloatBits value = {.number =
                               kind == 3 ? 0 : damage_lua.to_number(lua, field + 1)};
        event[field] = value.bits;
    }
    return 0;
}

static int transfer_position(void *lua) {
    return transfer_event(lua, 1);
}

static int transfer_rotation(void *lua) {
    return transfer_event(lua, 2);
}

static int transfer_dynamic(void *lua) {
    return transfer_event(lua, 3);
}

static int transfer_first(void *lua) {
    damage_lua.push_string(lua, "first");
    return 1;
}

static int transfer_camera_distance(void *lua) {
    damage_lua.push_number(lua, 1);
    return 1;
}

static void prepare_transfer_frame(void *lua) {
    sling_input[0] = transfer_input[2];
    sling_input[1] = transfer_input[3];
    sling_input[10] = 1;
    prepare_sling_launch(lua);
    damage_lua.get_field(lua, -10002, "objects");
    damage_lua.get_field(lua, -1, "world");
    damage_lua.get_field(lua, -1, "first");
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -2, "name");
    damage_number(lua, -2, "angle", transfer_input[7]);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    int self = damage_lua.get_top(lua);
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, self, "nextBirdTimer");
    damage_function(lua, self, "setRubberBandForSittingBird", damage_require);
    damage_lua.create_table(lua, 0, 6);
    damage_number(lua, -2, "startX", transfer_input[0]);
    damage_number(lua, -2, "startY", transfer_input[1]);
    damage_number(lua, -2, "height", transfer_input[4]);
    damage_number(lua, -2, "angle", transfer_input[5]);
    damage_number(lua, -2, "timer", transfer_input[6]);
    damage_lua.set_field(lua, self, "birdReloadAnimation");
    damage_lua.set_top(lua, 0);
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, -10002, "currentBirdIndex");
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -10002, "birdToSlingshotBirdName");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, -10002, "fillInNextBird");
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, -10002, "birdReady");
    damage_lua.create_table(lua, 0, 0);
    damage_lua.set_field(lua, -10002, "g_birdHats");
}

static void prepare_transfer_boundaries(void *lua) {
    damage_function(lua, -10002, "setPosition", transfer_position);
    damage_function(lua, -10002, "setRotation", transfer_rotation);
    damage_function(lua, -10002, "setObjectBodyDynamic", transfer_dynamic);
    damage_function(lua, -10002, "getNextBird", transfer_first);
    damage_function(lua, -10002, "getAudioName", transfer_first);
    damage_lua.push_string(lua, "first");
    damage_lua.set_field(lua, -10002, "g_sound_next_bird");
    damage_lua.create_table(lua, 0, 2);
    int camera = damage_lua.get_top(lua);
    damage_function(lua, camera, "getDistanceBetweenSnapPoints",
                    transfer_camera_distance);
    damage_function(lua, camera, "getDistanceToSnapPoint", transfer_camera_distance);
    damage_lua.set_field(lua, -10002, "cameraSystem");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "playAudio", damage_require);
    damage_lua.set_field(lua, -10002, "res");
    damage_lua.create_table(lua, 0, 1);
    damage_function(lua, -2, "notify", damage_require);
    damage_lua.set_field(lua, -10002, "eventManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_number(lua, 0);
    damage_lua.set_field(lua, -2, "EID_BIRD_PUT_TO_SLINGSHOT");
    damage_lua.set_field(lua, -10002, "events");
}

static void print_transfer_results(void *lua) {
    uint32_t summary[5];
    damage_lua.get_field(lua, -10002, "_ab_sling_self");
    damage_lua.get_field(lua, -1, "birdReloadAnimation");
    damage_lua.get_field(lua, -1, "timer");
    FloatBits timer = {.number = damage_lua.to_number(lua, -1)};
    summary[0] = timer.bits;
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "birdReady");
    summary[1] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "fillInNextBird");
    summary[2] = (uint32_t)(damage_lua.to_boolean(lua, -1) != 0);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "birdToSlingshotBirdName");
    summary[3] = (uint32_t)damage_lua.type(lua, -1);
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "currentBirdIndex");
    FloatBits index = {.number = damage_lua.to_number(lua, -1)};
    summary[4] = index.bits;
    damage_lua.set_top(lua, 0);
    print_words(summary, 5);
    uint32_t count = (uint32_t)transfer_event_count;
    print_words(&count, 1);
    for (size_t event = 0; event < transfer_event_count; ++event) {
        print_words(transfer_events[event], 3);
    }
}

#if defined(AB_ORACLE_SLING_START_ONLY)
#include "sling_start_probe.h"
#endif
#if defined(AB_ORACLE_SLING_RELOAD_ONLY)
#include "sling_reload_probe.h"
#endif

static void probe_sling_transfer(void) {
#if defined(AB_ORACLE_SLING_RELOAD_ONLY)
    probe_sling_reload();
    return;
#endif
#if defined(AB_ORACLE_SLING_START_ONLY)
    probe_sling_start();
    return;
#endif
    void *lua = open_sling_reference();
    prepare_transfer_boundaries(lua);
    uint32_t state = UINT32_C(0x52051aaf);
    semihost(4, "AB_ANDROID_SLING_TRANSFER_V1 4096\n");
    for (size_t index = 0; index < 4096; ++index) {
        for (size_t field = 0; field < 6; ++field) {
            transfer_input[field] = random_float(&state, 122, 8, 1);
        }
        FloatBits time = {.number = (float)(index % 121) / 100.0f};
        transfer_input[6] = time.bits;
        transfer_input[7] = random_float(&state, 122, 4, 1);
        FloatBits seconds = {.number = (float)(index % 17) / 100.0f};
        transfer_input[8] = seconds.bits;
        prepare_transfer_frame(lua);
        transfer_event_count = 0;
        print_words(transfer_input, 9);
        damage_lua.get_field(lua, -10002, "SlingshotSystem");
        damage_lua.get_field(lua, -1, "defaultAnimateBirdToSlingShot");
        damage_lua.get_field(lua, -10002, "_ab_sling_self");
        damage_lua.push_number(lua, seconds.number);
        if (damage_lua.call(lua, 2, 0, 0)) {
            const char *error = damage_lua.to_string(lua, -1, NULL);
            fail(error ? error : "Original sling-transfer update failed\n");
        }
        damage_lua.set_top(lua, 0);
        print_transfer_results(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB sling transfer reference complete\n");
}

#endif
