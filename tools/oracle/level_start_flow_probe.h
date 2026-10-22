#ifndef AB_ORACLE_LEVEL_START_FLOW_H
#define AB_ORACLE_LEVEL_START_FLOW_H

#include "reference_level_start_flow.h"

static unsigned lf_profile, lf_step, lf_created, lf_count, lf_done_events;
static unsigned lf_shows[10], lf_checks[10];
static uint32_t lf_trace[64][10];
static const void *lf_requests[32];
static unsigned lf_paused;
static unsigned lf_failed;
static void (*lf_get)(void *, int, int), (*lf_set)(void *, int, int);
static unsigned (*lf_length)(void *, int);

static void lf_checked_call(void *lua, int arguments, int results) {
    if (damage_lua.call(lua, arguments, results, 0)) {
        lf_failed = 1;
    }
}

static unsigned lf_identity(void *lua, int table) {
    if (damage_lua.type(lua, table) == 0)
        return 0;
    const void *pointer = damage_lua.to_pointer(lua, table);
    for (unsigned index = 0; index < lf_created; ++index)
        if (lf_requests[index] == pointer)
            return index + 1;
    fail("Unregistered original level-flow request identity\n");
    return 0;
}

static void lf_state(void *lua, uint32_t words[32]) {
    int top = damage_lua.get_top(lua);
    for (unsigned index = 0; index < 32; ++index)
        words[index] = 0;
    damage_lua.get_field(lua, -10002, "_ab_flow");
    int flow = top + 1;
    damage_lua.get_field(lua, flow, "done");
    words[0] = (uint32_t)damage_lua.type(lua, -1);
    words[1] = (uint32_t)damage_lua.to_boolean(lua, -1);
    damage_lua.set_top(lua, flow);
    damage_lua.get_field(lua, flow, "currentlyShowing");
    words[2] = lf_identity(lua, flow + 1);
    words[3] = lf_paused;
    damage_lua.set_top(lua, flow);
    const char *names[] = {"stagingQueue", "initQueue", "ingameQueue"};
    for (unsigned queue = 0; queue < 3; ++queue) {
        damage_lua.get_field(lua, flow, names[queue]);
        unsigned count = lf_length(lua, flow + 1);
        if (count > 8)
            fail("Original level-flow queue exceeds profile bound\n");
        unsigned offset = 4 + queue * 9;
        words[offset] = count;
        for (unsigned index = 0; index < count; ++index) {
            lf_get(lua, flow + 1, (int)index + 1);
            words[offset + index + 1] = lf_identity(lua, flow + 2);
            damage_lua.set_top(lua, flow + 1);
        }
        damage_lua.set_top(lua, flow);
    }
    words[31] = lf_created;
    damage_lua.set_top(lua, top);
}

static void lf_record(void *lua, unsigned kind, unsigned listener, unsigned type,
                      uint32_t value) {
    if (lf_count == 64)
        fail("Original level-flow trace exceeds profile bound\n");
    uint32_t state[32];
    lf_state(lua, state);
    uint32_t *row = lf_trace[lf_count++];
    row[0] = kind;
    row[1] = listener;
    row[2] = type;
    row[3] = value;
    row[4] = state[2];
    row[5] = state[4];
    row[6] = state[13];
    row[7] = state[22];
    row[8] = state[0];
    row[9] = state[1];
}

static void lf_method(void *lua, const char *name) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_flow");
    damage_lua.get_field(lua, top + 1, name);
    damage_lua.get_field(lua, -10002, "_ab_flow");
    lf_checked_call(lua, 1, 0);
    damage_lua.set_top(lua, top);
}

static void lf_notify(void *lua, const char *id) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "gamelua");
    damage_lua.get_field(lua, top + 1, "eventManager");
    damage_lua.get_field(lua, top + 2, "notify");
    damage_lua.get_field(lua, top + 1, "eventManager");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, id);
    damage_lua.set_field(lua, top + 5, "id");
    lf_checked_call(lua, 2, 0);
    damage_lua.set_top(lua, top);
}

static void lf_push(void *lua, unsigned name, unsigned listener, float appearing) {
    if (lf_created == 32 || listener >= 10)
        fail("Original level-flow prepared request exceeds bound\n");
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_flow");
    damage_lua.get_field(lua, top + 1, "pushRequest");
    damage_lua.get_field(lua, -10002, "_ab_flow");
    damage_lua.push_string(lua, reference_flow_name(name));
    damage_lua.push_number(lua, appearing);
    damage_lua.get_field(lua, -10002, "_ab_listeners");
    lf_get(lua, top + 6, (int)listener + 1);
    void (*remove)(void *, int) =
        (void (*)(void *, int))find_function("_Z10lua_removeP9lua_Statei");
    remove(lua, top + 6);
    lf_checked_call(lua, 4, 0);
    damage_lua.get_field(lua, top + 1, "stagingQueue");
    unsigned count = lf_length(lua, top + 2);
    damage_lua.get_field(lua, -10002, "_ab_requests");
    lf_get(lua, top + 2, (int)count);
    lf_requests[lf_created++] = damage_lua.to_pointer(lua, top + 4);
    lf_set(lua, top + 3, (int)lf_created);
    damage_lua.set_top(lua, top);
}

static unsigned lf_listener(void *lua) {
    damage_lua.get_field(lua, 1, "number");
    unsigned index = (unsigned)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, 1);
    if (index >= 10)
        fail("Invalid original level-flow listener identity\n");
    return index;
}

static int lf_show(void *lua) {
    unsigned listener = lf_listener(lua);
    lf_record(lua, 1, listener, 0, 0);
    ++lf_shows[listener];
    if (listener == 0 && (lf_profile & 1) && lf_shows[listener] == 1)
        lf_method(lua, "clearInitQueue");
    if (listener == 1 && (lf_profile & 2) && lf_shows[listener] == 1)
        lf_push(lua, 9, 9, 35);
    return 0;
}

static int lf_done(void *lua) {
    unsigned listener = lf_listener(lua);
    unsigned done = listener != 0 || lf_step >= 9 || (lf_profile & 16);
    unsigned type = listener == 0 && (lf_profile & 2) && done ? 3u : 1u;
    lf_record(lua, 2, listener, type, type == 3 ? 0u : done);
    ++lf_checks[listener];
    if (listener == 4 && (lf_profile & 4) && lf_checks[listener] == 1)
        lf_notify(lua, "init");
    if (type == 3)
        damage_lua.push_number(lua, 0);
    else
        damage_lua.push_boolean(lua, (int)done);
    return 1;
}

static int lf_pause(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 1)
        fail("Unexpected original level-flow pause arguments\n");
    lf_paused = (unsigned)damage_lua.to_boolean(lua, 1);
    lf_record(lua, 3, 0, 1, lf_paused);
    return 0;
}

static int lf_event(void *lua) {
    ++lf_done_events;
    lf_record(lua, 4, 0, 0, 0);
    if ((lf_profile & 8) && lf_done_events == 1)
        lf_push(lua, 8, 8, 50);
    return 0;
}

static int lf_root(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_root");
    return 1;
}

static int lf_child(void *lua) {
    if (!damage_named(lua, 2, "pausePage"))
        fail("Unexpected level-flow child query\n");
    damage_lua.get_field(lua, -10002, "_ab_pause");
    return 1;
}

static void probe_level_start_flow(void) {
    initialize_damage_lua();
    lf_get = (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    lf_set = (void (*)(void *, int, int))find_function("_Z11lua_rawsetiP9lua_Stateii");
    lf_length = (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    semihost(4, "AB_ANDROID_LEVEL_FLOW_V1 32 27\n");
    for (lf_profile = 0; lf_profile < 32; ++lf_profile) {
        lf_failed = 0;
        void *lua = damage_lua.new_state();
        if (!lua)
            fail("Cannot allocate original level-flow state\n");
        (void)damage_lua.open_base(lua);
        damage_lua.set_top(lua, 0);
        int (*open_table)(void *) =
            (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
        (void)open_table(lua);
        damage_lua.set_top(lua, 0);
        damage_function(lua, -10002, "requireFile", damage_require);
        damage_function(lua, -10002, "print", damage_require);
        damage_lua.create_table(lua, 0, 1);
        damage_lua.set_field(lua, -10002, "lua");
        load_damage_chunk(lua, "/data/local/tmp/ab520/flow-class.lua");
        load_damage_chunk(lua, "/data/local/tmp/ab520/flow-subsystems.lua");
        load_damage_chunk(lua, "/data/local/tmp/ab520/flow-events.lua");
        load_selected_function(lua, "/data/local/tmp/ab520/flow-exists.lua", "exists");
        load_selected_function(lua, "/data/local/tmp/ab520/flow-pausing.lua",
                               "_ab_pausing");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_boolean(lua, 0);
        damage_lua.set_field(lua, 1, "visible");
        damage_lua.set_field(lua, -10002, "_ab_pause");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_string(lua, "gameHud");
        damage_lua.set_field(lua, 1, "name");
        damage_function(lua, 1, "getChild", lf_child);
        damage_lua.set_field(lua, -10002, "_ab_root");
        damage_lua.create_table(lua, 0, 6);
        damage_function(lua, 1, "printError", damage_require);
        damage_function(lua, 1, "setGamePaused", lf_pause);
        damage_lua.get_field(lua, -10002, "exists");
        damage_lua.set_field(lua, 1, "exists");
        damage_lua.get_field(lua, -10002, "EventManager");
        damage_lua.get_field(lua, 2, "new");
        damage_lua.get_field(lua, -10002, "EventManager");
        lf_checked_call(lua, 1, 1);
        damage_lua.set_field(lua, 1, "eventManager");
        damage_lua.set_top(lua, 1);
        damage_lua.create_table(lua, 0, 3);
        const char *event_names[] = {"EID_LEVEL_LOADING_INIT", "EID_LEVEL_LOADING_DONE",
                                     "EID_LEVEL_START_SCREEN_FLOW_DONE"};
        const char *ids[] = {"init", "loaded", "done"};
        for (unsigned index = 0; index < 3; ++index) {
            damage_lua.push_string(lua, ids[index]);
            damage_lua.set_field(lua, 2, event_names[index]);
        }
        damage_lua.set_field(lua, 1, "events");
        damage_lua.create_table(lua, 0, 1);
        damage_function(lua, 2, "getRoot", lf_root);
        damage_lua.set_field(lua, 1, "menuManager");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.create_table(lua, 0, 0);
        damage_lua.set_field(lua, 2, "children");
        damage_lua.get_field(lua, -10002, "_ab_pausing");
        damage_lua.set_field(lua, 2, "isAnyPausingChildVisible");
        damage_lua.set_field(lua, 1, "notificationsFrame");
        damage_lua.set_field(lua, -10002, "gamelua");
        damage_lua.set_top(lua, 0);
        load_damage_chunk(lua, "/data/local/tmp/ab520/flow-module.lua");
        damage_lua.get_field(lua, -10002, "LevelStartScreenFlow");
        damage_lua.get_field(lua, 1, "new");
        damage_lua.get_field(lua, -10002, "LevelStartScreenFlow");
        lf_checked_call(lua, 1, 1);
        damage_lua.set_field(lua, -10002, "_ab_flow");
        damage_lua.set_top(lua, 0);
        damage_lua.create_table(lua, 10, 0);
        for (unsigned index = 0; index < 10; ++index) {
            damage_lua.create_table(lua, 0, 3);
            damage_lua.push_number(lua, (float)index);
            damage_lua.set_field(lua, 2, "number");
            damage_function(lua, 2, "onShowLevelStartScreen", lf_show);
            damage_function(lua, 2, "isLevelStartScreenDone", lf_done);
            lf_set(lua, 1, (int)index + 1);
            lf_shows[index] = lf_checks[index] = 0;
        }
        damage_lua.set_field(lua, -10002, "_ab_listeners");
        damage_lua.create_table(lua, 32, 0);
        damage_lua.set_field(lua, -10002, "_ab_requests");
        damage_lua.get_field(lua, -10002, "gamelua");
        damage_lua.get_field(lua, 1, "eventManager");
        damage_lua.get_field(lua, 2, "addEventListener");
        damage_lua.get_field(lua, 1, "eventManager");
        damage_lua.push_string(lua, "done");
        damage_lua.create_table(lua, 0, 2);
        damage_lua.push_string(lua, "observer");
        damage_lua.set_field(lua, 6, "name");
        damage_function(lua, 6, "eventTriggered", lf_event);
        lf_checked_call(lua, 3, 0);
        damage_lua.set_top(lua, 0);
        if (lf_failed)
            fail("Original level-flow setup failed\n");
        lf_created = lf_paused = lf_done_events = 0;
        for (lf_step = 0; lf_step < 27; ++lf_step) {
            lf_count = 0;
            ReferenceLevelFlow input = reference_level_flow(lf_profile, lf_step);
            if (lf_failed) {
                /* Preserve failed source state; do not resume it. */
            } else if (input.operation == REFERENCE_FLOW_PUSH)
                lf_push(lua, input.name, input.listener, input.appearing);
            else if (input.operation == REFERENCE_FLOW_UPDATE)
                lf_method(lua, "update");
            else if (input.operation == REFERENCE_FLOW_CLEAR)
                lf_method(lua, "clearInitQueue");
            else if (input.operation != REFERENCE_FLOW_SNAPSHOT)
                lf_notify(lua, input.operation == REFERENCE_FLOW_LOADING_INIT
                                   ? "init"
                                   : "loaded");
            uint32_t header[] = {lf_profile, lf_step, lf_count, lf_failed}, state[32];
            lf_state(lua, state);
            print_words(header, 4);
            print_words(state, 32);
            for (unsigned row = 0; row < lf_count; ++row)
                print_words(lf_trace[row], 10);
        }
        damage_lua.close_state(lua);
    }
    semihost(4, "AB level-flow reference complete\n");
}

#endif
