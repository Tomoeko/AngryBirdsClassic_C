#ifndef AB_ORACLE_UI_FRAME_TREE_H
#define AB_ORACLE_UI_FRAME_TREE_H

#include "ui_frame_tree_cases.h"
#include "ui_value_probe.h"

static const char *const ui_tree_globals[] = {"_ab_tree_a", "_ab_tree_b", "_ab_tree_c",
                                              "_ab_tree_d", "_ab_tree_e", "_ab_tree_f",
                                              "_ab_tree_g", "_ab_tree_h"};

static unsigned (*ui_tree_length)(void *, int);
static void (*ui_tree_raw_get)(void *, int, int);
static uint32_t ui_tree_events[16][10], ui_tree_event_count;
static uint32_t ui_tree_counts[8], ui_tree_order[8][8], ui_tree_parents[8];
static uint32_t ui_tree_entered[8][2];

static int ui_tree_unused_function(void *lua) {
    (void)lua;
    fail("Original tree fixture invoked an identity-only function\n");
}

static void ui_tree_push(void *lua, const uint32_t value[2]) {
    if (value[0] == 5) {
        damage_lua.get_field(lua, -10002, "_ab_tree_function");
    } else {
        ui_reference_value_push(lua, value[0], value[1], ui_tree_texts, 2);
    }
}

static void ui_tree_read(void *lua, int index, uint32_t output[2]) {
    if (damage_lua.type(lua, index) != 6) {
        ui_reference_value_read(lua, index, output, ui_tree_texts, 2);
        return;
    }
    int top = damage_lua.get_top(lua);
    const void *function = damage_lua.to_pointer(lua, index);
    damage_lua.get_field(lua, -10002, "_ab_tree_function");
    if (function != damage_lua.to_pointer(lua, -1)) {
        fail("Unexpected original tree value function identity\n");
    }
    damage_lua.set_top(lua, top);
    output[0] = 5;
    output[1] = 0;
}

static uint32_t ui_tree_id(void *lua, int index) {
    if (damage_lua.type(lua, index) == 0) {
        return 0;
    }
    if (damage_lua.type(lua, index) != 5) {
        fail("Original tree result is not a node\n");
    }
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, index, "_ab_id");
    uint32_t id = (uint32_t)damage_lua.to_number(lua, -1);
    damage_lua.set_top(lua, top);
    if (!id || id > 8) {
        fail("Original tree node identity exceeds bounds\n");
    }
    return id;
}

static void ui_tree_node(void *lua, uint32_t index) {
    if (index >= 8) {
        fail("Original tree fixture node exceeds bounds\n");
    }
    damage_lua.get_field(lua, -10002, ui_tree_globals[index]);
}

static void ui_tree_counter(void *lua, uint32_t output[2]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_tree_frame");
    damage_lua.get_field(lua, top + 1, "entryCounts");
    ui_tree_read(lua, top + 2, output);
    damage_lua.set_top(lua, top);
}

static int ui_tree_method(void *lua) {
    uint32_t method = (uint32_t)damage_lua.to_number(lua, -10003);
    if (damage_lua.get_top(lua) != 1 || method >= 3 || ui_tree_event_count == 16) {
        fail("Unexpected original tree method boundary\n");
    }
    uint32_t *event = ui_tree_events[ui_tree_event_count++];
    for (size_t index = 0; index < 10; ++index) {
        event[index] = 0;
    }
    event[0] = method + 1;
    event[1] = ui_tree_id(lua, 1);
    damage_lua.get_field(lua, 1, "entered");
    ui_tree_read(lua, 2, event + 2);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "_parent");
    event[4] = ui_tree_id(lua, 2);
    if (event[4]) {
        damage_lua.get_field(lua, 2, "children");
        event[8] = ui_tree_length(lua, 3);
        damage_lua.set_top(lua, 2);
        damage_lua.get_field(lua, 2, "_lookup");
        damage_lua.get_field(lua, 3, ui_tree_names[event[1] - 1]);
        if (damage_lua.type(lua, 4) == 5) {
            damage_lua.get_field(lua, 4, "index");
            event[9] = (uint32_t)damage_lua.to_number(lua, 5);
        }
    }
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, 1, "children");
    event[5] = ui_tree_length(lua, 2);
    damage_lua.set_top(lua, 1);
    ui_tree_counter(lua, event + 6);
    if (method < 2) {
        damage_lua.push_boolean(lua, method == 0);
        damage_lua.set_field(lua, 1, "entered");
    }
    return 0;
}

static void ui_tree_install_method(void *lua, int table, const char *name,
                                   uint32_t method) {
    damage_lua.push_number(lua, (float)method);
    damage_lua.push_closure(lua, ui_tree_method, 1);
    damage_lua.set_field(lua, table, name);
}

static void ui_tree_reset_node(void *lua, uint32_t node) {
    damage_lua.get_field(lua, -10002, "tree-init");
    ui_tree_node(lua, node);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_string(lua, ui_tree_names[node]);
    damage_lua.set_field(lua, 3, "name");
    if (damage_lua.call(lua, 2, 0, 0)) {
        fail("Original tree node initialization failed\n");
    }
}

static void ui_tree_initialize_nodes(void *lua) {
    damage_lua.get_field(lua, -10002, "_ab_tree_frame");
    damage_lua.push_nil(lua);
    damage_lua.set_field(lua, 1, "entryCounts");
    damage_lua.set_top(lua, 0);
    for (uint32_t node = 0; node < 8; ++node) {
        damage_lua.create_table(lua, 0, 22);
        damage_lua.push_number(lua, (float)(node + 1));
        damage_lua.set_field(lua, 1, "_ab_id");
        ui_tree_install_method(lua, 1, "onEntry", 0);
        ui_tree_install_method(lua, 1, "onExit", 1);
        ui_tree_install_method(lua, 1, "layout", 2);
        damage_lua.get_field(lua, -10002, "tree-remove");
        damage_lua.set_field(lua, 1, "removeChild");
        damage_lua.get_field(lua, -10002, "tree-child");
        damage_lua.set_field(lua, 1, "getChild");
        damage_lua.set_field(lua, -10002, ui_tree_globals[node]);
        ui_tree_reset_node(lua, node);
        ui_tree_counts[node] = ui_tree_parents[node] = 0;
        ui_tree_entered[node][0] = ui_tree_entered[node][1] = 0;
    }
}

static void ui_tree_snapshot(void *lua) {
    uint32_t counter[2];
    ui_tree_counter(lua, counter);
    print_words(counter, 2);
    for (uint32_t node = 0; node < 8; ++node) {
        ui_tree_node(lua, node);
        uint32_t output[4];
        damage_lua.get_field(lua, 1, "entered");
        ui_tree_read(lua, 2, output);
        ui_tree_entered[node][0] = output[0];
        ui_tree_entered[node][1] = output[1];
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "_parent");
        output[2] = ui_tree_parents[node] = ui_tree_id(lua, 2);
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "children");
        output[3] = ui_tree_counts[node] = ui_tree_length(lua, 2);
        if (output[3] > 8) {
            fail("Original tree children exceed bounded fixture\n");
        }
        print_words(output, 4);
        for (uint32_t index = 0; index < output[3]; ++index) {
            ui_tree_raw_get(lua, 2, (int)index + 1);
            ui_tree_order[node][index] = ui_tree_id(lua, 3);
            damage_lua.set_top(lua, 2);
        }
        if (output[3]) {
            print_words(ui_tree_order[node], output[3]);
        }
        damage_lua.set_top(lua, 1);
        damage_lua.get_field(lua, 1, "_lookup");
        uint32_t lookup[16] = {0};
        for (uint32_t child = 0; child < 8; ++child) {
            damage_lua.get_field(lua, 2, ui_tree_names[child]);
            if (damage_lua.type(lua, 3) == 5) {
                damage_lua.get_field(lua, 3, "child");
                lookup[child * 2] = ui_tree_id(lua, 4);
                damage_lua.set_top(lua, 3);
                damage_lua.get_field(lua, 3, "index");
                lookup[child * 2 + 1] = (uint32_t)damage_lua.to_number(lua, 4);
            }
            damage_lua.set_top(lua, 2);
        }
        print_words(lookup, 16);
        damage_lua.set_top(lua, 0);
    }
}

static void ui_tree_run(void *lua, const uint32_t command[8]) {
    static const char *const methods[] = {
        "tree-add",   "tree-remove", "tree-clear",  "tree-pop",   "tree-self",
        "tree-enter", "tree-exit",   "tree-layout", "tree-child", "tree-parent"};
    ui_tree_event_count = 0;
    uint32_t returned = 0;
    if (command[0] == UI_TREE_SET_ENTERED) {
        ui_tree_node(lua, command[1]);
        ui_tree_push(lua, command + 5);
        damage_lua.set_field(lua, 1, "entered");
        damage_lua.set_top(lua, 0);
    } else if (command[0] == UI_TREE_SET_COUNTER) {
        damage_lua.get_field(lua, -10002, "_ab_tree_frame");
        ui_tree_push(lua, command + 5);
        damage_lua.set_field(lua, 1, "entryCounts");
        damage_lua.set_top(lua, 0);
    } else if (command[0] == UI_TREE_RESET) {
        ui_tree_reset_node(lua, command[1]);
    } else {
        damage_lua.get_field(lua, -10002, methods[command[0]]);
        ui_tree_node(lua, command[1]);
        int count = 1;
        if (command[0] == UI_TREE_ADD || command[0] == UI_TREE_REMOVE) {
            ui_tree_node(lua, command[2]);
            ++count;
        }
        if (command[0] == UI_TREE_ADD) {
            if (command[3]) {
                damage_lua.push_number(lua, (float)command[4]);
            } else {
                damage_lua.push_boolean(lua, 0);
            }
            ++count;
        } else if (command[0] == UI_TREE_CHILD) {
            for (uint32_t part = 0; part < command[7]; ++part) {
                damage_lua.push_string(lua, ui_tree_names[command[2 + part]]);
                ++count;
            }
        }
        if (damage_lua.call(lua, count, 1, 0)) {
            const char *message = damage_lua.to_string(lua, -1, NULL);
            fail(message ? message : "Original Frame hierarchy command failed\n");
        }
        returned = ui_tree_id(lua, 1);
        damage_lua.set_top(lua, 0);
    }
    uint32_t output[] = {returned, ui_tree_event_count};
    print_words(output, 2);
    for (uint32_t event = 0; event < ui_tree_event_count; ++event) {
        print_words(ui_tree_events[event], 10);
    }
    ui_tree_snapshot(lua);
}

static int ui_tree_member(uint32_t parent, uint32_t child) {
    for (uint32_t index = 0; index < ui_tree_counts[parent]; ++index) {
        if (ui_tree_order[parent][index] == child + 1) {
            return 1;
        }
    }
    return 0;
}

static void ui_tree_command(uint32_t *random, uint32_t output[8]) {
    output[0] = (random_word(random) >> 16) % 13;
    output[1] = (random_word(random) >> 16) % 8;
    output[2] = (random_word(random) >> 16) % 8;
    output[3] = (random_word(random) >> 16) & 1;
    output[4] = (random_word(random) >> 16) % (ui_tree_counts[output[1]] + 1) + 1;
    output[5] = (random_word(random) >> 16) % 6;
    uint32_t value = random_word(random) >> 16;
    static const uint32_t numbers[] = {0,          0x80000000, 0x3f800000,
                                       0xbf800000, 0x7fc12345, 0x7f800000};
    output[6] = output[5] == 1   ? value & 1
                : output[5] == 2 ? numbers[value % 6]
                : output[5] == 3 ? value & 1
                : output[5] == 4 ? value % 3 + 1
                                 : 0;
    output[7] = 0;
    uint32_t node = output[1], child = output[2];
    if (output[0] == UI_TREE_ADD) {
        size_t attempts = 0;
        while (attempts < 8 && ui_tree_member(node, child)) {
            child = (child + 1) % 8;
            ++attempts;
        }
        output[2] = child;
        if (attempts == 8) {
            output[0] = UI_TREE_CHILD;
        }
    } else if (output[0] == UI_TREE_REMOVE || output[0] == UI_TREE_POP) {
        if (!ui_tree_counts[node]) {
            output[0] = UI_TREE_CHILD;
        } else {
            output[2] = ui_tree_order[node][value % ui_tree_counts[node]] - 1;
        }
    } else if (output[0] == UI_TREE_REMOVE_SELF) {
        uint32_t parent = ui_tree_parents[node];
        if (!parent || !ui_tree_member(parent - 1, node)) {
            output[0] = UI_TREE_PARENT;
        }
    } else if (output[0] == UI_TREE_ENTER && ui_tree_entered[node][0] &&
               (ui_tree_entered[node][0] != 1 || ui_tree_entered[node][1])) {
        output[0] = UI_TREE_EXIT;
    } else if (output[0] == UI_TREE_SET_COUNTER) {
        static const uint32_t counts[] = {0,          0x80000000, 0x3f800000,
                                          0xbf800000, 0x4b800000, 0xcb800000};
        output[5] %= 3;
        output[6] = output[5] == 2 ? counts[value % 6] : 0;
    }
    if (output[0] == UI_TREE_CHILD) {
        output[7] = value % 4 + 1;
        for (uint32_t part = 0; part < output[7]; ++part) {
            output[2 + part] = (random_word(random) >> 16) % 8;
        }
    }
}

static void ui_tree_dense_prefix(size_t step, uint32_t command[8]) {
    if (step < 16) {
        command[0] = UI_TREE_ADD;
        command[1] = (uint32_t)(step / 8);
        command[2] = (uint32_t)(step % 8);
        command[3] = 1;
        command[4] = step < 8 || !(step & 1) ? 1 : ui_tree_counts[command[1]] + 1;
        return;
    }
    static const uint32_t sequence[][3] = {
        {UI_TREE_ENTER, 1, 0}, {UI_TREE_LAYOUT, 0, 0},      {UI_TREE_REMOVE, 0, 3},
        {UI_TREE_POP, 0, 0},   {UI_TREE_REMOVE_SELF, 4, 0}, {UI_TREE_CLEAR, 0, 0},
        {UI_TREE_EXIT, 1, 0},  {UI_TREE_ENTER, 1, 0},       {UI_TREE_CLEAR, 1, 0},
        {UI_TREE_RESET, 1, 0}};
    if (step - 16 < sizeof(sequence) / sizeof(*sequence)) {
        for (size_t field = 0; field < 3; ++field) {
            command[field] = sequence[step - 16][field];
        }
    }
}

static void probe_ui_frame_tree(void) {
    initialize_damage_lua();
    ui_tree_length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    ui_tree_raw_get =
        (void (*)(void *, int, int))find_function("_Z11lua_rawgetiP9lua_Stateii");
    const char *(*set_upvalue)(void *, int, int) =
        (const char *(*)(void *, int, int))find_function(
            "_Z14lua_setupvalueP9lua_Stateii");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original Frame hierarchy state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    int (*open_table)(void *) =
        (int (*)(void *))find_function("_Z13luaopen_tableP9lua_State");
    (void)open_table(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "_ab_tree_function", ui_tree_unused_function);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "_ab_tree_frame");
    const char *names[] = {"tree-init",   "tree-add",   "tree-remove", "tree-clear",
                           "tree-self",   "tree-pop",   "tree-enter",  "tree-exit",
                           "tree-layout", "tree-child", "tree-parent"};
    const char *paths[] = {"/data/local/tmp/ab520/ui/tree-init-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-add-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-remove-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-clear-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-self-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-pop-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-enter-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-exit-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-layout-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-child-oracle.lua",
                           "/data/local/tmp/ab520/ui/tree-parent-oracle.lua"};
    for (size_t index = 0; index < 11; ++index) {
        load_selected_function(lua, paths[index], names[index]);
        if (index == 6 || index == 7) {
            damage_lua.get_field(lua, -10002, names[index]);
            damage_lua.get_field(lua, -10002, "_ab_tree_frame");
            if (!set_upvalue(lua, 1, 1)) {
                fail("Cannot bind original Frame lifecycle counter upvalue\n");
            }
            damage_lua.set_top(lua, 0);
        }
    }
    uint32_t random = 0x5207eec1;
    semihost(4, "AB_ANDROID_UI_FRAME_TREE_V2 32 128\n");
    for (size_t instance = 0; instance < 32; ++instance) {
        ui_tree_initialize_nodes(lua);
        for (size_t step = 0; step < 128; ++step) {
            uint32_t command[8];
            ui_tree_command(&random, command);
            ui_tree_dense_prefix(step, command);
            print_words(command, 8);
            ui_tree_run(lua, command);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI frame tree reference complete\n");
}

#endif
