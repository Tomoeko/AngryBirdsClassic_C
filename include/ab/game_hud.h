#ifndef AB_GAME_HUD_H
#define AB_GAME_HUD_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    float screen_width;
    AbVec2 margin;
    float scale;
    float score;
    float highscore;
    float font_leading;
    float measured_score_width;
    float previous_score_width;
    bool previous_width_present;
    bool retina;
    bool highscore_present;
    bool show_score;
    bool hide_scores;
    bool update_mode;
    bool tutorial_active;
} AbGameHudInput;

typedef enum {
    AB_HUD_FONT_BASIC,
    AB_HUD_FONT_SCORE,
} AbGameHudFont;

typedef enum {
    AB_HUD_SCORE_LABEL,
    AB_HUD_SCORE_VALUE,
    AB_HUD_HIGHSCORE_LABEL,
    AB_HUD_HIGHSCORE_VALUE,
} AbGameHudText;

typedef enum {
    AB_HUD_SET_FONT,
    AB_HUD_SET_STATE,
    AB_HUD_DRAW_WORLD,
    AB_HUD_DRAW_TEXT,
    AB_HUD_DRAW_CHILDREN,
} AbGameHudCallKind;

typedef struct {
    AbGameHudCallKind kind;
    AbGameHudFont font;
    AbVec2 position;
    AbVec2 scale;
    AbGameHudText text;
    int32_t number;
} AbGameHudCall;

enum { AB_GAME_HUD_CALLS = 17 };
typedef struct {
    AbGameHudCall calls[AB_GAME_HUD_CALLS];
    size_t count;
    float next_score_width;
} AbGameHudPlan;

/* Ordinary GameHud.draw score branch with no ads, eagle mode, powerup score or
 * floating scores. Font metrics/scale and current/high scores are resolved
 * caller inputs. The plan retains ordered font/state/text/world/child boundaries;
 * children retain the caller's original parent arguments. Text uses TEXTS_BASIC
 * and TOP/RIGHT anchors; label localization and number formatting are separate.
 * Four-argument state calls update translation/scales only, preserving angle,
 * pivot and alpha. Width history updates even with hidden scores.
 * All used numbers/intermediates/results must be finite. Visible numeric scores
 * must be in [0, 2147483648), truncating toward zero as the target's %d boundary.
 * No allocation, callbacks or resource reads. Failure preserves output. */
bool ab_game_hud_plan(const AbGameHudInput *input, AbGameHudPlan *output,
                      AbError *error);

#endif
