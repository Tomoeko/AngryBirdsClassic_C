#ifndef AB_UI_MENU_KEYS_H
#define AB_UI_MENU_KEYS_H

#include "ab/ui_menu_manager.h"

typedef enum {
    AB_UI_KEY_PRESS,
    AB_UI_KEY_HOLD,
    AB_UI_KEY_RELEASE,
} AbUiKeyPhase;

typedef enum {
    AB_UI_KEY_DEVICE_OTHER,
    AB_UI_KEY_DEVICE_WINDOWS,
    AB_UI_KEY_DEVICE_OSX,
    AB_UI_KEY_DEVICE_ROKU,
} AbUiKeyDevice;

typedef struct {
    size_t desktop_order[26], roku_order[11];
    bool ready;
} AbUiMenuKeyLayout;

typedef struct {
    void *context;
    AbUiKeyDevice device;
    bool show_editor;
    bool (*down)(void *context, AbUiKeyPhase phase, AbSpan key, bool *output,
                 AbError *error);
    bool (*event)(void *context, AbUiMenuNode *menu, AbSpan phase, AbSpan key,
                  AbUiPointerReply *reply, AbError *error);
} AbUiMenuKeyBoundaries;

/* Prepare the two fixed native key-map traversal orders once using the
 * recovered constructor capacities (32 desktop slots,16 Roku slots). This is
 * bounded data ordering, with no VM or allocation. Layout remains caller-owned. */
bool ab_ui_menu_key_layout(AbUiMenuKeyLayout *output, AbError *error);

/* Original delegateKeyEvents over current roots and independent input_order.
 * Both allowInput and delegateBackClicks, plus a current root channel, gate the
 * whole operation. Four release keys route until a truthy primary. Desktop/
 * editor keys go directly to the current root, with control/paste and native
 * map order; Roku preserves held-direction and press-before-release behavior.
 * Providers project raw key/global truthiness and retain stable borrowed
 * manager maps/order during callbacks. No allocation; prior effects survive a
 * callback failure. Actual platform key collection and widget methods remain
 * provider boundaries. Layout/output storage must not alias live manager data. */
bool ab_ui_menu_keys(AbUiMenuManager *manager, const AbUiMenuKeyLayout *layout,
                     const AbUiMenuKeyBoundaries *boundaries, AbError *error);

#endif
