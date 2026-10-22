#ifndef AB_UI_ANIMATED_IMAGE_APK_TEST_H
#define AB_UI_ANIMATED_IMAGE_APK_TEST_H

#include "ab/ui_animated_image.h"

typedef struct {
    AbUiAnimatedImage *image;
    size_t calls;
    bool stop;
} AnimatedFixture;

static bool animated_child(void *context, AbUiWidget *widget, float seconds, float time,
                           AbError *error) {
    AnimatedFixture *fixture = context;
    (void)widget;
    (void)seconds;
    (void)time;
    (void)error;
    ++fixture->calls;
    if (fixture->stop)
        fixture->image->stopped =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    return true;
}

static bool animated_image(Resources *resources, AbError *error) {
    Fixture fixture;
    CHECK(initialize(&fixture, resources, false, false, false, false, error));
    AbUiAnimatedImage image = {0};
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    AbUiImageArguments arguments = {.frame.name = text("electricity")};
    CHECK(ab_ui_animated_image_init(&image, &fixture.context,
                                    (AbUiWidgetChildren){frames, draw, pointer, 1},
                                    &arguments, error));
    CHECK(real(image.timer) == 0 && real(image.speed) == 1 &&
          image.current_frame.kind == AB_UI_POINTER_NONE);
    image.widget.frame->fields.x = number(300);
    image.widget.frame->fields.y = number(100);
    AbDataEntry entries[6];
    for (size_t index = 0; index < 6; ++index)
        entries[index] = (AbDataEntry){
            {.kind = AB_DATA_NUMBER, .bits = number((float)(index + 1)).number_bits},
            {.kind = AB_DATA_STRING,
             .string =
                 span(index & 1 ? "INGAME_BUTTON_EAGLE" : "INGAME_BUTTON_PAUSE")}};
    AbDataTable sprites = {entries, 6, 6, NULL};
    CHECK(ab_ui_animated_image_frames(&image, &sprites, error));
    AbUiDrawInput input = {0};
    CHECK(ab_ui_widget_draw(&image.widget, &input, error) && !fixture.list.count &&
          equal(image.widget.state.image.image, ""));
    AbUiWidget child = {0};
    CHECK(ab_ui_widget_init(&child, &fixture.context, AB_UI_WIDGET_FRAME,
                            (AbUiWidgetChildren){0}, NULL, NULL, NULL, error) &&
          ab_ui_widget_add(&image.widget, &child, error));
    AnimatedFixture observer = {.image = &image, .stop = true};
    child.methods = (AbUiWidgetMethods){.context = &observer, .update = animated_child};
    CHECK(ab_ui_widget_update(&image.widget, 0.375f, 1, error) && observer.calls == 1 &&
          real(image.timer) == 0 && image.current_frame.kind == AB_UI_POINTER_NONE);
    observer.stop = false;
    image.stopped = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(ab_ui_widget_update(&image.widget, 0.375f, 1.375f, error) &&
          real(image.timer) == 0.375f && real(image.current_frame) == 3 &&
          ab_ui_widget_draw(&image.widget, &input, error) && fixture.list.count == 1 &&
          equal(image.widget.state.image.image, "INGAME_BUTTON_PAUSE"));
    image.widget.frame->fields.visible =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    image.current_frame = number(2);
    CHECK(ab_ui_widget_draw(&image.widget, &input, error) && fixture.list.count == 1 &&
          equal(image.widget.state.image.image, "INGAME_BUTTON_EAGLE"));
    image.widget.frame->fields.visible =
        (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    CHECK(ab_ui_animated_image_current(&image, 4.75f, error) &&
          real(image.timer) == 0.625f && real(image.current_frame) == 2 &&
          ab_ui_widget_update(&image.widget, 0, 1.375f, error) &&
          real(image.current_frame) == 4);
    CHECK(ab_ui_widget_update(&image.widget, 0.375f, 1.75f, error) &&
          real(image.timer) == 1 && real(image.current_frame) == 0 &&
          ab_ui_pointer_value_truthy(&image.stopped));
    CHECK(ab_ui_animated_image_frames(&image, &sprites, error) &&
          real(image.timer) == 0 && real(image.current_frame) == 0 &&
          ab_ui_pointer_value_truthy(&image.stopped));
    image.stopped = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    image.looping = number(0);
    image.speed = number(2);
    CHECK(ab_ui_widget_update(&image.widget, 1.5625f, 3.3125f, error) &&
          real(image.timer) == 2.125f && real(image.current_frame) == 13);
    fixture.list.count = 0;
    CHECK(ab_ui_widget_draw(&image.widget, &input, error) && !fixture.list.count &&
          equal(image.widget.state.image.image, ""));
    CHECK(ab_ui_animated_image_frames(&image, NULL, error) && real(image.timer) == 0 &&
          !ab_ui_widget_update(&image.widget, 0.125f, 3.4375f, NULL) &&
          fixture.context.failed && real(image.timer) == 0.25f);
    return true;
}

#endif
