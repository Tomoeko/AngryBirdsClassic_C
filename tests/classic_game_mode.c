#include "ab/classic_game_mode.h"
#include "ab/classic_level_flow.h"
#include "root_query_menu.h"
#include "../tools/oracle/reference_game_mode.h"

#include <inttypes.h>
#include <stdio.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Game mode failed at %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct ModeTest ModeTest;
typedef struct {
    ModeTest *test;
    unsigned kind;
} ModeMethod;

struct ModeTest {
    AbClassicGameMode *owner;
    AbClassicModeFunction update, draw, other, *current, *identities[32];
    AbClassicPlayMode museum, other_play, *play, *museum_pointer;
    ModeMethod methods[3];
    AbDataValue accelerometer;
    bool physics, cursor, has_cursor, fail_cursor;
    unsigned profile, step, count, created;
    uint32_t rows[16][10];
};

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static unsigned identity(ModeTest *test) {
    if (!test->current)
        return 0;
    for (unsigned index = 0; index < test->created; ++index)
        if (test->identities[index] == test->current)
            return index + 1;
    if (test->created == 32)
        return UINT32_MAX;
    test->identities[test->created++] = test->current;
    return test->created;
}

static bool record(ModeTest *test, unsigned kind, const AbDataValue *arguments,
                   size_t count) {
    CHECK(test->count < 16 && count <= 2);
    uint32_t *row = test->rows[test->count++];
    memset(row, 0, 10 * sizeof(*row));
    row[0] = kind;
    row[1] = (uint32_t)count;
    for (size_t index = 0; index < count; ++index) {
        CHECK(arguments[index].kind <= AB_DATA_NUMBER);
        row[2 + index * 2] = arguments[index].kind == AB_DATA_NUMBER    ? 3u
                             : arguments[index].kind == AB_DATA_BOOLEAN ? 1u
                                                                        : 0u;
        row[3 + index * 2] = arguments[index].bits;
    }
    row[6] = identity(test);
    row[7] = test->physics;
    row[8] = test->cursor;
    row[9] = test->has_cursor;
    return true;
}

static bool other_query(void *context, AbDataValue *result, AbError *error) {
    ModeTest *test = context;
    (void)error;
    CHECK(record(test, 1, NULL, 0));
    unsigned kind = reference_game_mode(test->profile).other_result;
    *result = !kind       ? (AbDataValue){0}
              : kind == 3 ? number(0)
                          : (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    return true;
}

static bool physics(void *context, bool enabled, AbError *error) {
    ModeTest *test = context;
    (void)error;
    CHECK(test->physics == enabled);
    AbDataValue value = {.kind = AB_DATA_BOOLEAN, .bits = enabled};
    return record(test, 2, &value, 1);
}

static bool cursor(void *context, bool enabled, AbError *error) {
    ModeTest *test = context;
    if (test->fail_cursor)
        return ab_fail(error, "Intentional reached motion-cursor failure");
    test->has_cursor = true;
    test->cursor = enabled;
    AbDataValue value = {.kind = AB_DATA_BOOLEAN, .bits = enabled};
    return record(test, 3, &value, 1);
}

static bool method(void *context, const AbDataValue *arguments, size_t count,
                   AbError *error) {
    ModeMethod *method = context;
    (void)error;
    AbDataValue ignored;
    CHECK(!ab_classic_game_mode_root(method->test->owner, arguments, count, &ignored,
                                     NULL));
    return record(method->test, method->kind, arguments, count);
}

static bool compare(FILE *reference, ModeTest *test, const uint32_t *row,
                    size_t count) {
    for (size_t index = 0; reference && index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
        if (row[index] != expected) {
            fprintf(stderr, "Mode %u/%u field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    test->profile, test->step, index, row[index], expected);
            return false;
        }
    }
    return true;
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_GAME_MODE_V1 128 18\n"));
    }
    for (unsigned profile = 0; profile < 128; ++profile) {
        ReferenceGameMode input = reference_game_mode(profile);
        ModeTest test = {.physics = true, .profile = profile};
        QueryMenu menu;
        const unsigned roots[] = {1, 5, 2, 8};
        const char *names[] = {"pausePage"};
        CHECK(query_menu_prepare_named(
            (ReferenceRootQuery){.root = roots[input.root], .children = 1}, names,
            &menu, NULL));
        menu.frames[1].fields.pause_page_show =
            (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = input.show};
        test.museum = (AbClassicPlayMode){.kind = AB_CLASSIC_PLAY_MODE_MUSEUM,
                                          .menus = menu.manager};
        test.other_play = (AbClassicPlayMode){.kind = AB_CLASSIC_PLAY_MODE_OTHER,
                                              .context = &test,
                                              .is_in_level = other_query};
        test.play = input.museum ? &test.museum : &test.other_play;
        test.museum_pointer = &test.museum;
        test.accelerometer =
            input.accelerometer ? number(0) : (AbDataValue){.kind = AB_DATA_BOOLEAN};
        for (unsigned index = 0; index < 3; ++index)
            test.methods[index] = (ModeMethod){&test, index + 4};
        test.update = (AbClassicModeFunction){test.methods, method};
        test.draw = (AbClassicModeFunction){test.methods + 1, method};
        test.other = (AbClassicModeFunction){test.methods + 2, method};
        test.identities[0] = &test.update;
        test.identities[1] = &test.draw;
        test.identities[2] = &test.other;
        test.created = 3;
        test.current = &test.other;
        AbClassicGameModeConfig config = {.current = &test.current,
                                          .update_game = &test.update,
                                          .draw_game = &test.draw,
                                          .play_mode = &test.play,
                                          .museum_mode = &test.museum_pointer,
                                          .accelerometer_control = &test.accelerometer,
                                          .physics_enabled = &test.physics,
                                          .paused_functions = 32,
                                          .depth_limit = 16,
                                          .storage_bytes = 8192,
                                          .context = &test,
                                          .motion_cursor = cursor,
                                          .observe_physics = physics};
        CHECK(ab_classic_game_mode_open(config, &test.owner, NULL));
        for (test.step = 0; test.step < 18; ++test.step) {
            test.count = 0;
            bool okay = true;
            unsigned operation = reference_game_mode_operation(test.step);
            AbError error = {0};
            if (operation == REFERENCE_MODE_TOGGLE_PLAY)
                test.play = input.museum ? &test.other_play : &test.museum;
            else if (operation == REFERENCE_MODE_FAIL_CURSOR) {
                test.accelerometer = number(0);
                test.fail_cursor = true;
            } else if (operation == REFERENCE_MODE_CALL) {
                AbDataValue arguments[] = {number(0.125f), number(7)}, result;
                okay = ab_classic_game_mode_root(test.owner, arguments, 2, &result,
                                                 &error);
                CHECK(!okay || result.kind == AB_DATA_NIL);
            } else if (operation == REFERENCE_MODE_SET_OTHER)
                okay = ab_classic_game_mode_set(test.owner, &test.other, &error);
            else if (operation != REFERENCE_MODE_SNAPSHOT)
                okay = ab_classic_game_mode_pause(
                    test.owner,
                    operation == REFERENCE_MODE_PAUSE_ZERO
                        ? number(0)
                        : (AbDataValue){.kind = AB_DATA_BOOLEAN,
                                        .bits = operation == REFERENCE_MODE_PAUSE},
                    &error);
            CHECK(okay || test.step == 17);
            uint32_t state[] = {profile,
                                test.step,
                                !okay,
                                identity(&test),
                                test.physics,
                                test.cursor,
                                test.has_cursor,
                                (uint32_t)ab_classic_game_mode_paused_count(test.owner),
                                test.count};
            CHECK(compare(reference, &test, state, 9));
            for (unsigned row = 0; row < test.count; ++row)
                CHECK(compare(reference, &test, test.rows[row], 10));
        }
        ab_classic_game_mode_close(test.owner);
        ab_ui_menu_manager_close(menu.manager);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB game-mode reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

typedef struct {
    unsigned shows;
    AbDataValue done;
} FlowScreen;

static bool screen_show(void *context, AbError *error) {
    (void)error;
    ++((FlowScreen *)context)->shows;
    return true;
}

static bool screen_done(void *context, AbDataValue *result, AbError *error) {
    (void)error;
    *result = ((FlowScreen *)context)->done;
    return true;
}

static bool edges(void) {
    AbClassicGameMode *invalid = NULL;
    CHECK(!ab_classic_game_mode_open((AbClassicGameModeConfig){0}, &invalid, NULL) &&
          !invalid);
    QueryMenu menu;
    const char *names[] = {"pausePage"};
    CHECK(query_menu_prepare_named((ReferenceRootQuery){.root = 5, .children = 1},
                                   names, &menu, NULL));
    menu.frames[1].fields.visible = query_ui_flag(1);
    menu.frames[1].fields.pause_page_show = query_ui_flag(1);
    ModeTest test = {.physics = true};
    test.methods[0] = (ModeMethod){&test, 4};
    test.methods[1] = (ModeMethod){&test, 5};
    test.update = (AbClassicModeFunction){test.methods, method};
    test.draw = (AbClassicModeFunction){test.methods + 1, method};
    test.current = &test.update;
    test.museum =
        (AbClassicPlayMode){.kind = AB_CLASSIC_PLAY_MODE_MUSEUM, .menus = menu.manager};
    test.play = test.museum_pointer = &test.museum;
    test.identities[0] = &test.update;
    test.identities[1] = &test.draw;
    test.created = 2;
    AbClassicGameModeConfig config = {.current = &test.current,
                                      .update_game = &test.update,
                                      .draw_game = &test.draw,
                                      .play_mode = &test.play,
                                      .museum_mode = &test.museum_pointer,
                                      .accelerometer_control = &test.accelerometer,
                                      .physics_enabled = &test.physics,
                                      .paused_functions = 8,
                                      .depth_limit = 16,
                                      .storage_bytes = 8192};
    CHECK(ab_classic_game_mode_open(config, &test.owner, NULL));
    size_t bytes = ab_classic_game_mode_bytes(test.owner);
    CHECK(bytes <= config.storage_bytes);
    AbUiFrameState notification_state = {0};
    AbUiFrameNode notifications = {.state = &notification_state};
    AbEventManager *events = NULL;
    CHECK(ab_event_manager_open((AbEventManagerLimits){.event_types = 4,
                                                       .listeners_per_type = 2,
                                                       .notification_depth = 8,
                                                       .storage_bytes = 8192},
                                (AbEventManagerHost){0}, &events, NULL));
    AbClassicLevelFlow *flow = NULL;
    AbDataValue loading_init = {.kind = AB_DATA_STRING,
                                .string = {(const uint8_t *)"init", 4}},
                loading_done = {.kind = AB_DATA_STRING,
                                .string = {(const uint8_t *)"loaded", 6}},
                flow_done = {.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)"done", 4}};
    CHECK(ab_classic_level_flow_open(
              (AbClassicLevelFlowConfig){.menus = menu.manager,
                                         .notifications = &notifications,
                                         .events = events,
                                         .loading_init = loading_init,
                                         .loading_done = loading_done,
                                         .flow_done = flow_done,
                                         .requests = 8,
                                         .queue_generations = 16,
                                         .notifications_count = 4,
                                         .storage_bytes = 8192,
                                         .depth_limit = 8,
                                         .work_limit = 128,
                                         .context = test.owner,
                                         .set_paused = ab_classic_game_mode_set_paused},
              &flow, NULL) &&
          ab_classic_level_flow_initialize(flow, NULL));
    FlowScreen screen = {0};
    AbClassicLevelFlowListener listener = {&screen, screen_show, screen_done};
    CHECK(ab_classic_level_flow_push(flow, (AbSpan){(const uint8_t *)"screen", 6}, 30,
                                     &listener, NULL) &&
          ab_classic_level_flow_update(flow, NULL) && !screen.shows &&
          ab_classic_level_flow_update(flow, NULL) && screen.shows == 1 &&
          test.physics && test.current == &test.update &&
          ab_classic_level_flow_update(flow, NULL) && !test.physics &&
          test.current != &test.update &&
          ab_classic_game_mode_paused_count(test.owner) == 1);
    AbClassicModeFunction *retained_pause = test.current;
    AbDataValue arguments[] = {number(0.125f), number(7)};
    CHECK(ab_classic_game_mode_call(test.owner, arguments, 2, NULL) &&
          test.count == 1 && test.rows[0][0] == 5 && !test.rows[0][1]);
    screen.done = number(0);
    CHECK(ab_classic_level_flow_update(flow, NULL) && test.physics &&
          test.current == &test.update && ab_classic_level_flow_view(flow).done.bits &&
          ab_classic_game_mode_call(test.owner, arguments, 2, NULL) &&
          test.rows[1][0] == 4 && test.rows[1][1] == 2 &&
          ab_classic_game_mode_set_paused(test.owner, true, NULL) &&
          test.current != retained_pause &&
          ab_classic_game_mode_set(test.owner, retained_pause, NULL) &&
          ab_classic_game_mode_call(test.owner, arguments, 2, NULL) &&
          test.rows[2][0] == 5 && !test.rows[2][1]);
    ab_classic_level_flow_close(flow);
    ab_event_manager_close(events);
    ab_classic_game_mode_close(test.owner);
    config.storage_bytes = bytes - 1;
    CHECK(!ab_classic_game_mode_open(config, &test.owner, NULL) && !test.owner);
    config.storage_bytes = 8192;
    config.paused_functions = 0;
    test.current = &test.update;
    test.physics = true;
    AbError error = {0};
    CHECK(ab_classic_game_mode_open(config, &test.owner, NULL) &&
          !ab_classic_game_mode_set_paused(test.owner, true, &error) &&
          strstr(error.message, "Paused game-mode function storage exhausted") &&
          !test.physics && test.current == &test.update &&
          !ab_classic_game_mode_set_paused(test.owner, false, NULL));
    ab_classic_game_mode_close(test.owner);
    config.paused_functions = 4;
    test.accelerometer = number(0);
    test.physics = false;
    CHECK(ab_classic_game_mode_open(config, &test.owner, NULL) &&
          !ab_classic_game_mode_set_paused(test.owner, false, &error) &&
          strstr(error.message, "Missing reached actual motion-cursor") &&
          test.current == &test.update && !test.physics);
    ab_classic_game_mode_close(test.owner);
    test.accelerometer = (AbDataValue){0};
    test.play = NULL;
    CHECK(ab_classic_game_mode_open(config, &test.owner, NULL) &&
          !ab_classic_game_mode_set_paused(test.owner, true, &error) &&
          strstr(error.message, "Missing reached actual play-mode instance") &&
          !test.physics);
    ab_classic_game_mode_close(test.owner);
    ab_ui_menu_manager_close(menu.manager);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference ? "Game-mode routing matches 2304 original checkpoints"
                   : "Game-mode routing state and callback checks passed");
    return 0;
}
