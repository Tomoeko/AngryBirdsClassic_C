#ifndef AB_UI_HUD_POWERUPS_H
#define AB_UI_HUD_POWERUPS_H

#include "ab/powerup_catalog.h"
#include "ab/ui_slider.h"
#include "ab/ui_text_widget.h"

enum { AB_UI_HUD_POWERUPS = 6, AB_UI_HUD_POWERUP_CHILDREN = 7 };

typedef struct {
    AbUiWidget button, background, tag;
    AbUiTextWidget counter;
    AbUiSliderChild slider;
    AbUiFrameNode *frames[2], *background_frames[1];
    AbUiDrawChild draw[2], background_draw[1];
    AbUiPointerChild pointer[2], background_pointer[1];
    AbDataTable meta, tag_meta;
    AbDataEntry meta_fields[2], tag_meta_fields[2];
    const AbPowerupMetadata *metadata;
    char name[32], inactive_image[128];
    uint8_t settings_name[64];
    bool present, has_counter;
} AbUiHudPowerup;

typedef struct {
    AbUiSliderHost slider;
    AbUiFontResources *fonts;
    const AbPowerupCatalog *catalog;
    /* Actual registered records' enabled fields, in catalog activation order.
     * Keep this array live and stable for later layout passes. */
    const AbUiPointerValue *registered_enabled;
    AbSpan device_model;
    AbUiPointerValue global_enabled;
} AbUiHudPowerupsHost;

typedef struct {
    AbUiPointerValue visible;
    float birds_shot, bird_count;
    bool plus_signs, trial;
} AbUiHudPowerupsSource;

typedef struct {
    AbUiSlider slider;
    AbUiHudPowerup powerups[AB_UI_HUD_POWERUPS];
    AbUiWidget shop;
    AbUiSliderChild shop_item, *items[AB_UI_HUD_POWERUP_CHILDREN];
    AbUiFrameNode *frames[AB_UI_HUD_POWERUP_CHILDREN + 2];
    AbUiDrawChild draw[AB_UI_HUD_POWERUP_CHILDREN + 2];
    AbUiPointerChild pointer[AB_UI_HUD_POWERUP_CHILDREN + 2];
    AbDataTable shop_meta;
    AbDataEntry shop_meta_fields[2];
    AbUiHudPowerupsHost host;
    size_t count;
    bool shop_present, hide_powerups, initialized;
} AbUiHudPowerups;

/* GameHud.init's ordinary powerup slider subtree, including actual counter Text,
 * background and tag controls. Own fresh stable caller storage; borrow the real
 * registration metadata/enabled array, decoded fonts/resources and live slider
 * host. The parent argument is the actual HUD; init adds the slider before its
 * content. Source fields are resolved
 * current feature/visibility/bird values, not synthesized SDK or manager results.
 *
 * Preserve registration indices in names/meta, ~=false bar filtering, enabled
 * counter construction, source child order, images, touch padding and the
 * no-birds disable branch. Shop controls exist only for plus_signs && !trial.
 * This does not initialize PowerupManager, inventory, purchases, actions, other
 * HUD controls or the complete GameHud. No allocation, I/O, VM or frame defaults.
 * Reached failure preserves earlier effects and stops the shared widget context.
 */
bool ab_ui_hud_powerups_init(AbUiHudPowerups *owner, AbUiWidget *hud,
                             AbUiHudPowerupsHost host, AbUiHudPowerupsSource source,
                             AbError *error);
/* GameHud.layout's slider/counter subsection. Borrow the actual refreshed pause
 * button and resolved live margin/eagle visibility after its source disability
 * check. No parent layout or unrelated HUD effects are substituted here. */
bool ab_ui_hud_powerups_layout(AbUiHudPowerups *owner, AbUiWidget *pause, AbVec2 margin,
                               bool eagle_visible, AbError *error);
/* Actual PowerupButton.setEnabled, including its reached bg child refresh. */
bool ab_ui_hud_powerup_enable(AbUiHudPowerup *button, AbUiPointerValue enabled,
                              AbError *error);
/* Shared GameHud.addPowerupCounter for an already constructed real button with
 * its actual two-field metadata. Own fresh counter/tag storage in button and
 * reserve two additional child slots in its widget. The same constructor serves
 * the bar buttons and the separate black-bird control. */
bool ab_ui_hud_powerup_add_counter(AbUiHudPowerup *button, AbUiFontResources *fonts,
                                   AbSpan device_model, AbError *error);

#endif
