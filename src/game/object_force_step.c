#include "ab/object_force_step.h"

static bool request_effect(const AbObjectForceStep *step, AbObjectForceEffect effect,
                           AbSpan name, float gain, AbError *error) {
    if (!step->host) {
        return ab_fail(error, "Missing required object force resource host");
    }
    const AbObjectForceCall call = {effect, name, gain};
    return step->host(step->context, &call, error);
}

static bool update_interval(const AbObjectForceStep *step, float seconds, bool *pulse,
                            AbError *error) {
    AbObjectForceInterval *state = step->interval;
    AbObjectForceIntervalPlan plan;
    if (!ab_object_force_interval_plan(state, seconds, false, &plan, error)) {
        return false;
    }
    if (plan.pulse) {
        state->original_sprite = plan.next.original_sprite;
        state->original_present = true;
    }
    if (plan.sprite_requested) {
        state->sprite = plan.next.sprite;
        if (!request_effect(step, AB_FORCE_EFFECT_SPRITE, state->sprite, 0, error)) {
            return false;
        }
    }
    if (plan.pulse && step->sound) {
        AbObjectForceSoundPlan sound;
        if (!ab_object_force_sound_plan(step->sound, true, step->random, &sound,
                                        error) ||
            (sound.play && !request_effect(step, AB_FORCE_EFFECT_AUDIO, sound.name,
                                           sound.gain, error))) {
            return false;
        }
    }
    state->timer = plan.next.timer;
    state->timer_present = true;
    *pulse = plan.pulse;
    return true;
}

bool ab_object_force_step(AbBodyMotion *body, const AbObjectForceStep *step,
                          float seconds, AbError *error) {
    if (!body || !step || !step->forces) {
        return ab_fail(error, "Missing object force step state");
    }
    const bool disabled = step->forces->disabled;
    AbObjectForces gravity = *step->forces;
    gravity.gravity = step->gravity;
    if (step->gravity_enabled) {
        gravity.gravity_enabled = *step->gravity_enabled;
    }
    gravity.disabled = true;
    if (!ab_object_forces_apply(body, &gravity, error)) {
        return false;
    }
    if (disabled) {
        return true;
    }
    bool pulse = false;
    if (step->interval) {
        if (!update_interval(step, seconds, &pulse, error)) {
            return false;
        }
        if (!pulse) {
            return true;
        }
    }
    AbObjectForces force = *step->forces;
    force.disabled = false;
    force.gravity_enabled = true;
    force.impulse = pulse;
    return ab_object_forces_apply(body, &force, error);
}
