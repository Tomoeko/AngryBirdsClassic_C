#include "ab/classic_root.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataTable table;
    AbDataEntry entry;
} RootNotice;

struct AbClassicRoot {
    AbClassicRootConfig config;
    RootNotice *notices;
    size_t notice_count, bytes;
    bool updating, failed;
};

static bool observe(AbClassicRoot *root, AbClassicRootOwnedPhase phase, AbError *error);

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbSpan value, const char *name) {
    size_t size = strlen(name);
    return value.size == size &&
           (!size || (value.data && !memcmp(value.data, name, size)));
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached root field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite reached root field");
}

static bool numeric_equal(AbDataValue value, float expected) {
    float actual;
    return value.kind == AB_DATA_NUMBER && numeric(value, &actual, NULL) &&
           actual == expected;
}

static bool advance(AbDataValue *value, float seconds, bool initialize,
                    AbError *error) {
    if (initialize && !truthy(*value)) {
        *value = number(0);
        return true;
    }
    float prior;
    if (!numeric(*value, &prior, error))
        return false;
    float result = prior + seconds;
    if (!isfinite(result))
        return ab_fail(error, "Root clock arithmetic overflow");
    *value = number(result);
    return true;
}

static bool key(const AbDataTable *keys, const char *name) {
    const AbDataValue *value = ab_data_get(keys, text(name));
    return value && truthy(*value);
}

static bool call(AbClassicRoot *root, AbClassicRootService service,
                 const AbDataValue *arguments, size_t count, AbDataValue *result,
                 AbError *error) {
    AbClassicRootCall *binding = root->config.services[service];
    if (!binding || !binding->call)
        return ab_fail(error, "Missing reached root service %u", (unsigned)service);
    AbDataValue ignored = {0};
    return binding->call(binding->context, arguments, count, result ? result : &ignored,
                         error);
}

static bool timed(AbClassicRoot *root, AbClassicRootService service, float seconds,
                  bool with_time, AbError *error) {
    AbDataValue arguments[] = {number(seconds), root->config.globals->time};
    return call(root, service, arguments, with_time ? 2u : 1u, NULL, error);
}

static bool notify(AbClassicRoot *root, const AbDataValue *id, AbError *error) {
    if (!root->config.events || !id)
        return ab_fail(error, "Missing reached root event binding");
    if (root->notice_count == root->config.notifications)
        return ab_fail(error, "Root notification storage exhausted");
    RootNotice *notice = root->notices + root->notice_count++;
    notice->entry = (AbDataEntry){text("id"), *id};
    notice->table = (AbDataTable){&notice->entry, 1, 1, NULL};
    return ab_event_manager_notify(root->config.events, &notice->table, error);
}

static bool valid_calls(const AbClassicRootCalls *calls) {
    return !calls ||
           (calls->count <= UINT32_C(16777216) && calls->count <= calls->capacity &&
            (!calls->capacity || calls->items));
}

bool ab_classic_root_open(AbClassicRootConfig config, AbClassicRoot **output,
                          AbError *error) {
    if (!output)
        return ab_fail(error, "Missing root owner output");
    *output = NULL;
    if (!config.state || !config.flags || !config.globals || !config.skip_input ||
        config.flags->menu_present != (config.menus != NULL) || !config.held_keys ||
        !config.pressed_keys || !config.released_keys || !config.processes ||
        !config.native_animation || !config.audio_ramp || !config.url_requests ||
        !config.anger || !config.eagle_query || !config.subsystems ||
        !config.tail_updates || !valid_calls(config.subsystems) ||
        !valid_calls(config.updatable) || !valid_calls(config.tail_updates) ||
        config.notifications > (SIZE_MAX - sizeof(AbClassicRoot)) / sizeof(RootNotice))
        return ab_fail(error, "Invalid root owner bindings");
    size_t bytes = sizeof(AbClassicRoot) + config.notifications * sizeof(RootNotice);
    if (bytes > config.storage_bytes)
        return ab_fail(error, "Root owner storage exceeds bound");
    AbClassicRoot *root = calloc(1, bytes);
    if (!root)
        return ab_fail(error, "Cannot allocate root owner");
    root->config = config;
    root->notices = (RootNotice *)(root + 1);
    root->bytes = bytes;
    *output = root;
    return true;
}

void ab_classic_root_close(AbClassicRoot *root) {
    free(root);
}

size_t ab_classic_root_bytes(const AbClassicRoot *root) {
    return root ? root->bytes : 0;
}

static bool prepare_view(AbClassicRoot *root, AbError *error) {
    AbClassicRootState *state = root->config.state;
    state->clip_offset = (AbVec2){0};
    if (state->updated_width.kind != AB_DATA_NIL &&
        state->updated_height.kind != AB_DATA_NIL) {
        if (truthy(root->config.flags->restricted_aspect))
            return ab_fail(error, "Restricted root viewport needs its source owner");
        if (!numeric(state->updated_width, &state->width, error) ||
            !numeric(state->updated_height, &state->height, error))
            return false;
    }
    AbDataValue viewport[] = {number(state->clip_offset.x),
                              number(state->clip_offset.y)};
    if (!call(root, AB_ROOT_VIEWPORT, viewport, 2, NULL, error))
        return false;
    if (!isfinite(state->width) || !isfinite(state->height))
        return ab_fail(error, "Nonfinite reached root clip dimensions");
    AbDataValue clip[] = {number(0), number(0), number(state->width),
                          number(state->height)};
    if (!call(root, AB_ROOT_CLIP, clip, 4, NULL, error))
        return false;
    state->touch_offset.x = -1 * state->clip_offset.x;
    state->touch_offset.y = -1 * state->clip_offset.y;
    return true;
}

static bool early_updates(AbClassicRoot *root, float seconds, AbDataValue second,
                          AbError *error) {
    AbClassicRootState *state = root->config.state;
    AbClassicRootFlags *flags = root->config.flags;
    bool safe;
    if (!observe(root, AB_ROOT_SAFE_QUIT_QUERY, error) ||
        !ab_classic_safe_to_quit(root->config.menus, &safe, error))
        return false;
    state->safe_to_quit = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = safe};
    if (truthy(flags->gamer_present) && truthy(flags->gamer_dev_mode) &&
        !timed(root, AB_ROOT_GAMER_UPDATE, seconds, false, error))
        return false;
    if (!observe(root, AB_ROOT_ANGER_TIMERS_UPDATE, error) ||
        !ab_classic_anger_update(root->config.anger, error) ||
        !prepare_view(root, error))
        return false;
    if (key(root->config.held_keys, "CONTROL") && key(root->config.pressed_keys, "F") &&
        named(flags->device_model, "osx") &&
        !notify(root, root->config.fullscreen_event, error))
        return false;
    if (!truthy(flags->release_build) || truthy(flags->cheats_enabled)) {
        AbDataValue arguments[] = {number(seconds), second};
        if (!call(root, AB_ROOT_DEBUG_UPDATE, arguments, 2, NULL, error))
            return false;
    }
    state->dt = number(seconds);
    if (!advance(&state->global_time, seconds, true, error))
        return false;
    if (state->delayed_url) {
        const AbDataValue *deadline = ab_data_get(state->delayed_url, text("time"));
        float current, target;
        if (!deadline || !numeric(*deadline, &target, error) ||
            !numeric(state->global_time, &current, error))
            return ab_fail(error, "Invalid deferred root URL deadline");
        if (target <= current) {
            AbDataTable *request = state->delayed_url;
            state->delayed_url = NULL;
            const AbDataValue *url = ab_data_get(request, text("url"));
            AbDataValue argument = url ? *url : (AbDataValue){0};
            if (!call(root, AB_ROOT_OPEN_URL, &argument, 1, NULL, error))
                return false;
        }
    }
    if (truthy(state->game_was_resumed)) {
        if (numeric_equal(state->restore_fullscreen, 2)) {
            AbDataValue enabled = {.kind = AB_DATA_BOOLEAN, .bits = 1};
            if (!call(root, AB_ROOT_FULLSCREEN, &enabled, 1, NULL, error))
                return false;
            state->restore_fullscreen = (AbDataValue){0};
        }
        state->game_was_resumed = (AbDataValue){0};
    }
    return observe(root, AB_ROOT_URL_TIMERS_UPDATE, error) &&
           ab_url_requests_update(root->config.url_requests, seconds, error);
}

static bool audio_and_eagle(AbClassicRoot *root, float seconds, AbDataValue *eagle_used,
                            AbError *error) {
    AbClassicRootFlags *flags = root->config.flags;
    if (!truthy(flags->show_editor)) {
        AbDataValue argument = text("S"), released = {0};
        if (!call(root, AB_ROOT_KEY_RELEASED, &argument, 1, &released, error))
            return false;
        if (truthy(released) && !truthy(flags->block_audio_change)) {
            argument = text("KEYBOARD_COMMAND");
            if (!call(root, AB_ROOT_CHANGE_AUDIO, &argument, 1, NULL, error))
                return false;
        }
    }
    bool free_enabled;
    if (!observe(root, AB_ROOT_AUDIO_RAMP_UPDATE, error) ||
        !ab_classic_audio_ramp_update(root->config.audio_ramp, seconds, error) ||
        !observe(root, AB_ROOT_FREE_EAGLE_QUERY, error) ||
        !ab_classic_free_eagle(root->config.eagle_query, &free_enabled, error))
        return false;
    AbDataValue free_eagle = {.kind = AB_DATA_BOOLEAN, .bits = free_enabled};
    *eagle_used = (AbDataValue){0};
    if (flags->settings_present && truthy(flags->settings_converted) &&
        !call(root, AB_ROOT_EAGLE_USED_TIME, &free_eagle, 1, eagle_used, error))
        return false;
    if (eagle_used->kind != AB_DATA_NIL && root->config.events)
        return ab_fail(error, "Reached root eagle cooldown needs its source owner");
    return true;
}

static bool functions(AbClassicRoot *root, AbClassicRootCalls *calls, float seconds,
                      bool subsystems, AbError *error) {
    if (!calls)
        return true;
    if (!valid_calls(calls))
        return ab_fail(error, "Invalid reached root function table");
    size_t count = calls->count;
    AbClassicRootCall **captured = calls->items;
    for (size_t index = 0; index < count; ++index) {
        if (subsystems && (!valid_calls(calls) || index >= calls->count))
            return ab_fail(error, "Missing reached root subsystem slot");
        AbClassicRootCall *binding = subsystems ? calls->items[index] : captured[index];
        if (!binding || !binding->call)
            return ab_fail(error, "Missing reached root function binding");
        AbDataValue arguments[] = {number(seconds), root->config.globals->time};
        AbDataValue ignored = {0};
        if (!binding->call(binding->context, arguments, subsystems ? 1u : 2u, &ignored,
                           error))
            return false;
    }
    return true;
}

static bool resolution_changed(const AbClassicRootState *state) {
    return !numeric_equal(state->old_width, state->width) ||
           !numeric_equal(state->old_height, state->height);
}

static bool observe(AbClassicRoot *root, AbClassicRootOwnedPhase phase,
                    AbError *error) {
    return !root->config.observe ||
           root->config.observe(root->config.observer_context, phase, error);
}

static bool managers(AbClassicRoot *root, float seconds, AbError *error) {
    AbClassicRootConfig *config = &root->config;
    AbClassicRootState *state = config->state;
    if (!functions(root, config->subsystems, seconds, true, error))
        return false;
    if (resolution_changed(state) && config->events &&
        !notify(root, config->resolution_event, error))
        return false;
    if (config->flags->menu_present) {
        if (!timed(root, AB_ROOT_MENU_UPDATE, seconds, true, error))
            return false;
        if (truthy(state->input_delay)) {
            if (!advance(&state->input_delay, -seconds, false, error))
                return false;
            float delay;
            if (!numeric(state->input_delay, &delay, error))
                return false;
            if (delay < 0) {
                AbDataValue enabled = {.kind = AB_DATA_BOOLEAN, .bits = 1};
                if (!call(root, AB_ROOT_ALLOW_INPUT, &enabled, 1, NULL, error))
                    return false;
                state->input_delay = (AbDataValue){0};
            }
        }
    }
    if (config->flags->ads_present &&
        !timed(root, AB_ROOT_AD_UPDATE, seconds, true, error))
        return false;
    if (!functions(root, config->updatable, seconds, false, error))
        return false;
    if (config->events && (!observe(root, AB_ROOT_EVENT_TICK, error) ||
                           !ab_event_manager_tick(config->events, seconds, error)))
        return false;
    return observe(root, AB_ROOT_PROCESS_UPDATE, error) &&
           ab_process_manager_update(config->processes, seconds, config->skip_input,
                                     error) &&
           observe(root, AB_ROOT_NATIVE_ANIMATION, error) &&
           ab_native_animation_update(config->native_animation, seconds, error);
}

static bool clocks_and_clicks(AbClassicRoot *root, float seconds,
                              AbDataValue eagle_used, AbError *error) {
    AbClassicRootState *state = root->config.state;
    if (!ab_classic_game_globals_advance_time(root->config.globals, seconds, error) ||
        !advance(&state->playtime, seconds, true, error))
        return false;
    if (state->last_video_ad_time.kind == AB_DATA_NIL)
        state->last_video_ad_time = root->config.globals->time;
    if (truthy(root->config.flags->has_iaps) && eagle_used.kind != AB_DATA_NIL)
        return ab_fail(error, "Reached root eagle information needs its source owner");
    state->double_click = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    if (state->double_click_timer.kind == AB_DATA_NIL)
        state->double_click_timer = number(0);
    if (key(root->config.pressed_keys, "LBUTTON")) {
        if (!numeric_equal(state->double_click_state, 2)) {
            state->double_click_state = number(1);
            state->double_click_timer = number(0.5f);
        } else
            state->double_click_state = number(3);
    } else {
        float timer;
        if (!numeric(state->double_click_timer, &timer, error))
            return false;
        if (timer > 0 && key(root->config.released_keys, "LBUTTON")) {
            if (numeric_equal(state->double_click_state, 1))
                state->double_click_state = number(2);
            else if (numeric_equal(state->double_click_state, 3))
                state->double_click = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
        }
    }
    if (!advance(&state->double_click_timer, -seconds, false, error))
        return false;
    float timer;
    if (!numeric(state->double_click_timer, &timer, error))
        return false;
    if (timer <= 0) {
        state->double_click_timer = number(0);
        state->double_click_state = number(0);
    }
    return true;
}

static bool draw_and_tail(AbClassicRoot *root, float seconds, AbError *error) {
    AbClassicRootConfig *config = &root->config;
    AbClassicRootState *state = config->state;
    if (!timed(root, AB_ROOT_CURRENT_MODE, seconds, true, error))
        return false;
    if (config->flags->menu_present &&
        !call(root, AB_ROOT_MENU_DRAW, NULL, 0, NULL, error))
        return false;
    if (key(config->pressed_keys, "9"))
        state->show_background = (AbDataValue){.kind = AB_DATA_BOOLEAN,
                                               .bits = !truthy(state->show_background)};
    if (truthy(config->flags->ios) && config->flags->achievement_present &&
        !timed(root, AB_ROOT_UNLOCKING, seconds, true, error))
        return false;
    if (resolution_changed(state)) {
        state->old_width = number(state->width);
        state->old_height = number(state->height);
        if (config->flags->notifications_present &&
            !call(root, AB_ROOT_NOTIFICATION_LAYOUT, NULL, 0, NULL, error))
            return false;
        if (config->flags->menu_present &&
            !call(root, AB_ROOT_MENU_LAYOUT, NULL, 0, NULL, error))
            return false;
        if (truthy(config->flags->is_pc))
            return ab_fail(error, "Reached root PC resolution needs its source owner");
    }
    if (truthy(config->flags->is_pc))
        return ab_fail(
            error, "Reached root fullscreen synchronization needs its source owner");
    if ((!truthy(config->flags->release_build) ||
         truthy(config->flags->cheats_enabled)) &&
        !call(root, AB_ROOT_DEBUG_DRAW, NULL, 0, NULL, error))
        return false;
    AbDataValue reset[] = {number(0), number(0), number(1),
                           number(1), number(0), number(0)};
    if (!call(root, AB_ROOT_RENDER_STATE, reset, 6, NULL, error))
        return false;
    if (truthy(config->flags->cursor_enabled))
        return ab_fail(error, "Reached root software cursor needs its source owner");
    return functions(root, config->tail_updates, seconds, false, error) &&
           (!config->flags->shop_present ||
            timed(root, AB_ROOT_SHOP_UPDATE, seconds, false, error));
}

bool ab_classic_root_update(AbClassicRoot *root, float seconds,
                            AbDataValue second_argument, AbError *error) {
    if (!root || root->failed || root->updating || !isfinite(seconds))
        return ab_fail(error, "Invalid or stopped root update");
    root->updating = true;
    AbDataValue eagle_used = {0};
    bool okay = early_updates(root, seconds, second_argument, error) &&
                audio_and_eagle(root, seconds, &eagle_used, error) &&
                managers(root, seconds, error) &&
                clocks_and_clicks(root, seconds, eagle_used, error) &&
                draw_and_tail(root, seconds, error);
    root->updating = false;
    root->failed = root->failed || !okay;
    return okay;
}
