#ifndef AB_ORACLE_EVENT_MANAGER_H
#define AB_ORACLE_EVENT_MANAGER_H

#include "../../tests/event_manager_cases.h"

#include <stdbool.h>

static uint32_t em_sample, em_used, em_calls, em_swallowed, em_trace_count;
static uint32_t em_trace[EVENT_MANAGER_TRACE_CAPACITY][EVENT_MANAGER_TRACE_WORDS];
static const void *em_events[4];
static void (*em_push_value)(void *, int);
static unsigned (*em_length)(void *, int);
static const char *const em_event_names[] = {"_ab_em_event_0", "_ab_em_event_1",
                                             "_ab_em_event_2", "_ab_em_event_3"};
static const char *const em_listener_names[] = {
    "_ab_em_listener_0", "_ab_em_listener_1", "_ab_em_listener_2"};

static void em_snapshot(void *lua, uint32_t output[EVENT_MANAGER_SNAPSHOT_WORDS]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_em_manager");
    int manager = top + 1;
    damage_lua.get_field(lua, manager, "eventListeners");
    int groups = top + 2;
    for (uint32_t index = 0; index < 2; ++index) {
        damage_lua.get_field(lua, groups, index ? "B" : "A");
        output[0] += damage_lua.type(lua, -1) == 5;
        damage_lua.set_top(lua, groups);
    }
    damage_lua.set_top(lua, manager);
    const char *collections[] = {"queuedEvents", "bufferedEvents", "delayedEvents"};
    for (uint32_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, manager, collections[index]);
        if (damage_lua.type(lua, -1) != 5)
            fail("Original event manager collection is not a table\n");
        output[index + 1] = em_length(lua, -1);
        damage_lua.set_top(lua, manager);
    }
    damage_lua.get_field(lua, manager, "useBufferedQueue");
    output[4] = damage_lua.to_boolean(lua, -1) != 0;
    output[5] = em_used;
    output[6] = em_calls;
    output[7] = em_swallowed;
    damage_lua.set_top(lua, top);
    for (uint32_t index = 0; index < 4; ++index) {
        damage_lua.get_field(lua, -10002, em_event_names[index]);
        damage_lua.get_field(lua, top + 1, "delay");
        uint32_t tag = (uint32_t)damage_lua.type(lua, -1);
        if (tag != 0 && tag != 3)
            fail("Unexpected original fixture delay type\n");
        output[8 + index * 2] = tag;
        FloatBits number = {.number = damage_lua.to_number(lua, -1)};
        output[9 + index * 2] = number.bits;
        damage_lua.set_top(lua, top + 1);
        damage_lua.get_field(lua, top + 1, "id");
        output[16 + index] = damage_named(lua, -1, "A") ? 0u : 1u;
        damage_lua.set_top(lua, top);
    }
    for (uint32_t index = 0; index < 3; ++index) {
        damage_lua.get_field(lua, -10002, em_listener_names[index]);
        damage_lua.get_field(lua, top + 1, "eventTriggered");
        output[20 + index] = damage_lua.type(lua, -1) == 6;
        damage_lua.set_top(lua, top);
    }
}

static bool em_action(void *lua, EventFixtureAction action) {
    int top = damage_lua.get_top(lua);
    if (action.operation == EVENT_FIXTURE_CHANGE_ID) {
        damage_lua.get_field(lua, -10002, em_event_names[action.object]);
        damage_lua.push_string(lua, action.id ? "B" : "A");
        damage_lua.set_field(lua, top + 1, "id");
        damage_lua.set_top(lua, top);
        return true;
    }
    const char *methods[] = {"addEventListener", "removeEventListener", "notify",
                             "queueEvent",       "notifyWithDelay",     "tick"};
    damage_lua.get_field(lua, -10002, "_ab_em_manager");
    damage_lua.get_field(lua, top + 1, methods[action.operation]);
    em_push_value(lua, top + 1);
    int arguments = 2;
    if (action.operation == EVENT_FIXTURE_ADD ||
        action.operation == EVENT_FIXTURE_REMOVE) {
        damage_lua.push_string(lua, action.id ? "B" : "A");
        damage_lua.get_field(lua, -10002, em_listener_names[action.object]);
        arguments = 3;
    } else if (action.operation == EVENT_FIXTURE_TICK) {
        FloatBits number = {.bits = action.number};
        damage_lua.push_number(lua, number.number);
    } else {
        damage_lua.get_field(lua, -10002, em_event_names[action.object]);
        if (action.operation == EVENT_FIXTURE_DELAY) {
            FloatBits number = {.bits = action.number};
            damage_lua.push_number(lua, number.number);
            arguments = 3;
        }
    }
    bool okay = damage_lua.call(lua, arguments, 0, 0) == 0;
    damage_lua.set_top(lua, top);
    return okay;
}

static int em_callback(void *lua) {
    uint32_t role = (uint32_t)damage_lua.to_number(lua, -10003);
    if (damage_lua.get_top(lua) != 2 || role > 2 ||
        em_trace_count == EVENT_MANAGER_TRACE_CAPACITY)
        fail("Unexpected original event callback\n");
    const void *event = damage_lua.to_pointer(lua, 2);
    uint32_t event_role = UINT32_MAX;
    for (uint32_t index = 0; index < 4; ++index)
        if (event == em_events[index])
            event_role = index;
    if (event_role == UINT32_MAX)
        fail("Original event payload identity changed\n");
    ++em_calls;
    uint32_t *row = em_trace[em_trace_count++];
    row[0] = role;
    row[1] = event_role;
    em_snapshot(lua, row + 2);
    for (uint32_t bit = 0; bit < 8; ++bit) {
        uint32_t mask = 1u << bit;
        bool receiver = bit < 6 ? role == 0 : role == (bit == 6 ? 1u : 2u);
        if (!receiver || !(em_sample & mask) || (em_used & mask))
            continue;
        em_used |= mask;
        if (bit == 4) {
            damage_lua.get_field(lua, -10002, em_listener_names[1]);
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, 3, "eventTriggered");
            damage_lua.set_top(lua, 2);
        } else if (!em_action(lua, event_fixture_effects[bit])) {
            ++em_swallowed;
        }
    }
    return 0;
}

static void em_prepare(void *lua) {
    damage_lua.set_top(lua, 0);
    damage_lua.get_field(lua, -10002, "EventManager");
    damage_lua.get_field(lua, 1, "new");
    em_push_value(lua, 1);
    if (damage_lua.call(lua, 1, 1, 0))
        fail("Original event manager constructor failed\n");
    damage_lua.set_field(lua, -10002, "_ab_em_manager");
    damage_lua.set_top(lua, 0);
    const char *names[] = {"ListenerA", "ListenerB", "ListenerC"};
    for (uint32_t index = 0; index < 3; ++index) {
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_string(lua, names[index]);
        damage_lua.set_field(lua, 1, "name");
        damage_lua.push_number(lua, (float)index);
        damage_lua.push_closure(lua, em_callback, 1);
        damage_lua.set_field(lua, 1, "eventTriggered");
        damage_lua.set_field(lua, -10002, em_listener_names[index]);
    }
    for (uint32_t index = 0; index < 4; ++index) {
        damage_lua.create_table(lua, 0, 3);
        em_events[index] = damage_lua.to_pointer(lua, 1);
        damage_lua.push_string(lua, index == 2 ? "B" : "A");
        damage_lua.set_field(lua, 1, "id");
        damage_lua.push_number(lua, 0);
        damage_lua.set_field(lua, 1, "token");
        damage_lua.set_field(lua, -10002, em_event_names[index]);
    }
}

static void probe_event_manager(void) {
    initialize_damage_lua();
    em_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    em_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original event manager state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_function(lua, -10002, "print", damage_require);
    damage_function(lua, -10002, "printError", damage_require);
    damage_lua.get_field(lua, -10002, "_G");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/class.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/utils.lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/event-manager/manager.lua");
    semihost(4, "AB_ANDROID_EVENT_MANAGER_V1 256 24\n");
    for (uint32_t sample = 0; sample < EVENT_MANAGER_CASES; ++sample) {
        em_sample = sample;
        em_used = em_calls = em_swallowed = 0;
        em_prepare(lua);
        print_words(&sample, 1);
        for (uint32_t step = 0; step < EVENT_MANAGER_ACTIONS; ++step) {
            em_trace_count = 0;
            for (uint32_t row = 0; row < EVENT_MANAGER_TRACE_CAPACITY; ++row)
                for (uint32_t word = 0; word < EVENT_MANAGER_TRACE_WORDS; ++word)
                    em_trace[row][word] = 0;
            uint32_t output[25] = {0};
            output[0] = em_action(lua, event_fixture_actions[step]);
            output[1] = em_trace_count;
            em_snapshot(lua, output + 2);
            print_words(output, 25);
            for (uint32_t row = 0; row < em_trace_count; ++row)
                print_words(em_trace[row], EVENT_MANAGER_TRACE_WORDS);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB event manager reference complete\n");
}

#endif
