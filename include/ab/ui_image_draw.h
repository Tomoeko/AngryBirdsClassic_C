#ifndef AB_UI_IMAGE_DRAW_H
#define AB_UI_IMAGE_DRAW_H

#include "ab/ui_frame_draw.h"

typedef struct {
    AbUiDrawFrame frame;
    AbVec2 size;
    AbVec2 pivot;
    AbVec2 rotation_pivot;
    /* pivot x/y then rotation-pivot x/y. Missing/false pivots select zero;
     * missing/false rotation pivots select the corresponding resolved pivot. */
    uint32_t pivot_present;
    AbSpan image;
    float alpha;
    bool image_present;
    /* Caller resolves original visible == true, rather than general truthiness. */
    bool visible;
    bool floor_coordinates;
    bool alpha_present;
} AbUiImageDraw;

typedef enum {
    AB_UI_IMAGE_RENDER_STATE,
    AB_UI_IMAGE_SPRITE,
} AbUiImageDrawCallKind;

typedef struct {
    AbUiImageDrawCallKind kind;
    union {
        struct {
            /* x,y,scaleX,scaleY,angle,rotationPivotX,rotationPivotY[,alpha]. */
            float arguments[8];
            size_t count;
        } state;
        struct {
            AbSpan image;
            AbVec2 size;
        } sprite;
    };
} AbUiImageDrawCall;

typedef struct {
    AbUiImageDrawCall calls[3];
    size_t count;
    bool culled;
} AbUiImageDrawPlan;

/* Original Image.drawSelf boundary plan, preserving state/sprite/reset order.
 * Position adds self position times local_factor; scales multiply parent,
 * self, then local_factor. Floor coordinates optionally, and always floor sprite
 * dimensions. Zero-angle culling follows original signed bounds and anchor
 * fast path; rotated images bypass this script cull. A culled draw returns
 * before state reset. Absent/invisible images still reset state.
 * Sprite calls mean res.drawSprite("",image,0,0,"HPIVOT","VPIVOT",w,h).
 * State alpha is supplied only when present; reset alpha is one in that case.
 * Finite resolved inputs and pre-division arithmetic are required. Preserve
 * IEEE divide-by-zero infinities and positive default NaN for 0/0. Input text
 * remains borrowed; no callbacks, assets, allocation, GPU or child traversal.
 * Failure preserves the entire output plan. */
bool ab_ui_image_draw_plan(const AbUiImageDraw *image, const AbUiDrawInput *input,
                           AbVec2 screen_size, AbUiImageDrawPlan *output);

#endif
