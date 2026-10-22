#include "ab/camera_touch.h"
#include "ab/camera_selection.h"

#include <math.h>
#include <string.h>

static bool finite_point(AbVec2 point) {
    return isfinite(point.x) && isfinite(point.y);
}

static bool valid_input(const AbCameraTouchInput *input) {
    if (!input || (input->count && !input->touches) ||
        input->count >= UINT32_C(0x1000000)) {
        return false;
    }
    for (size_t index = 0; index < input->count; ++index) {
        const AbCameraTouch *touch = input->touches + index;
        if (touch->id < -16777216 || touch->id > 16777216 ||
            !finite_point(touch->position)) {
            return false;
        }
        for (size_t earlier = 0; earlier < index; ++earlier) {
            if (input->touches[earlier].id == touch->id) {
                return false;
            }
        }
    }
    return true;
}

static const AbCameraTouch *find_touch(const AbCameraTouchInput *input, int32_t id) {
    for (size_t index = 0; index < input->count; ++index) {
        if (input->touches[index].id == id) {
            return input->touches + index;
        }
    }
    return NULL;
}

static float distance(AbVec2 first, AbVec2 second) {
    float x = first.x - second.x;
    float y = first.y - second.y;
    float xx = x * x;
    float yy = y * y;
    return sqrtf(xx + yy);
}

static AbVec2 midpoint(AbVec2 first, AbVec2 second) {
    return (AbVec2){(first.x + second.x) / 2.0f, (first.y + second.y) / 2.0f};
}

static void idle(AbCameraTouchHandler *handler, AbCameraTouchShared *shared,
                 bool tapped, AbVec2 position) {
    *handler =
        (AbCameraTouchHandler){.initialized = true,
                               .state = AB_CAMERA_TOUCH_IDLE,
                               .latest_tap_position = tapped ? position : (AbVec2){0},
                               .latest_tap_position_present = tapped};
    shared->tap_count = tapped ? shared->tap_count + 1.0f : 0;
}

bool ab_camera_touch_init(AbCameraTouchHandler *handler, AbCameraTouchShared *shared) {
    if (!handler || !shared) {
        return false;
    }
    idle(handler, shared, false, (AbVec2){0});
    return true;
}

static bool enter_drag(AbCameraTouchHandler *handler, AbCameraSystem *camera,
                       const AbCameraTouch *touch, bool detect_tap) {
    *handler = (AbCameraTouchHandler){.state = AB_CAMERA_TOUCH_DRAG,
                                      .first_id = touch->id,
                                      .detect_tap = detect_tap,
                                      .initialized = true};
    return ab_camera_drag_start(&handler->drag, touch->position,
                                ab_camera_system_view(camera).mover->current_position);
}

static bool enter_zoom(AbCameraTouchHandler *handler, AbCameraSystem *camera,
                       const AbCameraTouch *first, const AbCameraTouch *second) {
    AbCameraSystemView view = ab_camera_system_view(camera);
    float separation = distance(first->position, second->position);
    if (!isfinite(separation)) {
        return false;
    }
    *handler = (AbCameraTouchHandler){.state = AB_CAMERA_TOUCH_ZOOM,
                                      .first_id = first->id,
                                      .second_id = second->id,
                                      .start_zoom = view.mover->current_zoom,
                                      .start_distance = separation,
                                      .previous_distance = separation,
                                      .initialized = true};
    return ab_camera_drag_start(&handler->drag,
                                midpoint(first->position, second->position),
                                view.mover->current_position);
}

static bool update_drag(AbCameraTouchHandler *handler, AbCameraSystem *camera,
                        float seconds, AbVec2 position, AbCameraTouchOutput *output) {
    AbCameraViewport viewport;
    AbVec2 target;
    if (!ab_camera_system_query_viewport(camera, &viewport) ||
        !ab_camera_drag_update(&handler->drag, seconds, position, &viewport.rendering,
                               &target)) {
        return false;
    }
    output->targets.values = (AbCameraValues){target, 0, 3};
    output->targets_present = true;
    return true;
}

static bool release_drag(AbCameraTouchHandler *handler, AbCameraSystem *camera,
                         AbCameraTouchOutput *output, bool *fling, AbVec2 *position) {
    AbCameraSystemView view = ab_camera_system_view(camera);
    AbCameraDragRelease released;
    if (!ab_camera_drag_release(&handler->drag, view.pixel_size, &released)) {
        return false;
    }
    float x_squared = released.normalized_velocity.x * released.normalized_velocity.x;
    float y_squared = released.normalized_velocity.y * released.normalized_velocity.y;
    *fling = sqrtf(x_squared + y_squared) > 0.5f;
    *position = released.screen_position;
    AbCameraValues wanted = {view.mover->current_position, view.mover->current_zoom, 7};
    if (*fling) {
        size_t selected;
        if (!ab_camera_snap_direction(view.limits->snaps, view.limits->snap_count,
                                      view.mover->current_position,
                                      released.normalized_velocity.x, &selected)) {
            return false;
        }
        wanted = (AbCameraValues){view.limits->snaps[selected].position, 0, 3};
    }
    AbCameraLimitContext context = {.pixel_size = view.pixel_size,
                                    .current_position = view.mover->current_position,
                                    .current_zoom = view.mover->current_zoom};
    if (!ab_camera_limits_position(view.limits, &context, &wanted, false, &wanted)) {
        return false;
    }
    output->targets = (AbCameraTargets){.values = {wanted.position, 0, 3},
                                        .movement_duration = 0.5f,
                                        .movement_tween = AB_CAMERA_TWEEN_CUBIC_OUT,
                                        .movement_tween_present = true};
    output->targets_present = true;
    return true;
}

static bool snap_zoom(AbCameraSystem *camera, float velocity, bool suppress,
                      AbCameraTouchOutput *output) {
    AbCameraSystemView view = ab_camera_system_view(camera);
    float zoom;
    if (!ab_camera_snap_zoom(view.mover->current_zoom, view.limits->close_zoom,
                             view.limits->far_zoom, suppress, velocity, &zoom)) {
        return false;
    }
    float duration = fabsf(zoom - view.mover->current_zoom) * 2.0f;
    output->targets.values.zoom = zoom;
    output->targets.values.present |= AB_CAMERA_VALUE_ZOOM;
    output->targets.movement_duration = duration;
    output->targets.zoom_duration = duration;
    output->targets.zoom_tween = AB_CAMERA_TWEEN_CUBIC_OUT;
    output->targets.zoom_tween_present = true;
    return isfinite(duration);
}

static bool update_idle(AbCameraTouchHandler *handler, AbCameraTouchShared *shared,
                        AbCameraSystem *camera, float seconds,
                        const AbCameraTouchInput *input, AbError *error) {
    handler->timer = handler->timer + seconds;
    if (!isfinite(handler->timer)) {
        return false;
    }
    if (handler->timer > 0.25f) {
        if (shared->tap_count == 1) {
            AbCameraMove move = {.duration = 0.5f,
                                 .duration_present = true,
                                 .movement_tween = AB_CAMERA_TWEEN_COSINE_IN_OUT,
                                 .movement_tween_present = true};
            if (!ab_camera_system_goto_snap(camera, 0, false, true, &move, error)) {
                return false;
            }
        }
        shared->tap_count = 0;
    }
    if (input->count == 1) {
        return enter_drag(handler, camera, input->touches, true);
    }
    if (input->count == 2) {
        return enter_zoom(handler, camera, input->touches, input->touches + 1);
    }
    return true;
}

static bool update_single(AbCameraTouchHandler *handler, AbCameraTouchShared *shared,
                          AbCameraSystem *camera, float seconds,
                          const AbCameraTouchInput *input, AbCameraTouchOutput *output,
                          AbError *error) {
    handler->timer = handler->timer + seconds;
    if (!isfinite(handler->timer)) {
        return false;
    }
    if (!input->count) {
        bool fling;
        AbVec2 position;
        if (!release_drag(handler, camera, output, &fling, &position)) {
            return false;
        }
        bool tapped = !fling && handler->timer < 0.25f && handler->detect_tap;
        idle(handler, shared, tapped, position);
        return !tapped || shared->tap_count != 2 ||
               ab_camera_system_toggle_zoom(camera, &position, true, error);
    }
    const AbCameraTouch *tracked = find_touch(input, handler->first_id);
    if (tracked) {
        if (!update_drag(handler, camera, seconds, tracked->position, output)) {
            return false;
        }
        output->targets.values.zoom = ab_camera_system_view(camera).mover->current_zoom;
        output->targets.values.present |= AB_CAMERA_VALUE_ZOOM;
    }
    if (input->count > 1) {
        const AbCameraTouch *first = tracked ? tracked : input->touches;
        const AbCameraTouch *second = input->touches;
        if (second == first) {
            ++second;
        }
        return enter_zoom(handler, camera, first, second);
    }
    return true;
}

static float pinch_zoom(float start_zoom, float separation, float start_distance) {
    float ratio = separation / start_distance;
    float zoom = start_zoom * ratio;
    if (isnan(zoom)) {
        uint32_t word = UINT32_C(0x7fc00000);
        memcpy(&zoom, &word, sizeof(zoom));
    }
    return zoom;
}

static bool update_pinch(AbCameraTouchHandler *handler, AbCameraTouchShared *shared,
                         AbCameraSystem *camera, float seconds,
                         const AbCameraTouchInput *input, AbCameraTouchOutput *output) {
    if (!input->count) {
        bool fling;
        AbVec2 position;
        if (!release_drag(handler, camera, output, &fling, &position) ||
            (!input->ingame && !snap_zoom(camera, handler->zoom_velocity,
                                          input->suppress_zoom_snap, output))) {
            return false;
        }
        idle(handler, shared, false, (AbVec2){0});
        return true;
    }
    const AbCameraTouch *first = find_touch(input, handler->first_id);
    const AbCameraTouch *second = find_touch(input, handler->second_id);
    if (!first || !second) {
        output->targets_present = true;
        if (!input->ingame && !snap_zoom(camera, handler->zoom_velocity,
                                         input->suppress_zoom_snap, output)) {
            return false;
        }
        const AbCameraTouch *remaining = first    ? first
                                         : second ? second
                                                  : input->touches;
        return enter_drag(handler, camera, remaining, false);
    }
    float separation = distance(first->position, second->position);
    float difference = separation - handler->previous_distance;
    if (!isfinite(separation) || !isfinite(difference)) {
        return false;
    }
    if ((difference > 0 && handler->previous_diff < 0) ||
        (difference < 0 && handler->previous_diff > 0)) {
        handler->start_zoom = ab_camera_system_view(camera).mover->current_zoom;
        handler->start_distance = handler->previous_distance;
    }
    handler->previous_diff = difference;
    handler->previous_distance = separation;
    handler->zoom_velocity = difference;
    if (!update_drag(handler, camera, seconds,
                     midpoint(first->position, second->position), output)) {
        return false;
    }
    output->targets.values.zoom =
        pinch_zoom(handler->start_zoom, separation, handler->start_distance);
    output->targets.values.present |= AB_CAMERA_VALUE_ZOOM;
    output->targets.zoom_tween = AB_CAMERA_TWEEN_END;
    output->targets.zoom_tween_present = true;
    return true;
}

bool ab_camera_touch_update(AbCameraTouchHandler *handler, AbCameraTouchShared *shared,
                            AbCameraSystem *camera, float seconds,
                            const AbCameraTouchInput *input,
                            AbCameraTouchOutput *output, AbError *error) {
    AbCameraSystemView view = ab_camera_system_view(camera);
    if (!handler || !handler->initialized || !shared || !view.mover || !output ||
        !isfinite(shared->tap_count) || !isfinite(seconds) || !valid_input(input)) {
        return ab_fail(error, "Invalid camera touch state or input");
    }
    AbCameraTouchOutput result = {0};
    if (!view.input_enabled || input->bird_dragged ||
        (!input->specialty_available && !input->released_after_specialty)) {
        idle(handler, shared, false, (AbVec2){0});
        *output = result;
        return true;
    }
    bool okay;
    switch (handler->state) {
        case AB_CAMERA_TOUCH_IDLE:
            okay = update_idle(handler, shared, camera, seconds, input, error);
            break;
        case AB_CAMERA_TOUCH_DRAG:
            okay =
                update_single(handler, shared, camera, seconds, input, &result, error);
            break;
        case AB_CAMERA_TOUCH_ZOOM:
            okay = update_pinch(handler, shared, camera, seconds, input, &result);
            break;
        default:
            okay = false;
            break;
    }
    if (!okay) {
        return ab_fail(error, "Cannot update camera touch state");
    }
    /* The original FSM's vararg pack/remove/unpack loses the interaction
     * result when the target slot is nil, including idle entry into a drag. */
    result.user_input = result.targets_present;
    *output = result;
    return true;
}
