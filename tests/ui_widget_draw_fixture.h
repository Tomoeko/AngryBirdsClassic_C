#ifndef AB_TEST_UI_WIDGET_DRAW_FIXTURE_H
#define AB_TEST_UI_WIDGET_DRAW_FIXTURE_H

#include "ab/ui_widget_draw.h"

static AbUiPointerValue widget_number(uint32_t bits) {
    return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER, .number_bits = bits};
}

static AbUiImageState widget_image(const uint32_t setup[25], AbSpan sprite) {
    AbUiImageState state = {0};
    AbUiFrameFields *frame = &state.frame.fields;
    frame->x = widget_number(setup[13]);
    frame->y = widget_number(setup[14]);
    frame->scale_x = widget_number(setup[15]);
    frame->scale_y = widget_number(setup[16]);
    frame->angle = widget_number(setup[17]);
    frame->w = widget_number(setup[18]);
    frame->h = widget_number(setup[19]);
    if (setup[1] == 1 || setup[1] == 2) {
        frame->visible =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = setup[1] == 2};
    } else if (setup[1] == 3) {
        frame->visible = widget_number(UINT32_C(0x3f800000));
    }
    if (setup[0] & 1) {
        state.image = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = sprite};
    }
    state.floor_coordinates = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                                 .boolean = (setup[0] & 2) != 0};
    if (setup[4]) {
        state.alpha = widget_number(setup[24]);
    }
    AbUiPointerValue *pivots[] = {&state.pivot_x, &state.pivot_y,
                                  &state.rotation_pivot_x, &state.rotation_pivot_y};
    for (size_t index = 0; index < 4; ++index) {
        *pivots[index] = setup[3] & (1u << index)
                             ? widget_number(setup[20 + index])
                             : (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    }
    return state;
}

#endif
