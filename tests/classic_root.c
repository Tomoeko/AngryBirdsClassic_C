#include "ab/classic_root.h"
#include "ab/classic_level_flow.h"
#include "ab/classic_game_mode.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "root_query_menu.h"

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Classic root failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct RootTest RootTest;
typedef struct {
    RootTest *test;
    unsigned kind;
} RootContext;

struct RootTest {
    AbClassicRoot *root;
    AbClassicRootState state;
    AbClassicRootFlags flags;
    AbClassicGameGlobals globals;
    AbDataValue skip, ids[3];
    AbEventManager *events;
    AbEventListener listener;
    AbProcessManager *processes;
    AbNativeEvents *native_events;
    AbNativeAnimation animation;
    AbAudioMixer *mixer;
    AbClassicAudioRampState ramp_state;
    AbClassicAudioRamp ramp;
    AbUrlRequests *urls;
    AbClassicAnger *anger;
    AbClassicAngerState anger_state;
    AbClassicAngerSettings *anger_settings;
    AbClassicLevelFlow *flow;
    AbUiFrameState notification_state;
    AbUiFrameNode notification_frame;
    QueryMenu query_menu;
    AbDataTable eagle_status;
    AbDataValue episode, clicked_from;
    AbClassicEagleQuery eagle_query;
    bool in_system_update;
    size_t systems;
    AbDataEntry key_entries[3][3], url_entries[2];
    AbDataTable keys[3], url;
    AbClassicRootCall services[AB_ROOT_SERVICE_COUNT], functions[17], *ordered[17];
    RootContext service_contexts[AB_ROOT_SERVICE_COUNT], function_contexts[17];
    AbClassicRootCalls subsystems, updatable, tail;
    AbClassicGameMode *game_mode;
    AbClassicModeFunction game_method, draw_method, *current_mode;
    AbClassicPlayMode museum_mode, *play_mode, *museum_pointer;
    AbDataValue accelerometer_control;
    bool physics_enabled;
    uint32_t trace[128][18];
    size_t count;
    unsigned case_id, step, fail_kind;
    float seconds;
};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value};
}

static void value_words(AbDataValue value, uint32_t *output) {
    static const uint32_t kinds[] = {0, 1, 3, 4, 5};
    output[0] = kinds[value.kind];
    output[1] =
        value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN ? value.bits : 0;
}

static bool record(RootTest *test, unsigned kind, const AbDataValue *arguments,
                   size_t count) {
    CHECK(test->count < 128 && count <= 6);
    uint32_t *row = test->trace[test->count++];
    memset(row, 0, sizeof(test->trace[0]));
    row[0] = kind;
    row[1] = (uint32_t)count;
    value_words(test->state.global_time, row + 2);
    value_words(test->globals.time, row + 4);
    for (size_t index = 0; index < count; ++index)
        value_words(arguments[index], row + 6 + index * 2);
    return true;
}

static bool boundary(void *context, const AbDataValue *arguments, size_t count,
                     AbDataValue *result, AbError *error) {
    RootContext *binding = context;
    RootTest *test = binding->test;
    unsigned kind = binding->kind;
    CHECK(record(test, kind, arguments, count) &&
          !ab_classic_root_update(test->root, 0, (AbDataValue){0}, NULL));
    *result = (AbDataValue){0};
    if (kind == 1)
        *result = boolean((test->case_id & 1) != 0);
    if (kind == 10) {
        CHECK(count == 1 && arguments[0].kind == AB_DATA_STRING &&
              arguments[0].string.size == 1 && arguments[0].string.data[0] == 'S');
        *result = (test->case_id & 2) && test->step == 2 ? number(0) : boolean(false);
    }
    if (kind == 11)
        CHECK(count == 1 && arguments[0].kind == AB_DATA_STRING &&
              arguments[0].string.size == strlen("KEYBOARD_COMMAND") &&
              !memcmp(arguments[0].string.data, "KEYBOARD_COMMAND",
                      arguments[0].string.size));
    if (kind == 7)
        CHECK(!test->state.delayed_url && count == 1 &&
              arguments[0].kind == AB_DATA_STRING &&
              arguments[0].string.size == strlen("ab-reference://root"));
    if (kind == 13)
        *result = boolean(false);
    if (kind == 18 && (test->case_id & 8) && test->step == 2)
        test->globals.time = number(7);
    if (kind == 103 && !ab_classic_level_flow_update(test->flow, error))
        return false;
    return kind != test->fail_kind ||
           ab_fail(error, "Intentional root service failure");
}

static bool event(void *context, AbDataTable *payload, AbError *error) {
    RootTest *test = context;
    (void)error;
    const AbDataValue *id = ab_data_get(payload, text("id"));
    CHECK(id && id->kind == AB_DATA_STRING && id->string.size == 1);
    return record(test, id->string.data[0] == 'f' ? 71u : 72u, NULL, 0);
}

static bool game_mode_method(void *context, const AbDataValue *arguments, size_t count,
                             AbError *error) {
    AbDataValue ignored;
    return boundary(context, arguments, count, &ignored, error);
}

static bool owned_phase(void *context, AbClassicRootOwnedPhase phase, AbError *error) {
    RootTest *test = context;
    (void)error;
    AbDataValue seconds = number(test->seconds);
    if (phase == AB_ROOT_SAFE_QUIT_QUERY || phase == AB_ROOT_FREE_EAGLE_QUERY)
        return record(test, phase == AB_ROOT_SAFE_QUIT_QUERY ? 1u : 13u, NULL, 0);
    unsigned kind = phase == AB_ROOT_ANGER_TIMERS_UPDATE ? 3u
                    : phase == AB_ROOT_AUDIO_RAMP_UPDATE ? 12u
                    : phase == AB_ROOT_URL_TIMERS_UPDATE ? 9u
                                                         : 60u + (unsigned)phase;
    return record(test, kind, &seconds, 1);
}

static AbDataValue clock_profile(unsigned profile) {
    if (!profile)
        return (AbDataValue){0};
    if (profile == 1)
        return boolean(false);
    return number(profile == 2 ? 0 : 16777216.0f);
}

static bool prepare(RootTest *test, unsigned case_id) {
    *test = (RootTest){.case_id = case_id};
    CHECK(query_menu_prepare((ReferenceRootQuery){.root = case_id & 1 ? 1u : 5u},
                             &test->query_menu, NULL));
    test->eagle_query = (AbClassicEagleQuery){.unlock_status = &test->eagle_status,
                                              .current_episode = &test->episode,
                                              .clicked_from = &test->clicked_from};
    test->ids[0] = text("f");
    test->ids[1] = text("r");
    test->ids[2] = text("EID_PROCESS_FINISHED");
    CHECK(ab_url_requests_open(8, 16, 65536, (AbUrlRequestHost){0}, &test->urls, NULL));
    CHECK(
        ab_audio_mixer_open((AbAudioMixerLimits){1, 1, 1, 4096}, &test->mixer, NULL) &&
        ab_classic_audio_ramp_initialize(&test->ramp_state, test->mixer, &test->ramp,
                                         NULL));
    CHECK(ab_event_manager_open((AbEventManagerLimits){.event_types = 8,
                                                       .listeners_per_type = 2,
                                                       .queued_events = 4,
                                                       .delayed_events = 4,
                                                       .notification_depth = 4,
                                                       .storage_bytes = 16384},
                                (AbEventManagerHost){0}, &test->events, NULL));
    test->listener = (AbEventListener){
        .name = text("root").string, .context = test, .event_triggered = event};
    CHECK(ab_classic_anger_open(
        (AbClassicAngerConfig){.state = &test->anger_state,
                               .settings = &test->anger_settings,
                               .events = test->events,
                               .load_level_late = text("EID_LOAD_LEVEL_LATE"),
                               .level_increased = text("EID_ANGER_LEVEL_INCREASED"),
                               .records = 4,
                               .iteration_limit = 16,
                               .storage_bytes = 16384},
        &test->anger, NULL));
    CHECK(ab_event_manager_add(test->events, test->ids[0], &test->listener, NULL) &&
          ab_event_manager_add(test->events, test->ids[1], &test->listener, NULL) &&
          ab_process_manager_open(
              (AbProcessManagerConfig){.events = test->events,
                                       .finished_event = test->ids + 2,
                                       .actions = 1,
                                       .generations = 1,
                                       .storage_bytes = 4096},
              NULL, &test->processes, NULL) &&
          ab_native_events_open(8, 4096, &test->native_events, NULL) &&
          ab_native_animation_initialize(test->native_events, &test->systems,
                                         &test->in_system_update, &test->animation,
                                         NULL));
    for (size_t index = 0; index < 3; ++index)
        test->keys[index] = (AbDataTable){test->key_entries[index], 0, 3, NULL};
    test->url_entries[0] = (AbDataEntry){text("time"), number(0.25f)};
    test->url_entries[1] = (AbDataEntry){text("url"), text("ab-reference://root")};
    test->url = (AbDataTable){test->url_entries, 2, 2, NULL};
    test->state = (AbClassicRootState){.global_time = clock_profile(case_id & 3),
                                       .playtime = clock_profile(case_id & 3),
                                       .input_delay = number(case_id & 4 ? 0 : 0.25f),
                                       .eagle_info_timer = number(1),
                                       .show_background = boolean(false),
                                       .updated_width = number(480),
                                       .updated_height = number(320),
                                       .width = 480,
                                       .height = 320,
                                       .delayed_url = &test->url};
    test->globals.time = clock_profile(case_id & 3);
    test->flags = (AbClassicRootFlags){.gamer_present = boolean((case_id & 8) != 0),
                                       .gamer_dev_mode = boolean(true),
                                       .release_build = boolean(true),
                                       .cheats_enabled = boolean((case_id & 8) != 0),
                                       .settings_converted = boolean(true),
                                       .has_iaps = boolean(true),
                                       .device_model =
                                           text(case_id & 4 ? "osx" : "android").string,
                                       .menu_present = true,
                                       .ads_present = true,
                                       .settings_present = (case_id & 1) != 0,
                                       .notifications_present = true,
                                       .shop_present = (case_id & 4) != 0};
    if (case_id & 4) {
        test->state.last_video_ad_time = boolean(false);
        test->state.old_width = number(480);
        test->state.old_height = number(320);
    }
    if (case_id & 8) {
        test->state.game_was_resumed = number(0);
        test->state.restore_fullscreen = number(2);
    }
    AbClassicRootConfig config = {.state = &test->state,
                                  .flags = &test->flags,
                                  .globals = &test->globals,
                                  .skip_input = &test->skip,
                                  .held_keys = test->keys,
                                  .pressed_keys = test->keys + 1,
                                  .released_keys = test->keys + 2,
                                  .fullscreen_event = test->ids,
                                  .resolution_event = test->ids + 1,
                                  .events = test->events,
                                  .processes = test->processes,
                                  .native_animation = &test->animation,
                                  .audio_ramp = &test->ramp,
                                  .url_requests = test->urls,
                                  .anger = test->anger,
                                  .menus = test->query_menu.manager,
                                  .eagle_query = &test->eagle_query,
                                  .subsystems = &test->subsystems,
                                  .updatable = &test->updatable,
                                  .tail_updates = &test->tail,
                                  .notifications = 16,
                                  .storage_bytes = 16384,
                                  .observer_context = test,
                                  .observe = owned_phase};
    for (unsigned index = 0; index < AB_ROOT_SERVICE_COUNT; ++index) {
        test->service_contexts[index] = (RootContext){test, index + 1};
        test->services[index] =
            (AbClassicRootCall){test->service_contexts + index, boundary};
        config.services[index] = test->services + index;
    }
    for (unsigned index = 0; index < 17; ++index) {
        unsigned kind = index < 13   ? 100 + index
                        : index < 15 ? 200 + index - 13
                                     : 300 + index - 15;
        test->function_contexts[index] = (RootContext){test, kind};
        test->functions[index] =
            (AbClassicRootCall){test->function_contexts + index, boundary};
        test->ordered[index] = test->functions + index;
    }
    test->subsystems = (AbClassicRootCalls){test->ordered, 13, 13};
    test->updatable =
        (AbClassicRootCalls){test->ordered + 13, case_id & 4 ? 2u : 0u, 2};
    test->tail = (AbClassicRootCalls){test->ordered + 15, case_id & 8 ? 2u : 0u, 2};
    test->game_method = (AbClassicModeFunction){
        test->service_contexts + AB_ROOT_CURRENT_MODE, game_mode_method};
    test->draw_method = test->game_method;
    test->current_mode = &test->game_method;
    test->museum_mode = (AbClassicPlayMode){.kind = AB_CLASSIC_PLAY_MODE_MUSEUM,
                                            .menus = test->query_menu.manager};
    test->play_mode = test->museum_pointer = &test->museum_mode;
    CHECK(ab_classic_game_mode_open(
        (AbClassicGameModeConfig){.current = &test->current_mode,
                                  .update_game = &test->game_method,
                                  .draw_game = &test->draw_method,
                                  .play_mode = &test->play_mode,
                                  .museum_mode = &test->museum_pointer,
                                  .accelerometer_control = &test->accelerometer_control,
                                  .physics_enabled = &test->physics_enabled,
                                  .paused_functions = 8,
                                  .storage_bytes = 4096,
                                  .depth_limit = 8},
        &test->game_mode, NULL));
    test->services[AB_ROOT_CURRENT_MODE] =
        (AbClassicRootCall){test->game_mode, ab_classic_game_mode_root};
    test->notification_frame = (AbUiFrameNode){.state = &test->notification_state};
    CHECK(ab_classic_level_flow_open(
              (AbClassicLevelFlowConfig){.menus = test->query_menu.manager,
                                         .notifications = &test->notification_frame,
                                         .events = test->events,
                                         .loading_init = text("EID_LEVEL_LOADING_INIT"),
                                         .loading_done = text("EID_LEVEL_LOADING_DONE"),
                                         .flow_done =
                                             text("EID_LEVEL_START_SCREEN_FLOW_DONE"),
                                         .requests = 4,
                                         .queue_generations = 8,
                                         .notifications_count = 4,
                                         .depth_limit = 8,
                                         .work_limit = 64,
                                         .context = test->game_mode,
                                         .set_paused = ab_classic_game_mode_set_paused,
                                         .storage_bytes = 8192},
              &test->flow, NULL) &&
          ab_classic_level_flow_initialize(test->flow, NULL));
    CHECK(ab_classic_root_open(config, &test->root, NULL) &&
          ab_classic_root_bytes(test->root) <= 16384);
    return true;
}

static void close_test(RootTest *test) {
    ab_classic_root_close(test->root);
    ab_classic_level_flow_close(test->flow);
    ab_classic_game_mode_close(test->game_mode);
    ab_classic_anger_close(test->anger);
    ab_ui_menu_manager_close(test->query_menu.manager);
    ab_process_manager_close(test->processes);
    ab_native_events_close(test->native_events);
    ab_audio_mixer_close(test->mixer);
    ab_url_requests_close(test->urls);
    ab_event_manager_close(test->events);
}

static void keys(RootTest *test) {
    for (unsigned index = 0; index < 3; ++index) {
        AbDataTable *table = test->keys + index;
        table->count = 0;
        if ((index == 1 && (test->step == 1 || test->step == 3)) ||
            (index == 2 && (test->step == 2 || test->step == 4)))
            table->entries[table->count++] = (AbDataEntry){
                text("LBUTTON"), test->case_id & 2 ? number(0) : boolean(true)};
        if (test->step == 1 && (test->case_id & 8) && index < 2)
            table->entries[table->count++] =
                (AbDataEntry){text(index ? "F" : "CONTROL"), boolean(true)};
        if (test->step == 4 && (test->case_id & 2) && index == 1)
            table->entries[table->count++] = (AbDataEntry){text("9"), number(0)};
    }
}

static bool compare(FILE *reference, const uint32_t *expected, size_t count) {
    if (!reference)
        return true;
    for (size_t index = 0; index < count; ++index) {
        uint32_t actual;
        CHECK(fscanf(reference, "%" SCNx32, &actual) == 1);
        if (actual != expected[index]) {
            fprintf(stderr,
                    "Root word %zu: original=%08" PRIx32 " owned=%08" PRIx32 "\n",
                    index, actual, expected[index]);
            return false;
        }
    }
    return true;
}

static bool snapshot(RootTest *test, FILE *reference) {
    AbClassicRootState *state = &test->state;
    AbDataValue values[] = {
        state->safe_to_quit,
        state->dt,
        state->global_time,
        test->globals.time,
        state->playtime,
        state->last_video_ad_time,
        state->input_delay,
        state->double_click,
        state->double_click_timer,
        state->double_click_state,
        state->show_background,
        state->game_was_resumed,
        state->restore_fullscreen,
        state->updated_width,
        state->updated_height,
        state->old_width,
        state->old_height,
        state->eagle_info_timer,
        state->delayed_url
            ? (AbDataValue){.kind = AB_DATA_TABLE, .table = state->delayed_url}
            : (AbDataValue){0}};
    uint32_t header[] = {test->case_id, test->step, (uint32_t)test->count};
    if (!compare(reference, header, 3)) {
        fprintf(stderr, "Root case %u step %u, owned boundary IDs:", test->case_id,
                test->step);
        for (size_t index = 0; index < test->count; ++index)
            fprintf(stderr, " %u", test->trace[index][0]);
        fputc('\n', stderr);
        return false;
    }
    for (size_t index = 0; index < sizeof(values) / sizeof(*values); ++index) {
        uint32_t pair[2];
        value_words(values[index], pair);
        CHECK(compare(reference, pair, 2));
    }
    uint32_t geometry[] = {number(state->width).bits, number(state->height).bits,
                           number(state->touch_offset.x).bits,
                           number(state->touch_offset.y).bits};
    CHECK(compare(reference, geometry, 4));
    for (size_t index = 0; index < test->count; ++index)
        CHECK(compare(reference, test->trace[index], 18));
    return true;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_ROOT_V1 16 4\n"));
    }
    static const float steps[] = {0.125f, 0.125f, 0, 0.0625f};
    for (unsigned case_id = 0; case_id < 16; ++case_id) {
        RootTest test;
        CHECK(prepare(&test, case_id));
        for (test.step = 1; test.step <= 4; ++test.step) {
            test.count = 0;
            test.seconds = steps[test.step - 1];
            keys(&test);
            AbError error = {0};
            if (!ab_classic_root_update(test.root, test.seconds, number(99), &error)) {
                fprintf(stderr, "Root case %u step %u: %s\n", case_id, test.step,
                        error.message);
                return false;
            }
            CHECK(snapshot(&test, reference));
        }
        close_test(&test);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB root reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    AbClassicRoot *invalid = NULL;
    CHECK(!ab_classic_root_open((AbClassicRootConfig){0}, &invalid, NULL) && !invalid);
    RootTest test;
    CHECK(prepare(&test, 0));
    test.step = 1;
    test.seconds = 0.125f;
    keys(&test);
    test.fail_kind = 18;
    AbError error = {0};
    CHECK(!ab_classic_root_update(test.root, test.seconds, number(99), &error) &&
          strstr(error.message, "Intentional root service failure") &&
          test.globals.time.bits == number(0).bits &&
          test.state.old_width.kind == AB_DATA_NIL &&
          test.trace[test.count - 1][0] == 18);
    size_t count = test.count;
    CHECK(!ab_classic_root_update(test.root, 0, number(99), NULL) &&
          test.count == count);
    close_test(&test);

    CHECK(prepare(&test, 0));
    test.flags.restricted_aspect = number(0);
    CHECK(!ab_classic_root_update(test.root, 1, number(99), &error) &&
          strstr(error.message, "Restricted root viewport") && test.count == 2 &&
          test.state.dt.kind == AB_DATA_NIL &&
          test.state.global_time.kind == AB_DATA_NIL);
    close_test(&test);

    CHECK(prepare(&test, 0));
    test.flags.cursor_enabled = boolean(true);
    CHECK(!ab_classic_root_update(test.root, 0, number(99), &error) &&
          strstr(error.message, "software cursor") &&
          test.trace[test.count - 1][0] == 24 &&
          test.state.old_width.bits == number(480).bits);
    close_test(&test);
    return true;
}

#include "classic_root_frame.h"

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && continuous_frame_checks() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference
             ? "Root scheduler matches 64 original checkpoints and ordered boundaries"
             : "Root scheduler failure and reentry checks passed");
    return 0;
}
