#ifndef AB_FLAME_EFFECT_H
#define AB_FLAME_EFFECT_H

#include "ab/base.h"
#include "ab/physics_geometry.h"
#include "ab/random.h"

typedef struct {
    AbSpan sprite;
    AbVec2 point;
} AbFlameOffset;

typedef struct {
    const AbSpan *sprites;
    size_t count;
    const AbFlameOffset *offsets;
    size_t offset_count;
    AbVec2 default_offset;
    bool default_present;
} AbFlameDefinition;

typedef struct {
    float timer, timer_speed, scale;
    float new_angle, old_angle, angle;
    AbSpan sprite;
    AbVec2 offset;
    bool sprite_present, angle_present;
} AbFlameState;

typedef struct {
    float seconds, body_angle;
    AbVec2 velocity;
    AbSpan object_sprite;
    bool object_sprite_present;
} AbFlameStep;

/* Resolved FlameEffect per-object update. Carry the timer by one period only,
 * select floor(timer * sprite_count) + 1 with native float arithmetic, preserve
 * absent out-of-range sprites, then compute the velocity-driven/jittered angle.
 * Consume one shared Lua-unit random draw on every valid update, including zero
 * elapsed time. Keep one shortest-angle correction and min(1,5*seconds) smoothing.
 * Select the object's sprite-specific offset, then default, then positive zero.
 * Definition strings/maps remain borrowed and immutable; original table alias
 * mutations are outside this resolved view. Finite used arithmetic is required;
 * scale and the previous new/old-angle values are unused and retained/overwritten
 * as observed. No body/resource effects, draw submission, Lua or allocation.
 * Failure preserves state; a late failure may retain a consumed random draw.
 */
bool ab_flame_update(AbFlameState *state, const AbFlameDefinition *definition,
                     const AbFlameStep *step, AbRandom48 *random, AbError *error);

typedef struct {
    AbSpan sprite;
    AbVec2 position;
    float scale, angle;
    bool sprite_present, remove_callback;
} AbFlameDraw;

/* Resolve the original extra-draw callback into one sprite submission, or its
 * removal when the world entry is nil/false. A missing entry ignores state and
 * numeric inputs. Otherwise scale the offset, rotate it by new_angle with Lua's
 * rounded double trig results, then add the object's script position. Sprite
 * angle is the independently smoothed angle. No RNG, resource or renderer calls.
 * Finite used arithmetic and a present numeric angle are required. The script
 * trig domain is |new_angle| <= 1024. Output is unchanged on failure.
 */
bool ab_flame_draw(const AbFlameState *state, bool object_present,
                   AbVec2 object_position, float body_scale, AbFlameDraw *output,
                   AbError *error);

typedef struct {
    const AbFlameDefinition *definition;
    const AbFlameStep *body;
    const float *z_order;
    float timer_speed, scale;
} AbFlameCreation;

typedef struct {
    void *context;
    bool (*register_update)(void *context, AbFlameState *state,
                            const AbFlameDefinition *definition, AbError *error);
    bool (*register_draw)(void *context, AbFlameState *state, float z_order,
                          AbError *error);
} AbFlameHost;

/* Create/replace persistent per-object state, register its update, invoke one
 * zero-time update, then register its extra draw at the current z_order + 1.
 * The caller's host retains the state/definition and object identity as needed;
 * all borrowed storage must outlive those registrations. body and z_order are
 * read after the update-registration callback, so its edits remain observable.
 * No implicit disposal callback. A late error retains preceding state, random
 * draws and host effects, matching the incremental construction boundary.
 */
bool ab_flame_apply(AbFlameState *state, const AbFlameCreation *creation,
                    AbRandom48 *random, AbFlameHost host, AbError *error);

#endif
