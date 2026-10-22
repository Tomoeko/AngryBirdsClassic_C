#ifndef AB_TEST_CLASSIC_GAME_MENU_INPUT_H
#define AB_TEST_CLASSIC_GAME_MENU_INPUT_H

#include "ab/classic_menu_input.h"
#include "ab/ui_menu_keys.h"

typedef struct {
    AbClassicMenuInput input;
    AbClassicMenuRawInput raw;
    AbUiMenuManager *manager;
    AbUiMenuBoundaries boundaries;
    AbUiMenuKeyLayout layout;
    unsigned phases;
} LifecycleGameMenuInput;

static bool lifecycle_menu_reset(void *context, AbUiMenuManager *manager,
                                 AbError *error) {
    LifecycleGameMenuInput *menu = context;
    (void)manager;
    return ab_classic_menu_input_reset(&menu->input, error);
}

static bool lifecycle_menu_clicks(void *context, AbError *error) {
    LifecycleGameMenuInput *menu = context;
    ++menu->phases;
    return ab_classic_menu_input_clicks(&menu->input, menu->manager, error);
}

static bool lifecycle_menu_keys(void *context, AbError *error) {
    LifecycleGameMenuInput *menu = context;
    ++menu->phases;
    /* This controlled empty-root setup has no reached key recipient. Actual
     * pointer/root/key routing has separate original corpora. */
    return ab_ui_menu_keys(menu->manager, &menu->layout,
                           &(AbUiMenuKeyBoundaries){.device = AB_UI_KEY_DEVICE_OTHER},
                           error);
}

/* Controlled empty raw input through the real reset/update/click/key owners.
 * Does not construct the actual GameHud tree or claim its selected bootstrap. */
static bool lifecycle_menu_open(AbClassicGameInput *input,
                                AbClassicMenuInputStorage storage,
                                LifecycleGameMenuInput *menu, AbError *error) {
    *menu = (LifecycleGameMenuInput){0};
    menu->raw = (AbClassicMenuRawInput){.pressed = input->config.pressed,
                                        .held = input->config.held,
                                        .released = input->config.released,
                                        .cursor = input->config.state->cursor};
    CHECK(ab_classic_menu_input_initialize(
              (AbClassicMenuInputConfig){.raw = &menu->raw, .storage = storage},
              &menu->input, error) &&
          ab_ui_menu_key_layout(&menu->layout, error));
    menu->boundaries =
        (AbUiMenuBoundaries){.context = menu,
                             .screen_size = {480, 320},
                             .reset_filtered_input = lifecycle_menu_reset,
                             .delegate_clicks = lifecycle_menu_clicks,
                             .delegate_keys = lifecycle_menu_keys};
    CHECK(ab_ui_menu_manager_open(8, 32768, &menu->boundaries, &menu->manager, error) &&
          ab_ui_menu_update(menu->manager, 0, 4, &menu->boundaries, error) &&
          menu->phases == 2 && !ab_classic_menu_input_view(&menu->input).count);
    AbClassicGameInputConfig config = input->config;
    config.menu_input = &menu->input;
    return ab_classic_game_input_initialize(config, input, error);
}

#endif
