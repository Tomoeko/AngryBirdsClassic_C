#include "ab/z_order.h"
#include "z_order_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static size_t prepare(uint32_t sample, AbZOrderEntry entries[AB_Z_ORDER_MAXIMUM]) {
    uint32_t keys[AB_Z_ORDER_MAXIMUM];
    size_t count = z_order_case(sample, keys);
    for (size_t index = 0; index < count; ++index) {
        memcpy(&entries[index].z, keys + index, sizeof(entries[index].z));
        entries[index].identity = index + 1;
    }
    return count;
}

static bool edges(void) {
    if (!ab_z_order_sort(NULL, 0, NULL) || ab_z_order_sort(NULL, 1, NULL) ||
        ab_z_order_sort(NULL, SIZE_MAX, NULL))
        return false;
    AbZOrderEntry invalid[] = {{1, 42}, {NAN, 87}, {-1, 19}};
    AbZOrderEntry retained[3];
    memcpy(retained, invalid, sizeof(invalid));
    if (ab_z_order_sort(invalid, 3, NULL) || memcmp(invalid, retained, sizeof(invalid)))
        return false;
    for (uint32_t sample = 0; sample < AB_Z_ORDER_CASES; ++sample) {
        AbZOrderEntry entries[AB_Z_ORDER_MAXIMUM], original[AB_Z_ORDER_MAXIMUM];
        size_t count = prepare(sample, entries);
        memcpy(original, entries, count * sizeof(*entries));
        if (!ab_z_order_sort(entries, count, NULL))
            return false;
        bool seen[AB_Z_ORDER_MAXIMUM] = {0};
        for (size_t index = 0; index < count; ++index) {
            uintptr_t id = entries[index].identity;
            if (!id || id > count || seen[id - 1] ||
                (index && entries[index].z < entries[index - 1].z) ||
                memcmp(&entries[index].z, &original[id - 1].z, sizeof(float)))
                return false;
            seen[id - 1] = true;
        }
    }
    return true;
}

static bool compare(FILE *file) {
    char line[80];
    if (!fgets(line, sizeof(line), file) ||
        strcmp(line, "AB_ANDROID_Z_ORDER_V1 2048\n"))
        return false;
    for (uint32_t sample = 0; sample < AB_Z_ORDER_CASES; ++sample) {
        AbZOrderEntry entries[AB_Z_ORDER_MAXIMUM];
        size_t count = prepare(sample, entries);
        uint32_t actual;
        if (fscanf(file, "%" SCNx32, &actual) != 1 || actual != count ||
            fgetc(file) != '\n' || !ab_z_order_sort(entries, count, NULL))
            return false;
        for (size_t index = 0; index < count; ++index) {
            if (fscanf(file, "%" SCNx32, &actual) != 1 ||
                fgetc(file) != (index + 1 == count ? '\n' : ' '))
                return false;
            if (actual != entries[index].identity) {
                fprintf(stderr,
                        "Native z-order differs at %u/%zu: %u != %" PRIuPTR "\n",
                        sample, index, actual, entries[index].identity);
                return false;
            }
        }
    }
    if (!fgets(line, sizeof(line), file) ||
        strcmp(line, "AB z-order reference complete\n") || fgetc(file) != EOF ||
        ferror(file))
        return false;
    puts("Native numeric z-order: 2048 exact permutations, including equal keys.");
    return true;
}

int main(int argc, char **argv) {
    if (!edges())
        return 1;
    if (argc == 1)
        return 0;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
