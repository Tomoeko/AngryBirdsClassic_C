#include "ab/ui_widget_draw.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Widget draw failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static bool record_plan(void *context, const AbUiImageDrawPlan *plan, AbError *error) {
    (void)error;
    *(AbUiImageDrawPlan *)context = *plan;
    return true;
}

static bool edge_checks(void) {
    AbUiImageButtonState state = {0};
    AbUiFrameFields *frame = &state.image.frame.fields;
    frame->x = number(10.75f);
    frame->y = number(-20.25f);
    frame->scale_x = frame->scale_y = number(1);
    frame->angle = number(0);
    AbUiPointerValue opaque = {.kind = AB_UI_POINTER_FUNCTION, .function = 9};
    frame->w = frame->h = opaque;
    state.image.rotation_pivot_x = state.image.rotation_pivot_y = opaque;
    state.image.pivot_x = state.image.pivot_y = opaque;
    state.image.resource_pivot_x = state.image.resource_pivot_y = opaque;
    state.image.image = opaque;
    state.image.floor_coordinates = opaque;
    state.hover_scale = opaque;
    AbUiImageDraw view;
    AbUiDrawInput input = {0};
    AbUiImageDrawPlan plan = {0};
    CHECK(ab_ui_image_state_draw_view(&state.image, &view) && !view.visible &&
          view.image_present && !view.pivot_present &&
          ab_ui_image_button_state_draw(&state, NULL, 0, &input, (AbVec2){480, 320},
                                        record_plan, &plan, NULL) &&
          plan.count == 1 && plan.calls[0].state.count == 7);
    frame->visible = number(1);
    CHECK(ab_ui_image_state_draw_view(&state.image, &view) && !view.visible);
    frame->visible = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    state.image.image = (AbUiPointerValue){0};
    CHECK(ab_ui_image_state_draw_view(&state.image, &view) && view.visible &&
          !view.image_present);
    AbUiImageDraw saved = view;
    state.image.image = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(!ab_ui_image_state_draw_view(&state.image, &view) &&
          memcmp(&view, &saved, sizeof(view)) == 0);
    state.image.image = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT};
    frame->w = number(8.75f);
    frame->h = number(6.25f);
    state.image.pivot_x = (AbUiPointerValue){0};
    state.image.pivot_y = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    state.image.rotation_pivot_x = number(-0.0f);
    state.image.rotation_pivot_y = number(3.5f);
    state.image.floor_coordinates = number(0);
    state.image.alpha = number(0);
    frame->angle = number(0.5f);
    CHECK(ab_ui_image_state_draw_view(&state.image, &view) &&
          view.pivot_present == 12 && view.floor_coordinates && view.alpha_present &&
          signbit(view.rotation_pivot.x) &&
          ab_ui_image_draw_plan(&view, &input, (AbVec2){480, 320}, &plan) &&
          plan.count == 3 && plan.calls[0].state.arguments[0] == 10 &&
          plan.calls[0].state.arguments[1] == -21 &&
          plan.calls[0].state.arguments[7] == 0 && plan.calls[1].sprite.size.x == 8 &&
          plan.calls[1].sprite.size.y == 6 && plan.calls[2].state.arguments[7] == 1);
    saved = view;
    state.image.alpha = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(!ab_ui_image_state_draw_view(&state.image, &view) &&
          memcmp(&view, &saved, sizeof(view)) == 0);
    state.image.alpha = number(NAN);
    CHECK(!ab_ui_image_state_draw_view(&state.image, &view));
    state.image.alpha = (AbUiPointerValue){0};
    state.hover_enabled = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT};
    AbUiImageDrawPlan saved_plan = plan;
    CHECK(!ab_ui_image_button_state_draw(&state, NULL, 0, &input, (AbVec2){480, 320},
                                         record_plan, &plan, NULL) &&
          memcmp(&plan, &saved_plan, sizeof(plan)) == 0);
    state.hover_scale = number(1);
    CHECK(ab_ui_image_button_state_draw(&state, NULL, 0, &input, (AbVec2){480, 320},
                                        record_plan, &plan, NULL) &&
          plan.count == 3 && plan.calls[0].state.arguments[2] == 1.1f);
    frame->x = (AbUiPointerValue){0};
    CHECK(!ab_ui_image_state_draw_view(&state.image, &view) &&
          !ab_ui_frame_state_draw_view(NULL, &view.frame) &&
          !ab_ui_image_state_draw_view(&state.image, NULL));
    puts(
        "Canonical draw field gates, strict visibility, numeric zero truthiness, "
        "empty image names, unused state, hover gating and failure preservation pass.");
    return true;
}

int main(void) {
    return edge_checks() ? 0 : 1;
}
