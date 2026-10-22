#include "ab/object_update.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object update failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct UpdateCheck UpdateCheck;
typedef struct {
    UpdateCheck *check;
    uint32_t id;
} Identity;

struct UpdateCheck {
    AbObjectUpdates state;
    AbObjectUpdateFunction functions[3];
    Identity identities[3];
    AbObjectUpdateList lists[16];
    const AbObjectUpdateFunction *items[16][4];
    size_t next_list;
    uint32_t calls[4][2], count, mutation;
    bool mutated, fail;
};

static uint32_t to_bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static float from_bits(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static bool add(UpdateCheck *check, uint32_t id, AbError *error) {
    bool promotion = check->state.object.kind == AB_OBJECT_UPDATE_FUNCTION;
    CHECK(id >= 1 && id <= 3 && (!promotion || check->next_list < 16));
    AbObjectUpdateList *list = promotion ? check->lists + check->next_list : NULL;
    if (!ab_object_update_add(&check->state, check->functions + id - 1, list, error)) {
        return false;
    }
    check->next_list += promotion;
    return true;
}

static bool invoke(void *context, void *object, float seconds, AbError *error) {
    Identity *identity = context;
    UpdateCheck *check = identity->check;
    CHECK(object == check && check->count < 4);
    check->calls[check->count][0] = identity->id;
    check->calls[check->count++][1] = to_bits(seconds);
    if (check->mutation && !check->mutated) {
        check->mutated = true;
        if (check->mutation == 1) {
            if (!add(check, 3, error)) {
                return false;
            }
        } else if (check->mutation == 2) {
            if (!ab_object_update_remove(&check->state,
                                         check->functions + identity->id - 1, error)) {
                return false;
            }
        } else {
            CHECK(ab_object_update_clear(&check->state, error));
        }
    }
    return !check->fail || ab_fail(error, "Test object update callback failure");
}

static void prepare(UpdateCheck *check) {
    memset(check, 0, sizeof(*check));
    for (size_t index = 0; index < 3; ++index) {
        check->identities[index] = (Identity){check, (uint32_t)index + 1};
        check->functions[index] =
            (AbObjectUpdateFunction){check->identities + index, invoke};
    }
    for (size_t index = 0; index < 16; ++index) {
        check->lists[index] = (AbObjectUpdateList){check->items[index], 0, 4};
    }
}

static bool value(UpdateCheck *check, AbObjectUpdateValue state, uint32_t output[6]) {
    memset(output, 0, 6 * sizeof(*output));
    if (state.kind == AB_OBJECT_UPDATE_NONE) {
        return true;
    }
    output[0] = state.kind == AB_OBJECT_UPDATE_FUNCTION ? 6 : 5;
    output[1] =
        state.kind == AB_OBJECT_UPDATE_FUNCTION ? 1 : (uint32_t)state.list->count;
    CHECK(output[1] <= 4);
    for (size_t index = 0; index < output[1]; ++index) {
        const AbObjectUpdateFunction *function = state.kind == AB_OBJECT_UPDATE_FUNCTION
                                                     ? state.function
                                                     : state.list->items[index];
        CHECK(function >= check->functions && function < check->functions + 3);
        output[2 + index] = (uint32_t)(function - check->functions) + 1;
    }
    return true;
}

static bool operate(UpdateCheck *check, uint32_t word, float seconds, bool *query) {
    uint32_t kind = word % 6, id = word / 6 + 1;
    CHECK(word < 18);
    check->count = 0;
    check->mutated = false;
    *query = false;
    switch (kind) {
        case 0:
            return add(check, id, NULL);
        case 1:
            return ab_object_update_remove(&check->state, check->functions + id - 1,
                                           NULL);
        case 2:
            return ab_object_update_clear(&check->state, NULL);
        case 3:
            return ab_object_update_has(&check->state, check->functions + id - 1, query,
                                        NULL);
        case 4:
            return ab_object_update_call(check->state.object, check, seconds, NULL);
        default:
            return ab_object_update_call(check->state.registered, check, seconds, NULL);
    }
}

static bool edge_checks(void) {
    UpdateCheck check;
    prepare(&check);
    CHECK(!ab_object_update_remove(&check.state, check.functions, NULL));
    CHECK(add(&check, 1, NULL) && add(&check, 1, NULL) &&
          check.state.object.kind == AB_OBJECT_UPDATE_LIST &&
          check.state.object.list->count == 2);
    CHECK(!add(&check, 1, NULL) && check.state.object.list->count == 2);
    CHECK(ab_object_update_remove(&check.state, check.functions, NULL) &&
          check.state.object.kind == AB_OBJECT_UPDATE_FUNCTION &&
          check.state.registered.kind == AB_OBJECT_UPDATE_LIST &&
          check.state.registered.list->count == 1);
    AbObjectUpdateList *alias = check.state.registered.list;
    CHECK(!ab_object_update_add(&check.state, check.functions + 1, alias, NULL));
    CHECK(add(&check, 2, NULL) && check.state.registered.list != alias);
    check.fail = true;
    CHECK(!ab_object_update_call(check.state.registered, &check, 0, NULL) &&
          check.count == 1);
    CHECK(!ab_object_update_add(NULL, check.functions, NULL, NULL));
    CHECK(!ab_object_update_has(&check.state, check.functions, NULL, NULL));
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
          strcmp(header, "AB_ANDROID_OBJECT_UPDATE_V1 4096\n") == 0);
    size_t calls = 0, errors = 0, collapsed_aliases = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[18];
        CHECK(read_words(file, input, 18) && input[1] <= 3 && input[17] <= 3);
        UpdateCheck check;
        prepare(&check);
        check.mutation = input[17];
        for (size_t index = 0; index < input[1]; ++index) {
            CHECK(add(&check, input[2 + index], NULL));
        }
        for (size_t index = 5; index < 17; ++index) {
            uint32_t expected[15], actual[15];
            CHECK(read_words(file, expected, 15) && expected[2] <= 4 &&
                  input[index] < 18);
            bool query = false;
            actual[0] = !operate(&check, input[index], from_bits(input[0]), &query);
            actual[1] = query;
            actual[2] = check.count;
            CHECK(value(&check, check.state.object, actual + 3) &&
                  value(&check, check.state.registered, actual + 9));
            for (size_t field = 0; field < 15; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Object update case %zu op %zu field %zu: %08" PRIx32
                            " instead of %08" PRIx32 "\n",
                            item, index - 5, field, actual[field], expected[field]);
                    return false;
                }
            }
            for (size_t call = 0; call < expected[2]; ++call) {
                uint32_t event[2];
                CHECK(read_words(file, event, 2) &&
                      memcmp(event, check.calls[call], sizeof(event)) == 0);
            }
            calls += check.count;
            errors += actual[0];
            collapsed_aliases += check.state.object.kind == AB_OBJECT_UPDATE_FUNCTION &&
                                 check.state.registered.kind == AB_OBJECT_UPDATE_LIST;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB object update reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native object-update cases / 49152 operations match: %zu calls, "
           "%zu assertion/call errors, %zu collapsed registry aliases\n",
           calls, errors, collapsed_aliases);
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
    puts("Object update duplicate promotion, alias lifetime and failure checks pass");
    return 0;
}
