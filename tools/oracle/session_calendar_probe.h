#ifndef AB_ORACLE_SESSION_CALENDAR_H
#define AB_ORACLE_SESSION_CALENDAR_H

/* Original calendar wrapper and original Class/deepCopy/formatTime/module
 * setup, shared by the timer and AngerLevel composition probes only. */
static const char *st_date_fields[] = {"year", "month",   "day",
                                       "hour", "minutes", "seconds"};
static const char *st_duration_fields[] = {"hours", "minutes", "seconds"};
static float st_date[6], st_elapsed;
static unsigned st_clock_calls;
static void *st_wrapper;
static uint32_t st_game[5];
static int (*st_native_difference)(void *, void *);

static void st_numeric_table(void *lua, const char **fields, const float *values,
                             unsigned count) {
    damage_lua.create_table(lua, 0, (int)count);
    int table = damage_lua.get_top(lua);
    for (unsigned index = 0; index < count; ++index) {
        FloatBits value = {.number = values[index]};
        damage_number(lua, table, fields[index], value.bits);
    }
}

static int st_clock(void *lua) {
    if (damage_lua.get_top(lua))
        fail("Unexpected session clock arguments\n");
    ++st_clock_calls;
    float values[6];
    for (unsigned index = 0; index < 6; ++index)
        values[index] = st_date[index];
    values[5] += st_elapsed;
    st_numeric_table(lua, st_date_fields, values, 6);
    return 1;
}

static int st_difference(void *lua) {
    if (damage_lua.get_top(lua) != 2)
        fail("Unexpected session calendar arguments\n");
    return st_native_difference(st_game, st_wrapper);
}

static void st_call(void *lua, const char *method, int arguments, int results) {
    /* Arguments are already above the timer and selected method. */
    if (damage_lua.call(lua, arguments, results, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : method);
    }
}

static void st_value(void *lua, int index, uint32_t *output) {
    output[0] = (uint32_t)damage_lua.type(lua, index);
    output[1] = 0;
    if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, index)};
        output[1] = value.bits;
    } else if (output[0] == 1)
        output[1] = (uint32_t)damage_lua.to_boolean(lua, index);
    else if (output[0] != 0)
        fail("Unexpected session scalar type\n");
}

static void st_field(void *lua, int table, const char *name, uint32_t *output) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    st_value(lua, -1, output);
    damage_lua.set_top(lua, top);
}

static void st_table_words(void *lua, int table, const char *field, const char **names,
                           unsigned count, uint32_t *output) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, field);
    output[0] = damage_lua.type(lua, -1) == 5;
    for (unsigned index = 0; index < count; ++index) {
        output[index + 1] = 0;
        if (output[0]) {
            uint32_t value[2];
            st_field(lua, top + 1, names[index], value);
            if (value[0] != 3)
                fail("Nonnumeric session date/duration field\n");
            output[index + 1] = value[1];
        }
    }
    damage_lua.set_top(lua, top);
}

static void *st_open(void) {
    initialize_damage_lua();
    void *(*allocate)(unsigned) = (void *(*)(unsigned))find_function("_Znwj");
    void (*construct)(void *) = (void (*)(void *))find_function("_ZN3lua8LuaStateC1Ev");
    void (*claim)(void *) = (void (*)(void *))find_function("_ZN4lang6Object5claimEv");
    st_native_difference = (int (*)(void *, void *))find_function(
        "_ZN7GameLua26getTimeDifferenceInSecondsEPN3lua8LuaStateE");
    int (*set_environment)(const char *, const char *, int) =
        (int (*)(const char *, const char *, int))find_function("setenv");
    void (*refresh_zone)(void) = (void (*)(void))find_function("tzset");
    if (set_environment("TZ", "UTC0", 1))
        fail("Cannot configure isolated session timer timezone\n");
    refresh_zone();
    uint32_t *wrapper = allocate(20);
    if (!wrapper)
        fail("Cannot allocate original session wrapper\n");
    construct(wrapper);
    claim(wrapper);
    st_wrapper = wrapper;
    st_game[4] = (uint32_t)(uintptr_t)wrapper;
    void *lua = (void *)(uintptr_t)wrapper[2];
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    (void)damage_lua.open_math(lua);
    damage_lua.set_top(lua, 0);
    int (*open_string)(void *) =
        (int (*)(void *))find_function("_Z14luaopen_stringP9lua_State");
    (void)open_string(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "requireFile", damage_require);
    damage_lua.create_table(lua, 0, 1);
    damage_lua.set_field(lua, -10002, "lua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/session-class.lua");
    load_selected_function(lua, "/data/local/tmp/ab520/session-copy.lua", "deepCopy");
    load_selected_function(lua, "/data/local/tmp/ab520/session-format.lua",
                           "formatTime");
    damage_lua.create_table(lua, 0, 3);
    damage_function(lua, 1, "getCurrentTime", st_clock);
    damage_function(lua, 1, "getTimeDifferenceInSeconds", st_difference);
    damage_lua.get_field(lua, -10002, "formatTime");
    damage_lua.set_field(lua, 1, "formatTime");
    damage_lua.set_field(lua, -10002, "gamelua");
    load_damage_chunk(lua, "/data/local/tmp/ab520/session-timer.lua");
    return lua;
}

static void st_close(void) {
    void (*release)(void *) =
        (void (*)(void *))find_function("_ZN4lang6Object7releaseEv");
    release(st_wrapper);
}

#endif
