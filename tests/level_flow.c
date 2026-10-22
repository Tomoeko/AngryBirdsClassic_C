#include "ab/level_flow.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Level flow failed at line %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbLevelCompletionState *completion;
    AbLevelGoals *goals;
    AbLevelBeforeEnding *before;
    const uint32_t *input;
    uint32_t queries;
    uint32_t notifications;
    uint32_t hooks;
    bool notification_observed;
    bool hook_observed;
} FlowHost;

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t word(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool query_goals(void *context, bool *cleared, AbError *error) {
    (void)error;
    FlowHost *host = context;
    ++host->queries;
    uint32_t mutation = host->input[9];
    if (mutation & 16) {
        AbLevelCompletionState *state = host->completion;
        state->prevent_ending = true;
        state->eagle_launched_literal_true = true;
        state->eagle_timer_present = false;
        state->do_not_timeout = (mutation & 1) != 0;
        state->timed_out = (mutation & 2) != 0;
        state->moving_objects = (mutation & 4) != 0;
        state->do_not_wait_literal_true = (mutation & 8) != 0;
    }
    *cleared = host->input[8] >= 2;
    return true;
}

static bool notify_goals(void *context, AbError *error) {
    (void)error;
    FlowHost *host = context;
    ++host->notifications;
    host->notification_observed = host->goals->notification_sent;
    host->goals->notification_sent = false;
    host->goals->disabled = true;
    static const bool new_goal = true;
    host->goals->remaining_goals = &new_goal;
    host->goals->goal_count = 1;
    return true;
}

static bool before_hook(void *context, AbError *error) {
    (void)error;
    FlowHost *host = context;
    ++host->hooks;
    host->hook_observed = host->before->script_called;
    if (host->input[19] & 1) {
        host->before->timer = number(host->input[18]);
    }
    if (host->input[19] & 2) {
        host->before->script_called = false;
    }
    return true;
}

static bool fail_hook(void *context, AbError *error) {
    (void)context;
    return ab_fail(error, "Expected callback failure");
}

static bool fail_query(void *context, bool *cleared, AbError *error) {
    (void)context;
    *cleared = false;
    return ab_fail(error, "Expected query failure");
}

static bool edge_checks(void) {
    AbError error = {0};
    bool output = true;
    AbLevelCompletionState completion = {.eagle_launched_literal_true = true,
                                         .eagle_timer_present = true,
                                         .eagle_timer = NAN};
    CHECK(!ab_level_check_complete(&completion, NULL, NULL, &output, &error));
    CHECK(output);
    completion.prevent_ending = true;
    CHECK(ab_level_check_complete(&completion, NULL, NULL, &output, &error));
    CHECK(!output);
    completion = (AbLevelCompletionState){0};
    output = true;
    CHECK(!ab_level_check_complete(&completion, fail_query, NULL, &output, &error));
    CHECK(output);
    CHECK(!ab_level_check_complete(NULL, NULL, NULL, &output, &error));
    CHECK(!ab_level_check_complete(&completion, NULL, NULL, NULL, &error));
    AbLevelGoals goals = {.goal_count = 1};
    CHECK(!ab_level_check_goals(&goals, NULL, NULL, &output, &error));
    goals.goal_count = 0;
    CHECK(!ab_level_check_goals(&goals, fail_hook, NULL, &output, &error));
    CHECK(output && !goals.notification_sent);
    goals.notification_sent = true;
    CHECK(ab_level_check_goals(&goals, NULL, NULL, &output, &error) && output);
    AbLevelBeforeEnding before = {.timer = 1};
    CHECK(!ab_level_before_ending(&before, 0.1f, fail_hook, NULL, &error));
    CHECK(before.script_called && before.timer == 1);
    CHECK(ab_level_before_ending(&before, 0.1f, fail_hook, NULL, &error));
    CHECK(word(before.timer) == word(0.9f));
    before.timer = FLT_MAX;
    CHECK(!ab_level_before_ending(&before, -FLT_MAX, NULL, NULL, &error));
    CHECK(before.timer == FLT_MAX);
    CHECK(!ab_level_before_ending(&before, NAN, NULL, NULL, &error));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_LEVEL_FLOW_V1 4096\n") == 0);
    size_t queries = 0, notifications = 0, hooks = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[20], expected[17];
        CHECK(read_words(file, input, 20) && read_words(file, expected, 17));
        static const size_t kinds[] = {0, 1, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14};
        for (size_t field = 0; field < sizeof(kinds) / sizeof(*kinds); ++field) {
            CHECK(input[kinds[field]] <= 4);
        }
        CHECK(input[2] <= 1 && input[9] <= 31 && input[17] <= 1 && input[19] <= 3);
        AbLevelCompletionState completion = {.eagle_timer = number(input[3]),
                                             .prevent_ending = input[0] >= 2,
                                             .eagle_launched_literal_true =
                                                 input[1] == 2,
                                             .eagle_timer_present = input[2] != 0,
                                             .do_not_timeout = input[4] >= 2,
                                             .timed_out = input[5] >= 2,
                                             .moving_objects = input[6] >= 2,
                                             .do_not_wait_literal_true = input[7] == 2};
        bool remaining[] = {input[12] >= 2, input[13] >= 2};
        AbLevelGoals goals = {.remaining_goals = remaining,
                              .goal_count = 2,
                              .disabled = input[10] >= 2,
                              .notification_sent = input[11] >= 2};
        AbLevelBeforeEnding before = {.timer = number(input[15]),
                                      .script_called = input[14] >= 2};
        FlowHost host = {.completion = &completion,
                         .goals = &goals,
                         .before = &before,
                         .input = input};
        bool complete, cleared;
        AbError error = {0};
        CHECK(ab_level_check_complete(&completion, query_goals, &host, &complete,
                                      &error));
        CHECK(ab_level_check_goals(&goals, notify_goals, &host, &cleared, &error));
        CHECK(ab_level_before_ending(&before, number(input[16]),
                                     input[17] ? before_hook : NULL, &host, &error));
        uint32_t actual[] = {complete,
                             host.queries,
                             completion.prevent_ending,
                             completion.eagle_launched_literal_true,
                             completion.eagle_timer_present,
                             completion.do_not_timeout,
                             completion.timed_out,
                             completion.moving_objects,
                             completion.do_not_wait_literal_true,
                             cleared,
                             goals.notification_sent,
                             host.notifications,
                             host.notification_observed,
                             before.script_called,
                             word(before.timer),
                             host.hooks,
                             host.hook_observed};
        for (size_t field = 0; field < 17; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native flow %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        queries += host.queries;
        notifications += host.notifications;
        hooks += host.hooks;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB level flow reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original level completion/goal/before-ending cases match: "
           "%zu goal queries, %zu notifications, %zu hooks.\n",
           queries, notifications, hooks);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_level_flow_tests [NATIVE_LEVEL_FLOW_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        perror(argv[1]);
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
