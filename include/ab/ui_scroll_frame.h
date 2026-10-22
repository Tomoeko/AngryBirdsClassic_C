#ifndef AB_UI_SCROLL_FRAME_H
#define AB_UI_SCROLL_FRAME_H

#include "ab/event_manager.h"
#include "ab/ui_widget.h"

typedef struct AbUiScrollFrame AbUiScrollFrame;

typedef enum {
    AB_UI_SCROLL_X,
    AB_UI_SCROLL_Y,
    AB_UI_SCROLL_XY,
} AbUiScrollAxis;

typedef AbScreenClipRect AbUiScrollClip;

typedef struct {
    AbUiFrameFields frame;
    AbUiScrollAxis axis;
    bool blocks_input, ignore_culling;
    AbUiPointerValue block_distance;
} AbUiScrollArguments;

typedef struct {
    AbEventManager *events;
    AbDataValue pause_event;
    /* Actual current gamelua.g_dt/cursor.wheel, supplied by application input. */
    const float *input_seconds, *wheel;
    void *context;
    /* NULL binds the actual shared widget render context. */
    bool (*set_clip)(void *context, AbUiScrollClip clip, AbError *error);
} AbUiScrollHost;

struct AbUiScrollFrame {
    AbUiWidget widget;
    AbUiScrollHost host;
    AbEventListener listener;
    AbVec2 *anchors;
    size_t anchor_count, anchor_capacity, current_anchor, target_anchor;
    AbUiScrollAxis axis;
    AbAabb bounds;
    AbVec2 drag_start, drag_last, drag_last_update;
    float speed_x[5], speed_y[5], drag_timer, velocity, block_distance;
    size_t drag_index;
    AbUiScrollClip clip;
    bool dragging, drag_has_moved, blocks_input, ignore_culling;
    bool clip_present, draw_in_three_phases, subscribed;
    void *derived_context;
    bool (*anchor_changed)(void *context, AbUiScrollFrame *scroll, size_t anchor,
                           AbError *error);
};

/* Concrete ScrollFrame2.lua class over actual widgets and EventManager. The
 * widget.scroll pair is canonical; Frame geometry and input see the same state.
 * Copy anchors into stable caller storage. Entry resets motion after the real
 * base entry/subscription; reset retains dragHasMoved, as the source does.
 * Source pointer handling visits children twice on some paths; preserve both
 * calls even though only the second reply is returned. LHOLD samples actual
 * application g_dt through host.input_seconds, independently of update's dt.
 *
 * Named draw methods dispatch into actual derived child classes. Screen clips
 * bind the shared concrete render context by default; an explicit host may
 * supply its actual graphics context. Whole episode/level graphs are separate.
 * No allocation/VM. Numeric fields used by the source must be finite. Failed
 * calls retain preceding effects and stop the shared widget context. */
bool ab_ui_scroll_frame_init(AbUiScrollFrame *scroll, AbUiWidgetContext *context,
                             AbUiWidgetChildren children, AbVec2 *anchors,
                             size_t capacity, const AbUiScrollArguments *arguments,
                             AbUiScrollHost host, AbError *error);
bool ab_ui_scroll_frame_detach(AbUiScrollFrame *scroll, AbError *error);
bool ab_ui_scroll_frame_reset(AbUiScrollFrame *scroll, AbError *error);
bool ab_ui_scroll_frame_set_anchors(AbUiScrollFrame *scroll, const AbVec2 *anchors,
                                    size_t count, AbError *error);
bool ab_ui_scroll_frame_set_current(AbUiScrollFrame *scroll, size_t anchor,
                                    AbError *error);
bool ab_ui_scroll_frame_set_target(AbUiScrollFrame *scroll, size_t anchor,
                                   AbError *error);
bool ab_ui_scroll_frame_next(AbUiScrollFrame *scroll, AbError *error);
bool ab_ui_scroll_frame_previous(AbUiScrollFrame *scroll, AbError *error);
bool ab_ui_scroll_frame_nearest(AbUiScrollFrame *scroll, AbError *error);
bool ab_ui_scroll_frame_flick(AbUiScrollFrame *scroll, float speed, AbUiScrollAxis axis,
                              AbError *error);
bool ab_ui_scroll_frame_begin_drag(AbUiScrollFrame *scroll, AbVec2 point,
                                   AbError *error);
bool ab_ui_scroll_frame_update_drag(AbUiScrollFrame *scroll, AbVec2 point,
                                    AbError *error);
bool ab_ui_scroll_frame_release(AbUiScrollFrame *scroll, AbError *error);
bool ab_ui_scroll_frame_set_clip(AbUiScrollFrame *scroll, const AbUiScrollClip *clip,
                                 AbError *error);

#endif
