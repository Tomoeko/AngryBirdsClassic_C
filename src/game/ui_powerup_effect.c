#include "ab/ui_powerup_effect.h"
#include "ab/script_math.h"

#include <math.h>
#include <string.h>

static AbUiPointerValue number(float value) {
    AbUiPointerValue output = {.kind = AB_UI_POINTER_NUMBER};
    memcpy(&output.number_bits, &value, sizeof(value));
    return output;
}

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static void scale(AbUiPowerupEffect *effect, float value) {
    effect->widget.frame->fields.scale_x = number(value);
    effect->widget.frame->fields.scale_y = number(value);
}

static bool lifecycle(void *context, AbUiWidget *widget, AbUiFrameMethod method,
                      AbError *error) {
    AbUiPowerupEffect *effect = context;
    if (!ab_ui_widget_lifecycle_base(widget, method, error))
        return false;
    if (method == AB_UI_FRAME_ENTRY) {
        effect->timer = 0;
        scale(effect, 8);
        uint32_t handle;
        return ab_audio_bank_play(
            effect->host.audio,
            (AbAudioPlay){
                .name = {.kind = AB_DATA_STRING, .string = span("powerup_intro")},
                .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                .loop = {.kind = AB_DATA_BOOLEAN}},
            &handle, error);
    }
    if (method == AB_UI_FRAME_LAYOUT) {
        widget->frame->fields.x = number(0.5f * effect->host.screen->x);
        widget->frame->fields.y = number(0.5f * effect->host.screen->y);
    }
    return true;
}

static bool emit(AbUiPowerupEffect *effect, AbSpan name, float amount, AbVec2 size,
                 AbError *error) {
    AbParticleEmission emission = {
        .definition = name,
        .amount = amount,
        .position = {effect->host.screen->x * 0.5f, effect->host.screen->y * 0.5f},
        .size = size,
        .ignore_limits = true,
        .alternate = true};
    return ab_particles_emit(effect->host.particles, &emission, error);
}

static bool update(void *context, AbUiWidget *widget, float seconds, float time,
                   AbError *error) {
    AbUiPowerupEffect *effect = context;
    if (!ab_ui_widget_update_base(widget, seconds, time, error))
        return false;
    effect->timer += seconds;
    float scaling;
    if (!isfinite(effect->timer) ||
        !ab_ui_scaling_get(effect->host.scaling, &scaling, error))
        return ab_fail(error, "Powerup effect timer exceeds finite domain");
    float timer = effect->timer;
    float value = (-7.0f * fminf(timer, 0.2f)) / 0.2f;
    value += 8;
    scale(effect, value * scaling);
    float shake = 0;
    if (timer >= 0.2f && timer <= 0.2f + 0.4f) {
        shake = fminf(timer, 0.2f + 0.4f) - 0.2f;
        shake = (0.4f - shake) / 0.4f;
        shake *= effect->host.screen->y / 768.0f;
        shake *= 30;
    }
    effect->host.shake->amplitude = shake;
    effect->host.shake->present |= AB_CAMERA_SHAKE_AMPLITUDE;
    if (timer >= 0.2f && !effect->triggered) {
        if (effect->host.menu_particle_scale)
            *effect->host.menu_particle_scale = 1;
        if (!emit(effect, span("powerupBurst"), 50, (AbVec2){0}, error) ||
            !emit(effect, span("powerupSmokeBuff"), 20, effect->particles_size,
                  error) ||
            !emit(effect, effect->particles, 1, (AbVec2){0}, error))
            return false;
        effect->triggered = true;
    }
    if (timer >= 1 && timer < 1.1f) {
        float angle = (timer - 1) * 10;
        angle *= 3.1415927410125732421875f;
        float sine;
        if (!ab_script_sine(angle, &sine))
            return ab_fail(error, "Powerup pulse exceeds sine domain");
        scale(effect, (1 + sine * 0.25f) * scaling);
    } else if (timer >= 1.1f) {
        value = (timer - 1.1f) * 6.666666507720947265625f;
        scale(effect, (1 - value) * scaling);
    }
    return true;
}

static bool draw(void *context, AbUiWidget *widget, const AbUiDrawInput *input,
                 AbError *error) {
    AbUiPowerupEffect *effect = context;
    return effect->host.draw_menu_particles(effect->host.context, error) &&
           ab_ui_widget_draw_base(widget, input, error);
}

bool ab_ui_powerup_effect_init(AbUiPowerupEffect *effect, AbUiWidgetContext *context,
                               AbUiPowerupEffectHost host, AbSpan sprite,
                               AbSpan particles, AbVec2 particles_size,
                               AbError *error) {
    if (!effect || !context || !host.scaling || !host.screen || !host.audio ||
        !host.particles || !host.shake || !host.draw_menu_particles || !sprite.data ||
        !sprite.size || !particles.data || !particles.size ||
        !isfinite(particles_size.x) || !isfinite(particles_size.y))
        return ab_fail(error, "Missing actual PowerupEffect owners");
    *effect = (AbUiPowerupEffect){
        .host = host, .particles = particles, .particles_size = particles_size};
    AbUiImageArguments arguments = {
        .image = {.kind = AB_UI_POINTER_TEXT, .text = sprite}};
    if (!ab_ui_widget_init(&effect->widget, context, AB_UI_WIDGET_IMAGE,
                           (AbUiWidgetChildren){0}, NULL, &arguments, NULL, error))
        return false;
    effect->widget.methods = (AbUiWidgetMethods){
        .context = effect, .lifecycle = lifecycle, .update = update, .draw = draw};
    return true;
}
