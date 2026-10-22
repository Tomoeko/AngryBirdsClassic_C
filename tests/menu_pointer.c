#include "ab/menu_pointer.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Menu pointer failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t consume;
    uint32_t calls[16][4];
    size_t count;
    bool fail;
} Boundaries;

typedef struct {
    Boundaries *boundaries;
    unsigned slot;
} Route;

static uint32_t bits(float value) {
    uint32_t word;
    memcpy(&word, &value, sizeof(word));
    return word;
}

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static bool route(void *context, AbMenuPointerEvent event, AbVec2 cursor,
                  bool *consumed, AbError *error) {
    (void)error;
    Route *entry = context;
    Boundaries *boundaries = entry->boundaries;
    CHECK(boundaries->count < 16 && (unsigned)event < 4);
    uint32_t *call = boundaries->calls[boundaries->count++];
    call[0] = (uint32_t)event;
    call[1] = entry->slot + 1;
    call[2] = bits(cursor.x);
    call[3] = bits(cursor.y);
    *consumed =
        (boundaries->consume & (1u << ((unsigned)event * 4 + entry->slot))) != 0;
    return !boundaries->fail;
}

static bool edge_checks(void) {
    Boundaries boundaries = {.consume = UINT32_C(0xffff)};
    Route entries[4];
    AbMenuPointerRoute routes[4];
    for (unsigned index = 0; index < 4; ++index) {
        entries[index] = (Route){&boundaries, index};
        routes[index] = (AbMenuPointerRoute){route, entries + index};
    }
    AbMenuPointerInput input = {.cursor = {12, 34}, .events = 15, .allow_input = true};
    AbMenuPointerResult result;
    CHECK(ab_menu_pointer_filter(routes, 4, &input, &result, NULL) &&
          result.present == 15 && !result.accepted && !result.copy_touches &&
          boundaries.count == 4 && boundaries.calls[0][0] == 0 &&
          boundaries.calls[1][0] == 1 && boundaries.calls[3][0] == 3);
    boundaries = (Boundaries){0};
    CHECK(ab_menu_pointer_filter(routes, 4, &input, &result, NULL) &&
          boundaries.count == 16 && result.accepted == 15 && result.copy_touches);
    input.events = 0;
    CHECK(ab_menu_pointer_filter(NULL, 0, &input, &result, NULL) &&
          result.present == 8 && result.accepted == 0 && result.copy_touches);
    input.allow_input = false;
    CHECK(ab_menu_pointer_filter(routes, 4, &input, &result, NULL) &&
          result.present == 8 && !result.accepted && !result.copy_touches);
    AbMenuPointerResult saved = result;
    input.allow_input = true;
    input.events = 1;
    boundaries = (Boundaries){.fail = true};
    CHECK(!ab_menu_pointer_filter(routes, 4, &input, &result, NULL) &&
          memcmp(&result, &saved, sizeof(saved)) == 0 && boundaries.count == 1);
    input.events = 16;
    CHECK(!ab_menu_pointer_filter(routes, 4, &input, &result, NULL));
    input.events = 0;
    input.cursor.x = NAN;
    CHECK(!ab_menu_pointer_filter(routes, 4, &input, &result, NULL) &&
          !ab_menu_pointer_filter(NULL, 1, &input, &result, NULL) &&
          !ab_menu_pointer_filter(routes, 4, NULL, &result, NULL) &&
          !ab_menu_pointer_filter(routes, 4, &input, NULL, NULL));
    puts("Menu pointer independent routing, first-consumer stop, key presence, "
         "whole-touch gate and failed-output preservation pass.");
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
          strcmp(header, "AB_ANDROID_MENU_POINTER_V1 4096\n") == 0);
    size_t callbacks = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[10], touches[8][3], expected[5];
        CHECK(words(file, input, 10) && input[5] <= 8);
        for (size_t index = 0; index < input[5]; ++index) {
            CHECK(words(file, touches[index], 3));
        }
        CHECK(words(file, expected, 5) && expected[4] <= 16);
        Boundaries boundaries = {.consume = input[2]};
        Route entries[4];
        AbMenuPointerRoute routes[4];
        uint32_t seen = 0;
        for (unsigned index = 0; index < 4; ++index) {
            CHECK(input[6 + index] >= 1 && input[6 + index] <= 4);
            unsigned slot = input[6 + index] - 1;
            CHECK(!(seen & (1u << slot)));
            seen |= 1u << slot;
            entries[index] = (Route){&boundaries, slot};
            routes[index] =
                (AbMenuPointerRoute){.callback = input[1] & (1u << slot) ? NULL : route,
                                     .context = entries + index};
        }
        AbMenuPointerInput source = {.cursor = {number(input[3]), number(input[4])},
                                     .events = input[0] & 15,
                                     .allow_input = (input[0] & 16) != 0};
        AbMenuPointerResult result;
        CHECK(ab_menu_pointer_filter(routes, 4, &source, &result, NULL));
        uint32_t actual[] = {result.present, result.accepted, result.copy_touches,
                             result.copy_touches ? input[5] : 0,
                             (uint32_t)boundaries.count};
        for (size_t field = 0; field < 5; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Menu pointer case %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        for (size_t index = 0; index < expected[4]; ++index) {
            uint32_t call[4];
            CHECK(words(file, call, 4) &&
                  memcmp(call, boundaries.calls[index], sizeof(call)) == 0);
        }
        callbacks += boundaries.count;
        for (size_t index = 0; index < input[5]; ++index) {
            uint32_t member[3];
            CHECK(words(file, member, 3) &&
                  member[0] == (uint32_t)result.copy_touches &&
                  member[1] == (result.copy_touches ? touches[index][1] : 0) &&
                  member[2] == (result.copy_touches ? touches[index][2] : 0));
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB menu pointer reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original pointer filters, %zu ordered menu callbacks and "
           "filtered touch memberships/coordinates match.\n",
           callbacks);
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
