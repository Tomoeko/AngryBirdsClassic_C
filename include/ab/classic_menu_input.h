#ifndef AB_CLASSIC_MENU_INPUT_H
#define AB_CLASSIC_MENU_INPUT_H

#include "ab/classic_touches.h"
#include "ab/integer_order.h"
#include "ab/ui_menu_manager.h"

typedef struct {
    const AbDataTable *pressed, *held, *released;
    AbVec2 cursor;
    AbDataValue wheel, extra_mouse_events, hover_scaling;
    /* Actual raw native table traversal order, not platform insertion order. */
    const AbClassicTouch *touches;
    size_t touch_count;
} AbClassicMenuRawInput;

typedef struct {
    AbClassicTouch *filtered, *copied;
    int32_t *keys;
    size_t capacity;
    AbIntegerOrderWorkspace order;
} AbClassicMenuInputStorage;

typedef struct {
    const AbClassicMenuRawInput *raw;
    AbClassicMenuInputStorage storage;
    AbUiMenuPointerCallback pointer;
    void *context;
    bool (*extra_mouse)(void *context, AbUiMenuManager *manager, AbError *error);
    bool (*hover)(void *context, AbUiMenuManager *manager, AbError *error);
} AbClassicMenuInputConfig;

typedef struct {
    const AbDataTable *pressed, *held, *released;
    AbDataValue wheel;
    const AbClassicTouch *touches;
    size_t count;
} AbClassicMenuInputView;

typedef struct AbClassicMenuInput {
    AbClassicMenuInputConfig config;
    AbDataEntry button[3];
    AbDataTable tables[3];
    AbDataValue wheel;
    size_t count;
    bool initialized, calling, failed;
} AbClassicMenuInput;

/* Concrete MenuManager resetFilteredInput11829/delegateClicks9119 ordinary
 * left-button/wheel binding, using the actual manager's current roots and input
 * order. Raw keys remain independent of the filtered tables, as in the target.
 * Touches project integer ID, x/y and scalar consumed; extra payload fields,
 * arbitrary numeric keys and retained Lua table identities are outside this
 * fixed Museum input domain. Fresh numeric deep-copy traversal uses the shared
 * native-matched table-order helper, twice at the two actual copy boundaries.
 *
 * All storage/raw key tables/touch data/providers remain borrowed. Storage
 * capacity covers both touch buffers and keys; order arrays obey integer_order.
 * Buffers must be disjoint from raw data and each other. Initialize at a stable
 * address and do not copy this owner. No allocations or platform input are
 * supplied. Reset invalidates previous views/copies. Failure preserves earlier
 * callback/table writes and stops the owner; reentry rejects independently.
 */
bool ab_classic_menu_input_initialize(AbClassicMenuInputConfig config,
                                      AbClassicMenuInput *output, AbError *error);
bool ab_classic_menu_input_reset(AbClassicMenuInput *input, AbError *error);
AbClassicMenuInputView ab_classic_menu_input_view(const AbClassicMenuInput *input);
bool ab_classic_menu_input_clicks(AbClassicMenuInput *input, AbUiMenuManager *manager,
                                  AbError *error);
/* Actual getFilteredTouches12140 copy. Valid until reset, another copy or close
 * of borrowed storage. Raw cancel callers use view().touches instead. */
bool ab_classic_menu_input_copy_touches(AbClassicMenuInput *input,
                                        const AbClassicTouch **output, size_t *count,
                                        AbError *error);

#endif
