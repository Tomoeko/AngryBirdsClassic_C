#include "ab/game_update_tail.h"

#include <math.h>

static bool call_host(AbGameTailHost host, void *context, AbGameTailOperation operation,
                      AbGameTailObject *object, float seconds, float time, bool *reply,
                      AbError *error) {
    AbGameTailCall call = {
        .operation = operation, .object = object, .seconds = seconds, .time = time};
    if (!host(context, &call, error)) {
        return false;
    }
    if (reply) {
        *reply = call.result;
    }
    return true;
}

static bool valid_joint_queue(const AbGameJointQueue *queue) {
    return queue && queue->count <= queue->capacity &&
           queue->capacity <= SIZE_MAX / sizeof(*queue->names) &&
           (!queue->capacity || queue->names);
}

bool ab_game_joint_defer(void *context, AbSpan name, AbError *error) {
    AbGameJointQueue *queue = context;
    if (!valid_joint_queue(queue) || (name.size && !name.data) ||
        queue->count == queue->capacity) {
        return ab_fail(error, "Invalid/full deferred joint destruction queue");
    }
    queue->names[queue->count++] = name;
    return true;
}

static bool update_joints(AbGameTailJoints *joints, AbGameTailHost host, void *context,
                          AbError *error) {
    if (!joints) {
        return true;
    }
    if ((joints->count && !joints->entries) || joints->count > UINT32_C(16777216) ||
        !valid_joint_queue(joints->destroy)) {
        return ab_fail(error, "Invalid game joint update bindings");
    }
    for (size_t index = 0; index < joints->count; ++index) {
        const AbGameJointEntry *entry = joints->entries + index;
        if (!entry->back_and_forth) {
            continue;
        }
        if (entry->name.size && !entry->name.data) {
            return ab_fail(error, "Invalid game joint limit-check name");
        }
        AbGameTailCall call = {.operation = AB_GAME_TAIL_CHECK_JOINT_LIMIT,
                               .joint_name = entry->name};
        if (!host(context, &call, error)) {
            return false;
        }
    }
    size_t index = 0;
    while (index < joints->destroy->count) {
        if (!valid_joint_queue(joints->destroy)) {
            return ab_fail(error, "Invalid deferred joint queue during destruction");
        }
        AbSpan name = joints->destroy->names[index++];
        if (name.size && !name.data) {
            return ab_fail(error, "Invalid deferred joint destruction name");
        }
        AbGameTailCall call = {.operation = AB_GAME_TAIL_DESTROY_JOINT,
                               .joint_name = name};
        if (!host(context, &call, error)) {
            return false;
        }
    }
    joints->destroy->count = 0;
    return true;
}

static bool update_objects(AbGameTailWorld world, float seconds, float time,
                           AbGameTailHost host, void *context, AbError *error) {
    if (!world.begin(world.context, error))
        return false;
    for (;;) {
        AbGameTailObject *object = NULL;
        if (!world.next(world.context, &object, error))
            return false;
        if (!object)
            break;
        bool alive = false;
        if (!call_host(host, context, AB_GAME_TAIL_ANIMATION, object, seconds, time,
                       NULL, error) ||
            !call_host(host, context, AB_GAME_TAIL_ROCKET, object, seconds, time, NULL,
                       error) ||
            !call_host(host, context, AB_GAME_TAIL_BIRD, object, seconds, time, &alive,
                       error)) {
            return false;
        }
        if (alive && !call_host(host, context, AB_GAME_TAIL_BOUNDARIES, object, seconds,
                                time, &alive, error)) {
            return false;
        }
        if (alive && !call_host(host, context, AB_GAME_TAIL_OBJECT_EFFECT, object,
                                seconds, time, NULL, error)) {
            return false;
        }
    }
    return true;
}

static bool update_hooks(AbGameTailHooks *hooks, float seconds, float time,
                         AbGameTailHost host, void *context, AbError *error) {
    if (!hooks) {
        return true;
    }
    if ((hooks->count && !hooks->items) || hooks->count > UINT32_C(16777216)) {
        return ab_fail(error, "Invalid game object update hooks");
    }
    size_t count = hooks->count;
    for (size_t index = 0; index < count; ++index) {
        AbGameTailHook *hook = hooks->items + index;
        if (!hook->object || !hook->updates) {
            return ab_fail(error, "Invalid game object update hook binding");
        }
        AbObjectUpdateValue value = hook->updates->registered;
        if (value.kind == AB_OBJECT_UPDATE_NONE) {
            continue;
        }
        bool present = false;
        if (!call_host(host, context, AB_GAME_TAIL_HOOK_EXISTS, hook->object, seconds,
                       time, &present, error)) {
            return false;
        }
        if (!present) {
            hook->updates->registered = (AbObjectUpdateValue){0};
        } else if (!ab_object_update_call(value, hook->receiver, seconds, error)) {
            return false;
        }
    }
    return true;
}

bool ab_game_update_tail_world(AbGameTailState *state, AbGameTailWorld world,
                               float seconds, float time, AbGameTailHost host,
                               void *context, AbError *error) {
    if (!state || !world.begin || !world.next || !host || !isfinite(seconds) ||
        !isfinite(time)) {
        return ab_fail(error, "Invalid ordinary game update tail");
    }
    if (!call_host(host, context, AB_GAME_TAIL_SLING, NULL, seconds, time, NULL,
                   error)) {
        return false;
    }
    if (state->bird_fired && !call_host(host, context, AB_GAME_TAIL_WOBBLE, NULL,
                                        seconds, time, NULL, error)) {
        return false;
    }
    if (state->darkness_present) {
        float darkness = state->eagle_darkness - seconds * 0.4f;
        if (!isfinite(darkness)) {
            return ab_fail(error, "Game eagle darkness exceeds finite range");
        }
        darkness = darkness < 0.5f ? darkness : 0.5f;
        state->eagle_darkness = darkness < 0 ? 0 : darkness;
    }
    if (!update_hooks(state->hooks, seconds, time, host, context, error) ||
        !update_objects(world, seconds, time, host, context, error)) {
        return false;
    }
    if (state->mighty_timer_present &&
        !call_host(host, context, AB_GAME_TAIL_MIGHTY_TIMER, NULL, seconds, time, NULL,
                   error)) {
        return false;
    }
    const AbGameTailOperation before_joints[] = {AB_GAME_TAIL_CAMERA,
                                                 AB_GAME_TAIL_BOOMERANG};
    for (size_t index = 0; index < sizeof(before_joints) / sizeof(*before_joints);
         ++index) {
        if (!call_host(host, context, before_joints[index], NULL, seconds, time, NULL,
                       error)) {
            return false;
        }
    }
    if (!update_joints(state->joints, host, context, error)) {
        return false;
    }
    const AbGameTailOperation after_joints[] = {AB_GAME_TAIL_CHARACTER_ANIMATIONS,
                                                AB_GAME_TAIL_FLOATING_SCORES,
                                                AB_GAME_TAIL_SCORE};
    for (size_t index = 0; index < sizeof(after_joints) / sizeof(*after_joints);
         ++index) {
        if (!call_host(host, context, after_joints[index], NULL, seconds, time, NULL,
                       error)) {
            return false;
        }
    }
    if (state->red_special_available &&
        !call_host(host, context, AB_GAME_TAIL_SPECIAL_FEATHERS, NULL, seconds, time,
                   NULL, error)) {
        return false;
    }
    bool physics = false;
    if (!call_host(host, context, AB_GAME_TAIL_PHYSICS_ENABLED, NULL, seconds, time,
                   &physics, error) ||
        (physics && !call_host(host, context, AB_GAME_TAIL_LEVEL_EFFECTS, NULL, seconds,
                               time, NULL, error)) ||
        !call_host(host, context, AB_GAME_TAIL_BLOCK_SCRIPTS, NULL, seconds, time, NULL,
                   error)) {
        return false;
    }
    if (!world.begin(world.context, error))
        return false;
    for (;;) {
        AbGameTailObject *object = NULL;
        if (!world.next(world.context, &object, error))
            return false;
        if (!object)
            break;
        if (object->script_present &&
            !call_host(host, context, AB_GAME_TAIL_OBJECT_SCRIPT, object, seconds, time,
                       NULL, error)) {
            return false;
        }
    }
    if (!isfinite(state->cursor.x) || !isfinite(state->cursor.y)) {
        return ab_fail(error, "Invalid game cursor housekeeping");
    }
    state->old_cursor = state->cursor;
    if (!call_host(host, context, AB_GAME_TAIL_GFX_EFFECTS, NULL, seconds, time, NULL,
                   error) ||
        !call_host(host, context, AB_GAME_TAIL_DRAW, NULL, seconds, time, NULL,
                   error)) {
        return false;
    }
    state->skip_input = state->skip_trigger_specialty = false;
    if (state->time_since_present) {
        float elapsed = state->time_since_start + seconds;
        if (!isfinite(elapsed)) {
            return ab_fail(error, "Game elapsed time exceeds finite range");
        }
        state->time_since_start = elapsed;
    }
    return true;
}

typedef struct {
    AbGameTailObject *objects;
    size_t count, cursor;
} FixedWorld;

static bool fixed_begin(void *context, AbError *error) {
    (void)error;
    ((FixedWorld *)context)->cursor = 0;
    return true;
}

static bool fixed_next(void *context, AbGameTailObject **object, AbError *error) {
    (void)error;
    FixedWorld *world = context;
    *object = world->cursor < world->count ? world->objects + world->cursor++ : NULL;
    return true;
}

bool ab_game_update_tail(AbGameTailState *state, AbGameTailObject *objects,
                         size_t count, float seconds, float time, AbGameTailHost host,
                         void *context, AbError *error) {
    if ((count && !objects) || count > UINT32_C(16777216))
        return ab_fail(error, "Invalid fixed game world traversal");
    FixedWorld world = {objects, count, 0};
    return ab_game_update_tail_world(state,
                                     (AbGameTailWorld){&world, fixed_begin, fixed_next},
                                     seconds, time, host, context, error);
}
