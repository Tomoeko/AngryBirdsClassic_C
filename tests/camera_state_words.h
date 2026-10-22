#ifndef AB_TEST_CAMERA_STATE_WORDS_H
#define AB_TEST_CAMERA_STATE_WORDS_H

#include "ab/camera_system.h"
#include <string.h>

static inline uint32_t camera_state_word(float number) {
    uint32_t bits;
    memcpy(&bits, &number, sizeof(bits));
    return bits;
}

static inline bool camera_state_words(AbCameraSystemView view, uint32_t target,
                                      uint32_t *output) {
    if (!view.mover || !view.limits)
        return false;
    output[1] = view.automatic_present;
    output[2] = target;
    const AbCameraFollow *follow = view.follow;
    if (follow) {
        output[3] = 1;
        output[4] = follow->hit_present;
        if (follow->hit_present) {
            output[5] = camera_state_word(follow->hit_position.x);
            output[6] = camera_state_word(follow->hit_position.y);
        }
        output[7] = follow->hit_time_present;
        output[8] = follow->hit_time_present ? camera_state_word(follow->hit_time) : 0;
        output[9] = follow->original_zoom_present;
        output[10] = follow->original_zoom_present
                         ? camera_state_word(follow->original_zoom)
                         : 0;
        output[11] = follow->time_to_peak_present;
        output[12] =
            follow->time_to_peak_present ? camera_state_word(follow->time_to_peak) : 0;
        output[13] = follow->scroll_present;
        output[14] =
            follow->scroll_present ? camera_state_word(follow->scroll_timer) : 0;
        output[15] = follow->peak_flag_present;
        output[16] = follow->peak_is_hit_position;
        output[17] = 1;
        output[18] = follow->allow_cancel;
        output[19] = camera_state_word(follow->movement_duration_scaler);
        output[20] = camera_state_word(follow->velocity_compensation);
        output[21] = camera_state_word(follow->minimum_movement_duration);
        output[22] = camera_state_word(follow->timer);
    }
    const AbCameraMover *mover = view.mover;
    output[23] = (mover->target_position_present ? 1u : 0u) |
                 (mover->target_zoom_present ? 2u : 0u) |
                 (mover->start_position_present ? 4u : 0u) |
                 (mover->start_zoom_scale_present ? 8u : 0u) |
                 (mover->start_zoom_present ? 16u : 0u);
    output[24] = camera_state_word(mover->current_position.x);
    output[25] = camera_state_word(mover->current_position.y);
    if (mover->target_position_present) {
        output[26] = camera_state_word(mover->target_position.x);
        output[27] = camera_state_word(mover->target_position.y);
    }
    if (mover->start_position_present) {
        output[28] = camera_state_word(mover->start_position.x);
        output[29] = camera_state_word(mover->start_position.y);
    }
    output[30] = camera_state_word(mover->current_zoom);
    output[31] = mover->target_zoom_present ? camera_state_word(mover->target_zoom) : 0;
    output[32] = camera_state_word(mover->zoom_velocity);
    output[33] = mover->start_zoom_scale_present
                     ? camera_state_word(mover->start_zoom_scale)
                     : 0;
    output[34] = mover->start_zoom_present ? camera_state_word(mover->start_zoom) : 0;
    output[35] = camera_state_word(mover->movement_duration);
    output[36] = camera_state_word(mover->movement_timer);
    output[37] = camera_state_word(mover->zoom_duration);
    output[38] = camera_state_word(mover->zoom_timer);
    output[39] = view.follow_curves && view.follow_curves->custom_y
                     ? 5u
                     : (uint32_t)mover->movement_tween;
    output[40] = view.follow_curves && view.follow_curves->custom_zoom
                     ? 5u
                     : (uint32_t)mover->zoom_tween;
    output[41] = camera_state_word(view.screen_position.x);
    output[42] = camera_state_word(view.screen_position.y);
    if (view.viewport) {
        output[43] = camera_state_word(view.viewport->rendering.top_left.x);
        output[44] = camera_state_word(view.viewport->rendering.top_left.y);
        output[45] = camera_state_word(view.viewport->rendering.zoom.x);
    }
    output[51] = view.viewport != NULL;
    output[53] = camera_state_word(view.limits->close_zoom);
    output[54] = camera_state_word(view.limits->far_zoom);
    output[55] = camera_state_word(view.limits->bottom);
    output[56] = camera_state_word(view.limits->left);
    output[57] = camera_state_word(view.limits->right);
    output[58] = camera_state_word(view.limits->top);
    output[59] = view.limits->enabled;
    output[60] = camera_state_word(view.limits->horizontal_leeway);
    output[61] = camera_state_word(view.limits->absolute_bottom);
    return true;
}

static inline bool camera_sequence_words(const AbCameraSequenceManager *manager,
                                         const AbCameraSequence *loaded,
                                         bool store_count, uint32_t output[80]) {
    if (!manager ||
        (loaded && (loaded->count > 3 || (loaded->count && !loaded->stages))))
        return false;
    output[64] = manager->current != NULL;
    output[65] = camera_state_word((float)manager->stage_index);
    output[66] = camera_state_word(manager->timer);
    output[67] = manager->stage_timer_present;
    output[68] =
        manager->stage_timer_present ? camera_state_word(manager->stage_timer) : 0;
    output[69] = manager->done_callback;
    if (loaded) {
        if (store_count)
            output[62] = (uint32_t)loaded->count;
        for (size_t index = 0; index < loaded->count; ++index) {
            const AbCameraSequenceStage *stage = loaded->stages + index;
            size_t first = 70 + index * 3;
            output[first] = stage->duration_present;
            output[first + 1] =
                stage->duration_present ? camera_state_word(stage->duration) : 0;
            output[first + 2] = (uint32_t)stage->condition;
        }
    }
    return true;
}

#endif
