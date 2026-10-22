#ifndef AB_UI_FRAME_DRAW_H
#define AB_UI_FRAME_DRAW_H

#include "ab/ui_pointer.h"

typedef struct {
    AbVec2 position;
    AbVec2 scale;
    float angle;
    float local_factor;
} AbUiDrawTransform;

typedef struct {
    AbUiDrawTransform transform;
    /* Numeric presence in x,y,scaleX,scaleY,angle,factor order. Absent/false
     * defaults are 0,0,1,1,0,1. Numeric zero is present and is not defaulted. */
    uint32_t present;
} AbUiDrawInput;

/* Shared numeric default boundary. Require finite resolved values; failure
 * preserves output. Unused absent fields are not inspected. */
bool ab_ui_draw_resolve(const AbUiDrawInput *input, AbUiDrawTransform *output);

typedef struct {
    AbVec2 position;
    AbVec2 scale;
    float angle;
} AbUiDrawFrame;

typedef bool (*AbUiDrawCallback)(void *context, AbSpan method,
                                 const AbUiDrawTransform *transform, AbError *error);

typedef struct {
    AbUiDrawCallback callback;
    void *context;
    AbUiPointerValue tag;
    bool visible;
} AbUiDrawChild;

/* Base Frame.drawWithTag over a borrowed dense, stable child array in forward
 * order. Only child visibility is gated. A non-absent filter requires Lua-value
 * equality with the child's tag, including signed-zero equality, unequal NaN
 * and logical object identity. NULL/absent filter accepts every tag.
 * Resolve input defaults, then add frame position, multiply frame scale and add
 * frame angle separately. Pass local_factor unchanged. No rotation or scaling
 * of positions, scroll, active flag or self visibility is applied here.
 * Empty method is a valid method name; a NULL method selects exact draw.
 * Callbacks/geometry/children must remain stable, numeric arithmetic finite,
 * and count <=2^24. No allocation. Earlier callbacks remain on failure. */
bool ab_ui_frame_draw(const AbUiDrawFrame *frame, const AbUiDrawChild *children,
                      size_t count, const AbUiDrawInput *input,
                      const AbUiPointerValue *filter, const AbSpan *method,
                      AbError *error);

#endif
