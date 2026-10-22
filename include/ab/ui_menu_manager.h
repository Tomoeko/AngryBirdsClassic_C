#ifndef AB_UI_MENU_MANAGER_H
#define AB_UI_MENU_MANAGER_H

#include "ab/string_map.h"
#include "ab/ui_init.h"
#include "ab/ui_frame_tree.h"

typedef struct AbUiMenuManager AbUiMenuManager;
typedef struct AbUiMenuNode AbUiMenuNode;

typedef enum {
    AB_UI_MENU_PREPARE_DYNAMIC,
    AB_UI_MENU_ENTRY,
    AB_UI_MENU_EXIT,
    AB_UI_MENU_LAYOUT,
    AB_UI_MENU_UPDATE,
    AB_UI_MENU_DRAW,
    AB_UI_MENU_CLEAR_POINTER_FLAGS,
} AbUiMenuMethod;

struct AbUiMenuNode {
    AbUiFrameState *frame;
    uintptr_t identity;
    bool background_present;
    float background[4];
    void *context;
    /* UPDATE receives seconds/game_time; DRAW receives 0/0. Other methods
     * have no numeric arguments (represented here by 0/0). */
    bool (*method)(void *context, AbUiMenuNode *node, AbUiMenuMethod method,
                   float first, float second, AbError *error);
    /* Optional actual Frame tree for class-specific child queries. Its state
     * must be frame. Nonempty child state requires this binding when traversed. */
    AbUiFrameNode *tree;
};

typedef struct {
    void *context;
    AbVec2 screen_size;
    bool use_dynamic_assets;
    bool (*requirements)(void *context, AbUiMenuNode *menu, AbUiPointerValue *output,
                         AbError *error);
    bool (*load)(void *context, AbUiPointerValue requirements, AbError *error);
    bool (*delay_release)(void *context, AbUiPointerValue requirements, AbError *error);
    bool (*fullscreen_rectangle)(void *context, const float rgba[4], AbError *error);
    bool (*root_changed)(void *context, AbUiMenuNode *menu, AbSpan channel,
                         AbError *error);
    bool (*screen_clip)(void *context, AbVec2 origin, AbVec2 size, AbError *error);
    bool (*background_color)(void *context, const float rgb[3], AbError *error);
    bool (*reset_filtered_input)(void *context, AbUiMenuManager *manager,
                                 AbError *error);
    bool (*delegate_clicks)(void *context, AbError *error);
    bool (*delegate_keys)(void *context, AbError *error);
} AbUiMenuBoundaries;

typedef struct {
    const AbStringMap *menus, *queue, *links;
    const AbSpan *menu_order;
    size_t menu_count;
    AbVec2 screen_size;
    AbUiPointerValue allow_input, update_enabled, delegate_back_clicks;
    const AbSpan *input_order;
    size_t input_count;
} AbUiMenuManagerView;

/* Bounded owner for menu/current/queue/link dictionaries. Menu objects, frames,
 * order arrays, key bytes and providers remain borrowed. Maps allocate only at
 * open. Root channels are text (NULL means default "root"); roots are valid
 * objects or absent. Arbitrary malformed Lua root/key types, map mutation from
 * callbacks, allocation timing and full MenuManager constructor fields are
 * outside this controller contract. Explicit nil-key collection phases are
 * supported; original allocation/collection scheduling remains separate. */
bool ab_ui_menu_manager_open(size_t maximum_nodes, size_t allocation_limit,
                             const AbUiMenuBoundaries *boundaries,
                             AbUiMenuManager **output, AbError *error);
bool ab_ui_menu_manager_reset(AbUiMenuManager *manager,
                              const AbUiMenuBoundaries *boundaries, AbError *error);
void ab_ui_menu_manager_close(AbUiMenuManager *manager);
bool ab_ui_menu_manager_view(const AbUiMenuManager *manager,
                             AbUiMenuManagerView *output);
/* Explicit recovered table-collection boundary. Marks retained nil keys in
 * all three maps without inserting, deleting live values or rehashing. May run
 * during callbacks/iteration; collector scheduling remains a separate input. */
bool ab_ui_menu_manager_collect_nil_keys(AbUiMenuManager *manager);
bool ab_ui_menu_manager_order(AbUiMenuManager *manager, const AbSpan *order,
                              size_t count, AbError *error);
bool ab_ui_menu_manager_input_order(AbUiMenuManager *manager, const AbSpan *order,
                                    size_t count, AbError *error);
bool ab_ui_menu_manager_allow_input(AbUiMenuManager *manager, AbUiPointerValue value);
bool ab_ui_menu_manager_update_enabled(AbUiMenuManager *manager,
                                       AbUiPointerValue value);
bool ab_ui_menu_manager_delegate_back(AbUiMenuManager *manager, AbUiPointerValue value);

/* Builtin addLink/getLink/handleResult for pinned text result names and valid
 * menu objects or absent instances. A link with an absent instance still exists
 * and handleResult immediately deactivates its destination channel. A missing
 * link rejects before changing roots. Custom non-link records and generic Lua
 * property/table identities are outside this representation. */
bool ab_ui_menu_add_link(AbUiMenuManager *manager, AbSpan name, AbUiMenuNode *target,
                         AbError *error);
bool ab_ui_menu_get_link(const AbUiMenuManager *manager, AbSpan name,
                         AbUiMenuNode **output, AbError *error);
bool ab_ui_menu_handle_result(AbUiMenuManager *manager, AbSpan result,
                              const AbSpan *channel,
                              const AbUiMenuBoundaries *boundaries, AbError *error);

/* Exact original lifecycle order. setRoot assigns before prepare/entry/layout/
 * notification and does not exit an older root. Exit removes the current map
 * value only after exit/dynamic release. changeRoot queues non-absent roots;
 * absent roots exit immediately then nil-write the queue. getRoot selects a
 * queued root first; active inspects only current. setRootVisible ignores its
 * requested value and always sets the selected canonical frame visible=true.
 * Callback failures preserve earlier state writes/effects. No mutation allocates. */
bool ab_ui_menu_set_root(AbUiMenuManager *manager, AbUiMenuNode *menu,
                         const AbSpan *channel, const AbUiMenuBoundaries *boundaries,
                         AbError *error);
bool ab_ui_menu_exit_root(AbUiMenuManager *manager, const AbSpan *channel,
                          const AbUiMenuBoundaries *boundaries, AbError *error);
bool ab_ui_menu_change_root(AbUiMenuManager *manager, AbUiMenuNode *menu,
                            const AbSpan *channel, const AbUiMenuBoundaries *boundaries,
                            AbError *error);
bool ab_ui_menu_get_root(const AbUiMenuManager *manager, const AbSpan *channel,
                         AbUiMenuNode **output, AbError *error);
bool ab_ui_menu_is_active(const AbUiMenuManager *manager, const AbSpan *channel,
                          bool *output, AbError *error);
bool ab_ui_menu_set_root_visible(AbUiMenuManager *manager, const AbSpan *channel,
                                 AbUiPointerValue requested, AbError *error);

/* Process queued replacements in native hash order, then resize/layout and
 * update in menu_order. A processed queue suppresses input reset/delegation.
 * Draw sets logical clip, optional current-root raw RGB background, then draws
 * ordered menus at 0,0 without an extra visible/active gate. Resume/pause use
 * current-map physical order. Finite used geometry/time; stable borrowed maps,
 * keys, orders and methods during callbacks, except the explicit collection
 * metadata phase. Actual screen/widget methods remain provider boundaries. */
bool ab_ui_menu_update(AbUiMenuManager *manager, float seconds, float game_time,
                       const AbUiMenuBoundaries *boundaries, AbError *error);
bool ab_ui_menu_draw(AbUiMenuManager *manager, const AbUiMenuBoundaries *boundaries,
                     AbError *error);
bool ab_ui_menu_resume(AbUiMenuManager *manager, AbError *error);
bool ab_ui_menu_pause(AbUiMenuManager *manager, AbError *error);

typedef bool (*AbUiMenuPointerCallback)(void *context, AbUiMenuNode *menu, AbSpan event,
                                        AbVec2 point, AbUiPointerReply *reply,
                                        AbError *error);

/* Original delegatePointerEvent: traverse current roots in input_order, skipping
 * absent roots and continuing after absent/false primary replies. Stop at the
 * first truthy primary and return accepted=false; otherwise accepted=true.
 * Queue, allow_input, visibility/activity and auxiliary replies are unused.
 * Order/key/node storage remains borrowed and stable during callbacks. Finite
 * coordinates, supported primary values and valid event text are required.
 * No allocation. Failure preserves accepted and earlier callback effects. */
bool ab_ui_menu_pointer(AbUiMenuManager *manager, AbSpan event, AbVec2 point,
                        AbUiMenuPointerCallback callback, void *context, bool *accepted,
                        AbError *error);

#endif
