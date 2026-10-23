#ifndef AB_UI_POWERUP_EFFECT_H
#define AB_UI_POWERUP_EFFECT_H

#include "ab/audio_bank.h"
#include "ab/camera_shake.h"
#include "ab/particles.h"
#include "ab/ui_scaling.h"
#include "ab/ui_widget.h"

typedef struct {
    AbUiScaling *scaling;
    const AbVec2 *screen;
    AbAudioBank *audio;
    AbParticles *particles;
    AbCameraShake *shake;
    float *menu_particle_scale;
    void *context;
    bool (*draw_menu_particles)(void *context, AbError *error);
} AbUiPowerupEffectHost;

typedef struct {
    AbUiWidget widget;
    AbUiPowerupEffectHost host;
    AbSpan particles;
    AbVec2 particles_size;
    float timer;
    bool triggered;
} AbUiPowerupEffect;

/* PowerupEffect's source image lifecycle, pulse, shake and three menu particle
 * emissions. Own stable caller storage; borrow loaded resources and canonical
 * services. Enclosing NotificationsFrame owns the separate timed-child removal.
 * No allocation, asset loading, substitute effects or implicit frame schedule. */
bool ab_ui_powerup_effect_init(AbUiPowerupEffect *effect, AbUiWidgetContext *context,
                               AbUiPowerupEffectHost host, AbSpan sprite,
                               AbSpan particles, AbVec2 particles_size, AbError *error);

#endif
