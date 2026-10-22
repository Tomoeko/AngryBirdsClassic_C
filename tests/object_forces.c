#include "ab/object_forces.h"

#include "ab/script_math.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object forces failed at line %d: %s\n", __LINE__,         \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t to_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static AbSpan sprite_span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static uint32_t sprite_code(AbSpan value) {
    const char *const names[] = {"rest", "active", "saved"};
    for (uint32_t index = 0; index < 3; ++index) {
        size_t length = strlen(names[index]);
        if (value.size == length && memcmp(value.data, names[index], length) == 0) {
            return index + 1;
        }
    }
    return 0;
}

static bool interval_edge_checks(void) {
    AbObjectForceInterval state = {.interval = 1,
                                   .timer_present = true,
                                   .sprite = sprite_span("rest"),
                                   .applied_sprite = sprite_span("active"),
                                   .original_sprite = sprite_span("saved"),
                                   .original_present = true};
    AbObjectForceIntervalPlan plan;
    AbError error = {0};
    CHECK(ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(plan.pulse && plan.sprite_requested && plan.next.timer == 1 &&
          sprite_code(plan.next.sprite) == 2 &&
          sprite_code(plan.next.original_sprite) == 1);
    state = plan.next;
    CHECK(ab_object_force_interval_plan(&state, 0.25f, false, &plan, &error));
    CHECK(!plan.pulse && plan.sprite_requested && plan.next.timer == 0.75f &&
          sprite_code(plan.next.sprite) == 1 && plan.next.original_present);
    state = plan.next;
    CHECK(ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(!plan.pulse && plan.sprite_requested && plan.next.original_present);
    state.timer = from_bits(UINT32_C(0x3f4ccccd));
    CHECK(ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(!plan.pulse && !plan.sprite_requested);
    state.timer = from_bits(UINT32_C(0x3f4ccccc));
    CHECK(ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(!plan.pulse && plan.sprite_requested);
    state.timer = -1;
    state.interval = 0.25f;
    CHECK(ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(plan.pulse && plan.next.timer == -0.75f);
    state.interval = 0;
    state.timer_present = false;
    CHECK(ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(plan.pulse && to_bits(plan.next.timer) == 0);
    state.interval = NAN;
    state.timer = NAN;
    state.sprite = (AbSpan){NULL, 1};
    CHECK(ab_object_force_interval_plan(&state, NAN, true, &plan, &error));
    CHECK(!plan.pulse && !plan.sprite_requested && isnan(plan.next.interval) &&
          isnan(plan.next.timer) && !plan.next.sprite.data &&
          plan.next.sprite.size == 1);
    AbObjectForceIntervalPlan saved = plan;
    CHECK(!ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(memcmp(&plan, &saved, sizeof(plan)) == 0);
    state.interval = 0;
    CHECK(!ab_object_force_interval_plan(&state, 0, false, &plan, &error));
    CHECK(memcmp(&plan, &saved, sizeof(plan)) == 0);
    CHECK(!ab_object_force_interval_plan(NULL, 0, false, &plan, &error));
    CHECK(!ab_object_force_interval_plan(&state, 0, false, NULL, &error));
    return true;
}

static bool edge_checks(void) {
    CHECK(interval_edge_checks());
    AbBodyMotion body = {.mass = {.type = AB_BODY_DYNAMIC, .mass = 2}, .sleep_time = 7};
    AbObjectForces settings = {.gravity = {0, 10}, .disabled = true};
    AbError error = {0};
    CHECK(ab_object_forces_apply(&body, &settings, &error));
    CHECK(body.awake && body.sleep_time == 0 && body.force.y == -20);
    body = (AbBodyMotion){.mass = {.type = AB_BODY_DYNAMIC, .linear_velocity = {1, -1}},
                          .sleep_time = 7};
    settings = (AbObjectForces){.force = {2, -2},
                                .object_mass = 3,
                                .gravity_enabled = true,
                                .force_x_present = true,
                                .force_y_present = true,
                                .target_present = true,
                                .target_velocity = 1};
    CHECK(ab_object_forces_apply(&body, &settings, &error));
    CHECK(body.awake && body.sleep_time == 0 && body.force.x == 0 && body.force.y == 0);
    body.awake = false;
    body.sleep_time = 7;
    settings.force = (AbVec2){-2, 2};
    CHECK(ab_object_forces_apply(&body, &settings, &error));
    CHECK(body.force.x == -6 && body.force.y == 6 && body.awake);
    AbBodyMotion saved = body;
    settings.disabled = true;
    settings.object_mass = NAN;
    settings.gravity = (AbVec2){NAN, NAN};
    CHECK(ab_object_forces_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    settings.disabled = false;
    CHECK(!ab_object_forces_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    settings.gravity_enabled = false;
    settings.gravity = (AbVec2){0, 1};
    CHECK(!ab_object_forces_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    settings.object_mass = FLT_MAX;
    settings.force.x = FLT_MAX;
    CHECK(!ab_object_forces_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    CHECK(!ab_object_forces_apply(NULL, &settings, &error));
    CHECK(!ab_object_forces_apply(&body, NULL, &error));
    body.mass.type = AB_BODY_KINEMATIC;
    saved = body;
    CHECK(ab_object_forces_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    AbVec2 polar = {7, 8};
    CHECK(!ab_script_polar(1025, 1, &polar));
    CHECK(polar.x == 7 && polar.y == 8);
    CHECK(ab_script_polar(-0.0f, -2, &polar));
    CHECK(polar.x == -2 && to_bits(polar.y) == 0);
    body = (AbBodyMotion){.mass.type = AB_BODY_DYNAMIC, .sleep_time = 7};
    settings = (AbObjectForces){.gravity_enabled = true,
                                .relative_present = true,
                                .relative_force = 3,
                                .object_mass = NAN,
                                .force = {NAN, NAN},
                                .force_x_present = true,
                                .force_y_present = true};
    CHECK(ab_object_forces_apply(&body, &settings, &error));
    CHECK(body.awake && body.sleep_time == 0 && body.force.x == 3 && body.force.y == 0);
    body.mass.angle = 1025;
    saved = body;
    CHECK(!ab_object_forces_apply(&body, &settings, &error));
    CHECK(memcmp(&body, &saved, sizeof(body)) == 0);
    body = (AbBodyMotion){.mass = {.type = AB_BODY_DYNAMIC, .inverse_mass = 1}};
    settings = (AbObjectForces){.gravity_enabled = true,
                                .force = {2, 0},
                                .force_x_present = true,
                                .object_mass = 3,
                                .impulse = true};
    CHECK(ab_object_forces_apply(&body, &settings, &error));
    CHECK(body.awake && body.mass.linear_velocity.x == 6 && body.force.x == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

#include "object_force_audio.h"
#include "object_force_step.h"

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    bool relative = strcmp(marker, "AB_ANDROID_OBJECT_FORCES_RELATIVE_V1 4096\n") == 0;
    bool audio = strcmp(marker, "AB_ANDROID_OBJECT_FORCES_AUDIO_V1 4096\n") == 0;
    bool interval =
        audio || strcmp(marker, "AB_ANDROID_OBJECT_FORCES_INTERVAL_V1 4096\n") == 0;
    CHECK(relative || interval ||
          strcmp(marker, "AB_ANDROID_OBJECT_FORCES_V1 4096\n") == 0);
    size_t changed = 0, wakes = 0, disabled_cancellation = 0, pulses = 0, restores = 0,
           velocity_changes = 0, sound_requests = 0, sound_draws = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[36] = {0}, expected[8], interval_expected[4] = {0};
        ForceSoundSnapshot audio_snapshot = {0};
        CHECK(read_words(file, input,
                         audio      ? 36u
                         : interval ? 30u
                         : relative ? 25u
                                    : 22u) &&
              read_words(file, expected, 8));
        if (interval) {
            CHECK(read_words(file, interval_expected, 4));
            CHECK(input[27] <= 3);
        }
        CHECK(input[0] <= 2 && input[1] <= 1 && input[2] <= 1 && input[3] <= 5 &&
              input[4] <= 15);
        AbBodyMotion body = {
            .mass = {.type = (AbBodyType)input[0],
                     .mass = from_bits(input[6]),
                     .linear_velocity = {from_bits(input[13]), from_bits(input[14])},
                     .center = {from_bits(input[15]), from_bits(input[16])},
                     .angular_velocity = from_bits(input[20]),
                     .angle = from_bits(input[22]),
                     .inverse_mass = from_bits(input[28]),
                     .inverse_inertia = from_bits(input[29])},
            .force = {from_bits(input[17]), from_bits(input[18])},
            .torque = from_bits(input[19]),
            .sleep_time = from_bits(input[5]),
            .awake = input[1] != 0,
        };
        AbBodyMotion before = body;
        const AbBodyMotion initial = body;
        AbObjectForces settings = {
            .gravity = {from_bits(input[7]), from_bits(input[8])},
            .force = {from_bits(input[10]), from_bits(input[11])},
            .object_mass = input[4] & 1 ? from_bits(input[9]) : 0,
            .relative_force = from_bits(input[23]),
            .relative_present = input[24] != 0,
            .target_velocity = from_bits(input[12]),
            .gravity_enabled = input[2] != 0,
            .disabled = input[3] == 2,
            .force_x_present = (input[4] & 2) != 0,
            .force_y_present = (input[4] & 4) != 0,
            .target_present = (input[4] & 8) != 0,
        };
        AbError error = {0};
        const AbObjectForces original_settings = settings;
        if (interval) {
            AbObjectForceInterval state = {
                .interval = from_bits(input[25]),
                .timer = from_bits(input[26]),
                .timer_present = (input[27] & 1) != 0,
                .original_present = (input[27] & 2) != 0,
                .sprite = sprite_span("rest"),
                .applied_sprite = sprite_span("active"),
                .original_sprite = input[27] & 2 ? sprite_span("saved") : (AbSpan){0},
            };
            AbObjectForceIntervalPlan plan;
            CHECK(ab_object_force_interval_plan(&state, from_bits(input[21]),
                                                settings.disabled, &plan, &error));
            uint32_t values[] = {plan.next.timer_present,
                                 plan.next.timer_present ? to_bits(plan.next.timer) : 0,
                                 sprite_code(plan.next.sprite),
                                 sprite_code(plan.next.original_sprite)};
            for (size_t field = 0; field < 4; ++field) {
                if (values[field] != interval_expected[field]) {
                    fprintf(stderr,
                            "Native interval case %zu field %zu: %08" PRIx32
                            " instead of %08" PRIx32 "\n",
                            item, field, values[field], interval_expected[field]);
                    return false;
                }
            }
            CHECK(plan.pulse == (interval_expected[2] == 2));
            pulses += plan.pulse;
            restores += plan.sprite_requested && !plan.pulse;
            settings.impulse = plan.pulse;
            settings.disabled = settings.disabled || !plan.pulse;
            if (audio) {
                if (!force_sound_native_check(file, input, plan.pulse, &sound_requests,
                                              &sound_draws, &audio_snapshot)) {
                    fprintf(stderr, "Native force audio case %zu failed\n", item);
                    return false;
                }
            }
        }
        CHECK(ab_object_forces_apply(&body, &settings, &error));
        uint32_t actual[] = {36 | (body.awake ? 2u : 0u),
                             to_bits(body.force.x),
                             to_bits(body.force.y),
                             to_bits(body.torque),
                             to_bits(body.sleep_time),
                             to_bits(body.mass.linear_velocity.x),
                             to_bits(body.mass.linear_velocity.y),
                             to_bits(body.mass.angular_velocity)};
        for (size_t field = 0; field < 8; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native case %zu field %zu: %08" PRIx32 " instead of %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        if (!force_step_native_check(input, &initial, &body, &original_settings,
                                     interval, interval_expected,
                                     audio ? &audio_snapshot : NULL)) {
            fprintf(stderr, "Native whole force-adder case %zu failed\n", item);
            return false;
        }
        bool force_changed = memcmp(&body.force, &before.force, sizeof(body.force));
        changed += force_changed;
        wakes += body.awake && !before.awake;
        velocity_changes +=
            memcmp(&body.mass.linear_velocity, &before.mass.linear_velocity,
                   sizeof(body.mass.linear_velocity)) != 0;
        disabled_cancellation +=
            input[3] == 2 && !settings.gravity_enabled && force_changed;
        before.force = body.force;
        before.torque = body.torque;
        before.awake = body.awake;
        before.sleep_time = body.sleep_time;
        before.mass.linear_velocity = body.mass.linear_velocity;
        before.mass.angular_velocity = body.mass.angular_velocity;
        CHECK(memcmp(&before, &body, sizeof(body)) == 0);
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB object forces reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    CHECK(changed > 0 && wakes > 0 && disabled_cancellation > 0);
    printf("All 4096 native force-adder cases match: %zu force changes, "
           "%zu wakes, %zu disabled gravity cancellations.\n",
           changed, wakes, disabled_cancellation);
    if (interval) {
        CHECK(pulses > 0 && restores > 0 && velocity_changes > 0);
        printf("Interval states match: %zu pulses, %zu sprite restorations, "
               "%zu velocity changes.\n",
               pulses, restores, velocity_changes);
    }
    if (audio) {
        CHECK(sound_requests > 0 && sound_draws > 0);
        printf("Pulse sounds match: %zu requests and %zu native remainder draws.\n",
               sound_requests, sound_draws);
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fputs("Usage: ab_object_forces_tests [NATIVE_TRACE]\n", stderr);
        return 2;
    }
    bool okay = edge_checks() && force_sound_edge_checks() && force_step_edge_checks();
    if (okay && argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        if (!file) {
            perror(argv[1]);
            return 1;
        }
        okay = native_checks(file);
        if (fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
