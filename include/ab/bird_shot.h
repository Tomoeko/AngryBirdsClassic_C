#ifndef AB_BIRD_SHOT_H
#define AB_BIRD_SHOT_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

typedef struct {
    const char *name;
    const char *sprite;
    AbVec2 position;
    AbVec2 start_impulse;
    AbVec2 launch_velocity;
    float mass;
    bool shot;
    bool has_collided;
    bool override_drawing;
    bool has_script;
} AbShotBird;

typedef struct {
    const char *on_shot;
    const char *sprite_flying;
    const char *launch_sound;
} AbShotDefinition;

typedef struct {
    AbShotBird *dragged;
    AbShotBird *flying;
    const char *current_bird_name;
    float tutorial_timer;
    float birds_shot;
    float bird_count;
    float next_bird_timer;
    float bird_reload_delay;
    float last_score_time;
    float time;
    float old_score;
    float score;
    size_t other_bird_count;
    bool specialty_available;
    bool moving_above_tolerance;
    bool selection_played;
    bool bird_ready;
    bool bird_fired;
    bool has_level_hook;
} AbShotState;

typedef enum {
    AB_SHOT_SET_POSITION = 1,
    AB_SHOT_SET_VELOCITY,
    AB_SHOT_READ_SHOT_COUNT,
    AB_SHOT_READ_BACKWARDS_COUNT,
    AB_SHOT_NOTIFY_BIRD,
    AB_SHOT_APPLY_IMPULSE,
    AB_SHOT_LEVEL_HOOK,
    AB_SHOT_READ_DEFINITION,
    AB_SHOT_DEFINITION_HOOK,
    AB_SHOT_NOTIFY_ALL,
    AB_SHOT_SET_SPRITE,
    AB_SHOT_ATTACH_PARTICLES,
    AB_SHOT_RESOLVE_AUDIO,
    AB_SHOT_PLAY_AUDIO,
    AB_SHOT_PLAY_SLING_SOUND,
    AB_SHOT_SCRIPT_LAUNCH,
} AbShotOperation;

typedef struct {
    AbShotOperation operation;
    AbShotBird *bird;
    const char *text;
    AbVec2 vector;
    AbVec2 point;
    AbVec2 *launch_position;
    AbVec2 *launch_impulse;
    float value;
    float backwards_count;
    AbShotDefinition definition;
} AbShotCall;

/* Run one launchBirdWithImpulse state transition. Host calls execute in order:
 * position/velocity reset, two statistics reads, bird event, impulse, optional
 * level/definition hooks, flags/counters, optional all-birds event, sprite,
 * particles, optional audio, optional object script, final stored velocity.
 * Position/velocity setters and impulse application belong to the host.
 * READ_* returns value or definition; RESOLVE_AUDIO returns text. PLAY_AUDIO
 * requests gain one and no loop. Other calls consume the populated fields.
 * NOTIFY_BIRD receives borrowed mutable original position/impulse vectors and
 * the two statistics values; later operations reread those vectors and bird
 * metadata. Script-launch velocity and final stored velocity each independently
 * evaluate (1 / current mass) * current impulse, around the script callback.
 *
 * The host may update state/bird metadata during calls, including position and
 * mass. Required birds/names and used numbers must remain valid and finite,
 * mass positive. Definition/audio identifiers are borrowed strings or NULL.
 * The local other-bird list is represented by its active count, reset to zero.
 * No Lua environment, shared-table identity, runtime callbacks or audio engine
 * is implemented here. No allocation. Invalid initial inputs reject before
 * work; a host error or unsupported callback result leaves partial progress.
 */
typedef bool (*AbShotHost)(void *context, AbShotCall *call, AbError *error);

bool ab_bird_shot(AbShotState *state, AbVec2 *position, AbVec2 *impulse,
                  AbShotHost host, void *context, AbError *error);

typedef enum {
    AB_SHOT_PUBLISH_START_IMPULSE,
    AB_SHOT_PUBLISH_FLYING,
    AB_SHOT_PUBLISH_FLAGS,
    AB_SHOT_PUBLISH_VELOCITY,
    AB_SHOT_PUBLISH_OTHER_BIRDS,
    AB_SHOT_PUBLISH_SCORE,
} AbShotPublication;

/* A C storage bridge, separate from original service calls. Publish newly
 * assigned fields at these exact boundaries, before the next original service
 * can observe them. The bridge may report a storage failure but must not mutate
 * transition state, invoke game services or reenter. Earlier writes survive.
 * A NULL bridge is the same contract as ab_bird_shot. */
typedef bool (*AbShotPublisher)(void *context, AbShotPublication publication,
                                const AbShotState *state, AbError *error);
bool ab_bird_shot_publish(AbShotState *state, AbVec2 *position, AbVec2 *impulse,
                          AbShotHost host, AbShotPublisher publish, void *context,
                          AbError *error);

#endif
