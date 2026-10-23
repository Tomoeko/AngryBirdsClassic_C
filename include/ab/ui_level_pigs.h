#ifndef AB_UI_LEVEL_PIGS_H
#define AB_UI_LEVEL_PIGS_H

#include "ab/particles.h"
#include "ab/ui_level_selection.h"

typedef struct {
    AbRandom48 *random;
    void *context;
    bool (*assets)(void *context, bool loading, AbError *error);
    bool (*emit)(void *context, const AbParticleEmission *emission, AbError *error);
    bool (*draw_particles)(void *context, AbError *error);
    bool has_web_browser, sugotoku, disable_cross_sell, talkweb_allowed;
    bool cross_promotion_disabled, freemium_china;
} AbUiLevelPigsHost;

typedef struct {
    AbUiLevelSelection *levels;
    AbUiWidgetMethods inherited;
    AbUiLevelPigsHost host;
    AbUiWidget pig, renderer;
    AbDataEntry event_field;
    AbDataTable event;
    char sprites[3][4][48];
    AbVec2 origin;
    float timer, animation_timer, particle_timer;
    size_t animation;
    bool initialized, present;
} AbUiLevelPigs;

/* Pinned BadPiggiesLevelSelection init1751/entry2403/exit2753/pointer3012/
 * layout3273/update3608 and particle Frame.draw1614. Wrap an unentered base;
 * keep source child order, feature gates, shared script RNG, four-frame art,
 * entrance tween, bob/rotation and actual alternate-list rocket emissions.
 * Resources, particle storage/native draw and enclosing service events remain
 * explicit host boundaries. The host loads GREEN_UPSELL at entry and releases
 * it before clearing menu particles at exit. No frame allocation or VM.
 * Caller owns stable zeroed storage through base detach and scene departure. */
bool ab_ui_level_pigs_attach(AbUiLevelPigs *owner, AbUiLevelSelection *levels,
                             AbUiLevelPigsHost host, AbError *error);

#endif
