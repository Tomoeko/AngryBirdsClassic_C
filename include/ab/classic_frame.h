#ifndef AB_CLASSIC_FRAME_H
#define AB_CLASSIC_FRAME_H

#include "ab/classic_frame_particles.h"
#include "ab/classic_root.h"
#include "ab/classic_simulation.h"

typedef struct {
    AbClassicLevel *level;
    AbClassicSimulation *simulation;
    AbClassicRoot *root;
    AbClassicFrameParticles *particles;
    const bool *physics_enabled;
    const AbDataValue *physics_speed_factor;
    bool *update_available_pending;
    const bool *update_available_result;
    void *context;
    bool (*update_available)(void *context, bool available, AbError *error);
} AbClassicFrameConfig;

typedef struct {
    AbClassicFrameConfig config;
    bool initialized, updating, failed;
} AbClassicFrame;

/* Continuous GameLua.update body after native URL/notification/input publication:
 * reset menu-particle draw flag, run owned physics/interpolation/properties,
 * deliver a pending updateAvailable, execute the real root, then update/draw
 * particles and Spotlight. Borrow all owners and canonical fields until close.
 * The simulation must own this same level; particle and physics flags must share
 * one address. No state defaults, factories, platform input or GPU submission.
 * Native prefix work must actually finish before each call. */
bool ab_classic_frame_initialize(AbClassicFrameConfig config, AbClassicFrame *output,
                                 AbError *error);

/* seconds is GameLua's unscaled argument, already capped by GameApp when called
 * from the app. Both original root arguments receive these same seconds; the
 * second is not the native accumulated time. Read physics/speed before simulation,
 * but reread the live physics flag after root for particles. Native and script
 * clocks remain separate. Numeric speed and non-string nonnumeric values are
 * supported; string-number coercion needs its source boundary and rejects.
 * No frame allocation. Reentry/invalid arguments preserve the running owner.
 * After a reached failure, preserve prior effects and stop this frame owner.
 * Output commits only after the whole body succeeds. */
bool ab_classic_frame_update(AbClassicFrame *frame, float seconds,
                             AbFrameClockPlan *output, AbError *error);

#endif
