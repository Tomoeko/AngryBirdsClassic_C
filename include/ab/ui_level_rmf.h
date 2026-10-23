#ifndef AB_UI_LEVEL_RMF_H
#define AB_UI_LEVEL_RMF_H

#include "ab/ui_level_selection.h"

typedef struct {
    AbUiLevelSelection *levels;
    AbUiWidgetMethods inherited;
    AbUiWidget left, right;
    float slide_phase, tint_phase;
    uint32_t solid_texture;
    bool initialized;
} AbUiLevelRmf;

/* ep9_level_selection.lua's RMFLevelSelection over its actual unentered base
 * grid. Borrow the base graph, decoded decorations and renderer until its last
 * exit/draw. Own two alternate images and source slide/tint phases. Constructor
 * order, entry reset, base-first layout/update and pointer traversal are retained.
 * No allocation, settings defaults, page unlocking or scene substitution.
 * The enclosing root calls background AFTER its background and BEFORE children,
 * matching the source's scoped onBackgroundDrawn hook. This does not install the
 * full MainMenuRoot or the unused enclosing statistics/SDK services. */
bool ab_ui_level_rmf_attach(AbUiLevelRmf *owner, AbUiLevelSelection *levels,
                            uint32_t solid_texture, AbError *error);
bool ab_ui_level_rmf_background(AbUiLevelRmf *owner, AbError *error);

#endif
