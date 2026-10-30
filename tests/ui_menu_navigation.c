#include "ab/ui_menu_navigation.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Menu navigation failed at %d\n", __LINE__);               \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiMenuManager *manager;
    AbUiMenuFrame frames[3];
    unsigned calls[16], count, fail;
    bool installed_at_notification;
} Context;

static bool record(Context *context, unsigned call) {
    CHECK(context->count < 16);
    context->calls[context->count++] = call;
    return context->fail != call;
}

static bool method(void *context, AbUiMenuNode *menu, AbUiMenuMethod operation,
                   float first, float second, AbError *error) {
    (void)menu;
    (void)first;
    (void)second;
    (void)error;
    return record(context, (unsigned)operation + 1);
}

static bool tree_method(void *context, AbUiFrameNode *node, AbUiFrameMethod operation,
                        AbError *error) {
    Context *provider = context;
    for (size_t index = 0; index < 3; ++index) {
        if (provider->frames[index].tree == node) {
            return method(context, provider->frames[index].menu,
                          (AbUiMenuMethod)((int)operation + (int)AB_UI_MENU_ENTRY), 0,
                          0, error);
        }
    }
    return false;
}

static bool reset(void *context, AbUiMenuManager *manager, AbError *error) {
    (void)error;
    ((Context *)context)->manager = manager;
    return true;
}

static bool notify_root(void *context, AbUiMenuNode *menu, AbSpan channel,
                        AbError *error) {
    (void)context;
    (void)menu;
    (void)channel;
    (void)error;
    return true;
}

static bool pushed(void *context, AbUiMenuNode *menu, AbSpan channel, AbError *error) {
    (void)channel;
    (void)error;
    Context *provider = context;
    provider->installed_at_notification = false;
    for (size_t index = 0; index < 3; ++index) {
        if (provider->frames[index].menu == menu) {
            provider->installed_at_notification =
                provider->frames[index].remove_from_frame;
        }
    }
    return record(provider, 8);
}

static bool pop_requested(void *context, AbError *error) {
    (void)error;
    return record(context, 9);
}

static bool exercise(Context *context, AbUiMenuBoundaries *boundaries) {
    AbUiMenuNavigation navigation = {context->manager, context->frames, 3,
                                     context,          pushed,          pop_requested};
    AbUiMenuFrame *root = context->frames, *queued = root + 1, *child = root + 2;
    AbUiMenuTarget target = {AB_UI_MENU_TARGET_FRAME, child->menu, {0}};
    CHECK(!ab_ui_menu_event(&navigation, AB_UI_MENU_EVENT_PUSH_FRAME, target, NULL,
                            boundaries, NULL) &&
          context->count == 1 && !context->installed_at_notification &&
          child->remove_from_frame && !child->tree->parent);
    child->remove_from_frame = false;
    context->count = 0;
    context->fail = 8;
    CHECK(!ab_ui_menu_event(&navigation, AB_UI_MENU_EVENT_PUSH_FRAME, target, NULL,
                            boundaries, NULL) &&
          !child->remove_from_frame);
    context->fail = 0;
    CHECK(
        ab_ui_menu_set_root(context->manager, root->menu, NULL, boundaries, NULL) &&
        ab_ui_menu_change_root(context->manager, queued->menu, NULL, boundaries, NULL));
    context->count = 0;
    queued->tree->entered = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER};
    context->fail = AB_UI_MENU_ENTRY + 1;
    CHECK(!ab_ui_menu_event(&navigation, AB_UI_MENU_EVENT_PUSH_FRAME, target, NULL,
                            boundaries, NULL) &&
          context->count == 2 && context->calls[0] == 8 &&
          context->calls[1] == AB_UI_MENU_ENTRY + 1 &&
          queued->tree->state->child_count == 1 &&
          child->tree->parent == queued->tree && !root->tree->state->child_count &&
          child->remove_from_frame);
    context->fail = 0;
    AbUiFrameNode *popped;
    CHECK(ab_ui_menu_pop_frame(&navigation, NULL, &popped, NULL) &&
          popped == child->tree && child->tree->parent == queued->tree &&
          !queued->tree->state->child_count);
    context->count = 0;
    CHECK(ab_ui_menu_frame_remove(&navigation, child, NULL) && context->count == 1 &&
          context->calls[0] == 9);
    context->count = 0;
    CHECK(ab_ui_menu_event(&navigation, AB_UI_MENU_EVENT_PUSH_FRAME, target, NULL,
                           boundaries, NULL) &&
          context->count == 4 && context->calls[0] == 8 &&
          context->calls[1] == AB_UI_MENU_ENTRY + 1 &&
          context->calls[2] == AB_UI_MENU_LAYOUT + 1 &&
          context->calls[3] == AB_UI_MENU_LAYOUT + 1);
    CHECK(ab_ui_menu_manager_delegate_back(
        context->manager, (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN}));
    target.kind = AB_UI_MENU_TARGET_ABSENT;
    AbUiMenuManagerView view;
    CHECK(!ab_ui_menu_event(&navigation, AB_UI_MENU_EVENT_CHANGE_SCENE, target, NULL,
                            boundaries, NULL) &&
          ab_ui_menu_manager_view(context->manager, &view) &&
          !ab_ui_pointer_value_truthy(&view.delegate_back_clicks));
    target.kind = AB_UI_MENU_TARGET_LINK;
    target.link = (AbSpan){(const uint8_t *)"missing", 7};
    CHECK(!ab_ui_menu_event(&navigation, AB_UI_MENU_EVENT_CHANGE_SCENE, target, NULL,
                            boundaries, NULL) &&
          ab_ui_menu_manager_view(context->manager, &view) &&
          ab_ui_pointer_value_truthy(&view.delegate_back_clicks));
    CHECK(ab_ui_menu_manager_reset(context->manager, boundaries, NULL) &&
          child->remove_from_frame && child->tree->parent == queued->tree &&
          queued->tree->state->child_count == 1);
    return true;
}

int main(void) {
    Context context = {0};
    AbUiFrameState states[3] = {0};
    AbUiFrameNode trees[3] = {0};
    AbUiFrameNode *children[3][3] = {0};
    AbUiMenuNode menus[3] = {0};
    const char *names[] = {"root", "queued", "child"};
    for (size_t index = 0; index < 3; ++index) {
        states[index].fields.name = (AbUiPointerValue){
            .kind = AB_UI_POINTER_TEXT,
            .text = {(const uint8_t *)names[index], strlen(names[index])}};
        trees[index] = (AbUiFrameNode){.state = states + index,
                                       .children = children[index],
                                       .capacity = 3,
                                       .context = &context,
                                       .method = tree_method};
        menus[index] = (AbUiMenuNode){.frame = states + index,
                                      .identity = index + 1,
                                      .context = &context,
                                      .method = method};
        context.frames[index] = (AbUiMenuFrame){menus + index, trees + index, false};
    }
    AbUiMenuBoundaries boundaries = {.context = &context,
                                     .screen_size = {480, 320},
                                     .root_changed = notify_root,
                                     .reset_filtered_input = reset};
    AbUiMenuManager *manager;
    if (!ab_ui_menu_manager_open(8, 65536, &boundaries, &manager, NULL)) {
        return 1;
    }
    bool okay = exercise(&context, &boundaries);
    ab_ui_menu_manager_close(manager);
    return okay ? 0 : 1;
}
