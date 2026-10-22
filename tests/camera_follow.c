#include "ab/camera_follow.h"
#include "camera_follow_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Camera follow check failed at %d: %s\n", __LINE__,        \
                    #value);                                                           \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t input[FOLLOW_INPUT_WORDS];
    uint32_t events[FOLLOW_MAX_EVENTS][FOLLOW_EVENT_WORDS];
    uint32_t count, ordinals[11];
    AbCameraFollowBird bird;
    AbCameraFollow state;
    AbCameraFollowHost host;
    uint32_t fail_operation;
} FollowFixture;

static uint32_t caller_pc(AbCameraFollowOperation operation, AbCameraFollowSite site) {
    switch (operation) {
        case AB_CAMERA_FOLLOW_PREDICT:
            return 18;
        case AB_CAMERA_FOLLOW_TRANSFORM:
            return site == AB_CAMERA_FOLLOW_INITIAL ? 36 : 130;
        case AB_CAMERA_FOLLOW_PROJECT:
            return site == AB_CAMERA_FOLLOW_INITIAL ? 46
                   : site == AB_CAMERA_FOLLOW_PEAK  ? 144
                                                    : 9;
        case AB_CAMERA_FOLLOW_ZOOM:
            return site == AB_CAMERA_FOLLOW_INITIAL        ? 62
                   : site == AB_CAMERA_FOLLOW_PEAK_EXTENTS ? 162
                   : site == AB_CAMERA_FOLLOW_ZOOM_START   ? 171
                                                           : 231;
        case AB_CAMERA_FOLLOW_SNAP:
            return 68;
        case AB_CAMERA_FOLLOW_VALIDATE:
            return site == AB_CAMERA_FOLLOW_PEAK    ? 152
                   : site == AB_CAMERA_FOLLOW_FINAL ? 238
                                                    : 17;
        case AB_CAMERA_FOLLOW_EXTENTS:
            return 163;
        case AB_CAMERA_FOLLOW_ZOOM_FIT:
            return site == AB_CAMERA_FOLLOW_PEAK ? 178 : 204;
        case AB_CAMERA_FOLLOW_SEQUENCE:
            return 270;
        case AB_CAMERA_FOLLOW_SCREEN_X:
            return 8;
    }
    return UINT32_MAX;
}

static bool host(void *context, AbCameraFollowCall *call, AbError *error) {
    FollowFixture *fixture = context;
    uint32_t kind = (uint32_t)call->operation;
    CHECK(kind >= 1 && kind <= 10 && fixture->count < FOLLOW_MAX_EVENTS);
    uint32_t *event = fixture->events[fixture->count++];
    memset(event, 0, FOLLOW_EVENT_WORDS * sizeof(*event));
    event[0] = kind;
    event[1] = ++fixture->ordinals[kind];
    event[2] = caller_pc(call->operation, call->site);
    event[3] = call->site == AB_CAMERA_FOLLOW_LIVE_Y ? 21u : 278u;
    if (kind == 1) {
        CHECK(call->bird == &fixture->bird && call->bird->launch_present);
        event[4] = 15;
        event[5] = follow_word(call->bird->position.x);
        event[6] = follow_word(call->bird->position.y);
        event[7] = follow_word(call->bird->launch_velocity.x);
        event[8] = follow_word(call->bird->launch_velocity.y);
    } else if (kind == 5 || kind == 6) {
        const AbCameraFollowTargets *targets = &call->targets;
        event[4] = targets->values.present;
        if (event[4] & 1u)
            event[5] = follow_word(targets->values.position.x);
        if (event[4] & 2u)
            event[6] = follow_word(targets->values.position.y);
        if (event[4] & 4u)
            event[7] = follow_word(targets->values.zoom);
        if (call->site == AB_CAMERA_FOLLOW_INITIAL ||
            call->site == AB_CAMERA_FOLLOW_FINAL) {
            event[4] |= 8;
            event[8] = follow_word(targets->movement_duration);
            if (targets->zoom_duration_present) {
                event[4] |= 16;
                event[9] = follow_word(targets->zoom_duration);
            }
            event[10] = targets->custom_y ? 2u : 1u;
            event[11] = targets->custom_zoom;
        }
        event[12] = kind == 6;
    } else if (kind == 2 || kind == 3 || kind == 7 || kind == 8) {
        event[4] = kind == 3 ? 1u : kind == 7 ? 7u : 3u;
        event[5] = follow_word(call->point.x);
        if (kind != 3)
            event[6] = follow_word(call->point.y);
        if (kind == 7)
            event[7] = follow_word(call->zoom);
    }
    camera_follow_response(fixture->input, kind, event[1], event);
    if (fixture->fail_operation == kind) {
        return ab_fail(error, "Controlled follow-camera service failure");
    }
    call->vector = (AbVec2){follow_number(event[13]), follow_number(event[14])};
    call->number = follow_number(event[13]);
    if (kind == 1) {
        call->prediction = (AbBirdImpactPrediction){
            .point = call->vector,
            .point_present = !(fixture->input[1] & FOLLOW_NO_HIT),
            .time = follow_number(event[15])};
    } else if (kind == 5) {
        call->snap_id = follow_number(event[15]);
    } else if (kind == 6) {
        call->values = (AbCameraValues){
            .position = call->vector, .zoom = follow_number(event[15]), .present = 7};
    } else if (kind == 7) {
        call->number = follow_number(event[14]);
    }
    return true;
}

static bool prepare(FollowFixture *fixture, uint32_t sample) {
    camera_follow_case(sample, fixture->input);
    const uint32_t *input = fixture->input;
    fixture->bird = (AbCameraFollowBird){
        .position = {follow_number(input[2]), follow_number(input[3])},
        .launch_velocity = {follow_number(input[4]), follow_number(input[5])},
        .launch_present = !(input[1] & FOLLOW_NO_LAUNCH)};
    float compensation = follow_number(input[29]);
    CHECK(ab_camera_follow_init(
        &fixture->state, &fixture->bird,
        input[1] & FOLLOW_COMPENSATION_PRESENT ? &compensation : NULL, NULL));
    fixture->state.allow_cancel = (input[1] & FOLLOW_ALLOW_CANCEL) != 0;
    if (input[1] & FOLLOW_CACHED_HIT) {
        fixture->state.hit_position =
            (AbVec2){follow_number(input[7]), follow_number(input[8])};
        fixture->state.hit_time = follow_number(input[9]);
        fixture->state.hit_present = fixture->state.hit_time_present = true;
    }
    if (input[1] & FOLLOW_SCROLL_PRESENT) {
        fixture->state.scroll_timer = follow_number(input[27]);
        fixture->state.scroll_present = true;
    }
    fixture->state.peak_is_hit_position = (input[1] & FOLLOW_PEAK_PRESENT) != 0;
    fixture->state.peak_flag_present = fixture->state.peak_is_hit_position;
    fixture->host = (AbCameraFollowHost){fixture, follow_number(input[6]), host};
    return true;
}

static void snapshot(const FollowFixture *fixture, const AbCameraFollowResult *result,
                     uint32_t output[FOLLOW_STATE_WORDS]) {
    const AbCameraFollow *state = &fixture->state;
    output[1] = result->done;
    output[2] = result->targets_present;
    if (result->targets_present) {
        const AbCameraFollowTargets *targets = &result->targets;
        output[3] =
            targets->values.present | 8u | (targets->zoom_duration_present ? 16u : 0u);
        output[4] = follow_word(targets->values.position.x);
        output[5] = follow_word(targets->values.position.y);
        output[6] = follow_word(targets->values.zoom);
        output[7] = follow_word(targets->movement_duration);
        output[8] =
            targets->zoom_duration_present ? follow_word(targets->zoom_duration) : 0;
        output[9] = targets->custom_y ? 2u : 1u;
        output[10] = targets->custom_zoom;
    }
    output[11] = state->hit_present;
    if (state->hit_present) {
        output[12] = follow_word(state->hit_position.x);
        output[13] = follow_word(state->hit_position.y);
    }
    output[14] = state->hit_time_present ? follow_word(state->hit_time) : 0;
    output[15] = state->peak_is_hit_position;
    output[16] = state->original_zoom_present;
    output[17] = state->original_zoom_present ? follow_word(state->original_zoom) : 0;
    output[18] = state->time_to_peak_present;
    output[19] = state->time_to_peak_present ? follow_word(state->time_to_peak) : 0;
    output[20] = state->scroll_present;
    output[21] = state->scroll_present ? follow_word(state->scroll_timer) : 0;
    output[22] = follow_word(state->movement_duration_scaler);
    output[23] = follow_word(state->velocity_compensation);
    output[24] = follow_word(state->minimum_movement_duration);
    output[25] = state->hit_time_present;
    output[26] = state->peak_flag_present;
    output[27] = 1;
    output[28] = state->allow_cancel;
    output[29] = follow_word(state->timer);
}

static bool curves(FollowFixture *fixture, const AbCameraFollowResult *result,
                   uint32_t output[FOLLOW_CURVE_SAMPLES][FOLLOW_CURVE_WORDS]) {
    if (!result->targets_present)
        return true;
    for (uint32_t sample = 0; sample < FOLLOW_CURVE_SAMPLES; ++sample) {
        const AbCameraFollowTargets *targets = &result->targets;
        float value, time = camera_follow_curve_time(sample);
        if (targets->custom_zoom) {
            CHECK(ab_camera_follow_zoom_tween(&targets->zoom_curve, time, 3, &value,
                                              NULL));
            output[sample][0] = 1;
            output[sample][1] = follow_word(value);
        }
        CHECK(ab_camera_tween(AB_CAMERA_TWEEN_SMOOTHSTEP, time, 1, 2, 3, &value));
        output[sample][2] = 1;
        output[sample][3] = follow_word(value);
        if (targets->custom_y) {
            CHECK(ab_camera_follow_y_tween(&fixture->host, &value, NULL));
            output[sample][4] = 1;
            output[sample][5] = follow_word(value);
        }
    }
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  uint32_t update, const char *stage) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr,
                    "Follow sample %u update %u %s word %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    sample, update, stage, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool reference(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_CAMERA_FOLLOW_V1 256 4 6\n"));
    size_t matched = 0, boundaries = 0, failures = 0;
    for (uint32_t sample = 0; sample < FOLLOW_CASES; ++sample) {
        FollowFixture fixture = {0};
        CHECK(prepare(&fixture, sample));
        CHECK(words(file, fixture.input, FOLLOW_INPUT_WORDS, sample, 0, "input"));
        matched += FOLLOW_INPUT_WORDS;
        for (uint32_t update = 0; update < FOLLOW_UPDATES; ++update) {
            fixture.count = 0;
            AbCameraFollowResult result = {0};
            AbError error = {0};
            bool okay = ab_camera_follow_update(
                &fixture.state, camera_follow_seconds(update),
                update == 0 && (fixture.input[1] & FOLLOW_CANCEL), &fixture.host,
                &result, &error);
            uint32_t output[FOLLOW_STATE_WORDS] = {0};
            output[0] = !okay;
            snapshot(&fixture, &result, output);
            uint32_t curve[FOLLOW_CURVE_SAMPLES][FOLLOW_CURVE_WORDS] = {{0}};
            CHECK(curves(&fixture, &result, curve));
            output[31] = fixture.count;
            CHECK(words(file, output, FOLLOW_STATE_WORDS, sample, update, "state"));
            for (uint32_t index = 0; index < FOLLOW_CURVE_SAMPLES; ++index) {
                CHECK(words(file, curve[index], FOLLOW_CURVE_WORDS, sample, update,
                            "curve"));
            }
            for (uint32_t index = 0; index < fixture.count; ++index) {
                CHECK(words(file, fixture.events[index], FOLLOW_EVENT_WORDS, sample,
                            update, "event"));
            }
            matched += FOLLOW_STATE_WORDS + FOLLOW_CURVE_SAMPLES * FOLLOW_CURVE_WORDS +
                       (size_t)fixture.count * FOLLOW_EVENT_WORDS;
            boundaries += fixture.count;
            failures += !okay;
        }
    }
    CHECK(fscanf(file, " %79[^\n]", marker) == 1 &&
          !strcmp(marker, "AB camera follow reference complete"));
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf(
        "Matched %zu follow-camera words, 1024 updates, %zu boundaries, %zu failures\n",
        matched, boundaries, failures);
    return true;
}

typedef struct {
    FollowFixture fixture;
    AbCameraMover *mover;
    AbCameraFollowZoom zoom;
    AbCameraAxis axes[2];
    size_t count;
    bool fail;
} FollowMoverFixture;

static bool mover_curve(void *context, AbCameraAxis axis, float time, float start,
                        float change, float duration, float *output, AbError *error) {
    (void)start;
    (void)change;
    FollowMoverFixture *fixture = context;
    CHECK(fixture->count < 2 && fixture->mover->current_position.x == 0);
    fixture->axes[fixture->count++] = axis;
    bool okay = axis == AB_CAMERA_AXIS_Y
                    ? ab_camera_follow_y_tween(&fixture->fixture.host, output, error)
                    : ab_camera_follow_zoom_tween(&fixture->zoom, time, duration,
                                                  output, error);
    return okay && !fixture->fail;
}

static bool mover_controls(void) {
    AbCameraMover mover;
    FollowMoverFixture fixture = {.mover = &mover,
                                  .zoom = {.approach_duration = 1,
                                           .start_zoom = 1,
                                           .approach_change = 1,
                                           .hold_duration = 0.1f,
                                           .peak_zoom = 2,
                                           .finish_zoom = 1}};
    CHECK(prepare(&fixture.fixture, 3));
    CHECK(ab_camera_mover_init(&mover) &&
          ab_camera_mover_tweens(&mover, AB_CAMERA_TWEEN_SMOOTHSTEP,
                                 AB_CAMERA_TWEEN_SMOOTHSTEP) &&
          ab_camera_mover_position(&mover, (AbVec2){8, 16}, 1) &&
          ab_camera_mover_zoom(&mover, 2, 1));
    AbCameraMover saved = mover;
    AbCameraMoverCurves selected = {
        .context = &fixture, .axes = 6, .evaluate = mover_curve};
    CHECK(ab_camera_mover_update_curves(&mover, 0.5f, &selected, NULL));
    CHECK(fixture.count == 2 && fixture.axes[0] == AB_CAMERA_AXIS_Y &&
          fixture.axes[1] == AB_CAMERA_AXIS_ZOOM && mover.current_position.x == 4 &&
          mover.current_zoom == 1.5f && fixture.fixture.count == 3);
    mover = saved;
    fixture.count = 0;
    fixture.fail = true;
    CHECK(!ab_camera_mover_update_curves(&mover, 0.5f, &selected, NULL));
    CHECK(fixture.count == 1 && fixture.fixture.count == 6 &&
          !memcmp(&mover, &saved, sizeof(mover)));
    selected.axes = 8;
    CHECK(!ab_camera_mover_update_curves(&mover, 0.5f, &selected, NULL) &&
          fixture.count == 1 && !memcmp(&mover, &saved, sizeof(mover)));
    return true;
}

#include "camera_follow_owner.h"

static bool controls(void) {
    FollowFixture fixture = {0};
    CHECK(prepare(&fixture, 3));
    fixture.fail_operation = AB_CAMERA_FOLLOW_PROJECT;
    AbCameraFollowResult result = {.done = true, .targets_present = true};
    CHECK(!ab_camera_follow_update(&fixture.state, 0.25f, false, &fixture.host, &result,
                                   NULL));
    CHECK(result.done && result.targets_present && fixture.state.hit_present &&
          fixture.state.hit_time_present && fixture.count == 3 &&
          fixture.events[2][0] == AB_CAMERA_FOLLOW_PROJECT);
    float value = 17;
    fixture.fail_operation = AB_CAMERA_FOLLOW_VALIDATE;
    CHECK(!ab_camera_follow_y_tween(&fixture.host, &value, NULL) && value == 17);
    CHECK(!ab_camera_follow_zoom_tween(NULL, 0, 1, &value, NULL) && value == 17);
    fixture.state.target = NULL;
    CHECK(ab_camera_follow_update(&fixture.state, 0.25f, true, NULL, &result, NULL) &&
          result.done && !result.targets_present);
    CHECK(!ab_camera_follow_update(&fixture.state, 0.25f, false, NULL, &result, NULL));
    return mover_controls() && owner_follow_controls() &&
           owner_follow_absent_fit_zoom();
}

int main(int argc, char **argv) {
    if (argc == 1)
        return controls() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = reference(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
