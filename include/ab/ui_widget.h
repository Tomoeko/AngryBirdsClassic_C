#ifndef AB_UI_WIDGET_H
#define AB_UI_WIDGET_H

#include "ab/ui_widget_draw.h"
#include "ab/ui_widget_pointer.h"
#include "ab/ui_image_hit.h"
#include "ab/ui_image_render.h"

typedef struct AbUiWidget AbUiWidget;

typedef struct {
    AbUiImageResourceBoundaries images;
    AbUiImageRenderBoundaries render;
    AbUiImageRenderState *render_state;
    AbDrawList *draw_list;
    /* Resolved current g_screen_clip_rect_offset; NULL is the source fallback. */
    const AbVec2 *screen_clip_offset;
    AbUiFrameLifecycle lifecycle;
    /* BGBox's source global _, shared by all panels in this context. */
    AbUiPointerValue discarded_sprite_bound;
    AbVec2 cursor;
    float touch_count;
    size_t maximum_depth, depth;
    bool animation_installed, scale_buttons_on_touch, left_button_held, device_ipad;
    void *audio_context;
    bool (*play_audio)(void *context, AbUiPointerValue sound, float volume, bool loop,
                       AbError *error);
    bool failed;
} AbUiWidgetContext;

typedef struct {
    AbUiFrameNode **frames;
    AbUiDrawChild *draw;
    AbUiPointerChild *pointer;
    size_t capacity;
} AbUiWidgetChildren;

typedef enum {
    AB_UI_WIDGET_FRAME,
    AB_UI_WIDGET_IMAGE,
    AB_UI_WIDGET_IMAGE_BUTTON,
} AbUiWidgetKind;

typedef struct {
    void *context;
    bool (*lifecycle)(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error);
    bool (*draw)(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error);
    bool (*pointer)(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error);
    bool (*update)(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error);
    bool (*named_draw)(void *context, AbUiWidget *widget, AbSpan method,
                       const AbUiDrawInput *input, AbError *error);
} AbUiWidgetMethods;

struct AbUiWidget {
    AbUiWidgetContext *context;
    AbUiWidgetKind kind;
    /* Frame and Image use this same canonical prefix. The remaining button
     * fields are inspected only by ImageButton methods. */
    AbUiImageButtonState state;
    /* A derived class may own its actual Frame inside another state object. */
    AbUiFrameState *frame;
    AbUiFrameNode tree;
    AbUiFrameGeometry geometry;
    AbUiWidgetChildren children;
    AbUiWidgetMethods methods;
    AbUiPointerValue padding[4]; /* left, right, top, bottom */
    AbUiPointerValue scroll[2];
    AbUiPointerValue tag;
    char default_name[40];
    bool initialized, drawing, pointing, updating;
};

/* Caller-owned concrete Frame/Image/ImageButton instances and stable child
 * storage. Bind actual image resources, rendering and audio services in one
 * shared context. No allocation, VM or resource loading. Used names, arguments,
 * children and providers remain borrowed through the widget's lifetime.
 * Initialize fresh zeroed storage. Constructor callbacks preserve their source
 * prefixes; a reached failure stops the shared context.
 *
 * Derived classes install actual C methods and call the corresponding base
 * entry point explicitly. An absent override selects its real builtin method.
 * Foreign frame nodes, padding objects and mutation of a traversed child array
 * are unsupported. Padding fields use the instance's own stable object token.
 */
bool ab_ui_widget_init(AbUiWidget *widget, AbUiWidgetContext *context,
                       AbUiWidgetKind kind, AbUiWidgetChildren children,
                       const AbUiFrameFields *frame, const AbUiImageArguments *image,
                       const AbUiImageButtonArguments *button, AbError *error);
/* Preserve constructor virtual image-setter dispatch into a concrete class.
 * The setter may access canonical state before the tree is initialized. */
bool ab_ui_widget_init_derived(AbUiWidget *widget, AbUiWidgetContext *context,
                               AbUiWidgetKind kind, AbUiWidgetChildren children,
                               const AbUiFrameFields *frame,
                               const AbUiImageArguments *image,
                               const AbUiImageButtonArguments *button,
                               AbUiImageInitSetter setter, AbError *error);
/* Construct a Frame-derived class around its own canonical Frame. The actual
 * class initializer receives the widget's default-name boundary and runs once.
 * External state and callback context must remain stable during construction;
 * the state must outlive the widget. No copy or mirrored Frame is retained. */
typedef bool (*AbUiWidgetFrameInitializer)(void *context,
                                           const AbUiInitBoundaries *boundaries,
                                           AbError *error);
bool ab_ui_widget_init_frame(AbUiWidget *widget, AbUiWidgetContext *context,
                             AbUiWidgetChildren children, AbUiFrameState *frame,
                             AbUiWidgetFrameInitializer initialize,
                             void *initialize_context, AbError *error);
bool ab_ui_widget_add(AbUiWidget *parent, AbUiWidget *child, AbError *error);
/* Present source position, one-based in [1,child_count+1]. */
bool ab_ui_widget_insert(AbUiWidget *parent, AbUiWidget *child, size_t position,
                         AbError *error);

bool ab_ui_widget_lifecycle_base(AbUiWidget *widget, AbUiFrameMethod method,
                                 AbError *error);
/* Dispatch the actual derived lifecycle method, including for a scene root. */
bool ab_ui_widget_lifecycle(AbUiWidget *widget, AbUiFrameMethod method, AbError *error);
bool ab_ui_widget_draw(AbUiWidget *widget, const AbUiDrawInput *input, AbError *error);
/* Derived menu classes expose the actual named background/midground/foreground
 * draw methods. Missing named methods fail rather than substituting draw. */
bool ab_ui_widget_draw_named(AbUiWidget *widget, AbSpan method,
                             const AbUiDrawInput *input, AbError *error);
bool ab_ui_widget_draw_base(AbUiWidget *widget, const AbUiDrawInput *input,
                            AbError *error);
bool ab_ui_widget_draw_self(AbUiWidget *widget, const AbUiDrawInput *input,
                            AbError *error);
bool ab_ui_widget_draw_with_tag(AbUiWidget *widget, const AbUiDrawInput *input,
                                const AbUiPointerValue *tag, AbError *error);
bool ab_ui_widget_pointer(AbUiWidget *widget, AbSpan event, AbVec2 point,
                          AbUiPointerReply *output, AbError *error);
bool ab_ui_widget_pointer_base(AbUiWidget *widget, AbSpan event, AbVec2 point,
                               AbUiPointerReply *output, AbError *error);
bool ab_ui_widget_update(AbUiWidget *widget, float seconds, float time, AbError *error);
bool ab_ui_widget_update_base(AbUiWidget *widget, float seconds, float time,
                              AbError *error);
bool ab_ui_widget_clear_pointer_flags(AbUiWidget *widget, AbError *error);
/* Read only the fields used by the corresponding Frame query. Position ignores
 * self scroll and every scale/angle; cumulative scale ignores positions/scroll. */
bool ab_ui_widget_screen_position(AbUiWidget *widget, AbVec2 *output, AbError *error);
bool ab_ui_widget_cumulative_scale(AbUiWidget *widget, AbVec2 *output, AbError *error);
/* Actual ImageButton.hitTest can also receive an Image-derived class. Read
 * resource bounds/pivot rather than the mutable draw dimensions; absent/false
 * hit padding is the source's empty table. Frame-only receivers are invalid. */
bool ab_ui_widget_image_hit(AbUiWidget *widget, AbVec2 point, bool *output,
                            AbError *error);
/* Real screen-clip host for ScrollFrame. Context is the shared
 * AbUiWidgetContext; retain clip through subsequent render-state changes. */
bool ab_ui_widget_set_screen_clip(void *context, AbScreenClipRect clip, AbError *error);

#endif
