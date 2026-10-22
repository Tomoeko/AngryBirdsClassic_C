#ifndef AB_UI_LEVEL_HIDDEN_EGGS_H
#define AB_UI_LEVEL_HIDDEN_EGGS_H

#include "ab/settings_golden_eggs.h"
#include "ab/ui_level_selection.h"

typedef enum {
    AB_UI_LEVEL_DANGER_ABOVE,
    AB_UI_LEVEL_MINE_AND_DINE,
} AbUiLevelHiddenEggKind;

typedef struct {
    AbUiLevelSelection *levels;
    AbSettingsStatistics *highscores;
    AbUiWidget egg;
    AbUiWidgetMethods inherited;
    AbUiLevelHiddenEggKind kind;
    AbDataTable event;
    AbDataEntry event_fields[2];
    float timer, start_scale;
    bool present, fading, initialized;
} AbUiLevelHiddenEggs;

/* Install the actual DangerAbove/MineAndDine subclass methods on a fresh,
 * unentered LevelSelection. Own stable caller storage until after root exit.
 * Resources, settings, event IDs and notification consumers are real borrowed
 * owners. Retain the source hidden-egg placement, discovery event and animation;
 * the app listener owns revealGoldenEgg, persistence and reward presentation.
 * No allocation, resource loading, unlock bypass or replacement scene. */
bool ab_ui_level_hidden_eggs_attach(AbUiLevelHiddenEggs *owner,
                                    AbUiLevelSelection *levels,
                                    AbSettingsStatistics *highscores,
                                    AbUiLevelHiddenEggKind kind, AbError *error);

#endif
