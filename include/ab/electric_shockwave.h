#ifndef AB_ELECTRIC_SHOCKWAVE_H
#define AB_ELECTRIC_SHOCKWAVE_H

#include "ab/bird_shockwave.h"
#include "ab/random.h"

typedef enum {
    AB_ELECTRIC_SPARK,
    AB_ELECTRIC_RAY,
    AB_ELECTRIC_INNER,
    AB_ELECTRIC_OUTER,
    AB_ELECTRIC_BIRD,
    AB_ELECTRIC_CHARGING_BIRD,
    AB_ELECTRIC_SHOCKED_BIRD,
    AB_ELECTRIC_SPRITES
} AbElectricSprite;

typedef struct {
    AbElectricSprite sprite;
    AbVec2 position;
    float scale, angle, alpha;
} AbElectricSpriteDraw;

typedef struct {
    float targets[6], values[6], velocities[6];
    float outer_timer, inner_timer, lightning_timer;
    float outer_angle, inner_angle, lightning_angle;
    float offset_x, offset_y, scale_offset, alpha_offset;
    AbElectricSpriteDraw sparks[10];
    size_t spark_count, bird_index;
    bool bird_visible;
} AbElectricShockwave;

/* ElectricShockwave.lua init2364/update3671/draw6500 in the pinned APK.
 * Keep its shared script RNG consumption, including init's zero-time update
 * and draw-time orientation changes. The caller supplies the pre-advance wave
 * to update and post-advance wave to draw. A puff request is the actual pair of
 * smokeBuff/blackBuff emissions; the application owns those engine effects.
 * No resource lookup, registry mutation or allocation. Sprite names are stable
 * original lookup keys. Native whole-effect capture comparison remains open. */
AbSpan ab_electric_shockwave_sprite(AbElectricSprite sprite);
bool ab_electric_shockwave_init(AbElectricShockwave *effect, AbRandom48 *random,
                                AbError *error);
bool ab_electric_shockwave_update(AbElectricShockwave *effect,
                                  const AbBirdShockwave *wave, float visual_scale,
                                  float seconds, AbRandom48 *random, bool *puff,
                                  AbError *error);
bool ab_electric_shockwave_draw(AbElectricShockwave *effect,
                                const AbBirdShockwave *wave, AbVec2 position,
                                float visual_scale, float bird_scale, float bird_angle,
                                AbRandom48 *random, AbElectricSpriteDraw *draws,
                                size_t capacity, size_t *count, AbError *error);

#endif
