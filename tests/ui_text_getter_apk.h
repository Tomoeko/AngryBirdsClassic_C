#ifndef AB_UI_TEXT_GETTER_APK_TEST_H
#define AB_UI_TEXT_GETTER_APK_TEST_H

typedef struct {
    AbUiPointerValue value;
    size_t calls;
} FontGetterFixture;

static bool font_getter(void *context, AbUiPointerValue *output, AbError *error) {
    FontGetterFixture *fixture = context;
    (void)error;
    ++fixture->calls;
    *output = fixture->value;
    return true;
}

static bool font_getters(Resources *resources, AbError *error) {
    Fixture fixture;
    CHECK(initialize(&fixture, resources, false, false, false, false, error));
    FontGetterFixture global = {.value = text("FONT_MENU")};
    AbUiFontGetter getter = {&global, font_getter};
    resources->fonts.getters = &getter;
    resources->fonts.getter_count = 1;
    AbUiPointerValue function = {.kind = AB_UI_POINTER_FUNCTION,
                                 .function = (uintptr_t)&getter};
    AbUiTextWidget label = {0};
    AbUiTextArguments args = {.text = text("1-1"), .font = function};
    CHECK(ab_ui_text_widget_init(&label, &fixture.context, (AbUiWidgetChildren){0},
                                 &resources->fonts, NULL, &args, error) &&
          global.calls == 1 && label.state.font_getter.function == function.function &&
          equal(label.state.fields.font, "FONT_MENU") &&
          resources->fonts.current == resources->bindings + 2);
    global.value = text("FONT_BASIC");
    CHECK(ab_ui_widget_lifecycle(&label.widget, AB_UI_FRAME_LAYOUT, error) &&
          global.calls == 2 && equal(label.state.fields.font, "FONT_BASIC") &&
          resources->fonts.current == resources->bindings + 2);
    label.widget.frame->fields.x = number(200);
    label.widget.frame->fields.y = number(100);
    AbUiDrawInput input = {0};
    CHECK(ab_ui_widget_draw(&label.widget, &input, error) && fixture.list.count == 3 &&
          global.calls == 2 && resources->fonts.current == resources->bindings);
    CHECK(ab_ui_text_widget_set_font(&label, text("FONT_LS_SMALL"), error) &&
          label.state.font_getter.function == function.function &&
          equal(label.state.fields.font, "FONT_LS_SMALL"));
    global.value = text("FONT_MENU");
    CHECK(ab_ui_widget_lifecycle(&label.widget, AB_UI_FRAME_LAYOUT, error) &&
          global.calls == 3 && equal(label.state.fields.font, "FONT_MENU") &&
          resources->fonts.current == resources->bindings);
    AbUiPointerValue unknown = {.kind = AB_UI_POINTER_FUNCTION, .function = 1};
    CHECK(!ab_ui_text_widget_set_font(&label, unknown, NULL) &&
          fixture.context.failed && label.state.font_getter.function == 1 &&
          equal(label.state.fields.font, "FONT_MENU"));
    Fixture rejected;
    CHECK(initialize(&rejected, resources, false, false, false, false, error));
    AbUiTextWidget fallback = {0};
    global.value = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(!ab_ui_text_widget_init(&fallback, &rejected.context, (AbUiWidgetChildren){0},
                                  &resources->fonts, NULL, &args, NULL) &&
          rejected.context.failed &&
          fallback.state.font_getter.function == function.function &&
          fallback.state.fields.font.kind == AB_UI_POINTER_FUNCTION &&
          fallback.state.fields.font.function == function.function);
    resources->fonts.getters = NULL;
    resources->fonts.getter_count = 0;
    return true;
}

#endif
