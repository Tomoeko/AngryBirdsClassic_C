#ifndef AB_ORACLE_UI_IMAGE_STATE_H
#define AB_ORACLE_UI_IMAGE_STATE_H

#include "ui_image_state_cases.h"
#include "ui_value_probe.h"

static void (*ui_image_state_push_value)(void *, int);
static int (*ui_image_state_set_metatable)(void *, int);
static unsigned (*ui_image_state_length)(void *, int);
static int (*ui_image_state_next)(void *, int);
static uint32_t ui_image_state_setup[8], ui_image_state_old[70],
    ui_image_state_args[40];
static uint32_t ui_image_state_map[5], ui_image_state_geometry[5][4];
static uint32_t ui_image_state_events[8][80], ui_image_state_event_count;
static const void *ui_image_state_self_pointer, *ui_image_state_old_padding;
static unsigned ui_image_state_require_count;
static int ui_image_state_recording;

static int ui_image_state_identity_function(void *lua) {
    (void)lua;
    fail("Original image constructor invoked an identity-only value\n");
}

static void ui_image_state_push(void *lua, const uint32_t input[2]) {
    if (input[0] == 5) {
        damage_lua.get_field(lua, -10002, "_ab_image_state_function");
    } else {
        ui_reference_value_push(lua, input[0], input[1], ui_image_state_texts, 9);
    }
}

static void ui_image_state_read(void *lua, int index, uint32_t output[2]) {
    if (damage_lua.type(lua, index) != 6) {
        ui_reference_value_read(lua, index, output, ui_image_state_texts, 9);
        return;
    }
    int top = damage_lua.get_top(lua);
    const void *value = damage_lua.to_pointer(lua, index);
    damage_lua.get_field(lua, -10002, "_ab_image_state_function");
    if (value != damage_lua.to_pointer(lua, -1)) {
        fail("Unexpected original image constructor function identity\n");
    }
    damage_lua.set_top(lua, top);
    output[0] = 5;
    output[1] = 0;
}

static void ui_image_state_frame(void *lua) {
    damage_lua.get_field(lua, -10002, "ui");
    int table = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, "Frame");
}

static void ui_image_state_counter(void *lua, const uint32_t value[2]) {
    int top = damage_lua.get_top(lua);
    ui_image_state_frame(lua);
    int frame = damage_lua.get_top(lua);
    ui_image_state_push(lua, value);
    damage_lua.set_field(lua, frame, "entryCounts");
    damage_lua.set_top(lua, top);
}

static void ui_image_state_capture(void *lua, uint32_t output[76]) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "_ab_image_state_self");
    int self = top + 1;
    for (size_t field = 0; field < 35; ++field) {
        damage_lua.get_field(lua, self, ui_image_state_fields[field]);
        ui_image_state_read(lua, self + 1, output + field * 2);
        damage_lua.set_top(lua, self);
    }
    damage_lua.get_field(lua, self, "children");
    output[70] = ui_image_state_length(lua, self + 1);
    damage_lua.set_top(lua, self);
    damage_lua.get_field(lua, self, "_lookup");
    output[71] = ui_image_state_length(lua, self + 1);
    damage_lua.set_top(lua, self);
    damage_lua.get_field(lua, self, "entered");
    ui_image_state_read(lua, self + 1, output + 72);
    damage_lua.set_top(lua, self);
    ui_image_state_frame(lua);
    int frame = damage_lua.get_top(lua);
    damage_lua.get_field(lua, frame, "entryCounts");
    ui_image_state_read(lua, frame + 1, output + 74);
    damage_lua.set_top(lua, top);
}

static uint32_t *ui_image_state_event(void *lua, uint32_t kind, uint32_t id,
                                      uint32_t first, uint32_t second) {
    if (!ui_image_state_recording || ui_image_state_event_count == 8) {
        fail("Unexpected original image constructor boundary\n");
    }
    uint32_t *event = ui_image_state_events[ui_image_state_event_count++];
    event[0] = kind;
    event[1] = id;
    event[2] = first;
    event[3] = second;
    ui_image_state_capture(lua, event + 4);
    return event;
}

static int ui_image_state_name(void *lua) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 5) {
        fail("Unexpected original image constructor name boundary\n");
    }
    ui_image_state_self_pointer = damage_lua.to_pointer(lua, 1);
    ui_image_state_push_value(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_image_state_self");
    (void)ui_image_state_event(lua, 0, 0, 6, 0);
    damage_lua.push_string(lua, ui_image_state_texts[6]);
    return 1;
}

static int ui_image_state_resource(void *lua, uint32_t kind) {
    if (damage_lua.get_top(lua) != 1 || damage_lua.type(lua, 1) != 4) {
        fail("Unexpected original image constructor resource arguments\n");
    }
    uint32_t id = 5;
    for (uint32_t candidate = 0; candidate < 5; ++candidate) {
        if (damage_named(lua, 1, ui_image_state_texts[candidate])) {
            id = candidate;
            break;
        }
    }
    if (id == 5) {
        fail("Original image constructor used an unknown resource\n");
    }
    uint32_t first = kind == 1 ? ui_image_state_map[id]
                               : ui_image_state_geometry[id][(kind - 2) * 2];
    uint32_t second = kind == 1 ? 0 : ui_image_state_geometry[id][(kind - 2) * 2 + 1];
    (void)ui_image_state_event(lua, kind, id, first, second);
    if (kind == 1) {
        damage_lua.push_string(lua, ui_image_state_texts[first]);
        return 1;
    }
    FloatBits x = {.bits = first}, y = {.bits = second};
    damage_lua.push_number(lua, x.number);
    damage_lua.push_number(lua, y.number);
    return 2;
}

static int ui_image_state_localize(void *lua) {
    return ui_image_state_resource(lua, 1);
}

static int ui_image_state_bounds(void *lua) {
    return ui_image_state_resource(lua, 2);
}

static int ui_image_state_pivot(void *lua) {
    return ui_image_state_resource(lua, 3);
}

static int ui_image_state_require(void *lua) {
    static const char *const expected[] = {"lua/lua.lua", "lua/Class.lua",
                                           "ui/Frame.lua", "ui/Frame.lua"};
    if (damage_lua.get_top(lua) != 1 || ui_image_state_require_count >= 4 ||
        !damage_named(lua, 1, expected[ui_image_state_require_count])) {
        fail("Unexpected original image class dependency request\n");
    }
    ++ui_image_state_require_count;
    return 0;
}

static void ui_image_state_prepare_arguments(void *lua) {
    if (ui_image_state_setup[1] != 2) {
        if (ui_image_state_setup[1]) {
            damage_lua.push_boolean(lua, 0);
        } else {
            damage_lua.push_nil(lua);
        }
        return;
    }
    damage_lua.create_table(lua, 0, 24);
    int table = damage_lua.get_top(lua);
    for (size_t field = 0; field < 20; ++field) {
        ui_image_state_push(lua, ui_image_state_args + field * 2);
        damage_lua.set_field(lua, table, ui_image_state_arguments[field]);
    }
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, table, "floorCoordinates");
    damage_lua.push_number(lua, 999);
    damage_lua.set_field(lua, table, "alpha");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, table, "clickStarted");
}

static void ui_image_state_old_object(void *lua, int button) {
    damage_lua.create_table(lua, 0, 42);
    for (size_t field = 0; field < 35; ++field) {
        ui_image_state_push(lua, ui_image_state_old + field * 2);
        damage_lua.set_field(lua, 1, ui_image_state_fields[field]);
    }
    ui_image_state_push(lua, ui_image_state_setup + 3);
    damage_lua.set_field(lua, 1, "entered");
    damage_lua.create_table(lua, 0, 1);
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 2, "old");
    ui_image_state_push_value(lua, 2);
    damage_lua.set_field(lua, 1, "children");
    damage_lua.set_field(lua, 1, "_lookup");
    damage_lua.get_field(lua, 1, "hitAreaPadding");
    ui_image_state_old_padding = damage_lua.to_pointer(lua, 2);
    damage_lua.set_top(lua, 1);
    damage_lua.get_field(lua, -10002, button ? "ImageButton" : "Image");
    if (!ui_image_state_set_metatable(lua, 1)) {
        fail("Cannot prepare original image object metatable\n");
    }
    ui_image_state_self_pointer = damage_lua.to_pointer(lua, 1);
    damage_lua.set_field(lua, -10002, "_ab_image_state_self");
}

static uint32_t ui_image_state_padding(void *lua, int button) {
    uint32_t kind = ui_image_state_setup[1] == 2 ? ui_image_state_args[24] : 0;
    uint32_t payload = ui_image_state_args[25];
    int defaulted = button && (!kind || (kind == 1 && !payload));
    if (!defaulted) {
        return 0;
    }
    damage_lua.get_field(lua, -10002, "_ab_image_state_self");
    damage_lua.get_field(lua, 1, "hitAreaPadding");
    if (damage_lua.type(lua, 2) != 5 || ui_image_state_length(lua, 2) ||
        damage_lua.to_pointer(lua, 2) == ui_image_state_old_padding) {
        fail("Original button did not create fresh empty padding\n");
    }
    damage_lua.push_nil(lua);
    if (ui_image_state_next(lua, 2)) {
        fail("Original default button padding is not empty\n");
    }
    /* Label only after checking the actual new empty table, after all constructor
     * effects/callbacks. Allocation timing is not part of the C value model. */
    damage_lua.push_number(lua, (float)ui_image_state_setup[7]);
    damage_lua.set_field(lua, 2, "_ab_ui_identity");
    damage_lua.set_top(lua, 0);
    return 1;
}

static void ui_image_state_run(void *lua) {
    int button = (ui_image_state_setup[0] & 1) != 0;
    int fresh = (ui_image_state_setup[0] & 2) != 0;
    ui_image_state_self_pointer = ui_image_state_old_padding = NULL;
    ui_image_state_counter(lua, ui_image_state_setup + 5);
    ui_image_state_event_count = 0;
    ui_image_state_recording = 1;
    if (!fresh) {
        ui_image_state_old_object(lua, button);
        damage_lua.get_field(lua, -10002,
                             button ? "_ab_button_init" : "_ab_image_init");
        damage_lua.get_field(lua, -10002, "_ab_image_state_self");
    } else {
        damage_lua.get_field(lua, -10002, button ? "_ab_button_new" : "_ab_image_new");
        damage_lua.get_field(lua, -10002, button ? "ImageButton" : "Image");
    }
    ui_image_state_prepare_arguments(lua);
    if (damage_lua.call(lua, 2, fresh ? 1 : 0, 0)) {
        const char *message = damage_lua.to_string(lua, -1, NULL);
        fail(message ? message : "Original image constructor failed\n");
    }
    if (fresh && damage_lua.to_pointer(lua, 1) != ui_image_state_self_pointer) {
        fail("Original Class.new did not return captured constructor self\n");
    }
    damage_lua.set_top(lua, 0);
    uint32_t default_padding = ui_image_state_padding(lua, button);
    uint32_t output[76];
    ui_image_state_capture(lua, output);
    uint32_t metadata[] = {default_padding, ui_image_state_event_count};
    print_words(metadata, 2);
    print_words(output, 76);
    for (uint32_t event = 0; event < ui_image_state_event_count; ++event) {
        print_words(ui_image_state_events[event], 80);
    }
    if (ui_image_state_setup[2]) {
        damage_lua.get_field(lua, -10002, "_ab_image_state_self");
        static const size_t restored[] = {21, 22, 23, 33, 34};
        for (size_t field = 0; field < 5; ++field) {
            size_t index = restored[field];
            ui_image_state_push(lua, ui_image_state_old + index * 2);
            damage_lua.set_field(lua, 1, ui_image_state_fields[index]);
        }
        ui_image_state_push(lua, ui_image_state_setup + 3);
        damage_lua.set_field(lua, 1, "entered");
        damage_lua.get_field(lua, 1, "onEntry");
        damage_lua.set_field(lua, -10002, "_ab_image_entry");
        damage_lua.set_top(lua, 0);
        ui_image_state_counter(lua, ui_image_state_setup + 5);
        ui_image_state_recording = 0;
        damage_lua.get_field(lua, -10002, "_ab_image_entry");
        damage_lua.get_field(lua, -10002, "_ab_image_state_self");
        uint32_t status = damage_lua.call(lua, 1, 0, 0) != 0;
        damage_lua.set_top(lua, 0);
        print_words(&status, 1);
        ui_image_state_capture(lua, output);
        print_words(output, 76);
    }
    ui_image_state_recording = 0;
}

static void ui_image_state_value(uint32_t *random, uint32_t output[2]) {
    output[0] = (random_word(random) >> 16) % 6;
    uint32_t value = random_word(random) >> 16;
    static const uint32_t numbers[] = {0,          0x80000000, 0x3f800000, 0xbf800000,
                                       0x7fc12345, 0x7f800000, 1,          0x43f00000};
    output[1] = output[0] == 1   ? value & 1
                : output[0] == 2 ? numbers[value % 8]
                : output[0] == 3 ? value % 9
                : output[0] == 4 ? value % 17 + 1
                                 : 0;
}

static void probe_ui_image_state(void) {
    initialize_damage_lua();
    ui_image_state_push_value =
        (void (*)(void *, int))find_function("_Z13lua_pushvalueP9lua_Statei");
    ui_image_state_set_metatable =
        (int (*)(void *, int))find_function("_Z16lua_setmetatableP9lua_Statei");
    ui_image_state_length =
        (unsigned (*)(void *, int))find_function("_Z10lua_objlenP9lua_Statei");
    ui_image_state_next =
        (int (*)(void *, int))find_function("_Z8lua_nextP9lua_Statei");
    void *lua = damage_lua.new_state();
    if (!lua) {
        fail("Cannot allocate original image class state\n");
    }
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", ui_image_state_require);
    damage_function(lua, -10002, "tostring", ui_image_state_name);
    damage_function(lua, -10002, "_ab_image_state_function",
                    ui_image_state_identity_function);
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "getLocalizationString", ui_image_state_localize);
    damage_lua.push_boolean(lua, 0);
    damage_lua.set_field(lua, 1, "g_enableHoverScaling");
    damage_lua.push_boolean(lua, 1);
    damage_lua.set_field(lua, 1, "g_scale_buttons_on_touch");
    damage_lua.set_field(lua, -10002, "gamelua");
    damage_lua.create_table(lua, 0, 2);
    damage_function(lua, 1, "getSpriteBounds", ui_image_state_bounds);
    damage_function(lua, 1, "getSpritePivot", ui_image_state_pivot);
    damage_lua.set_field(lua, -10002, "res");
    const char *paths[] = {"/data/local/tmp/ab520/ui/ctor-lua-base.lua",
                           "/data/local/tmp/ab520/ui/ctor-class.lua",
                           "/data/local/tmp/ab520/ui/ctor-Frame.lua",
                           "/data/local/tmp/ab520/ui/ctor-Image.lua",
                           "/data/local/tmp/ab520/ui/ctor-ImageButton.lua"};
    for (size_t index = 0; index < 5; ++index) {
        load_damage_chunk(lua, paths[index]);
    }
    if (ui_image_state_require_count != 4) {
        fail("Original image class dependencies were not prepared\n");
    }
    for (int button = 0; button < 2; ++button) {
        damage_lua.get_field(lua, -10002, button ? "ImageButton" : "Image");
        damage_lua.get_field(lua, 1, "new");
        damage_lua.set_field(lua, -10002, button ? "_ab_button_new" : "_ab_image_new");
        damage_lua.get_field(lua, 1, "init");
        damage_lua.set_field(lua, -10002,
                             button ? "_ab_button_init" : "_ab_image_init");
        damage_lua.set_top(lua, 0);
        damage_lua.get_field(lua, -10002, button ? "_ab_button_new" : "_ab_image_new");
        damage_lua.get_field(lua, -10002, button ? "ImageButton" : "Image");
        damage_lua.create_table(lua, 0, 1);
        damage_lua.push_string(lua, "named");
        damage_lua.set_field(lua, 3, "name");
        if (damage_lua.call(lua, 2, 0, 0)) {
            fail("Cannot prepare original class __index state\n");
        }
    }
    uint32_t random = 0x5201a6e1;
    semihost(4, "AB_ANDROID_UI_IMAGE_STATE_V1 4096\n");
    for (size_t test = 0; test < 4096; ++test) {
        uint32_t *setup = ui_image_state_setup;
        setup[0] = (uint32_t)(test % 4);
        setup[1] = test % 11 < 9 ? 2 : (uint32_t)(test % 2);
        setup[2] = (uint32_t)((test & 1) && test % 3 == 0);
        ui_image_state_value(&random, setup + 3);
        setup[5] = (random_word(&random) >> 16) % 3;
        static const uint32_t counters[] = {0,          0x80000000, 0x3f800000,
                                            0xbf800000, 0x4b800000, 0xcb800000};
        setup[6] = setup[5] == 2 ? counters[random_word(&random) % 6] : 0;
        setup[7] = (uint32_t)test + 100;
        for (size_t field = 0; field < 35; ++field) {
            ui_image_state_value(&random, ui_image_state_old + field * 2);
        }
        for (size_t field = 0; field < 20; ++field) {
            ui_image_state_value(&random, ui_image_state_args + field * 2);
        }
        if (setup[0] & 2) {
            ui_image_state_args[0] = (uint32_t)((test >> 2) & 1);
            ui_image_state_args[1] = 0;
        }
        uint32_t image_kind = (random_word(&random) >> 16) % 3;
        ui_image_state_args[20] = image_kind == 2 ? 3 : image_kind;
        ui_image_state_args[21] =
            image_kind == 2 ? (random_word(&random) >> 16) % 5 : 0;
        print_words(setup, 8);
        print_words(ui_image_state_old, 70);
        print_words(ui_image_state_args, 40);
        for (uint32_t image = 0; image < 5; ++image) {
            ui_image_state_map[image] = (random_word(&random) >> 16) % 5;
            for (size_t field = 0; field < 4; ++field) {
                ui_image_state_geometry[image][field] =
                    random_float(&random, 126, 6, 1);
            }
        }
        print_words(ui_image_state_map, 5);
        for (size_t image = 0; image < 5; ++image) {
            print_words(ui_image_state_geometry[image], 4);
        }
        ui_image_state_run(lua);
    }
    damage_lua.close_state(lua);
    semihost(4, "AB UI image state reference complete\n");
}

#endif
