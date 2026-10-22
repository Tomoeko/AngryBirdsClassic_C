#ifndef AB_HERO_SCREEN_TINTER_H
#define AB_HERO_SCREEN_TINTER_H

#include "ab/base.h"

typedef struct {
    void *context;
    bool (*update)(void *context, float seconds, AbError *error);
    bool (*update_values)(void *context, AbError *error);
} AbHeroTintTween;

typedef struct {
    AbHeroTintTween *tween;
    bool finished, updating;
} AbHeroScreenTinter;

/* Fresh HeroBirdScreenTinter module state: nil tween and finished=true.
 * Initialize has no allocation, output, graphics or RNG effects. */
bool ab_hero_screen_tinter_init(AbHeroScreenTinter *tinter);

/* Original update prototype 265: invoke the current tween's update(seconds),
 * reread the shared tween for updateValues(), then clear it if the current
 * finished flag is true. Callbacks may replace tween/finished; preserve their
 * borrowed stable storage until each callback returns. Missing reached methods
 * fail after retaining earlier effects. An absent tween ignores seconds.
 *
 * This binds the update phase, including its real initial inactive gate.
 * Starting a tint requires the actual Tweener factory/descriptors, listener and
 * overlay owner; those are separate work. No dummy active tween or tint result
 * is supplied. No allocation, RNG or implicit numeric conversion. Serialize
 * calls and stop the enclosing frame after failure; callbacks cannot reenter.
 */
bool ab_hero_screen_tinter_update(AbHeroScreenTinter *tinter, float seconds,
                                  AbError *error);

#endif
