#ifndef AB_UI_EPISODE_BUTTON_H
#define AB_UI_EPISODE_BUTTON_H

#include "ab/random.h"
#include "ab/particles.h"
#include "ab/ui_scaling.h"
#include "ab/ui_text_widget.h"

typedef struct {
    bool normal, wrapped_selected;
} AbUiEpisodeUgcClass;

typedef struct {
    const AbDataTable *episodes;
    AbUiFontResources *fonts;
    AbUiScaling *scaling;
    AbRandom48 *random;
    bool pc_profile;
    /* Source module upvalues and settings are shared by actual UGC instances.
     * Particles are reached only when unwrapping a card with a parent. */
    AbUiEpisodeUgcClass *ugc_class;
    const AbDataTable *settings;
    AbParticles *particles;
    void *context;
    bool (*stars)(void *context, AbUiPointerValue episode, float *earned,
                  float *available, AbError *error);
    bool (*score)(void *context, AbUiPointerValue episode, float *output,
                  AbError *error);
    bool (*feathers)(void *context, AbUiPointerValue episode, float *earned,
                     float *available, AbError *error);
    bool (*show_eagle)(void *context, AbUiPointerValue episode, bool *output,
                       AbError *error);
    bool (*complete)(void *context, AbUiPointerValue episode, bool *output,
                     AbError *error);
    bool (*newest_change_seen)(void *context, AbUiPointerValue episode, bool *output,
                               AbError *error);
    bool (*has_new_levels)(void *context, AbUiPointerValue episode, bool *output,
                           AbError *error);
    bool (*new_levels_page)(void *context, AbUiPointerValue episode,
                            AbUiPointerValue *output, AbError *error);
    bool (*page_visited)(void *context, AbUiPointerValue episode, AbUiPointerValue page,
                         bool *output, AbError *error);
} AbUiEpisodeButtonHost;

typedef enum {
    AB_UI_EPISODE_STANDARD,
    AB_UI_EPISODE_EGG_DEFENDER,
    AB_UI_EPISODE_GOLDEN_EGGS,
    AB_UI_EPISODE_UGC,
} AbUiEpisodeButtonClass;

typedef struct {
    AbUiWidget widget;
    AbUiWidget lock, score_box, star_box, feather_box, new_sign, ribbon;
    AbUiTextWidget score, stars, feather_score;
    AbUiFrameNode *frames[9];
    AbUiDrawChild draw[9];
    AbUiPointerChild pointer[9];
    AbUiEpisodeButtonHost host;
    AbUiEpisodeButtonClass class_kind;
    AbUiWidget *reference_button;
    AbUiPointerValue episode, locked, disable_new_sign, new_timer;
    AbVec2 new_original_position;
    float gift_start, gift_end, gift_time, ribbon_time;
    bool gift_reverse, ribbon_reverse;
    uint8_t score_label[32], star_label[65], feather_label[65];
} AbUiEpisodeButton;

/* Concrete EpisodeButtonBase/EpisodeButton over actual ImageButton, Image and
 * Text children. Retain original names/tags/order, three draw phases, layout,
 * lock state, score refresh and new-sign pulse. Borrow the real catalog,
 * statistics/settings services, shared script RNG, Scaling and fonts. Providers
 * run only at their reached source calls; no scores, unlocks, settings or service
 * responses are supplied by this class. Profiles and class dependencies must
 * already be prepared by the owner. No allocation, script execution or I/O.
 * Use fresh stable storage; reached failures retain effects and stop context. */
bool ab_ui_episode_button_init(AbUiEpisodeButton *button, AbUiWidgetContext *context,
                               const AbUiImageButtonArguments *arguments,
                               AbUiEpisodeButtonHost host, AbError *error);
/* Actual declared subclasses, including GoldenEggButton's three-child graph
 * and Egg Defender's additional screen-relative new-sign placement. */
bool ab_ui_episode_button_init_class(AbUiEpisodeButton *button,
                                     AbUiWidgetContext *context,
                                     const AbUiImageButtonArguments *arguments,
                                     AbUiEpisodeButtonHost host,
                                     AbUiEpisodeButtonClass class_kind, AbError *error);
bool ab_ui_episode_button_set_episode(AbUiEpisodeButton *button,
                                      AbUiPointerValue episode,
                                      const AbDataTable *metadata, AbError *error);
bool ab_ui_episode_button_set_open(AbUiEpisodeButton *button, AbUiPointerValue open,
                                   AbError *error);
bool ab_ui_episode_button_update_scores(AbUiEpisodeButton *button, AbError *error);
bool ab_ui_episode_button_new_sign(AbUiEpisodeButton *button, AbError *error);

#endif
