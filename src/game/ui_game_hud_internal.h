#ifndef AB_UI_GAME_HUD_INTERNAL_H
#define AB_UI_GAME_HUD_INTERNAL_H

#include "ab/ui_game_hud_controls.h"

bool ab_ui_game_hud_project(AbDataValue value, AbUiPointerValue *output,
                            AbError *error);
bool ab_ui_game_hud_query(AbUiGameHudControls *controls, AbUiGameHudQuery kind,
                          AbUiPointerValue *output, AbError *error);
bool ab_ui_game_hud_eagle_query(AbUiGameHudControls *controls, AbUiEagleQuery kind,
                                AbUiPointerValue *output, AbError *error);

#endif
