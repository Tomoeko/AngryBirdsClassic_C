#include "ab/camera_sequence.h"
#include "ab/target_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera sequence failed at %d: %s\n", __LINE__,            \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbCameraSequenceOperation operation;
    size_t stage_index;
    float stage_timer;
    bool stage_timer_present;
    bool active;
    float timer;
    bool done_callback;
} Event;

typedef struct {
    Event events[64];
    size_t count;
    bool custom_complete;
    bool mutate_custom_timer;
    bool mutate_done_timer;
    bool mutate_delegate_timer;
    bool fail;
} Host;

static bool host(void *context, AbCameraSequenceManager *manager,
                 AbCameraSequenceCall *call, AbError *error) {
    Host *state = context;
    if (state->count == 64) {
        return ab_fail(error, "Sequence test event overflow");
    }
    state->events[state->count++] =
        (Event){.operation = call->operation,
                .stage_index = manager->stage_index,
                .stage_timer = manager->stage_timer,
                .stage_timer_present = manager->stage_timer_present,
                .active = manager->current != NULL,
                .timer = manager->timer,
                .done_callback = manager->done_callback};
    if (call->operation == AB_CAMERA_SEQUENCE_DELEGATE &&
        state->mutate_delegate_timer) {
        manager->timer = 17;
    }
    if (call->operation == AB_CAMERA_SEQUENCE_CUSTOM_CONDITION) {
        call->condition_result = state->custom_complete;
        if (state->mutate_custom_timer) {
            manager->stage_timer = 32;
        }
    }
    if (call->operation == AB_CAMERA_SEQUENCE_DONE) {
        /* Native end invokes its callback before reset clears this field. */
        manager->timer = 99;
        if (state->mutate_done_timer) {
            manager->stage_timer = 64;
            manager->stage_timer_present = true;
        }
    }
    return !state->fail || ab_fail(error, "Controlled sequence host failure");
}

static bool duration_and_interruption(void) {
    Host state = {0};
    AbCameraSequenceManager manager;
    CHECK(ab_camera_sequence_init(&manager, host, &state, NULL));
    CHECK(!manager.stage_timer_present && !manager.current && manager.timer == 0 &&
          ab_camera_sequence_update(&manager, NAN, true, false, false, NULL));
    AbCameraSequenceStage stages[] = {
        {.duration = 0, .duration_present = true},
        {.duration = 2, .duration_present = true, .interrupt_on_input = true},
        {.duration = 1, .duration_present = true, .interrupt_on_input = true}};
    AbCameraSequence source = {stages, 3};
    CHECK(ab_camera_sequence_start(&manager, &source, true, NULL) &&
          manager.stage_index == 1 && manager.stage_timer == 0 &&
          stages[0].condition == AB_CAMERA_CONDITION_DURATION && state.count == 1);
    CHECK(ab_camera_sequence_update(&manager, 0, false, false, true, NULL) &&
          manager.stage_index == 1);
    CHECK(ab_camera_sequence_update(&manager, 0.5f, false, false, true, NULL) &&
          manager.stage_index == 2 && manager.stage_timer == 0 && state.count == 2);
    CHECK(ab_camera_sequence_update(&manager, 2, false, false, true, NULL) &&
          manager.stage_index == 2 && manager.stage_timer == 2);
    CHECK(ab_camera_sequence_update(&manager, 0.25f, false, false, true, NULL) &&
          manager.stage_index == 3 && manager.stage_timer == 0);
    CHECK(ab_camera_sequence_update(&manager, 1, false, false, true, NULL) &&
          manager.stage_index == 3);
    CHECK(ab_camera_sequence_update(&manager, 0.125f, false, false, true, NULL) &&
          !manager.current && manager.stage_index == 0 && manager.timer == 0 &&
          !manager.done_callback && manager.stage_timer == 1.125f &&
          state.events[3].operation == AB_CAMERA_SEQUENCE_DONE &&
          state.events[3].stage_index == 4 && state.events[3].active &&
          state.events[3].stage_timer == 1.125f);
    CHECK(ab_camera_sequence_start(&manager, &source, true, NULL) &&
          ab_camera_sequence_update(&manager, 0.125f, true, false, true, NULL) &&
          manager.stage_index == 2 && manager.current);
    CHECK(ab_camera_sequence_update(&manager, NAN, true, false, true, NULL) &&
          !manager.current && manager.stage_timer == 0 &&
          state.events[state.count - 1].stage_index == 2);
    AbCameraSequence empty = {0};
    size_t before = state.count;
    CHECK(ab_camera_sequence_start(&manager, &empty, true, NULL) && !manager.current &&
          state.count == before + 1 &&
          state.events[before].operation == AB_CAMERA_SEQUENCE_DONE &&
          state.events[before].stage_index == 1);
    return true;
}

static bool conditions_and_failures(void) {
    Host state = {0};
    AbCameraSequenceManager manager;
    CHECK(ab_camera_sequence_init(&manager, host, &state, NULL));
    AbCameraSequenceStage stage = {.condition = AB_CAMERA_CONDITION_NO_MOVING,
                                   .minimum_duration = 0.25f};
    AbCameraSequence source = {&stage, 1};
    CHECK(ab_camera_sequence_start(&manager, &source, false, NULL) &&
          ab_camera_sequence_update(&manager, 0.5f, false, true, true, NULL) &&
          manager.stage_timer == 0 && manager.current);
    CHECK(ab_camera_sequence_update(&manager, 0.25f, false, false, true, NULL) &&
          manager.current);
    CHECK(ab_camera_sequence_update(&manager, 0.125f, false, false, true, NULL) &&
          !manager.current && manager.stage_timer == 0.375f && state.count == 1);
    stage.condition = AB_CAMERA_CONDITION_SPECIALTY;
    CHECK(ab_camera_sequence_start(&manager, &source, false, NULL) &&
          ab_camera_sequence_update(&manager, -0.5f, false, false, true, NULL) &&
          manager.current && manager.stage_timer == -0.5f);
    CHECK(ab_camera_sequence_update(&manager, 0, false, false, false, NULL) &&
          !manager.current);
    stage.condition = AB_CAMERA_CONDITION_CUSTOM;
    state.mutate_custom_timer = true;
    CHECK(ab_camera_sequence_start(&manager, &source, true, NULL) &&
          ab_camera_sequence_update(&manager, 0, false, false, true, NULL) &&
          manager.current && manager.stage_timer == 32);
    state.custom_complete = true;
    CHECK(ab_camera_sequence_update(&manager, 0, false, false, true, NULL) &&
          !manager.current && manager.stage_timer == 32 && manager.timer == 0);
    state.fail = true;
    stage.condition = AB_CAMERA_CONDITION_UNSET;
    CHECK(!ab_camera_sequence_start(&manager, &source, false, NULL) &&
          manager.current == &source && manager.stage_index == 1 &&
          stage.condition == AB_CAMERA_CONDITION_ALWAYS && manager.stage_timer == 0);
    state.fail = false;
    CHECK(!ab_camera_sequence_update(&manager, INFINITY, false, false, false, NULL) &&
          manager.stage_index == 1 && manager.stage_timer == 0);
    CHECK(ab_camera_sequence_reset(&manager) && !manager.current &&
          manager.stage_timer_present && manager.stage_timer == 0);
    AbCameraSequence invalid = {&stage, SIZE_MAX};
    CHECK(!ab_camera_sequence_start(&manager, &invalid, false, NULL) &&
          !manager.current && !ab_camera_sequence_start(&manager, NULL, false, NULL) &&
          !ab_camera_sequence_init(NULL, host, &state, NULL) &&
          !ab_camera_sequence_init(&manager, NULL, &state, NULL) &&
          !ab_camera_sequence_reset(NULL) && !ab_camera_sequence_end(NULL, NULL) &&
          !ab_camera_sequence_update(NULL, 0, false, false, false, NULL));
    return true;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool same_words(const uint32_t *actual, const uint32_t *expected, size_t count,
                       size_t item, size_t step, const char *kind) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Native sequence %zu command %zu %s field %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    item, step, kind, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static AbDataValue data_string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue data_number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(value)};
}

static bool static_reader_checks(void) {
    AbDataEntry fields[] = {{data_string("action"), data_string("wait")},
                            {data_string("duration"), data_number(0)},
                            {data_string("interruptOnUserInput"), data_number(0)},
                            {data_string("condition"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable properties = {fields, 4, 4, NULL};
    AbDataEntry entry = {data_number(1), {.kind = AB_DATA_TABLE, .table = &properties}};
    AbDataTable table = {&entry, 1, 1, NULL};
    AbCameraSequence sequence = {0};
    size_t budget = sizeof(AbCameraSequenceStage);
    CHECK(ab_camera_sequence_read(&table, budget, &sequence, NULL) &&
          sequence.count == 1 && sequence.stages[0].duration_present &&
          sequence.stages[0].duration == 0 && sequence.stages[0].interrupt_on_input &&
          sequence.stages[0].condition == AB_CAMERA_CONDITION_UNSET &&
          sequence.stages[0].properties == &properties);
    ab_camera_sequence_free(&sequence);
    CHECK(!sequence.stages && !sequence.count &&
          !ab_camera_sequence_read(&table, budget - 1, &sequence, NULL) &&
          !sequence.stages);
    entry.key = data_number(2);
    CHECK(!ab_camera_sequence_read(&table, budget, &sequence, NULL) &&
          !sequence.stages);
    entry.key = data_number(1);
    fields[3].value = data_string("customCondition");
    CHECK(!ab_camera_sequence_read(&table, budget, &sequence, NULL) &&
          !sequence.stages);
    fields[3].value = data_string("untilSpecialtyTriggered");
    CHECK(ab_camera_sequence_read(&table, budget, &sequence, NULL) &&
          sequence.stages[0].condition == AB_CAMERA_CONDITION_SPECIALTY);
    ab_camera_sequence_free(&sequence);
    fields[3].value = data_string("noMovingObjects");
    CHECK(!ab_camera_sequence_read(&table, budget, &sequence, NULL) &&
          !sequence.stages);
    fields[1].key = data_string("minDuration");
    fields[1].value = data_number(0.25f);
    CHECK(ab_camera_sequence_read(&table, budget, &sequence, NULL) &&
          sequence.stages[0].condition == AB_CAMERA_CONDITION_NO_MOVING &&
          sequence.stages[0].minimum_duration == 0.25f &&
          !sequence.stages[0].duration_present);
    ab_camera_sequence_free(&sequence);
    CHECK(!ab_camera_sequence_read(NULL, budget, &sequence, NULL) &&
          !ab_camera_sequence_read(&table, budget, NULL, NULL));
    table.count = 0;
    CHECK(ab_camera_sequence_read(&table, 0, &sequence, NULL) && !sequence.stages &&
          !sequence.count);
    ab_camera_sequence_free(&sequence);
    ab_camera_sequence_free(NULL);
    return true;
}

static bool apk_sequences(const char *path) {
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData data = {0};
    AbCameraSequence sequence = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    bool okay = false;
    if (!ab_file_read(path, 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error)) {
        goto release_assets;
    }
    const char *paths[] = {
        "assets/data/scripts/cameraSequences/DefaultLevelStartCameraSequence.lua",
        "assets/data/scripts/cameraSequences/RestartLevelCameraSequence.lua"};
    for (size_t index = 0; index < 2; ++index) {
        if (!ab_target_data_open(&archive, paths[index], (AbSpan){key, sizeof(key)},
                                 1024 * 1024, NULL, &data, &error)) {
            goto release_assets;
        }
        const AbDataValue *value =
            ab_data_get(data.data.globals, data_string("sequence"));
        if (!value || value->kind != AB_DATA_TABLE ||
            !ab_camera_sequence_read(value->table, 65536, &sequence, &error) ||
            sequence.count != (index == 0 ? 4u : 2u)) {
            goto release_assets;
        }
        for (size_t stage = 0; stage < sequence.count; ++stage) {
            if (!sequence.stages[stage].properties ||
                !sequence.stages[stage].action.size) {
                goto release_assets;
            }
        }
        ab_camera_sequence_free(&sequence);
        ab_target_data_close(&data);
    }
    okay = true;
    puts("Both original level-start/restart sequence tables load directly from APK "
         "into six prepared C stages without executing Lua.");

release_assets:
    memset(key, 0, sizeof(key));
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_camera_sequence_free(&sequence);
    ab_target_data_close(&data);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_CAMERA_SEQUENCE_V1 128 64\n") == 0);
    size_t event_counts[3] = {0};
    for (size_t item = 0; item < 128; ++item) {
        uint32_t setup[5];
        CHECK(words(file, setup, 5) && setup[0] <= 4 && setup[1] <= 1 &&
              setup[4] <= 15);
        AbCameraSequenceStage stages[4] = {
            {.duration_present = setup[1] != 0, .duration = number(setup[2])},
            {.condition = AB_CAMERA_CONDITION_NO_MOVING,
             .minimum_duration = number(setup[3])},
            {.condition = AB_CAMERA_CONDITION_SPECIALTY},
            {.condition = AB_CAMERA_CONDITION_CUSTOM}};
        for (size_t index = 0; index < 4; ++index) {
            stages[index].interrupt_on_input = ((setup[4] >> index) & 1) != 0;
        }
        AbCameraSequence source = {stages, setup[0]};
        AbCameraSequenceManager manager;
        Host state = {0};
        CHECK(ab_camera_sequence_init(&manager, host, &state, NULL));
        for (size_t step = 0; step < 64; ++step) {
            uint32_t input[6], expected[7];
            CHECK(words(file, input, 6) && input[0] <= 3 && input[2] <= 1 &&
                  input[3] <= 1 && input[4] <= 1 && input[5] <= 31);
            state.count = 0;
            state.custom_complete = (input[5] & 1) != 0;
            state.mutate_custom_timer = (input[5] & 4) != 0;
            state.mutate_done_timer = (input[5] & 8) != 0;
            state.mutate_delegate_timer = (input[5] & 16) != 0;
            bool okay;
            switch (input[0]) {
                case 0:
                    okay = ab_camera_sequence_reset(&manager);
                    break;
                case 1:
                    okay = ab_camera_sequence_start(&manager, &source,
                                                    (input[5] & 2) != 0, NULL);
                    break;
                case 2:
                    okay = ab_camera_sequence_update(&manager, number(input[1]),
                                                     input[2] != 0, input[3] != 0,
                                                     input[4] != 0, NULL);
                    break;
                default:
                    okay = ab_camera_sequence_end(&manager, NULL);
                    break;
            }
            CHECK(okay && words(file, expected, 7));
            const uint32_t actual[] = {
                manager.current != NULL,
                bits((float)manager.stage_index),
                bits(manager.timer),
                manager.stage_timer_present ? 3u : 0u,
                manager.stage_timer_present ? bits(manager.stage_timer) : 0,
                manager.done_callback,
                (uint32_t)state.count};
            CHECK(same_words(actual, expected, 7, item, step, "state"));
            for (size_t index = 0; index < state.count; ++index) {
                CHECK(words(file, expected, 7));
                const Event *event = state.events + index;
                const uint32_t action[] = {
                    (uint32_t)event->operation,
                    event->active,
                    bits((float)event->stage_index),
                    bits(event->timer),
                    event->stage_timer_present ? 3u : 0u,
                    event->stage_timer_present ? bits(event->stage_timer) : 0,
                    event->done_callback};
                CHECK(same_words(action, expected, 7, item, step, "boundary") &&
                      (unsigned)event->operation < 3);
                ++event_counts[event->operation];
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera sequence reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8192 native camera manager commands/states match; %zu action, "
           "%zu custom-condition and %zu done boundaries match.\n",
           event_counts[0], event_counts[1], event_counts[2]);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "--apk") == 0) {
        return apk_sequences(argv[2]) ? 0 : 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        bool okay = file && compare_native(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    if (argc != 1) {
        return 2;
    }
    if (!duration_and_interruption() || !conditions_and_failures() ||
        !static_reader_checks()) {
        return 1;
    }
    puts("Camera stage timing, interruption, conditions, callback order and failure "
         "boundaries pass.");
    return 0;
}
