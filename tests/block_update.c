#include "ab/block_script.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Block update failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct UpdateCheck UpdateCheck;
typedef struct {
    UpdateCheck *check;
    uint32_t id;
} Identity;

struct UpdateCheck {
    AbBlockScriptManager manager;
    AbBlockScript *registered[8];
    AbBlockScript scripts[2];
    Identity identities[2];
    AbBlockScriptUpdate update;
    AbBlockEventObject object;
    AbBlockEventObject *objects[1];
    AbBlockEventScript global;
    AbBlockEventScript *attached[1];
    AbBlockEventBinding bindings[2];
    uint32_t handlers, mutation, selected, appended, count, events[16][6];
    bool fail;
};

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static float from_bits(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t to_bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static bool record(UpdateCheck *check, uint32_t id, uint32_t seconds, uint32_t time,
                   uint32_t selected) {
    CHECK(check->count < 16);
    uint32_t *event = check->events[check->count++];
    event[0] = id;
    event[1] = seconds;
    event[2] = time;
    event[3] = check->handlers;
    event[4] = (uint32_t)check->manager.registered.count;
    event[5] = selected;
    return true;
}

static bool update_script(AbBlockScript *script, float seconds, float time,
                          AbError *error) {
    Identity *identity = script->events.context;
    UpdateCheck *check = identity->check;
    uint32_t id = identity->id;
    CHECK(record(check, id, to_bits(seconds), to_bits(time), check->selected));
    if (check->mutation & 1) {
        check->handlers ^= 1u << (2 - id);
    }
    if (check->mutation & 2) {
        check->selected ^= 1;
        check->update.event = span(check->selected ? "skip" : "evt");
    }
    if ((check->mutation & 4) && !check->appended) {
        check->appended = 1;
        CHECK(ab_block_script_register(&check->manager, check->scripts + 1, error));
    }
    if (check->mutation & 8) {
        check->handlers &= ~(1u << (id - 1));
    }
    for (size_t index = 0; index < 2; ++index) {
        check->scripts[index].update =
            check->handlers & (1u << index) ? update_script : NULL;
    }
    return !check->fail || ab_fail(error, "Test registered update failure");
}

static bool global_record(AbBlockEventScript *script, const AbDataValue *arguments,
                          size_t count, uint32_t selected) {
    CHECK(count == 2 && arguments[0].kind == AB_DATA_NUMBER &&
          arguments[1].kind == AB_DATA_NUMBER);
    return record(script->context, 0, arguments[0].bits, arguments[1].bits, selected);
}

static bool global_evt(AbBlockEventScript *script, AbBlockEventObject *object,
                       const AbDataTable *definition, const AbDataValue *arguments,
                       size_t count, AbError *error) {
    (void)object;
    (void)definition;
    (void)error;
    return global_record(script, arguments, count, 0);
}

static bool global_skip(AbBlockEventScript *script, AbBlockEventObject *object,
                        const AbDataTable *definition, const AbDataValue *arguments,
                        size_t count, AbError *error) {
    (void)object;
    (void)definition;
    (void)error;
    return global_record(script, arguments, count, 1);
}

static bool applicable(AbBlockScript *script, const AbDataTable *definition,
                       bool *output, AbError *error) {
    (void)script;
    (void)definition;
    (void)error;
    *output = true;
    return true;
}

static bool world(void *context, AbSpan name, bool *present, AbError *error) {
    (void)context;
    (void)name;
    (void)error;
    *present = true;
    return true;
}

static bool definition(void *context, AbBlockEventObject *object,
                       const AbDataTable **output, AbError *error) {
    (void)context;
    (void)object;
    (void)error;
    *output = NULL;
    return true;
}

static bool prepare(UpdateCheck *check, const uint32_t input[12]) {
    memset(check, 0, sizeof(*check));
    CHECK(input[2] <= 8 && input[3] <= 15 && input[4] <= 6 && input[5] <= 1);
    check->mutation = input[3];
    check->selected = input[5];
    check->manager.registered = (AbBlockScriptList){check->registered, 0, 8};
    for (size_t index = 0; index < 2; ++index) {
        uint32_t kind = index ? input[2] / 3 : input[2] % 3;
        check->identities[index] = (Identity){check, (uint32_t)index + 1};
        check->scripts[index] =
            (AbBlockScript){.events.context = check->identities + index,
                            .can_apply = applicable,
                            .update = kind == 2 ? update_script : NULL};
        if (kind == 2) {
            check->handlers |= 1u << index;
        }
    }
    for (size_t index = 0; index < input[4]; ++index) {
        CHECK(input[6 + index] >= 1 && input[6 + index] <= 2 &&
              ab_block_script_register(&check->manager,
                                       check->scripts + input[6 + index] - 1, NULL));
    }
    check->bindings[0] = (AbBlockEventBinding){span("evt"), global_evt};
    check->bindings[1] = (AbBlockEventBinding){span("skip"), global_skip};
    check->global = (AbBlockEventScript){check->bindings, 2, check};
    check->attached[0] = &check->global;
    check->object = (AbBlockEventObject){.name = span("global"),
                                         .scripts = check->attached,
                                         .count = 1,
                                         .scripts_present = true,
                                         .tracked = true,
                                         .capacity = 1};
    check->objects[0] = &check->object;
    check->update = (AbBlockScriptUpdate){check->objects,
                                          1,
                                          {check, definition, world},
                                          span(check->selected ? "skip" : "evt")};
    return true;
}

static bool edge_checks(void) {
    const uint32_t input[12] = {0, 0, 8, 15, 1, 0, 1};
    UpdateCheck check;
    CHECK(prepare(&check, input));
    check.fail = true;
    CHECK(!ab_block_scripts_update(&check.manager, &check.update, 0, 0, NULL) &&
          check.count == 1 && check.appended && check.manager.registered.count == 2);
    CHECK(!ab_block_scripts_update(NULL, &check.update, 0, 0, NULL));
    CHECK(!ab_block_scripts_update(&check.manager, NULL, 0, 0, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_BLOCK_UPDATE_V1 4096\n") == 0);
    size_t calls = 0, scripts = 0, appends = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[12], expected[5];
        CHECK(read_words(file, input, 12) && read_words(file, expected, 5) &&
              expected[0] <= 16);
        UpdateCheck check;
        CHECK(prepare(&check, input));
        AbError error = {0};
        if (!ab_block_scripts_update(&check.manager, &check.update, from_bits(input[0]),
                                     from_bits(input[1]), &error)) {
            fprintf(stderr, "Native block update %zu: %s\n", item, error.message);
            return false;
        }
        uint32_t actual[] = {check.count, check.handlers,
                             (uint32_t)check.manager.registered.count, check.selected,
                             check.appended};
        CHECK(memcmp(actual, expected, sizeof(actual)) == 0);
        for (size_t index = 0; index < expected[0]; ++index) {
            uint32_t event[6];
            CHECK(read_words(file, event, 6));
            for (size_t field = 0; field < 6; ++field) {
                if (event[field] != check.events[index][field]) {
                    fprintf(stderr, "Block update %zu call %zu field %zu differs\n",
                            item, index, field);
                    return false;
                }
            }
            scripts += event[0] != 0;
        }
        calls += check.count;
        appends += check.appended;
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB block update reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native block-update cases match: %zu calls, %zu script updates, "
           "4096 global events, %zu in-pass appends\n",
           calls, scripts, appends);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2 || !edge_checks()) {
        return 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        bool okay = file && native_checks(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    puts("Registered block update failure boundaries and storage checks pass");
    return 0;
}
