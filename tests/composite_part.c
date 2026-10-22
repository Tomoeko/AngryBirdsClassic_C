#include "ab/composite_part.h"
#include "ab/resource_owners.h"
#include "composite_part_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Composite part check failed at %d: %s\n", __LINE__,       \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static void source_noop(const AbResourceSource *source) {
    (void)source;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        if (fscanf(file, "%" SCNx32, output + index) != 1) {
            return false;
        }
    }
    return fgetc(file) == '\n';
}

static void put_float(uint32_t *output, float value) {
    memcpy(output, &value, sizeof(value));
}

static bool native_case(FILE *file, uint32_t sample) {
    uint32_t input[15], generated[15], expected[13], actual[13] = {0};
    composite_part_case(sample, generated);
    CHECK(read_words(file, input, 15) && memcmp(input, generated, sizeof(input)) == 0 &&
          read_words(file, expected, 13));
    uint8_t bytes[256];
    size_t size = composite_part_data(input, bytes);
    AbCompositeSet data = {0};
    CHECK(ab_composite_read((AbSpan){bytes, size}, 16384, &data, NULL) &&
          data.count == 1 && data.composites[0].part_count == 1);
    const AbCompositePart *part = data.composites[0].parts;
    AbSpriteRegistryEntry registered[4];
    AbSpriteRegistry registry = {registered, 0, 4};
    AbResourceOwnerEntry entries[2];
    AbResourceOwners owners;
    CHECK(ab_resource_owners_init(&owners, entries, 2, &registry, NULL));
    AbSpriteRegion regions[2];
    AbResourceMember members[2];
    AbResourceSource sources[2];
    for (size_t source = 0; source < 2; ++source) {
        regions[source] = (AbSpriteRegion){
            .name = part->sprite,
            .width = (int16_t)(input[4] + (source ? 17u : 0u)),
            .height = (int16_t)(input[5] + (source ? 31u : 0u)),
            .pivot_x = (int16_t)((int32_t)input[6] + (source ? 3 : 0)),
            .pivot_y = (int16_t)((int32_t)input[7] - (source ? 4 : 0))};
        members[source] = (AbResourceMember){part->sprite, regions + source};
        sources[source] = (AbResourceSource){AB_SPRITE_REGISTRY_SPRITE,
                                             members + source,
                                             1,
                                             NULL,
                                             source_noop,
                                             source_noop,
                                             source_noop};
    }
    for (size_t order = 0; order < 2; ++order) {
        size_t source = order ^ ((input[1] >> 1) & 1u);
        bool upper_key = (source ^ (input[1] & 1u)) != 0;
        CHECK(ab_resource_owners_put(&owners, text(upper_key ? "zulu" : "alpha"),
                                     sources + source, NULL));
    }
    AbResourceMemberView selected;
    CHECK(ab_resource_owners_find_member(&owners, AB_SPRITE_REGISTRY_SPRITE,
                                         part->sprite, &selected));
    actual[0] = selected.member->resource == regions ? 1 : 2;
    CHECK(selected.member->resource == regions + actual[0] - 1);
    AbCompositeBoundsPart state;
    CHECK(ab_composite_part_state(part, data.extended, selected.member->resource,
                                  &state, NULL));
    put_float(actual + 1, state.offset.x);
    put_float(actual + 2, state.offset.y);
    put_float(actual + 3, state.scale.x);
    put_float(actual + 4, state.scale.y);
    put_float(actual + 5, state.factors.x);
    put_float(actual + 6, state.factors.y);
    put_float(actual + 7, state.rotation);
    actual[8] = state.enabled;
    AbCompositeBounds bounds;
    CHECK(ab_composite_bounds(&state, 1, &bounds, NULL));
    actual[9] = (uint32_t)bounds.width;
    actual[10] = (uint32_t)bounds.height;
    actual[11] = (uint32_t)bounds.pivot_x;
    actual[12] = (uint32_t)bounds.pivot_y;
    for (size_t field = 0; field < 13; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Original composite part %" PRIu32 " field %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    sample, field, actual[field], expected[field]);
            return false;
        }
    }
    uint32_t length;
    CHECK(fscanf(file, "%" SCNx32, &length) == 1 && length <= 64);
    char original_name[65];
    for (uint32_t byte = 0; byte < length; ++byte) {
        uint32_t value;
        CHECK(fscanf(file, "%" SCNx32, &value) == 1 && value <= 255);
        original_name[byte] = (char)value;
    }
    original_name[length] = 0;
    CHECK(fgetc(file) == '\n');
    char name[128];
    size_t name_length;
    CHECK(ab_composite_part_name(part, data.extended, name, sizeof(name), &name_length,
                                 NULL) &&
          name_length == length && memcmp(name, original_name, length) == 0);
    CHECK(ab_resource_owners_clear(&owners, NULL));
    ab_composite_free(&data);
    return true;
}

static bool native_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_COMPOSITE_PART_V1 4096\n") == 0);
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        CHECK(native_case(file, sample));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB composite part reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("Matched 4,096 original KA3D/RVIO loader parts: first sheet-key resolution, "
         "sprite/tag identity, offsets, scales, flips, degree conversion and bounds");
    return true;
}

static bool edge_checks(void) {
    AbSpriteRegion region = {.width = 10, .height = 20};
    AbCompositePart part = {.sprite = text("bird"),
                            .tag = text("part"),
                            .scale_x_bits = 0x7fc00000,
                            .scale_y_bits = 0x7f800000,
                            .rotation_bits = 0x7fc00000,
                            .flip_x = true};
    AbCompositeBoundsPart state = {.rotation = 99};
    CHECK(ab_composite_part_state(&part, false, &region, &state, NULL) &&
          state.scale.x == 1 && state.scale.y == 1 && state.factors.x == 1 &&
          state.rotation == 0 && state.enabled);
    state.rotation = 99;
    CHECK(!ab_composite_part_state(&part, true, &region, &state, NULL) &&
          state.rotation == 99);
    char name[32];
    size_t length;
    CHECK(ab_composite_part_name(&part, true, name, sizeof(name), &length, NULL) &&
          length == 9 && strcmp(name, "bird#part") == 0);
    CHECK(!ab_composite_part_name(&part, true, name, 1, &length, NULL) && length == 9 &&
          strcmp(name, "bird#part") == 0);
    part.tag = (AbSpan){NULL, SIZE_MAX};
    CHECK(ab_composite_part_name(&part, false, name, sizeof(name), &length, NULL) &&
          length == 4 && strcmp(name, "bird") == 0);
    CHECK(!ab_composite_part_name(&part, true, name, sizeof(name), &length, NULL) &&
          length == 4 && strcmp(name, "bird") == 0);
    AbResourceMemberView view = {.member = (const AbResourceMember *)&region};
    CHECK(!ab_resource_owners_find_member(NULL, AB_SPRITE_REGISTRY_SPRITE, text("bird"),
                                          &view) &&
          view.member == (const AbResourceMember *)&region);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
