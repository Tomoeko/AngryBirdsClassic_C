#include "ab/integer_order.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Integer order failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbIntegerOrderWorkspace empty = {0};
    AbIntegerOrderLayout layout = {9, 7};
    CHECK(ab_integer_order_growing(NULL, 0, &empty, &layout, NULL) &&
          layout.array_capacity == 0 && layout.hash_capacity == 0);
    int32_t keys[] = {2, 3, 4};
    size_t indices[4], slots[4];
    AbIntegerOrderNode nodes[4];
    AbIntegerOrderWorkspace workspace = {indices, 4, slots, 4, nodes, 4};
    CHECK(ab_integer_order_growing(keys, 3, &workspace, &layout, NULL) &&
          layout.array_capacity == 4 && layout.hash_capacity == 0 && indices[0] == 0 &&
          indices[1] == 1 && indices[2] == 2);
    keys[0] = -1;
    keys[1] = 0;
    keys[2] = 1;
    CHECK(ab_integer_order_growing(keys, 3, &workspace, &layout, NULL) &&
          layout.array_capacity == 1 && layout.hash_capacity == 2 && indices[0] == 2);
    AbIntegerOrderLayout saved = layout;
    keys[1] = keys[0];
    CHECK(!ab_integer_order_growing(keys, 3, &workspace, &layout, NULL) &&
          layout.array_capacity == saved.array_capacity &&
          layout.hash_capacity == saved.hash_capacity);
    keys[1] = 16777217;
    CHECK(!ab_integer_order_growing(keys, 3, &workspace, &layout, NULL));
    keys[1] = 0;
    workspace.node_capacity = 2;
    CHECK(!ab_integer_order_growing(keys, 3, &workspace, &layout, NULL) &&
          !ab_integer_order_growing(NULL, 3, &workspace, &layout, NULL) &&
          !ab_integer_order_growing(keys, 3, NULL, &layout, NULL) &&
          !ab_integer_order_growing(keys, 3, &workspace, NULL, NULL));
    workspace.node_capacity = 4;
    keys[0] = -16777216;
    keys[1] = 16777216;
    CHECK(ab_integer_order_growing(keys, 3, &workspace, &layout, NULL));
    puts("Fresh integer table array density, hash/zero keys, exact-ID bounds and "
         "workspace validation pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_INTEGER_ORDER_V1 4096 3\n") == 0);
    size_t compared = 0, changed_copies = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t setup[2], key_words[64];
        int32_t keys[64];
        uint32_t labels[64];
        CHECK(words(file, setup, 2) && setup[0] <= 64 && setup[1] <= 1);
        size_t count = setup[0];
        if (count) {
            CHECK(words(file, key_words, count));
        }
        for (size_t index = 0; index < count; ++index) {
            memcpy(keys + index, key_words + index, sizeof(*keys));
            labels[index] = (uint32_t)index;
        }
        for (size_t pass = 0; pass < 3; ++pass) {
            uint32_t expected_layout[3], expected[64];
            CHECK(words(file, expected_layout, 3));
            if (count) {
                CHECK(words(file, expected, count));
            }
            size_t indices[64], array_slots[64];
            AbIntegerOrderNode nodes[64];
            AbIntegerOrderWorkspace workspace = {indices, 64,    array_slots,
                                                 64,      nodes, 64};
            AbIntegerOrderLayout layout;
            CHECK(ab_integer_order_growing(keys, count, &workspace, &layout, NULL));
            if (layout.array_capacity != expected_layout[0] ||
                layout.hash_capacity != expected_layout[1] ||
                count != expected_layout[2]) {
                fprintf(stderr,
                        "Integer order case %zu pass %zu capacities %zu/%zu "
                        "!= %" PRIu32 "/%" PRIu32 "\n",
                        item, pass, layout.array_capacity, layout.hash_capacity,
                        expected_layout[0], expected_layout[1]);
                return false;
            }
            int32_t next_keys[64];
            uint32_t next_labels[64];
            bool changed = false;
            for (size_t index = 0; index < count; ++index) {
                CHECK(indices[index] < count);
                uint32_t actual = labels[indices[index]];
                if (actual != expected[index]) {
                    fprintf(stderr,
                            "Integer order case %zu pass %zu item %zu: "
                            "%" PRIu32 " != %" PRIu32 "\n",
                            item, pass, index, actual, expected[index]);
                    return false;
                }
                next_keys[index] = keys[indices[index]];
                next_labels[index] = actual;
                changed |= actual != labels[index];
            }
            changed_copies += pass && changed;
            for (size_t index = 0; index < count; ++index) {
                keys[index] = next_keys[index];
                labels[index] = next_labels[index];
            }
            compared += count;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB integer order reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All %zu entries across 4096 native integer tables and two original "
           "deep copies match; %zu copies change traversal order.\n",
           compared, changed_copies);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
