#include "ab/hero_screen_tinter.h"

bool ab_hero_screen_tinter_init(AbHeroScreenTinter *tinter) {
    if (!tinter)
        return false;
    *tinter = (AbHeroScreenTinter){.finished = true};
    return true;
}

bool ab_hero_screen_tinter_update(AbHeroScreenTinter *tinter, float seconds,
                                  AbError *error) {
    if (!tinter || tinter->updating)
        return ab_fail(error, "Invalid or reentrant hero screen tinter");
    tinter->updating = true;
    bool okay = true;
    if (tinter->tween) {
        AbHeroTintTween *current = tinter->tween;
        if (!current->update) {
            okay = ab_fail(error, "Missing actual hero tint tween update");
        } else {
            okay = current->update(current->context, seconds, error);
        }
        if (okay) {
            current = tinter->tween;
            okay =
                current && current->update_values
                    ? current->update_values(current->context, error)
                    : ab_fail(error, "Missing current hero tint updateValues method");
        }
    }
    if (okay && tinter->finished)
        tinter->tween = NULL;
    tinter->updating = false;
    return okay;
}
