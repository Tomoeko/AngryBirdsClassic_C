#include "ab/block_script.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Block script failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct ScriptCheck ScriptCheck;
typedef struct {
    ScriptCheck *check;
    unsigned id;
} ScriptIdentity;

struct ScriptCheck {
    AbDataTable definitions[3];
    const AbDataTable *current;
    unsigned events[64][5];
    size_t count, definitions_read;
    unsigned fail_initialize, fail_dispose;
    bool applicable;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static bool record(AbBlockScript *script, unsigned kind, AbBlockEventObject *object,
                   const AbDataTable *definition) {
    ScriptIdentity *identity = script->events.context;
    ScriptCheck *check = identity->check;
    CHECK(check->count < 64);
    unsigned *event = check->events[check->count++];
    event[0] = kind;
    event[1] = identity->id;
    event[2] = object ? *(const unsigned *)object->context : 0;
    event[3] = definition ? (unsigned)(definition - check->definitions) + 1 : 0;
    event[4] = object ? (unsigned)object->count : 0;
    return true;
}

static bool can_apply(AbBlockScript *script, const AbDataTable *definition,
                      bool *applicable, AbError *error) {
    (void)error;
    ScriptIdentity *identity = script->events.context;
    *applicable = identity->check->applicable;
    return record(script, 1, NULL, definition);
}

static bool initialize(AbBlockScript *script, AbError *error) {
    (void)error;
    ScriptIdentity *identity = script->events.context;
    identity->check->current = identity->check->definitions + 1;
    return record(script, 2, NULL, NULL) &&
           identity->check->fail_initialize != identity->id;
}

static bool apply(AbBlockScript *script, AbBlockEventObject *object,
                  const AbDataTable *definition, AbError *error) {
    (void)error;
    ScriptIdentity *identity = script->events.context;
    identity->check->current = identity->check->definitions + 2;
    return record(script, 3, object, definition);
}

static bool dispose(AbBlockScript *script, AbError *error) {
    (void)error;
    ScriptIdentity *identity = script->events.context;
    return record(script, 4, NULL, NULL) &&
           identity->check->fail_dispose != identity->id;
}

static bool definition(void *context, AbBlockEventObject *object,
                       const AbDataTable **output, AbError *error) {
    (void)object;
    (void)error;
    ScriptCheck *check = context;
    ++check->definitions_read;
    *output = check->current;
    return true;
}

static bool edge_checks(void) {
    ScriptCheck check = {.applicable = true};
    check.current = check.definitions;
    ScriptIdentity identities[] = {{&check, 1}, {&check, 2}};
    AbBlockScript scripts[2];
    for (size_t index = 0; index < 2; ++index) {
        scripts[index] = (AbBlockScript){.events.context = identities + index,
                                         .can_apply = can_apply,
                                         .initialize = initialize,
                                         .apply = apply,
                                         .dispose = dispose};
    }
    AbBlockScript *registrations[3], *initialized[2];
    AbTrackedBlockScriptObject tracked[3];
    AbBlockScriptManager manager = {.registered = {registrations, 0, 3},
                                    .initialized = {initialized, 0, 2},
                                    .tracked = tracked,
                                    .capacity = 3};
    CHECK(!ab_block_script_register(NULL, scripts, NULL));
    CHECK(!ab_block_script_register(&manager, NULL, NULL));
    CHECK(ab_block_script_register(&manager, scripts, NULL));
    CHECK(ab_block_script_register(&manager, scripts + 1, NULL));
    CHECK(ab_block_script_register(&manager, scripts, NULL));
    CHECK(!ab_block_script_register(&manager, scripts, NULL) &&
          manager.registered.count == 3);
    unsigned ids[] = {1, 2, 3};
    AbBlockEventScript *attached[3][16];
    AbBlockEventObject objects[3];
    const char *names[] = {"first", "second", "third"};
    for (size_t index = 0; index < 3; ++index) {
        objects[index] = (AbBlockEventObject){.name = span(names[index]),
                                              .scripts = attached[index],
                                              .context = ids + index,
                                              .capacity = 16};
    }
    AbBlockEventHost host = {.context = &check, .definition = definition};
    CHECK(ab_block_scripts_apply(&manager, objects, host, NULL));
    CHECK(check.definitions_read == 1 && check.count == 8 &&
          manager.initialized.count == 2 && manager.count == 1 &&
          objects[0].scripts_present && objects[0].tracked && objects[0].count == 3 &&
          attached[0][0] == &scripts[0].events &&
          attached[0][1] == &scripts[1].events && attached[0][2] == attached[0][0]);
    const unsigned expected[8][4] = {{1, 1, 0, 1}, {2, 1, 0, 0}, {3, 1, 1, 1},
                                     {1, 2, 0, 1}, {2, 2, 0, 0}, {3, 2, 1, 1},
                                     {1, 1, 0, 1}, {3, 1, 1, 1}};
    for (size_t index = 0; index < 8; ++index) {
        CHECK(memcmp(check.events[index], expected[index], sizeof(expected[index])) ==
              0);
    }
    CHECK(check.events[2][4] == 1 && check.events[5][4] == 2 &&
          check.events[7][4] == 3);
    CHECK(ab_block_scripts_apply(&manager, objects + 1, host, NULL));
    CHECK(check.definitions_read == 2 && check.count == 14 && manager.count == 2 &&
          manager.initialized.count == 2 && check.events[8][3] == 3);
    CHECK(ab_block_scripts_apply(&manager, objects, host, NULL) && manager.count == 2 &&
          objects[0].count == 6 && check.count == 20);
    AbDataEntry entries[3];
    size_t indices[3];
    AbStringOrderNode nodes[4];
    AbBlockEventObject *ordered[3];
    AbBlockScriptOrder order = {.entries = entries,
                                .entry_capacity = 3,
                                .strings = {indices, 3, nodes, 4},
                                .objects = ordered,
                                .object_capacity = 3};
    CHECK(ab_block_script_order(&manager, &order, NULL) && order.count == 2 &&
          order.hash_capacity == 2 && ordered[0] != ordered[1]);
    size_t invalid[] = {0, 0}, disposal[] = {1, 0};
    CHECK(!ab_block_scripts_clear(&manager, invalid, 2, NULL) && check.count == 20);
    scripts[1].dispose = NULL;
    size_t partial[] = {0, 1};
    CHECK(!ab_block_scripts_clear(&manager, partial, 2, NULL) && check.count == 21 &&
          manager.initialized.count == 2 && manager.count == 2);
    scripts[1].dispose = dispose;
    check.fail_dispose = 2;
    CHECK(!ab_block_scripts_clear(&manager, disposal, 2, NULL) && check.count == 22 &&
          manager.initialized.count == 2);
    check.fail_dispose = 0;
    CHECK(ab_block_scripts_clear(&manager, disposal, 2, NULL) && check.count == 24 &&
          manager.initialized.count == 0 && manager.count == 0 &&
          manager.registered.count == 3 && !objects[0].tracked && !objects[1].tracked &&
          objects[0].count == 6 && objects[1].count == 3);
    check.fail_initialize = 1;
    objects[2].capacity = 0;
    CHECK(!ab_block_scripts_apply(&manager, objects + 2, host, NULL) &&
          manager.initialized.count == 1 && manager.count == 0 &&
          objects[2].scripts_present && objects[2].count == 0);
    check.fail_initialize = 0;
    CHECK(!ab_block_scripts_apply(&manager, objects + 2, host, NULL) &&
          manager.initialized.count == 1 && objects[2].count == 0);
    objects[2].capacity = 16;
    CHECK(ab_block_scripts_apply(&manager, objects + 2, host, NULL) &&
          manager.initialized.count == 2 && objects[2].count == 3);
    objects[2].tracked = false;
    CHECK(!ab_block_scripts_apply(&manager, objects, host, NULL) &&
          objects[0].count == 7 && manager.count == 1);
    CHECK(ab_block_scripts_clear(&manager, disposal, 2, NULL));
    CHECK(!ab_block_scripts_apply(NULL, objects, host, NULL));
    CHECK(!ab_block_scripts_apply(&manager, NULL, host, NULL));
    CHECK(!ab_block_script_order(&manager, NULL, NULL));
    CHECK(!ab_block_scripts_clear(NULL, NULL, 0, NULL));
    CHECK(ab_block_scripts_clear(&manager, NULL, 0, NULL));
    return true;
}

#include "block_script_native.h"

int main(int argc, char **argv) {
    if (argc > 2 || !edge_checks()) {
        return 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        bool okay = file && native_script_checks(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    puts("Block registration, cached definitions, initialization, attachment, disposal "
         "and limits pass");
    return 0;
}
