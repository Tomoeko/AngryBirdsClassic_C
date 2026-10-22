#ifndef AB_ORACLE_FORCE_QUEUE_H
#define AB_ORACLE_FORCE_QUEUE_H

#include "selected_function.h"

static uint32_t queue_events[32][6], queue_event_count, queue_pass, queue_mask,
    queue_mutation;
static const char *const queue_names[] = {"first", "second", "third"};

static void queue_membership(void *lua) {
    int top = damage_lua.get_top(lua);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.create_table(lua, 0, 3);
    for (size_t index = 0; index < 3; ++index) {
        if (queue_mask & (1u << index)) {
            /* A false entry is still non-nil to the original force closure. */
            damage_lua.push_boolean(lua, 0);
            damage_lua.set_field(lua, top + 2, queue_names[index]);
        }
    }
    damage_lua.set_field(lua, top + 1, "world");
    damage_lua.set_field(lua, -10002, "objects");
    damage_lua.set_top(lua, top);
}

static int queue_native_boundary(void *lua) {
    if (damage_lua.get_top(lua) != 5 || queue_event_count >= 32) {
        fail("Unexpected original queued force boundary\n");
    }
    uint32_t *event = queue_events[queue_event_count++];
    event[0] = queue_pass;
    event[1] = 0;
    for (size_t index = 0; index < 3; ++index) {
        if (damage_named(lua, 1, queue_names[index])) {
            event[1] = (uint32_t)index + 1;
        }
    }
    if (!event[1]) {
        fail("Unknown original queued force object\n");
    }
    for (int argument = 2; argument <= 5; ++argument) {
        if (damage_lua.type(lua, argument) != 3) {
            fail("Nonnumeric original queued force argument\n");
        }
        FloatBits value = {.number = damage_lua.to_number(lua, argument)};
        event[argument] = value.bits;
    }
    queue_mask ^= queue_mutation;
    queue_membership(lua);
    return 0;
}

static void queue_call(void *lua, const char *name, const uint32_t *request) {
    damage_lua.get_field(lua, -10002, name);
    int arguments = 0;
    if (request) {
        damage_lua.push_string(lua, queue_names[request[0] - 1]);
        for (size_t field = 1; field < 5; ++field) {
            FloatBits value = {.bits = request[field]};
            damage_lua.push_number(lua, value.number);
        }
        arguments = 5;
    }
    if (damage_lua.call(lua, arguments, 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original queued force call failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void probe_force_queue(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original force queue state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    const char *names[] = {"queue-add", "queue-clear", "queue-apply"};
    const char *paths[] = {"/data/local/tmp/ab520/force-queue/add.lua",
                           "/data/local/tmp/ab520/force-queue/clear.lua",
                           "/data/local/tmp/ab520/force-queue/apply.lua"};
    for (size_t index = 0; index < 3; ++index) {
        load_selected_function(lua, paths[index], names[index]);
    }
    damage_function(lua, -10002, "applyForceNative", queue_native_boundary);
    uint32_t random = UINT32_C(0x520f0ce5);
    semihost(4, "AB_ANDROID_FORCE_QUEUE_V1 4096\n");
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[] = {(uint32_t)(item % 9), (uint32_t)((item / 9) % 4),
                            (uint32_t)((item / 36) % 8), (uint32_t)((item / 288) % 8)};
        uint32_t requests[8][5];
        for (size_t index = 0; index < 8; ++index) {
            requests[index][0] = random_word(&random) % 3 + 1;
            for (size_t field = 1; field < 5; ++field) {
                requests[index][field] = random_float(&random, 112, 30, 1);
                if (item < 128) {
                    requests[index][field] =
                        item & (1u << field) ? UINT32_C(0x80000000) : 0;
                }
            }
        }
        queue_event_count = 0;
        queue_mutation = setup[3];
        queue_call(lua, "queue-clear", NULL);
        for (size_t index = 0; index < setup[0]; ++index) {
            queue_call(lua, "queue-add", requests[index]);
        }
        for (queue_pass = 0; queue_pass < setup[1]; ++queue_pass) {
            queue_mask = (setup[2] ^ (queue_pass * 3)) & 7;
            queue_membership(lua);
            queue_call(lua, "queue-apply", NULL);
        }
        queue_call(lua, "queue-clear", NULL);
        queue_pass = 3;
        queue_call(lua, "queue-apply", NULL);
        queue_call(lua, "queue-add", requests[0]);
        queue_pass = 4;
        queue_mask = 7;
        queue_membership(lua);
        queue_call(lua, "queue-apply", NULL);
        queue_call(lua, "queue-clear", NULL);
        queue_pass = 5;
        queue_call(lua, "queue-apply", NULL);
        uint32_t output[] = {queue_mask, queue_event_count};
        print_words(setup, 4);
        for (size_t index = 0; index < 8; ++index) {
            print_words(requests[index], 5);
        }
        print_words(output, 2);
        for (size_t index = 0; index < queue_event_count; ++index) {
            print_words(queue_events[index], 6);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB force queue reference complete\n");
}

#endif
