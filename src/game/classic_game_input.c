#include "ab/classic_game_input.h"
#include "ab/bird_collision_prelude.h"
#include "ab/classic_menu_input.h"

#include <math.h>
#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue field(const AbDataTable *table, AbDataValue key) {
    const AbDataValue *value = ab_data_get(table, key);
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool literal_true(AbDataValue value) {
    return value.kind == AB_DATA_BOOLEAN && value.bits;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Unsupported concrete game input number");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite concrete game input number");
}

static bool equal(AbDataValue left, AbDataValue right) {
    if (left.kind != right.kind)
        return false;
    if (left.kind == AB_DATA_NIL)
        return true;
    if (left.kind == AB_DATA_NUMBER) {
        float a, b;
        memcpy(&a, &left.bits, sizeof(a));
        memcpy(&b, &right.bits, sizeof(b));
        return a == b;
    }
    if (left.kind == AB_DATA_BOOLEAN)
        return !!left.bits == !!right.bits;
    if (left.kind == AB_DATA_TABLE)
        return left.table == right.table;
    return left.string.size == right.string.size &&
           (!left.string.size ||
            (left.string.data && right.string.data &&
             !memcmp(left.string.data, right.string.data, left.string.size)));
}

static bool hat(AbClassicGameInput *owner, bool rising_only, AbError *error) {
    AbClassicGameInputConfig *config = &owner->config;
    AbDataValue flying = *config->flying;
    if (flying.kind == AB_DATA_NIL)
        return true;
    if (flying.kind != AB_DATA_TABLE || !flying.table)
        return ab_fail(error, "Unresolved game input bird/hat map");
    AbDataValue name = field(flying.table, text("name"));
    AbDataValue entry = {0};
    const AbBirdCollisionHat *actual_hat = NULL;
    if (config->hats) {
        uintptr_t handle = 0;
        bool present = false;
        if (name.kind != AB_DATA_STRING ||
            !ab_string_map_get(config->hats, name.string, &handle, &present, error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Invalid canonical game-input hat key");
        if (!present)
            return true;
        actual_hat = (const AbBirdCollisionHat *)handle;
        if (!actual_hat ||
            (actual_hat->object_name.size && !actual_hat->object_name.data))
            return ab_fail(error, "Invalid canonical game-input hat record");
    } else {
        if (config->state->hats.kind != AB_DATA_TABLE || !config->state->hats.table)
            return ab_fail(error, "Unresolved game input bird/hat map");
        entry = field(config->state->hats.table, name);
        if (!truthy(entry))
            return true;
    }
    if (rising_only) {
        float velocity = 0;
        if (!numeric(field(flying.table, text("yVel")), &velocity, error))
            return false;
        if (!(velocity > 0))
            return true;
    }
    AbDataValue hat_name;
    if (actual_hat)
        hat_name =
            (AbDataValue){.kind = AB_DATA_STRING, .string = actual_hat->object_name};
    else {
        if (entry.kind != AB_DATA_TABLE || !entry.table)
            return ab_fail(error, "Invalid game input hat record");
        hat_name = field(entry.table, text("hat"));
    }
    AbDataValue object = field(&owner->level.properties->world, hat_name);
    if (!truthy(object))
        return true;
    return config->release_hat ? config->release_hat(config->context, name, error)
                               : ab_fail(error, "Missing actual releaseHat service");
}

static bool update(AbClassicGameInput *owner, AbClassicGameTouchInput touches,
                   float seconds, AbDataValue time, AbError *error) {
    AbClassicGameInputConfig *config = &owner->config;
    AbClassicGameInputState *state = config->state;
    AbClassicGameGlobals *globals = ab_classic_shots_globals(config->shots);
    bool triggered = false;
    if (!hat(owner, true, error))
        return false;
    if (!*config->level_completed && truthy(field(config->pressed, text("LBUTTON"))) &&
        !truthy(state->skip_input) && !truthy(state->skip_trigger_specialty) &&
        config->flying->kind != AB_DATA_NIL && truthy(globals->specialty_available) &&
        !equal(time, state->eagle_click_time)) {
        if (!hat(owner, false, error) || !ab_classic_bird_specialty_trigger_flying(
                                             config->specialty, seconds, error))
            return false;
        triggered = true;
        state->touch_released_after_specialty = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
    bool gestures = truthy(state->using_gestures);
    if ((!gestures && truthy(field(config->released, text("LBUTTON")))) ||
        (gestures && state->touch_count.kind == AB_DATA_NUMBER &&
         !(state->touch_count.bits & UINT32_C(0x7fffffff))))
        state->touch_released_after_specialty =
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    if (truthy(state->disable_drag))
        state->skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    if (truthy(state->skip_input)) {
        if (config->menu_input) {
            if (config->menu_input->failed)
                return ab_fail(error, "Failed actual menu input at touch cancel");
            AbClassicMenuInputView view =
                ab_classic_menu_input_view(config->menu_input);
            touches.raw = view.touches;
            touches.raw_count = view.count;
        }
        if (!ab_classic_touches_cancel(config->touches, touches.raw, touches.raw_count,
                                       error))
            return false;
    } else {
        if (config->menu_input &&
            !ab_classic_menu_input_copy_touches(config->menu_input, &touches.copied,
                                                &touches.copied_count, error))
            return false;
        if (!ab_classic_touches_handle(config->touches, touches.copied,
                                       touches.copied_count, seconds,
                                       &state->skip_input, error))
            return false;
    }
    if (gestures || truthy(state->delta_sling) || truthy(state->accelerometer))
        return ab_fail(error, "Active gesture/delta/motion game input is not bound");
    AbCameraSystemView camera = ab_camera_system_view(config->camera);
    AbSlingMouseInput mouse = {
        .cursor = state->cursor,
        .cursor_physics = state->cursor_physics,
        .seconds = seconds,
        .screen_width = *config->screen_width,
        .zoom = camera.mover->current_zoom,
        .pressed = !triggered && truthy(field(config->pressed, text("LBUTTON"))),
        .held = truthy(field(config->held, text("LBUTTON"))),
        .released = truthy(field(config->released, text("LBUTTON"))),
        .skip_input = literal_true(state->skip_input),
        .level_completed = *config->level_completed};
    if (mouse.pressed && !mouse.level_completed && !mouse.skip_input &&
        !numeric(state->touch_count, &mouse.touch_count, error))
        return false;
    AbClassicSlingView sling = ab_classic_sling_view(config->sling);
    if (mouse.held && !mouse.level_completed && !mouse.skip_input &&
        (mouse.pressed || (sling.mouse.in_game_pressed && sling.mouse.drag_started)) &&
        ab_classic_shots_state(config->shots)->bird_ready &&
        !ab_classic_sling_is_visible(config->sling, &mouse.sling_visible, error))
        return false;
    if (!ab_classic_sling_mouse(config->sling, mouse, error))
        return false;
    if (literal_true(state->cheats_enabled) &&
        (!config->complete_cheat || !config->complete_cheat(config->context, error)))
        return config->complete_cheat
                   ? false
                   : ab_fail(error, "Missing actual levelCompleteCheat service");
    config->tail->cursor = state->cursor;
    return true;
}

bool ab_classic_game_input_initialize(AbClassicGameInputConfig config,
                                      AbClassicGameInput *output, AbError *error) {
    AbClassicLevelView level =
        ab_classic_level_view(ab_classic_shots_level(config.shots));
    if (!output || !level.properties || !ab_classic_shots_globals(config.shots) ||
        !config.sling || !config.specialty || !config.touches || !config.camera ||
        !config.tail || !config.state || !config.flying || !config.level_completed ||
        !config.pressed || !config.held || !config.released || !config.screen_width)
        return ab_fail(error, "Invalid concrete game input bindings");
    if (config.menu_input &&
        (!config.menu_input->initialized || config.menu_input->failed ||
         config.menu_input->config.raw->pressed != config.pressed ||
         config.menu_input->config.raw->held != config.held ||
         config.menu_input->config.raw->released != config.released))
        return ab_fail(error, "Mismatched actual menu/game raw key bindings");
    *output =
        (AbClassicGameInput){.config = config, .level = level, .initialized = true};
    return true;
}

bool ab_classic_game_input_update(AbClassicGameInput *owner,
                                  AbClassicGameTouchInput touches, float seconds,
                                  AbDataValue time, AbError *error) {
    if (!owner || !owner->initialized || owner->updating || owner->failed ||
        owner->level.world->locked || owner->level.world->failed)
        return ab_fail(error, "Invalid, failed or reentrant concrete game input");
    owner->updating = true;
    bool okay = update(owner, touches, seconds, time, error);
    owner->updating = false;
    owner->failed = !okay;
    return okay;
}

bool ab_classic_game_input_finish_cursor(AbClassicGameInput *owner, AbError *error) {
    if (!owner || !owner->initialized || owner->failed)
        return ab_fail(error, "Invalid concrete game cursor housekeeping");
    return ab_classic_sling_finish_input(owner->config.sling,
                                         owner->config.tail->old_cursor, error);
}

void ab_classic_game_input_finish_draw(AbClassicGameInput *owner) {
    if (owner && owner->initialized) {
        owner->config.state->skip_input = (AbDataValue){.kind = AB_DATA_BOOLEAN};
        owner->config.state->skip_trigger_specialty =
            (AbDataValue){.kind = AB_DATA_BOOLEAN};
    }
}
