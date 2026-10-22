#include "ab/ui_scroll_frame.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "ScrollFrame check failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbUiPointerValue number(float value) {
    AbUiPointerValue result = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&result.number_bits, &value, sizeof(value));
    return result;
}

static float real(AbUiPointerValue value) {
    float result;
    memcpy(&result, &value.number_bits, sizeof(result));
    return result;
}

static AbDataValue key(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

typedef struct {
    AbEventManager *events;
    AbUiScrollFrame scroll;
    AbVec2 anchors[4];
    AbUiWidget child;
    AbUiFrameNode *frames[1];
    AbUiDrawChild draws[1];
    AbUiPointerChild pointers[1];
    AbUiWidgetContext context;
    float dt, wheel;
    unsigned pointers_seen, draws_seen, anchors_seen, clips_seen;
    AbVec2 last_point;
    AbUiDrawTransform last_transform;
    char phases[3][24];
    AbUiScrollClip clips[2];
    bool fail_clip, fail_pointer, fail_draw;
} Fixture;

static bool pointer(void *context, AbUiWidget *widget, AbSpan event, AbVec2 point,
                    AbUiPointerReply *output, AbError *error) {
    (void)event;
    Fixture *fixture = context;
    fixture->pointers_seen++;
    fixture->last_point = point;
    if (fixture->fail_pointer)
        return ab_fail(error, "Controlled child pointer failure");
    *output = (AbUiPointerReply){
        .values = {{.kind = AB_UI_POINTER_NUMBER,
                    .number_bits = number((float)fixture->pointers_seen).number_bits},
                   {0},
                   {.kind = AB_UI_POINTER_OBJECT, .object = (uintptr_t)widget}}};
    return true;
}

static bool named_draw(void *context, AbUiWidget *widget, AbSpan method,
                       const AbUiDrawInput *input, AbError *error) {
    (void)widget;
    Fixture *fixture = context;
    CHECK(fixture->draws_seen < 3 && method.size < sizeof(fixture->phases[0]));
    memcpy(fixture->phases[fixture->draws_seen], method.data, method.size);
    fixture->draws_seen++;
    CHECK(ab_ui_draw_resolve(input, &fixture->last_transform));
    return !fixture->fail_draw || ab_fail(error, "Controlled named draw failure");
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    return named_draw(context, widget, span("draw"), input, error);
}

static bool clip(void *context, AbUiScrollClip value, AbError *error) {
    Fixture *fixture = context;
    CHECK(fixture->clips_seen < 2);
    fixture->clips[fixture->clips_seen++] = value;
    return !fixture->fail_clip || ab_fail(error, "Controlled clip failure");
}

static bool changed(void *context, AbUiScrollFrame *scroll, size_t anchor,
                    AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(scroll == &fixture->scroll && anchor == scroll->current_anchor);
    fixture->anchors_seen++;
    return true;
}

static bool open_fixture(Fixture *fixture, AbError *error) {
    fixture->context = (AbUiWidgetContext){
        .render = {.viewport_width = 480, .viewport_height = 320}, .maximum_depth = 32};
    fixture->dt = 0.02f;
    CHECK(ab_event_manager_open((AbEventManagerLimits){4, 4, 4, 4, 4, 16384},
                                (AbEventManagerHost){0}, &fixture->events, error));
    CHECK(ab_ui_scroll_frame_init(
        &fixture->scroll, &fixture->context,
        (AbUiWidgetChildren){fixture->frames, fixture->draws, fixture->pointers, 1},
        fixture->anchors, 4, NULL,
        (AbUiScrollHost){fixture->events, key("PAUSE"), &fixture->dt, &fixture->wheel,
                         fixture, clip},
        error));
    fixture->scroll.derived_context = fixture;
    fixture->scroll.anchor_changed = changed;
    AbUiFrameFields fields = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span("child")},
        .x = number(100),
        .y = number(50),
        .w = number(40),
        .h = number(30)};
    CHECK(ab_ui_widget_init(&fixture->child, &fixture->context, AB_UI_WIDGET_FRAME,
                            (AbUiWidgetChildren){0}, &fields, NULL, NULL, error));
    fixture->child.methods = (AbUiWidgetMethods){
        .context = fixture, .pointer = pointer, .draw = draw, .named_draw = named_draw};
    CHECK(ab_ui_widget_add(&fixture->scroll.widget, &fixture->child, error));
    CHECK(ab_ui_widget_lifecycle(&fixture->scroll.widget, AB_UI_FRAME_ENTRY, error));
    return true;
}

static bool close_fixture(Fixture *fixture) {
    CHECK(ab_ui_scroll_frame_detach(&fixture->scroll, NULL));
    ab_event_manager_close(fixture->events);
    return true;
}

static bool motion(void) {
    Fixture fixture = {0};
    CHECK(open_fixture(&fixture, NULL));
    AbUiScrollFrame *scroll = &fixture.scroll;
    AbVec2 anchors[] = {{0, 0}, {100, 0}, {200, 50}};
    CHECK(ab_ui_scroll_frame_set_anchors(scroll, anchors, 3, NULL));
    CHECK(scroll->bounds.lower.x == 0 && scroll->bounds.upper.x == 200 &&
          scroll->bounds.upper.y == 50 && scroll->current_anchor == 1);
    anchors[1].x = 999;
    CHECK(scroll->anchors[1].x == 100);
    CHECK(ab_ui_scroll_frame_set_target(scroll, 2, NULL));
    CHECK(ab_ui_widget_update(&scroll->widget, 0.0625f, 1, NULL));
    CHECK(real(scroll->widget.scroll[0]) == 50 && scroll->current_anchor == 1 &&
          fixture.anchors_seen == 0);
    CHECK(ab_ui_widget_update(&scroll->widget, 0.0625f, 2, NULL));
    CHECK(real(scroll->widget.scroll[0]) == 75 && scroll->current_anchor == 2 &&
          fixture.anchors_seen == 1);
    CHECK(ab_ui_widget_update(&scroll->widget, 1, 3, NULL));
    CHECK(real(scroll->widget.scroll[0]) == 100);
    CHECK(ab_ui_scroll_frame_next(scroll, NULL) && scroll->target_anchor == 3);
    CHECK(ab_ui_scroll_frame_set_current(scroll, 3, NULL));
    CHECK(ab_ui_scroll_frame_previous(scroll, NULL) && scroll->target_anchor == 2);
    CHECK(ab_ui_scroll_frame_set_current(scroll, 2, NULL));
    scroll->widget.scroll[0] = number(150);
    scroll->widget.scroll[1] = number(0);
    CHECK(ab_ui_scroll_frame_nearest(scroll, NULL) && scroll->target_anchor == 2);
    CHECK(ab_ui_scroll_frame_flick(scroll, 1000, AB_UI_SCROLL_X, NULL) &&
          scroll->target_anchor == 3 && scroll->velocity == 8);
    CHECK(ab_ui_scroll_frame_flick(scroll, -3000, AB_UI_SCROLL_X, NULL) &&
          scroll->target_anchor == 2 && scroll->velocity > 8);
    scroll->widget.scroll[0] = number(-10);
    CHECK(ab_ui_scroll_frame_flick(scroll, -3000, AB_UI_SCROLL_X, NULL) &&
          scroll->target_anchor == 1 && scroll->velocity == 8);
    CHECK(ab_ui_scroll_frame_set_current(scroll, 1, NULL));
    scroll->drag_index = 4;
    scroll->drag_timer = 0.001f;
    CHECK(ab_ui_scroll_frame_begin_drag(scroll, (AbVec2){100, 100}, NULL));
    CHECK(scroll->drag_index == 4 && scroll->drag_timer == 0.001f);
    CHECK(ab_ui_scroll_frame_update_drag(scroll, (AbVec2){80, 90}, NULL));
    CHECK(real(scroll->widget.scroll[0]) == 20 && real(scroll->widget.scroll[1]) == 0 &&
          scroll->drag_index == 5 && scroll->speed_x[4] > 900);
    CHECK(ab_ui_scroll_frame_release(scroll, NULL) && !scroll->dragging);
    scroll->drag_has_moved = true;
    CHECK(ab_ui_scroll_frame_reset(scroll, NULL) && scroll->drag_has_moved);
    CHECK(close_fixture(&fixture));
    return true;
}

static bool routing(void) {
    Fixture fixture = {0};
    CHECK(open_fixture(&fixture, NULL));
    AbUiScrollFrame *scroll = &fixture.scroll;
    scroll->widget.scroll[0] = number(30);
    AbUiPointerReply reply;
    CHECK(ab_ui_widget_pointer(&scroll->widget, span("LPRESS"), (AbVec2){50, 50},
                               &reply, NULL));
    CHECK(fixture.pointers_seen == 2 && real(reply.values[0]) == 2 &&
          fixture.last_point.x == 80 && scroll->dragging);
    CHECK(ab_ui_widget_pointer(&scroll->widget, span("LHOLD"), (AbVec2){20, 50}, &reply,
                               NULL));
    CHECK(fixture.pointers_seen == 3 && reply.values[0].kind == AB_UI_POINTER_NONE &&
          scroll->drag_has_moved && real(scroll->widget.scroll[0]) > 30);
    AbDataEntry entry = {key("id"), key("PAUSE")};
    AbDataTable event = {&entry, 1, 1, NULL};
    CHECK(ab_event_manager_notify(fixture.events, &event, NULL) && !scroll->dragging &&
          !scroll->drag_has_moved);
    AbUiScrollClip rectangle = {0, 0, 100, 100};
    CHECK(ab_ui_scroll_frame_set_clip(scroll, &rectangle, NULL));
    rectangle.width = 999;
    fixture.context.cursor = (AbVec2){100, 20};
    unsigned count = fixture.pointers_seen;
    CHECK(ab_ui_widget_pointer(&scroll->widget, span("LPRESS"), (AbVec2){20, 20},
                               &reply, NULL));
    CHECK(fixture.pointers_seen == count && !scroll->dragging);
    fixture.wheel = -1;
    CHECK(ab_ui_widget_pointer(&scroll->widget, span("WHEEL"), (AbVec2){20, 20}, &reply,
                               NULL));
    CHECK(scroll->target_anchor == 1);
    CHECK(close_fixture(&fixture));
    return true;
}

static bool drawing(void) {
    Fixture fixture = {0};
    CHECK(open_fixture(&fixture, NULL));
    AbUiScrollFrame *scroll = &fixture.scroll;
    AbUiDrawInput input = {.transform = {.position = {20.75f, 30.75f}}, .present = 3};
    scroll->widget.scroll[0] = number(3.5f);
    scroll->widget.scroll[1] = number(4.5f);
    scroll->widget.state.image.frame.fields.x = number(10.5f);
    scroll->widget.state.image.frame.fields.y = number(5.5f);
    scroll->widget.state.image.frame.fields.scale_x = number(2);
    /* ScrollFrame draw/culling do not read frame angles or parent draw scales.
     * Geometry queries read only their own relevant Frame fields. */
    scroll->widget.state.image.frame.fields.angle = (AbUiPointerValue){0};
    fixture.child.state.image.frame.fields.angle = (AbUiPointerValue){0};
    input.transform.scale = (AbVec2){NAN, NAN};
    input.transform.angle = NAN;
    input.transform.local_factor = NAN;
    input.present = 63;
    AbVec2 query;
    fixture.child.scroll[0] = number(NAN);
    fixture.child.scroll[1] = number(NAN);
    CHECK(ab_ui_widget_screen_position(&fixture.child, &query, NULL) &&
          query.x == 107 && query.y == 51);
    fixture.child.state.image.frame.fields.x = (AbUiPointerValue){0};
    CHECK(ab_ui_widget_cumulative_scale(&fixture.child, &query, NULL) && query.x == 2);
    fixture.child.state.image.frame.fields.x = number(100);
    AbUiScrollClip rectangle = {0, 0, 480, 320};
    CHECK(ab_ui_scroll_frame_set_clip(scroll, &rectangle, NULL));
    scroll->draw_in_three_phases = true;
    CHECK(ab_ui_widget_draw(&scroll->widget, &input, NULL));
    CHECK(fixture.draws_seen == 3 && fixture.clips_seen == 2 &&
          !strcmp(fixture.phases[0], "drawBackground") &&
          !strcmp(fixture.phases[1], "drawMidground") &&
          !strcmp(fixture.phases[2], "drawForeground") &&
          fixture.last_transform.position.x == 27 &&
          fixture.last_transform.position.y == 31 &&
          fixture.last_transform.scale.x == 2 && fixture.clips[1].width == 480 &&
          fixture.clips[1].height == 320);
    CHECK(close_fixture(&fixture));
    input = (AbUiDrawInput){0};
    Fixture failed = {0};
    CHECK(open_fixture(&failed, NULL));
    failed.fail_draw = true;
    CHECK(ab_ui_scroll_frame_set_clip(&failed.scroll, &rectangle, NULL));
    CHECK(!ab_ui_widget_draw(&failed.scroll.widget, &input, NULL) &&
          failed.clips_seen == 1 && failed.context.failed);
    CHECK(close_fixture(&failed));
    return true;
}

int main(void) {
    return motion() && routing() && drawing() ? 0 : 1;
}
