#ifndef AB_GAME_RECTANGLE_H
#define AB_GAME_RECTANGLE_H

#include "ab/base.h"

typedef struct {
    float color[4];
    float left, top, right, bottom;
    bool respect_state;
} AbGameRectangle;

typedef struct {
    uint32_t argb;
    int32_t left, top, width, height;
    bool reset_state;
} AbGameRectanglePlan;

/* GameLua.drawRect's context-fill arguments. Multiply normalized RGBA by255,
 * truncate each channel and pack shifted integer words without channel masking/
 * clamping. Truncate left/top and right-left/bottom-top separately.
 * The native context fill receives Context::Blend value0 (automatic selection).
 * respect_state=false requests the native full render-state reset before fill.
 * This plan does not itself mutate render state or draw. Finite intermediates
 * and int32 converted values required; failure preserves output. No allocation.
 * Full reset-state ownership, fill rasterization and GPU pixels are separate. */
bool ab_game_rectangle_plan(const AbGameRectangle *rectangle,
                            AbGameRectanglePlan *output, AbError *error);

#endif
