#ifndef AB_CLASSIC_ROOT_QUERIES_H
#define AB_CLASSIC_ROOT_QUERIES_H

#include "ab/lua_data.h"
#include "ab/ui_menu_manager.h"

typedef struct {
    const AbDataTable *unlock_status, *episodes;
    const AbDataValue *current_episode, *clicked_from;
} AbClassicEagleQuery;

/* Source isSafeToQuit9757 over the actual C MenuManager root and Frame tree.
 * No manager/root permits quitting. mainMenu/mainMenuRoot permit quitting only
 * with no direct child whose raw requireInput is truthy; visibility is ignored.
 * ForceUpdatePopup permits quitting. Other root names do not. Stable, dense,
 * valid builtin roots/children only; overridden getters/metatables excluded.
 * Empty source children may use no tree; nonempty children require its actual
 * shared state and dense storage. No allocation. Preserve output on failure. */
bool ab_classic_safe_to_quit(const AbUiMenuManager *menus, bool *output,
                             AbError *error);
/* Source isFreeEagleEnabled159329. Typed status records and episode metadata
 * stay live/borrowed. All nonnil status entries must be valid record tables;
 * within that profile their iteration order cannot change the Boolean result.
 * A truthy enabled record wins first, otherwise use the current episode's
 * mightyPower.free only when clicked_from differs from literal MAIN_MENU.
 * Absent/false current episode skips its lookup; numeric zero remains truthy.
 * No allocation, coercion, arbitrary table methods or mutable iteration. */
bool ab_classic_free_eagle(const AbClassicEagleQuery *query, bool *output,
                           AbError *error);

/* Source NotificationsFrame.isAnyPausingChildVisible3363. Direct children
 * need both raw visible and notificationsFramePause truthy; active is ignored. */
bool ab_classic_notifications_pausing(const AbUiFrameNode *notifications, bool *output,
                                      AbError *error);
/* Source LevelStartScreenFlow.currentlyIngame3634. Root must be gameHud AND
 * have an actual pausePage child whose visible is falsy. An absent root or
 * pausePage returns raw nil; other results are Boolean. Only then query the
 * actual global notifications tree. Stable
 * builtin MenuManager/Frame getters and dense trees only; no overrides or
 * concurrent mutation. Missing reached owners reject. No allocation; preserve
 * output on failure. */
bool ab_classic_level_start_ingame(const AbUiMenuManager *menus,
                                   const AbUiFrameNode *notifications,
                                   AbDataValue *output, AbError *error);

/* Source subsystemsapi.isPauseMenuVisible985. Absent manager/root/pausePage
 * returns false. The child's class-owned show must be literal Boolean true;
 * visible, numeric flags and the root name do not affect this query. */
bool ab_classic_pause_menu_visible(const AbUiMenuManager *menus, bool *output,
                                   AbError *error);
/* Source MuseumLevelFlow.isInLevel128. Absent root returns raw nil; a gameHud
 * root returns NOT the pause query above; other names return Boolean false.
 * The source requires an actual menuManager. Pure builtin getters, valid dense
 * Frame trees and class-owned show only. Preserve output on reached failure. */
bool ab_classic_museum_in_level(const AbUiMenuManager *menus, AbDataValue *output,
                                AbError *error);

#endif
