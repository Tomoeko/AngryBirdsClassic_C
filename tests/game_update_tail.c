#include "ab/game_update_tail.h"
#include "ab/data_order.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Game tail failed at line %d: %s\n", __LINE__,             \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t word(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t state_flags(const AbGameTailState *state) {
    return (uint32_t)state->bird_fired | ((uint32_t)state->red_special_available << 1) |
           ((uint32_t)state->mighty_timer_present << 2) |
           ((uint32_t)state->skip_input << 3) |
           ((uint32_t)state->skip_trigger_specialty << 4);
}

typedef struct {
    AbGameTailState *state;
    AbGameTailObject *objects;
    size_t object_count;
    uint32_t flags, bird_mask, outside_mask;
    uint32_t events[48][14];
    size_t count;
    AbGameTailOperation fail_at;
    bool hooks;
    uint32_t hook_mutation;
    bool append_joint;
} Host;

#define TAIL_TEST_HOOK_CALL 22

static const char *const joint_names[] = {"first", "second", "third", "missing",
                                          "absent"};

static AbSpan joint_name(size_t index) {
    return (AbSpan){(const uint8_t *)joint_names[index], strlen(joint_names[index])};
}

static uint32_t joint_id(AbSpan name) {
    for (uint32_t index = 0; index < 5; ++index) {
        AbSpan candidate = joint_name(index);
        if (name.size == candidate.size &&
            memcmp(name.data, candidate.data, name.size) == 0) {
            return index + 1;
        }
    }
    return 0;
}

static bool host(void *context, AbGameTailCall *call, AbError *error) {
    Host *fixture = context;
    AbGameTailState *state = fixture->state;
    if (call->operation == AB_GAME_TAIL_HOOK_EXISTS) {
        CHECK(fixture->hooks && call->object);
        call->result = call->object->identity >= 1 && call->object->identity <= 3;
        return true;
    }
    static const uint32_t arity[] = {0, 2, 2, 3, 3, 3, 3, 4, 1, 2, 1, 1, 1,
                                     1, 1, 0, 2, 2, 3, 1, 0, 0, 2, 1, 1};
    bool joint_call = call->operation == AB_GAME_TAIL_CHECK_JOINT_LIMIT ||
                      call->operation == AB_GAME_TAIL_DESTROY_JOINT;
    if (fixture->count >= 48 || call->operation < AB_GAME_TAIL_SLING ||
        (call->operation > AB_GAME_TAIL_DRAW &&
         !((call->operation == TAIL_TEST_HOOK_CALL && fixture->hooks) ||
           (joint_call && state->joints)))) {
        return ab_fail(error, "Unexpected game tail callback");
    }
    uint32_t object = call->object ? (uint32_t)call->object->identity : 0;
    if (joint_call) {
        object = joint_id(call->joint_name);
        CHECK(object != 0 && call->seconds == 0 && call->time == 0);
    }
    if (call->operation == TAIL_TEST_HOOK_CALL) {
        CHECK(object >= 1 && object <= 3);
    }
    uint32_t *event = fixture->events[fixture->count++];
    event[0] = call->operation;
    event[1] = object;
    event[2] = arity[call->operation];
    event[3] = call->operation == AB_GAME_TAIL_PHYSICS_ENABLED ||
                       call->operation == AB_GAME_TAIL_DRAW
                   ? 0
                   : word(call->seconds);
    event[4] = call->operation == AB_GAME_TAIL_LEVEL_EFFECTS ||
                       call->operation == AB_GAME_TAIL_BLOCK_SCRIPTS ||
                       call->operation == AB_GAME_TAIL_OBJECT_SCRIPT
                   ? word(call->time)
                   : 0;
    event[5] = word(state->cursor.x);
    event[6] = word(state->cursor.y);
    event[7] = word(state->old_cursor.x);
    event[8] = word(state->old_cursor.y);
    event[9] = state_flags(state);
    event[10] = state->darkness_present;
    event[11] = state->darkness_present ? word(state->eagle_darkness) : 0;
    event[12] = state->time_since_present;
    event[13] = state->time_since_present ? word(state->time_since_start) : 0;
    if (call->operation == fixture->fail_at) {
        return ab_fail(error, "Controlled tail callback failure");
    }
    if (call->operation == AB_GAME_TAIL_DESTROY_JOINT && fixture->append_joint) {
        fixture->append_joint = false;
        CHECK(ab_game_joint_defer(state->joints->destroy, joint_name(1), error));
    }
    if (call->operation == AB_GAME_TAIL_SLING && (fixture->flags & 128)) {
        state->bird_fired = !state->bird_fired;
        state->red_special_available = !state->red_special_available;
        state->mighty_timer_present = !state->mighty_timer_present;
        if (state->darkness_present) {
            state->eagle_darkness += 0.125f;
        }
    }
    if (call->operation == TAIL_TEST_HOOK_CALL &&
        (fixture->hook_mutation & (1u << (object - 1)))) {
        state->cursor.x += 0.25f;
        state->cursor.y -= 0.5f;
        state->bird_fired = !state->bird_fired;
        if (state->darkness_present) {
            state->eagle_darkness += 0.125f;
        }
    }
    if (call->operation == AB_GAME_TAIL_SCORE) {
        if (fixture->flags & 64) {
            state->cursor.x += 0.5f;
            state->cursor.y -= 0.25f;
        }
        if (fixture->flags & 128) {
            for (size_t index = 0; index < fixture->object_count; ++index) {
                fixture->objects[index].script_present =
                    !fixture->objects[index].script_present;
            }
        }
    }
    if (call->operation == AB_GAME_TAIL_GFX_EFFECTS && (fixture->flags & 64)) {
        state->cursor.x += 0;
        state->cursor.y += 1;
    }
    if (call->operation == AB_GAME_TAIL_DRAW && (fixture->flags & 128)) {
        state->time_since_present = true;
        state->time_since_start = 1.25f;
    }
    if (call->operation == AB_GAME_TAIL_BIRD ||
        call->operation == AB_GAME_TAIL_BOUNDARIES) {
        if (!object || object > 3) {
            return ab_fail(error, "Invalid game tail callback object");
        }
        uint32_t mask = call->operation == AB_GAME_TAIL_BIRD ? fixture->bird_mask
                                                             : fixture->outside_mask;
        call->result = (mask & (1u << (object - 1))) != 0;
    }
    if (call->operation == AB_GAME_TAIL_PHYSICS_ENABLED) {
        call->result = (fixture->flags & 16) != 0;
    }
    return true;
}

static bool invoke_hook(void *context, void *object, float seconds, AbError *error) {
    AbGameTailCall call = {.operation = (AbGameTailOperation)TAIL_TEST_HOOK_CALL,
                           .object = object,
                           .seconds = seconds};
    return host(context, &call, error);
}

typedef struct {
    Host host;
    AbGameTailHook rows[3];
    size_t count, cursor, calls, pruned;
    bool append, fail_next;
} HookProducer;

static bool producer_begin(void *context, AbError *error) {
    (void)error;
    HookProducer *producer = context;
    producer->cursor = 0;
    return true;
}

static bool producer_next(void *context, AbGameTailHook **output, AbError *error) {
    HookProducer *producer = context;
    if (producer->fail_next && producer->cursor == 1)
        return ab_fail(error, "Controlled live-hook continuation failure");
    *output =
        producer->cursor < producer->count ? producer->rows + producer->cursor++ : NULL;
    return true;
}

static bool producer_prune(void *context, AbError *error) {
    (void)error;
    ++((HookProducer *)context)->pruned;
    return true;
}

static bool producer_invoke(void *context, void *object, float seconds,
                            AbError *error) {
    HookProducer *producer = context;
    ++producer->calls;
    if (producer->append)
        producer->count = 3;
    return invoke_hook(&producer->host, object, seconds, error);
}

static bool producer_checks(void) {
    AbGameTailState state = {0};
    HookProducer producer = {
        .host = {.state = &state, .hooks = true}, .count = 1, .append = true};
    AbGameTailObject objects[] = {{1, false}, {2, false}, {4, false}};
    AbObjectUpdateFunction function = {&producer, producer_invoke};
    AbObjectUpdateValue value = {AB_OBJECT_UPDATE_FUNCTION, {.function = &function}};
    AbObjectUpdates updates[] = {{value, value}, {value, value}, {value, value}};
    for (size_t index = 0; index < 3; ++index)
        producer.rows[index] =
            (AbGameTailHook){objects + index, updates + index, objects + index};
    AbGameTailHooks hooks = {.count = 1,
                             .context = &producer,
                             .begin = producer_begin,
                             .next = producer_next,
                             .prune = producer_prune};
    state.hooks = &hooks;
    CHECK(ab_game_update_tail(&state, NULL, 0, 0.125f, 1, host, &producer.host, NULL) &&
          producer.calls == 2 && producer.pruned == 1 && producer.cursor == 3 &&
          updates[2].registered.kind == AB_OBJECT_UPDATE_NONE &&
          updates[2].object.kind == AB_OBJECT_UPDATE_FUNCTION);
    producer.host.count = producer.calls = producer.pruned = 0;
    producer.count = 1;
    producer.fail_next = true;
    CHECK(
        !ab_game_update_tail(&state, NULL, 0, 0.125f, 1, host, &producer.host, NULL) &&
        producer.calls == 1 && producer.pruned == 0 &&
        producer.host.events[producer.host.count - 1][0] == TAIL_TEST_HOOK_CALL);
    hooks.next = NULL;
    producer.calls = 0;
    CHECK(!ab_game_update_tail(&state, NULL, 0, 0, 0, host, &producer.host, NULL) &&
          producer.calls == 0);
    return true;
}

static bool prepare_hooks(Host *fixture, const uint32_t data[22],
                          AbGameTailHook items[4], AbObjectUpdates states[4],
                          AbObjectUpdateFunction *function, AbGameTailObject *missing,
                          size_t *count) {
    static const char *const names[] = {"first", "second", "third", "missing"};
    CHECK(data[15] <= 15 && data[16] <= 7 && data[17] <= 4);
    AbDataEntry entries[4];
    size_t indices[4], identities[4];
    AbStringOrderNode nodes[4];
    AbDataTable table = {.entries = entries, .capacity = 4};
    *function = (AbObjectUpdateFunction){fixture, invoke_hook};
    for (size_t index = 0; index < 4; ++index) {
        AbObjectUpdateValue value = {AB_OBJECT_UPDATE_FUNCTION, {.function = function}};
        states[index] = (AbObjectUpdates){.object = value};
        if (data[15] & (1u << index)) {
            states[index].registered = value;
            identities[table.count] = index;
            entries[table.count++] = (AbDataEntry){
                {.kind = AB_DATA_STRING,
                 .string = {(const uint8_t *)names[index], strlen(names[index])}},
                {.kind = AB_DATA_BOOLEAN, .bits = 1}};
        }
    }
    AbStringOrderWorkspace workspace = {indices, 4, nodes, 4};
    size_t capacity;
    CHECK(ab_data_string_order_growing(&table, &workspace, &capacity, NULL) &&
          table.count == data[17]);
    for (size_t index = 0; index < table.count; ++index) {
        size_t id = identities[indices[index]] + 1;
        CHECK(data[18 + index] == id);
        AbGameTailObject *object = missing;
        for (size_t row = 0; row < fixture->object_count; ++row) {
            if (fixture->objects[row].identity == id) {
                object = fixture->objects + row;
            }
        }
        items[index] = (AbGameTailHook){object, states + id - 1, object};
    }
    *count = table.count;
    return true;
}

static bool edge_checks(void) {
    AbGameTailState state = {.cursor = {2, 3},
                             .old_cursor = {4, 5},
                             .skip_input = true,
                             .skip_trigger_specialty = true};
    Host fixture = {.state = &state};
    AbError error = {0};
    CHECK(!ab_game_update_tail(&state, NULL, 1, 0, 0, host, &fixture, &error));
    CHECK(!ab_game_update_tail(&state, NULL, 0, NAN, 0, host, &fixture, &error));
    CHECK(!ab_game_update_tail(NULL, NULL, 0, 0, 0, host, &fixture, &error));
    CHECK(fixture.count == 0 && state.old_cursor.x == 4 && state.skip_input);
    fixture.fail_at = AB_GAME_TAIL_SCORE;
    CHECK(!ab_game_update_tail(&state, NULL, 0, 0, 0, host, &fixture, &error));
    CHECK(state.old_cursor.x == 4 && state.skip_input);
    CHECK(fixture.events[fixture.count - 1][0] == AB_GAME_TAIL_SCORE);
    fixture = (Host){.state = &state};
    state.darkness_present = true;
    state.eagle_darkness = NAN;
    CHECK(!ab_game_update_tail(&state, NULL, 0, 0, 0, host, &fixture, &error));
    CHECK(fixture.count == 1 && state.old_cursor.x == 4);
    fixture = (Host){.state = &state};
    state.darkness_present = false;
    state.time_since_present = true;
    state.time_since_start = FLT_MAX;
    CHECK(!ab_game_update_tail(&state, NULL, 0, FLT_MAX, 0, host, &fixture, &error));
    CHECK(!state.skip_input && !state.skip_trigger_specialty);
    CHECK(state.old_cursor.x == 2 && state.old_cursor.y == 3);
    CHECK(fixture.events[fixture.count - 1][0] == AB_GAME_TAIL_DRAW);
    state =
        (AbGameTailState){.cursor = {2, 3}, .old_cursor = {4, 5}, .skip_input = true};
    fixture = (Host){.state = &state,
                     .hooks = true,
                     .fail_at = (AbGameTailOperation)TAIL_TEST_HOOK_CALL};
    AbGameTailObject object = {1, false};
    AbObjectUpdateFunction function = {&fixture, invoke_hook};
    AbObjectUpdateValue value = {AB_OBJECT_UPDATE_FUNCTION, {.function = &function}};
    AbObjectUpdates updates = {value, value};
    AbGameTailHook hook = {&object, &updates, &object};
    AbGameTailHooks hooks = {.items = &hook, .count = 1};
    state.hooks = &hooks;
    CHECK(!ab_game_update_tail(&state, &object, 1, 0, 0, host, &fixture, &error) &&
          fixture.count == 2 && state.old_cursor.x == 4 && state.skip_input);
    fixture = (Host){.state = &state, .hooks = true};
    object.identity = 4;
    CHECK(ab_game_update_tail(&state, NULL, 0, 0, 0, host, &fixture, &error) &&
          updates.registered.kind == AB_OBJECT_UPDATE_NONE &&
          updates.object.kind == AB_OBJECT_UPDATE_FUNCTION);
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool prepare_joints(size_t sample, const uint32_t data[22],
                           AbGameJointEntry ordered[4], AbGameJointQueue *queue,
                           AbGameTailJoints *joints) {
    CHECK(data[15] == (sample & 15u) && data[16] == ((sample >> 4) & 15u) &&
          data[17] == (sample / 64) % 5);
    AbDataEntry entries[4];
    uint32_t ids[4];
    AbDataTable table = {.entries = entries, .capacity = 4};
    for (uint32_t index = 0; index < 4; ++index) {
        if (data[15] & (1u << index)) {
            ids[table.count] = index;
            entries[table.count++] =
                (AbDataEntry){{.kind = AB_DATA_STRING, .string = joint_name(index)},
                              {.kind = AB_DATA_BOOLEAN, .bits = 1}};
        }
    }
    size_t indices[4], capacity;
    AbStringOrderNode nodes[4];
    AbStringOrderWorkspace workspace = {indices, 4, nodes, 4};
    CHECK(ab_data_string_order_growing(&table, &workspace, &capacity, NULL));
    for (size_t index = 0; index < 4; ++index) {
        if (index < table.count) {
            uint32_t id = ids[indices[index]];
            CHECK(data[18 + index] == id + 1);
            ordered[index] =
                (AbGameJointEntry){joint_name(id), (data[16] & (1u << id)) != 0};
        } else {
            CHECK(data[18 + index] == 0);
        }
    }
    for (size_t index = 0; index < data[17]; ++index) {
        CHECK(
            ab_game_joint_defer(queue, joint_name(((sample >> 8) + index) % 5), NULL));
    }
    *joints = (AbGameTailJoints){ordered, table.count, queue};
    return true;
}

static bool joint_edge_checks(void) {
    AbSpan names[2];
    AbGameJointQueue queue = {names, 0, 2};
    CHECK(!ab_game_joint_defer(NULL, joint_name(0), NULL) &&
          !ab_game_joint_defer(&queue, (AbSpan){NULL, 1}, NULL) && !queue.count &&
          ab_game_joint_defer(&queue, joint_name(0), NULL));
    AbGameJointEntry entry = {joint_name(0), true};
    AbGameTailJoints joints = {&entry, 1, &queue};
    AbGameTailState state = {.joints = &joints};
    Host fixture = {.state = &state, .append_joint = true};
    CHECK(ab_game_update_tail(&state, NULL, 0, 0, 0, host, &fixture, NULL) &&
          !queue.count && !fixture.append_joint);
    CHECK(fixture.events[2][0] == AB_GAME_TAIL_BOOMERANG &&
          fixture.events[3][0] == AB_GAME_TAIL_CHECK_JOINT_LIMIT &&
          fixture.events[4][0] == AB_GAME_TAIL_DESTROY_JOINT &&
          fixture.events[4][1] == 1 &&
          fixture.events[5][0] == AB_GAME_TAIL_DESTROY_JOINT &&
          fixture.events[5][1] == 2 &&
          fixture.events[6][0] == AB_GAME_TAIL_CHARACTER_ANIMATIONS);
    CHECK(ab_game_joint_defer(&queue, joint_name(0), NULL) &&
          ab_game_joint_defer(&queue, joint_name(0), NULL) &&
          !ab_game_joint_defer(&queue, joint_name(0), NULL) && queue.count == 2);
    fixture = (Host){.state = &state, .fail_at = AB_GAME_TAIL_DESTROY_JOINT};
    CHECK(!ab_game_update_tail(&state, NULL, 0, 0, 0, host, &fixture, NULL) &&
          queue.count == 2 && fixture.count == 5);
    return true;
}

static bool native_checks(FILE *file, bool hooks, bool joint_phase) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, joint_phase ? "AB_ANDROID_GAME_UPDATE_JOINTS_V1 4096\n"
                         : hooks     ? "AB_ANDROID_GAME_UPDATE_HOOKS_V1 4096\n"
                                     : "AB_ANDROID_GAME_UPDATE_TAIL_V1 4096\n") == 0);
    size_t hook_calls = 0, pruned = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[22] = {0}, expected[12], actual[12] = {0};
        CHECK(read_words(file, data, hooks || joint_phase ? 22 : 15));
        CHECK(read_words(file, expected, 12));
        uint32_t flags = data[8];
        AbGameTailState state = {.cursor = {number(data[4]), number(data[5])},
                                 .old_cursor = {number(data[6]), number(data[7])},
                                 .eagle_darkness = number(data[2]),
                                 .time_since_start = number(data[3]),
                                 .bird_fired = (flags & 1) != 0,
                                 .darkness_present = (flags & 2) != 0,
                                 .mighty_timer_present = (flags & 4) != 0,
                                 .red_special_available = (flags & 8) != 0,
                                 .time_since_present = (flags & 32) != 0,
                                 .skip_input = true,
                                 .skip_trigger_specialty = true};
        AbGameTailObject objects[3];
        uint32_t seen = 0;
        for (size_t index = 0; index < 3; ++index) {
            uint32_t id = data[12 + index];
            CHECK(id >= 1 && id <= 3 && !(seen & (1u << (id - 1))));
            seen |= 1u << (id - 1);
            objects[index] = (AbGameTailObject){id, (data[11] & (1u << (id - 1))) != 0};
        }
        Host fixture = {.state = &state,
                        .objects = objects,
                        .object_count = 3,
                        .flags = flags,
                        .bird_mask = data[9],
                        .outside_mask = data[10]};
        AbGameTailHook hook_items[4];
        AbObjectUpdates hook_states[4];
        AbObjectUpdateFunction hook_function;
        AbGameTailObject missing = {4, false};
        AbGameTailHooks hook_view = {.items = hook_items};
        if (hooks) {
            fixture.hooks = true;
            fixture.hook_mutation = data[16];
            CHECK(prepare_hooks(&fixture, data, hook_items, hook_states, &hook_function,
                                &missing, &hook_view.count));
            state.hooks = &hook_view;
        }
        AbSpan destroy_names[5];
        AbGameJointQueue destroy = {destroy_names, 0, 5};
        AbGameJointEntry joint_entries[4];
        AbGameTailJoints joints;
        if (joint_phase) {
            CHECK(prepare_joints(item, data, joint_entries, &destroy, &joints));
            state.joints = &joints;
            fixture.append_joint = (item & 2048u) != 0;
        }
        AbError error = {0};
        CHECK(ab_game_update_tail(&state, objects, 3, number(data[0]), number(data[1]),
                                  host, &fixture, &error));
        actual[0] = word(state.cursor.x);
        actual[1] = word(state.cursor.y);
        actual[2] = word(state.old_cursor.x);
        actual[3] = word(state.old_cursor.y);
        actual[4] = state.darkness_present;
        actual[5] = state.darkness_present ? word(state.eagle_darkness) : 0;
        actual[6] = state.time_since_present;
        actual[7] = state.time_since_present ? word(state.time_since_start) : 0;
        actual[8] = state_flags(&state);
        for (size_t index = 0; index < 3; ++index) {
            if (objects[index].script_present) {
                actual[9] |= 1u << (objects[index].identity - 1);
            }
        }
        actual[10] = (uint32_t)fixture.count;
        if (hooks) {
            for (size_t index = 0; index < 4; ++index) {
                if (hook_states[index].registered.kind != AB_OBJECT_UPDATE_NONE) {
                    actual[11] |= 1u << index;
                }
                CHECK(hook_states[index].object.kind == AB_OBJECT_UPDATE_FUNCTION);
            }
            pruned += (data[15] & 8) != 0;
        }
        if (joint_phase) {
            actual[11] = (uint32_t)destroy.count;
        }
        for (size_t field = 0; field < 12; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native tail case %zu field %zu: %08" PRIx32
                        " expected %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        for (size_t index = 0; index < fixture.count; ++index) {
            hook_calls += fixture.events[index][0] == TAIL_TEST_HOOK_CALL;
            uint32_t event[14];
            CHECK(read_words(file, event, 14));
            for (size_t field = 0; field < 14; ++field) {
                if (fixture.events[index][field] != event[field]) {
                    fprintf(stderr,
                            "Native tail case %zu call %zu field %zu: "
                            "%08" PRIx32 " expected %08" PRIx32 "\n",
                            item, index, field, fixture.events[index][field],
                            event[field]);
                    return false;
                }
            }
        }
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, joint_phase ? "AB game update joints reference complete\n"
                         : hooks     ? "AB game update hooks reference complete\n"
                                 : "AB game update tail reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original ordinary game tails match every callback/state word, "
         "including callback mutations, gates, signed zero and housekeeping.");
    if (hooks) {
        printf("Composed %zu registered object hook calls and %zu missing-world "
               "prunes with recovered initial name-map order\n",
               hook_calls, pruned);
    }
    return true;
}

int main(int argc, char **argv) {
    bool hooks = argc == 3 && strcmp(argv[1], "--hooks") == 0;
    bool joints = argc == 3 && strcmp(argv[1], "--joints") == 0;
    if (argc > 3 || (argc == 3 && !hooks && !joints)) {
        return 2;
    }
    if (argc == 1) {
        return edge_checks() && joint_edge_checks() && producer_checks() ? 0 : 1;
    }
    FILE *file = fopen(argv[hooks || joints ? 2 : 1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file, hooks, joints);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
