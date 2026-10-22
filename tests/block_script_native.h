#ifndef AB_TEST_BLOCK_SCRIPT_NATIVE_H
#define AB_TEST_BLOCK_SCRIPT_NATIVE_H

#include "ab/block_script.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct NativeScriptCheck NativeScriptCheck;
typedef struct {
    NativeScriptCheck *check;
    uint32_t id;
} NativeScriptIdentity;

struct NativeScriptCheck {
    uint32_t input[20], events[128][7], event_count, version;
    AbBlockScriptManager manager;
    AbDataTable definitions[6];
    AbBlockScript scripts[2];
    NativeScriptIdentity identities[2];
    AbBlockScript *registered[4], *initialized[2];
    AbTrackedBlockScriptObject tracked[3];
    AbBlockEventScript *attached[3][32];
    AbBlockEventObject objects[3];
    uint32_t object_ids[3];
};

static bool record_native_script(AbBlockScript *script, uint32_t kind,
                                 AbBlockEventObject *object,
                                 const AbDataTable *definition) {
    NativeScriptIdentity *identity = script->events.context;
    NativeScriptCheck *check = identity->check;
    CHECK(check->event_count < 128);
    uint32_t *event = check->events[check->event_count++];
    event[0] = kind;
    event[1] = identity->id;
    event[2] = object ? *(const uint32_t *)object->context : 0;
    event[3] = definition ? (uint32_t)(definition - check->definitions) + 1 : 0;
    event[4] = object ? (uint32_t)object->count : 0;
    event[5] = (uint32_t)check->manager.initialized.count;
    event[6] = (uint32_t)check->manager.count;
    return true;
}

static void rebind_native_script(NativeScriptCheck *check, uint32_t mask) {
    if (check->input[3] & mask) {
        check->version ^= 1;
    }
}

static bool native_script_can_apply(AbBlockScript *script,
                                    const AbDataTable *definition, bool *applicable,
                                    AbError *error) {
    (void)error;
    NativeScriptIdentity *identity = script->events.context;
    NativeScriptCheck *check = identity->check;
    uint32_t id = (uint32_t)(definition - check->definitions) + 1;
    CHECK(record_native_script(script, 1, NULL, definition));
    rebind_native_script(check, 4);
    unsigned shift = ((identity->id - 1) * 3 + (id - 1) % 3) * 2;
    *applicable = ((check->input[0] >> shift) & 3) >= 2;
    return true;
}

static bool native_script_initialize(AbBlockScript *script, AbError *error) {
    (void)error;
    NativeScriptIdentity *identity = script->events.context;
    CHECK(record_native_script(script, 2, NULL, NULL));
    rebind_native_script(identity->check, 1);
    return true;
}

static bool native_script_apply(AbBlockScript *script, AbBlockEventObject *object,
                                const AbDataTable *definition, AbError *error) {
    (void)error;
    NativeScriptIdentity *identity = script->events.context;
    CHECK(record_native_script(script, 3, object, definition));
    rebind_native_script(identity->check, 2);
    return true;
}

static bool native_script_dispose(AbBlockScript *script, AbError *error) {
    (void)error;
    return record_native_script(script, 4, NULL, NULL);
}

static bool native_script_definition(void *context, AbBlockEventObject *object,
                                     const AbDataTable **output, AbError *error) {
    (void)error;
    NativeScriptCheck *check = context;
    uint32_t id = *(const uint32_t *)object->context;
    CHECK(id >= 1 && id <= 3);
    *output = check->definitions + check->version * 3 + id - 1;
    return true;
}

static void prepare_native_script(NativeScriptCheck *check, const uint32_t input[20]) {
    *check = (NativeScriptCheck){.version = input[19]};
    memcpy(check->input, input, sizeof(check->input));
    check->manager = (AbBlockScriptManager){.registered = {check->registered, 0, 4},
                                            .initialized = {check->initialized, 0, 2},
                                            .tracked = check->tracked,
                                            .capacity = 3};
    for (size_t index = 0; index < 2; ++index) {
        check->identities[index] = (NativeScriptIdentity){check, (uint32_t)index + 1};
        check->scripts[index] = (AbBlockScript){
            .events.context = check->identities + index,
            .can_apply = native_script_can_apply,
            .initialize = input[1] & (1u << index) ? native_script_initialize : NULL,
            .apply = input[2] & (1u << index) ? native_script_apply : NULL,
            .dispose = native_script_dispose,
        };
    }
    const char *const names[] = {"first", "second", "third"};
    for (size_t index = 0; index < 3; ++index) {
        check->object_ids[index] = (uint32_t)index + 1;
        check->objects[index] = (AbBlockEventObject){
            .name = span(names[index]),
            .scripts = check->attached[index],
            .context = check->object_ids + index,
            .scripts_present = (input[6] & (1u << index)) != 0,
            .capacity = 32,
        };
        if ((input[6] & input[7]) & (1u << index)) {
            check->attached[index][0] = &check->scripts[0].events;
            check->objects[index].count = 1;
        }
    }
}

static bool native_script_snapshot(NativeScriptCheck *check, uint32_t output[11]) {
    memset(output, 0, 11 * sizeof(*output));
    output[0] = check->version;
    output[1] = (uint32_t)check->manager.registered.count;
    output[2] = (uint32_t)check->manager.initialized.count;
    output[3] = (uint32_t)check->manager.count;
    for (size_t index = 0; index < 3; ++index) {
        output[4 + index] = (uint32_t)check->objects[index].count;
        output[7] |= check->objects[index].scripts_present ? 1u << index : 0;
        output[8] |= check->objects[index].tracked ? 1u << index : 0;
    }
    AbDataEntry entries[3];
    size_t indices[3];
    AbStringOrderNode nodes[4];
    AbBlockEventObject *objects[3];
    AbBlockScriptOrder order = {.entries = entries,
                                .entry_capacity = 3,
                                .strings = {indices, 3, nodes, 4},
                                .objects = objects,
                                .object_capacity = 3};
    CHECK(ab_block_script_order(&check->manager, &order, NULL));
    for (size_t index = 0; index < order.count; ++index) {
        uint32_t id = *(const uint32_t *)objects[index]->context;
        output[9] |= id << (index * 4);
    }
    output[10] = check->event_count;
    return true;
}

static bool read_native_script_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_native_script_snapshot(size_t item, const char *phase,
                                           const uint32_t actual[11],
                                           const uint32_t expected[11]) {
    for (size_t field = 0; field < 11; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr, "Native block script %zu %s field %zu: %u instead of %u\n",
                    item, phase, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool native_script_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_BLOCK_SCRIPT_V1 4096\n") == 0);
    size_t total = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[20], disposal[3], before[11], after[11], actual[11];
        CHECK(read_native_script_words(file, input, 20) &&
              read_native_script_words(file, disposal, 3) &&
              read_native_script_words(file, before, 11) &&
              read_native_script_words(file, after, 11) && input[4] <= 4 &&
              input[5] <= 6 && input[8] >= 1 && input[8] <= 3 && input[19] <= 1 &&
              disposal[0] <= 2 && after[10] <= 128);
        NativeScriptCheck check;
        prepare_native_script(&check, input);
        AbBlockEventHost host = {.context = &check,
                                 .definition = native_script_definition};
        for (size_t index = 0; index < input[4]; ++index) {
            uint32_t id = input[9 + index];
            CHECK(
                id >= 1 && id <= 2 &&
                ab_block_script_register(&check.manager, check.scripts + id - 1, NULL));
        }
        for (size_t index = 0; index < input[5]; ++index) {
            uint32_t id = input[13 + index];
            CHECK(id >= 1 && id <= 3 &&
                  ab_block_scripts_apply(&check.manager, check.objects + id - 1, host,
                                         NULL));
        }
        CHECK(native_script_snapshot(&check, actual) &&
              compare_native_script_snapshot(item, "before clear", actual, before));
        CHECK(check.manager.initialized.count == disposal[0]);
        size_t order[2];
        for (size_t index = 0; index < disposal[0]; ++index) {
            size_t found = check.manager.initialized.count;
            for (size_t entry = 0; entry < check.manager.initialized.count; ++entry) {
                NativeScriptIdentity *identity =
                    check.manager.initialized.items[entry]->events.context;
                if (identity->id == disposal[index + 1]) {
                    found = entry;
                }
            }
            CHECK(found < check.manager.initialized.count);
            order[index] = found;
        }
        CHECK(ab_block_scripts_clear(&check.manager, order, disposal[0], NULL));
        CHECK(ab_block_scripts_apply(&check.manager, check.objects + input[8] - 1, host,
                                     NULL));
        CHECK(native_script_snapshot(&check, actual) &&
              compare_native_script_snapshot(item, "after reapply", actual, after));
        for (size_t index = 0; index < check.event_count; ++index) {
            uint32_t event[7];
            CHECK(read_native_script_words(file, event, 7));
            if (memcmp(event, check.events[index], sizeof(event)) != 0) {
                fprintf(stderr, "Native block script %zu callback %zu differs\n", item,
                        index);
                return false;
            }
        }
        total += check.event_count;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB block script reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native block-script cases and %zu callbacks match with explicit "
           "disposal order\n",
           total);
    return true;
}

#endif
