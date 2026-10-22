#include "ab/object_visual.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object visual failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan name(size_t code) {
    static const uint8_t texts[][16] = {"",        "raw",      "composite",
                                        "missing", "bin\0ary", "old"};
    static const size_t sizes[] = {0, 3, 9, 7, 7, 3};
    return (AbSpan){texts[code], sizes[code]};
}

static uint32_t name_code(AbSpan value) {
    for (uint32_t code = 0; code < 6; ++code) {
        AbSpan candidate = name(code);
        if (value.size == candidate.size &&
            (!value.size || memcmp(value.data, candidate.data, value.size) == 0)) {
            return code;
        }
    }
    return UINT32_MAX;
}

typedef struct {
    AbObjectVisual *object;
    AbSpriteAssetState *asset;
    AbSpriteRegion regular;
    AbComposite composite;
    bool regular_available, composite_available, mutate, fail;
    uint32_t calls, composite_calls, missing, snapshot[3];
} Host;

static bool ordinary(void *context, AbSpan identifier, const AbSpriteRegion **output,
                     AbError *error) {
    Host *host = context;
    CHECK(identifier.size && host->object->sprite_present && host->calls == 0);
    ++host->calls;
    host->snapshot[0] = name_code(host->object->sprite);
    host->snapshot[1] = name_code(host->asset->name);
    host->snapshot[2] =
        (host->asset->sprite ? 1u : 0u) | (host->asset->composite ? 2u : 0u);
    CHECK(host->snapshot[0] == name_code(identifier) &&
          host->snapshot[1] == name_code(identifier));
    if (host->mutate) {
        host->composite_available = !host->composite_available;
    }
    *output = host->regular_available ? &host->regular : NULL;
    return !host->fail || ab_fail(error, "Test ordinary sprite lookup failure");
}

static bool composite(void *context, AbSpan identifier, const AbComposite **output,
                      AbError *error) {
    Host *host = context;
    (void)error;
    CHECK(host->calls == 1 && !host->composite_calls &&
          name_code(identifier) == name_code(host->asset->name));
    ++host->composite_calls;
    *output = host->composite_available ? &host->composite : NULL;
    return true;
}

static void missing(void *context, AbSpan identifier) {
    Host *host = context;
    (void)identifier;
    ++host->missing;
}

static AbSpriteAssetHost callbacks(Host *host) {
    return (AbSpriteAssetHost){host, ordinary, composite, missing};
}

static bool edge_checks(void) {
    AbSpriteAssetState asset = {.name = name(5)};
    AbObjectVisual object = {
        .sprite = name(5), .sprite_present = true, .attached = &asset};
    Host host = {.object = &object, .asset = &asset};
    asset.sprite = &host.regular;
    asset.composite = &host.composite;
    CHECK(ab_object_visual_set_sprite(&object, name(1), callbacks(&host), NULL) &&
          host.calls == 1 && host.composite_calls == 1 && host.missing == 1 &&
          !asset.sprite && !asset.composite && host.snapshot[2] == 0);
    host = (Host){.object = &object, .asset = &asset, .fail = true};
    asset.sprite = &host.regular;
    CHECK(!ab_object_visual_set_sprite(&object, name(2), callbacks(&host), NULL) &&
          name_code(object.sprite) == 2 && name_code(asset.name) == 2 &&
          !asset.sprite && !asset.composite && host.calls == 1);
    CHECK(ab_object_visual_set_sprite(&object, name(0), (AbSpriteAssetHost){0}, NULL) &&
          object.sprite_present && !object.sprite.size && !asset.name.size);
    object.attached = NULL;
    CHECK(ab_object_visual_set_sprite(&object, name(4), (AbSpriteAssetHost){0}, NULL) &&
          object.sprite.size == 7 && !asset.name.size);
    CHECK(!ab_object_visual_set_sprite(&object, (AbSpan){NULL, 1}, callbacks(&host),
                                       NULL) &&
          name_code(object.sprite) == 4);
    CHECK(!ab_sprite_asset_set(NULL, name(1), callbacks(&host), NULL));
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
          strcmp(header, "AB_ANDROID_OBJECT_VISUAL_V1 4096\n") == 0);
    size_t lookups = 0, composites = 0, absent = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[7];
        CHECK(read_words(file, input, 7) && input[0] <= 1 && input[1] <= 1 &&
              input[2] <= 1 && input[3] <= 4 && input[4] <= 2 && input[5] <= 3 &&
              input[6] <= 1);
        AbSpriteAssetState asset = {.name = name(5)};
        AbObjectVisual object = {.sprite = name(5),
                                 .sprite_present = input[4] == 2,
                                 .attached = input[0] ? &asset : NULL};
        Host host = {.object = &object,
                     .asset = &asset,
                     .regular_available = input[1] != 0,
                     .composite_available = input[2] != 0,
                     .mutate = input[6] != 0};
        asset.sprite = input[5] & 1 ? &host.regular : NULL;
        asset.composite = input[5] & 2 ? &host.composite : NULL;
        for (size_t step = 0; step < 2; ++step) {
            uint32_t expected[10];
            CHECK(read_words(file, expected, 10));
            host.calls = host.composite_calls = host.missing = 0;
            memset(host.snapshot, 0, sizeof(host.snapshot));
            CHECK(ab_object_visual_set_sprite(&object, name(input[3]), callbacks(&host),
                                              NULL));
            uint32_t actual[] = {object.sprite_present ? 4u : 0u,
                                 name_code(object.sprite),
                                 name_code(asset.name),
                                 asset.sprite == &host.regular,
                                 asset.composite == &host.composite,
                                 host.calls,
                                 host.snapshot[0],
                                 host.snapshot[1],
                                 host.snapshot[2],
                                 host.composite_available};
            for (size_t field = 0; field < 10; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Object visual %zu/%zu field %zu: %08" PRIx32
                            " instead of %08" PRIx32 "\n",
                            item, step, field, actual[field], expected[field]);
                    return false;
                }
            }
            lookups += host.calls;
            composites += host.composite_calls;
            absent += host.missing;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB object visual reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native object-visual cases / 8192 assignments match: %zu "
           "ordinary lookups, %zu composite attempts, %zu missing notices\n",
           lookups, composites, absent);
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
    puts("Object script/asset write order, empty/missing names and failures pass");
    return 0;
}
