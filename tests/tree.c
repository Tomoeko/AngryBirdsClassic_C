#include "ab/physics_tree.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t values[64];
    size_t count;
    size_t limit;
    bool invalid;
} QueryResult;

static float from_bits(uint32_t bits) {
    float number;
    memcpy(&number, &bits, sizeof(number));
    return number;
}

static uint32_t to_bits(float number) {
    uint32_t bits;
    memcpy(&bits, &number, sizeof(bits));
    return bits;
}

static bool record_query(void *context, int32_t proxy, uintptr_t payload) {
    QueryResult *result = context;
    if (proxy < 0 || !payload || payload > 64 || result->count >= 64) {
        result->invalid = true;
        return false;
    }
    result->values[result->count++] = (uint32_t)payload;
    return result->count < result->limit;
}

static bool edge_checks(void) {
    AbDynamicTree tree = {0};
    AbError error = {0};
    AbAabb box = {{0, 0}, {1, 1}};
    QueryResult result = {.limit = 64};
    CHECK(!ab_tree_open(15, &tree, &error));
    CHECK(!ab_tree_open(SIZE_MAX, &tree, &error));
    CHECK(ab_tree_open(16, &tree, &error));
    CHECK(tree.root == -1 && tree.count == 0 && tree.capacity == 16);
    CHECK(ab_tree_query(&tree, box, record_query, &result, &error));
    CHECK(result.count == 0);
    int32_t proxies[8];
    for (size_t index = 0; index < 8; ++index) {
        CHECK(ab_tree_create(&tree, box, index + 1, proxies + index, &error));
    }
    CHECK(tree.count == 15);
    AbTreeNode snapshot[16];
    memcpy(snapshot, tree.nodes, sizeof(snapshot));
    int32_t extra = -3;
    CHECK(!ab_tree_create(&tree, box, 9, &extra, &error));
    CHECK(tree.count == 15 && tree.capacity == 16 && extra == -3 &&
          memcmp(snapshot, tree.nodes, sizeof(snapshot)) == 0);
    bool moved = true;
    CHECK(ab_tree_move(&tree, proxies[0], box, (AbVec2){100, 100}, &moved));
    CHECK(!moved);
    result.limit = 3;
    CHECK(ab_tree_query(&tree, box, record_query, &result, &error));
    CHECK(result.count == 3 && !result.invalid);
    CHECK(!ab_tree_destroy(&tree, -1));
    CHECK(!ab_tree_destroy(&tree, INT32_MAX));
    CHECK(!ab_tree_move(&tree, proxies[0], box, (AbVec2){0, 0}, NULL));
    box.upper.x = from_bits(0x7fc00000);
    CHECK(!ab_tree_create(&tree, box, 9, &extra, &error));
    CHECK(!ab_tree_query(&tree, box, record_query, &result, &error));
    for (size_t index = 0; index < 8; ++index) {
        CHECK(ab_tree_destroy(&tree, proxies[index]));
    }
    CHECK(tree.count == 0 && tree.root == -1);
    ab_tree_close(&tree);
    ab_tree_close(&tree);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static AbAabb read_bounds(const uint32_t words[4]) {
    return (AbAabb){{from_bits(words[0]), from_bits(words[1])},
                    {from_bits(words[2]), from_bits(words[3])}};
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t operation, int32_t node) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Tree operation %zu node %" PRId32 " field %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    operation, node, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool compare_tree(FILE *file, const AbDynamicTree *tree, size_t operation) {
    uint32_t expected_state[6];
    CHECK(read_words(file, expected_state, 6));
    const uint32_t actual_state[] = {
        (uint32_t)tree->root,
        (uint32_t)tree->count,
        (uint32_t)tree->capacity,
        (uint32_t)tree->free_head,
        0,
        tree->insertion_count,
    };
    CHECK(compare_words(actual_state, expected_state, 6, operation, -1));
    for (int32_t index = 0; index < tree->capacity; ++index) {
        const AbTreeNode *node = tree->nodes + index;
        uint32_t expected[9], actual[9] = {0};
        CHECK(read_words(file, expected, 9));
        actual[5] = (uint32_t)node->parent;
        actual[8] = (uint32_t)node->height;
        if (node->height >= 0) {
            actual[0] = to_bits(node->bounds.lower.x);
            actual[1] = to_bits(node->bounds.lower.y);
            actual[2] = to_bits(node->bounds.upper.x);
            actual[3] = to_bits(node->bounds.upper.y);
            actual[4] = (uint32_t)node->payload;
            actual[6] = (uint32_t)node->children[0];
            actual[7] = (uint32_t)node->children[1];
        }
        CHECK(compare_words(actual, expected, 9, operation, index));
    }
    return true;
}

static bool native_checks(FILE *file, AbDynamicTree *tree) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_TREE_V1 4096\n") == 0);
    AbError error = {0};
    CHECK(ab_tree_open(128, tree, &error));
    int32_t proxies[64];
    for (size_t index = 0; index < 64; ++index) {
        proxies[index] = -1;
    }
    size_t query_values = 0;
    for (size_t operation = 0; operation < 4096; ++operation) {
        uint32_t input[8], expected_result;
        CHECK(read_words(file, input, 8) && input[0] <= 4 && input[1] < 64);
        CHECK(read_words(file, &expected_result, 1));
        AbAabb bounds = read_bounds(input + 2);
        AbVec2 displacement = {from_bits(input[6]), from_bits(input[7])};
        uint32_t result = 0;
        size_t slot = input[1];
        if (input[0] == 0) {
            CHECK(proxies[slot] == -1 &&
                  ab_tree_create(tree, bounds, slot + 1, proxies + slot, &error));
            result = (uint32_t)proxies[slot];
        } else if (input[0] == 1) {
            bool moved;
            CHECK(ab_tree_move(tree, proxies[slot], bounds, displacement, &moved));
            result = moved ? 1u : 0u;
        } else if (input[0] == 2) {
            CHECK(ab_tree_destroy(tree, proxies[slot]));
            proxies[slot] = -1;
        } else if (input[0] == 3) {
            CHECK(ab_tree_shift(tree, displacement));
        }
        CHECK(compare_words(&result, &expected_result, 1, operation, -2));
        CHECK(compare_tree(file, tree, operation));
        uint32_t query_input[5], expected_count, expected_values[64];
        CHECK(read_words(file, query_input, 5));
        CHECK(read_words(file, &expected_count, 1) && expected_count <= 64);
        if (expected_count) {
            CHECK(read_words(file, expected_values, expected_count));
        }
        QueryResult query = {.limit = query_input[4]};
        CHECK(ab_tree_query(tree, read_bounds(query_input), record_query, &query,
                            &error));
        CHECK(!query.invalid && query.count == expected_count);
        CHECK(compare_words(query.values, expected_values, expected_count, operation,
                            -3));
        query_values += expected_count;
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB tree reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 tree states and queries match; %zu ordered query results.\n",
           query_values);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_tree_tests [NATIVE_TREE_TRACE]\n");
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
        fprintf(stderr, "Cannot open native tree trace\n");
        return 1;
    }
    AbDynamicTree tree = {0};
    bool okay = native_checks(file, &tree);
    ab_tree_close(&tree);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
