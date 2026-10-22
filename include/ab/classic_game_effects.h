#ifndef AB_CLASSIC_GAME_EFFECTS_H
#define AB_CLASSIC_GAME_EFFECTS_H

#include "ab/burn_manager.h"
#include "ab/camera_shake.h"
#include "ab/classic_trajectories.h"
#include "ab/hero_screen_tinter.h"

typedef struct {
    AbCameraShake *shake;
    AbRandom48 *random;
    AbHeroScreenTinter *hero;
    AbClassicTrajectories *trajectories;
    AbBurnManager *burn;
} AbClassicGameEffectConfig;

typedef struct {
    AbClassicGameEffectConfig config;
    bool initialized, updating, failed;
} AbClassicGameEffects;

/* Borrow actual initialized owners and shared RNG at stable addresses. The
 * binding is caller-owned and initializes without effects/allocation. Output
 * preserves its prior contents on failure. Do not replace/free borrowed owners
 * or reenter from their callbacks. */
bool ab_classic_game_effects_initialize(AbClassicGameEffectConfig config,
                                        AbClassicGameEffects *output, AbError *error);

/* Contiguous GameUpdate prototype 110400 instructions 62..79: camera shake,
 * HeroBirdScreenTinter.update, TrajectoryRenderer.update, BurnManager.update.
 * Run after native post-physics pose publication and the earlier tutorial
 * prefix, before timeout/input/controller work. Use original unscaled seconds
 * and time; no implicit delta cap, clock advance, seed or skipped services.
 * Feed the resulting shake.offset to the later owned camera update.
 *
 * This is the concrete early effects block, not the complete GameUpdate
 * prelude/frame. Initial hero/ordinary-trajectory state has its verified empty
 * gates; reached active tint methods require their actual supplied tween.
 * Earlier state/RNG/renderer effects survive failure; reject later updates of
 * the failed binding. No frame allocation or GPU/audio device output.
 */
bool ab_classic_game_effects_update(AbClassicGameEffects *effects, float seconds,
                                    float time, AbError *error);

#endif
