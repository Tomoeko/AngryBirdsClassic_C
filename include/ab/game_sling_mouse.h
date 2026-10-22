#ifndef AB_GAME_SLING_MOUSE_H
#define AB_GAME_SLING_MOUSE_H

#include "ab/base.h"
#include "ab/physics_geometry.h"

enum { AB_SLING_MOUSE_SAMPLES = 5, AB_SLING_MOUSE_CALLS = 10 };

typedef struct {
    AbVec2 displacement;
    float seconds;
} AbSlingMouseSample;

typedef struct {
    AbVec2 old_cursor, start_physics, start_screen;
    AbSlingMouseSample samples[AB_SLING_MOUSE_SAMPLES];
    /* Original one-based ring index; sample_mask distinguishes absent entries. */
    uint32_t sample_index, sample_mask;
    bool drag_started, in_game_pressed;
} AbSlingMouseState;

typedef struct {
    AbVec2 cursor, cursor_physics, bird_position, rest;
    float seconds, touch_count, screen_width, zoom, shoot_range;
    bool pressed, held, released, skip_input, level_completed;
    bool bird_present, bird_dragged, bird_ready, sling_disabled, sling_visible;
} AbSlingMouseInput;

typedef enum {
    AB_SLING_MOUSE_DISABLED = 1,
    AB_SLING_MOUSE_RANGE,
    AB_SLING_MOUSE_ZOOM,
    AB_SLING_MOUSE_SELECT,
    AB_SLING_MOUSE_IS_DRAGGED,
    AB_SLING_MOUSE_REST,
    AB_SLING_MOUSE_VISIBLE,
    AB_SLING_MOUSE_STRETCH,
    AB_SLING_MOUSE_RELEASE,
} AbSlingMouseCallKind;

typedef struct {
    AbSlingMouseCallKind kind;
    AbVec2 vector;
    bool bird_dragged, drag_started, in_game_pressed;
} AbSlingMouseCall;

typedef struct {
    AbSlingMouseState next;
    AbSlingMouseCall calls[AB_SLING_MOUSE_CALLS];
    size_t count;
    bool bird_dragged;
} AbSlingMousePlan;

/* Ordinary gamelogic.updateGame mouse input phase after BehaviorTouchHandler
 * resolves skipInput. Gesture/delta/accelerometer modes and a triggered flying
 * bird specialty are outside this contract. Coordinates and resource/query
 * results are resolved caller inputs. Preserve selection radius arithmetic,
 * five-sample drag history, literal-true ready/visibility gates and release
 * ordering. Calls retain the state observed at each original boundary. SELECT
 * makes the local bird-dragged result true; STRETCH/RELEASE effects and callback
 * mutations remain external, as do table identity and later input housekeeping.
 * Finite used values/results, nonzero used zoom and a present current ring slot
 * are required. No allocation/callbacks; failure preserves output. */
bool ab_game_sling_mouse_plan(const AbSlingMouseState *state,
                              const AbSlingMouseInput *input, AbSlingMousePlan *output,
                              AbError *error);

#endif
