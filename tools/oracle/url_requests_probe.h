#ifndef AB_ORACLE_URL_REQUESTS_H
#define AB_ORACLE_URL_REQUESTS_H

static const char *ur_urls[] = {"u0", "u1", "u2", "u3"};
static const char *ur_saved[] = {"_ab_url_old0", "_ab_url_old1", "_ab_url_old2",
                                 "_ab_url_old3"};
static unsigned ur_case, ur_count;
static uint32_t ur_rows[32][8];

static unsigned ur_id(void *lua, int index) {
    unsigned length = 0;
    const char *value = damage_lua.to_string(lua, index, &length);
    if (!value || length != 2 || value[0] != 'u' || value[1] < '0' || value[1] > '3')
        fail("Unexpected original URL identifier\n");
    return (unsigned)(value[1] - '0');
}

static void ur_field_value(void *lua, int table, const char *name, uint32_t *output) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, table, name);
    output[0] = (uint32_t)damage_lua.type(lua, -1);
    output[1] = 0;
    if (output[0] == 3) {
        FloatBits value = {.number = damage_lua.to_number(lua, -1)};
        output[1] = value.bits;
    } else if (output[0] == 1)
        output[1] = (uint32_t)damage_lua.to_boolean(lua, -1);
    else if (output[0] != 0)
        fail("Unexpected original URL metadata\n");
    damage_lua.set_top(lua, top);
}

static void ur_lookup(void *lua, unsigned index) {
    damage_lua.get_field(lua, -10002, "g_fetchURLsMappedToCallbackInfo");
    damage_lua.get_field(lua, -1, ur_urls[index]);
}

static void ur_record(void *lua, unsigned kind, unsigned id, int extra_index) {
    if (ur_count == 32)
        fail("Original URL trace exceeds bound\n");
    uint32_t *row = ur_rows[ur_count++];
    for (unsigned index = 0; index < 8; ++index)
        row[index] = 0;
    row[0] = kind;
    row[1] = id;
    if (extra_index) {
        row[2] = (uint32_t)damage_lua.type(lua, extra_index);
        if (row[2] != 3)
            fail("Unexpected original URL callback extra\n");
        FloatBits value = {.number = damage_lua.to_number(lua, extra_index)};
        row[3] = value.bits;
    }
    int top = damage_lua.get_top(lua);
    for (unsigned index = 0; index < 4; ++index) {
        ur_lookup(lua, index);
        row[4 + index] = UINT32_MAX;
        if (damage_lua.type(lua, -1) == 5) {
            uint32_t value[2];
            ur_field_value(lua, -1, "timer", value);
            if (value[0] != 3)
                fail("Nonnumeric original URL timer in trace\n");
            row[4 + index] = value[1];
        }
        damage_lua.set_top(lua, top);
    }
}

static void ur_start(void *lua, unsigned index, unsigned duration, float extra,
                     int omit_timeout);

static int ur_response_callback(void *lua) {
    unsigned id = (unsigned)damage_lua.to_number(lua, -10003);
    if (damage_lua.get_top(lua) != 3 || ur_id(lua, 1) != id ||
        !damage_named(lua, 2, "body"))
        fail("Unexpected original URL response callback\n");
    ur_record(lua, 5, id, 3);
    if ((ur_case & 4) && id == 2)
        ur_start(lua, 2, 7, 88, 0);
    return 0;
}

static int ur_timeout_callback(void *lua) {
    unsigned id = (unsigned)damage_lua.to_number(lua, -10003);
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected original URL timeout callback\n");
    ur_record(lua, 6, id, 1);
    if ((ur_case & 4) && !id) {
        int top = damage_lua.get_top(lua);
        ur_lookup(lua, 1);
        if (damage_lua.type(lua, -1) != 5)
            fail("Missing original URL mutation target\n");
        damage_number(lua, top + 2, "callbackExtraData", UINT32_C(0x429a0000));
        damage_lua.set_top(lua, top);
        ur_start(lua, 0, 6, 99, 0);
    }
    return 0;
}

static int ur_fetch(void *lua) {
    if (damage_lua.get_top(lua) != 1)
        fail("Unexpected original native fetch arguments\n");
    ur_record(lua, 2, ur_id(lua, 1), 0);
    return 0;
}

static int ur_log(void *lua) {
    unsigned length = 0;
    const char *value = damage_lua.to_string(lua, 1, &length);
    if (damage_lua.get_top(lua) != 1 || !value || length < 12 || length > 128)
        fail("Unexpected original URL diagnostic\n");
    unsigned kind = value[8] == 's' ? 1u : 3u;
    for (unsigned index = 0; index + 8 <= length; ++index)
        if (value[index] == 'R' && value[index + 1] == 'E' && value[index + 2] == 'S')
            kind = 4;
    unsigned id = 0;
    if (kind != 4) {
        if (value[length - 2] != 'u' || value[length - 1] < '0' ||
            value[length - 1] > '3')
            fail("Unexpected original logged URL\n");
        id = (unsigned)(value[length - 1] - '0');
    }
    ur_record(lua, kind, id, 0);
    return 0;
}

static void ur_start(void *lua, unsigned index, unsigned duration, float extra,
                     int omit_timeout) {
    int top = damage_lua.get_top(lua);
    damage_lua.get_field(lua, -10002, "URLRequests");
    damage_lua.get_field(lua, top + 1, "startURLFetch");
    damage_lua.get_field(lua, -10002, "URLRequests");
    damage_lua.push_string(lua, ur_urls[index]);
    damage_lua.push_number(lua, (float)index);
    damage_lua.push_closure(lua, ur_response_callback, 1);
    if (duration == 1)
        damage_lua.push_nil(lua);
    else if (duration == 4)
        damage_lua.push_boolean(lua, 0);
    else {
        static const float times[] = {0, 0, 0.25f, 0.5f, 0, -1, 2, 3};
        damage_lua.push_number(lua, times[duration]);
    }
    if (omit_timeout)
        damage_lua.push_nil(lua);
    else {
        damage_lua.push_number(lua, (float)index);
        damage_lua.push_closure(lua, ur_timeout_callback, 1);
    }
    damage_lua.push_number(lua, extra);
    if (damage_lua.call(lua, 6, 0, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original URL start failed\n");
    }
    damage_lua.set_top(lua, top);
}

static void ur_snapshot(void *lua, unsigned step) {
    uint32_t header[] = {ur_case, step, ur_count};
    print_words(header, 3);
    for (unsigned index = 0; index < 4; ++index) {
        uint32_t values[5] = {0};
        ur_lookup(lua, index);
        if (damage_lua.type(lua, 2) == 5) {
            values[0] = 1;
            ur_field_value(lua, 2, "timer", values + 1);
            ur_field_value(lua, 2, "callbackExtraData", values + 3);
        }
        damage_lua.set_top(lua, 0);
        print_words(values, 5);
    }
    for (unsigned index = 0; index < 4; ++index) {
        uint32_t values[2] = {0};
        damage_lua.get_field(lua, -10002, ur_saved[index]);
        if (damage_lua.type(lua, 1) == 5)
            ur_field_value(lua, 1, "timer", values);
        damage_lua.set_top(lua, 0);
        print_words(values, 2);
    }
    for (unsigned index = 0; index < ur_count; ++index) {
        print_words(ur_rows[index], 4);
        print_words(ur_rows[index] + 4, 4);
    }
}

static void ur_update(void *lua, float seconds) {
    damage_lua.get_field(lua, -10002, "URLRequests");
    damage_lua.get_field(lua, 1, "updateURLCallbackTimers");
    damage_lua.get_field(lua, -10002, "URLRequests");
    damage_lua.push_number(lua, seconds);
    if (damage_lua.call(lua, 2, 0, 0)) {
        const char *error = damage_lua.to_string(lua, -1, NULL);
        fail(error ? error : "Original URL update failed\n");
    }
    damage_lua.set_top(lua, 0);
}

static void probe_url_requests(void) {
    initialize_damage_lua();
    void *lua = damage_lua.new_state();
    if (!lua)
        fail("Cannot allocate original URL state\n");
    (void)damage_lua.open_base(lua);
    damage_lua.set_top(lua, 0);
    damage_function(lua, -10002, "native_startURLThread", ur_fetch);
    damage_function(lua, -10002, "loginfo", ur_log);
    semihost(4, "AB_ANDROID_URL_REQUESTS_V1 8 9\n");
    for (ur_case = 0; ur_case < 8; ++ur_case) {
        load_damage_chunk(lua, "/data/local/tmp/ab520/url-requests.lua");
        for (unsigned index = 0; index < 4; ++index) {
            damage_lua.push_nil(lua);
            damage_lua.set_field(lua, -10002, ur_saved[index]);
        }
        for (unsigned step = 0; step < 9; ++step) {
            ur_count = 0;
            if (step == 1) {
                ur_start(lua, 0, ur_case & 1 ? 1u : 0u, 10, 0);
                ur_start(lua, 1, 2, 11, (ur_case & 2) != 0);
                ur_start(lua, 2, 3, 12, 0);
                ur_start(lua, 3, 4, 13, 0);
                for (unsigned index = 0; index < 4; ++index) {
                    ur_lookup(lua, index);
                    damage_lua.set_field(lua, -10002, ur_saved[index]);
                    damage_lua.set_top(lua, 0);
                }
            } else if (step == 2 || step == 4 || step == 5 || step == 8)
                ur_update(lua, step == 2 ? 0.25f : step == 5 ? 0.125f : 0);
            else if (step == 3 || step == 6) {
                damage_lua.get_field(lua, -10002, "nativecallback_urlResponse");
                damage_lua.push_string(lua, "u2");
                damage_lua.push_string(lua, "body");
                if (damage_lua.call(lua, 2, 0, 0)) {
                    const char *error = damage_lua.to_string(lua, -1, NULL);
                    fail(error ? error : "Original URL response failed\n");
                }
            } else if (step == 7)
                ur_start(lua, 1, 5, 55, 0);
            ur_snapshot(lua, step);
        }
    }
    damage_lua.close_state(lua);
    semihost(4, "AB URL requests reference complete\n");
}

#endif
