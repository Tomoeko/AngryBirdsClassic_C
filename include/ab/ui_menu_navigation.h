#ifndef AB_UI_MENU_NAVIGATION_H
#define AB_UI_MENU_NAVIGATION_H

#include "ab/ui_frame_tree.h"
#include "ab/ui_menu_manager.h"

typedef struct {
    AbUiMenuNode *menu;
    AbUiFrameNode *tree;
    bool remove_from_frame;
} AbUiMenuFrame;

typedef enum {
    AB_UI_MENU_EVENT_OTHER,
    AB_UI_MENU_EVENT_CHANGE_SCENE,
    AB_UI_MENU_EVENT_PUSH_FRAME,
    AB_UI_MENU_EVENT_POP_FRAME,
} AbUiMenuEventKind;

typedef enum {
    AB_UI_MENU_TARGET_ABSENT,
    AB_UI_MENU_TARGET_FRAME,
    AB_UI_MENU_TARGET_LINK,
} AbUiMenuTargetKind;

typedef struct {
    AbUiMenuTargetKind kind;
    AbUiMenuNode *frame;
    AbSpan link;
} AbUiMenuTarget;

typedef struct {
    AbUiMenuManager *manager;
    AbUiMenuFrame *frames;
    size_t frame_count;
    void *context;
    bool (*frame_pushed)(void *context, AbUiMenuNode *frame, AbSpan channel,
                         AbError *error);
    bool (*pop_requested)(void *context, AbError *error);
} AbUiMenuNavigation;

/* Borrowed canonical menu/frame bindings. Each menu/frame state is the same
 * object, with unique stable text child names and bounded caller-owned arrays.
 * Compose original menu events with the recovered Frame child operations.
 * Original duplicate/malformed child-table assertion prefixes, arbitrary Lua
 * target values and custom link records are outside this contract. Callback
 * effects and earlier writes survive failure; nothing allocates or owns nodes. */
bool ab_ui_menu_event(AbUiMenuNavigation *navigation, AbUiMenuEventKind event,
                      AbUiMenuTarget target, const AbSpan *channel,
                      const AbUiMenuBoundaries *boundaries, AbError *error);
bool ab_ui_menu_pop_frame(AbUiMenuNavigation *navigation, const AbSpan *channel,
                          AbUiFrameNode **output, AbError *error);

/* The original assigned removeFromFrame closure has no captured channel or
 * arguments. Calling it emits only EID_POP_FRAME; event delivery is a provider
 * boundary. The binding retains this installed callback after detach/reset. */
bool ab_ui_menu_frame_remove(const AbUiMenuNavigation *navigation,
                             const AbUiMenuFrame *frame, AbError *error);

#endif
