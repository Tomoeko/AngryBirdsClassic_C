#include "ab/extra_draw.h"
#include "extra_draw_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    AbExtraDraws *draws;
    char names[AB_EXTRA_DRAW_MAXIMUM][9];
    AbExtraDrawFunction functions[AB_EXTRA_DRAW_MAXIMUM];
    uint32_t events[128], count, remove;
    bool fail, has_z;
} Fixture;

static bool record(void *context, void *parameter, AbExtraDrawInput input,
                   AbError *error) {
    Fixture *fixture = context;
    uint32_t id = (uint32_t)(uintptr_t)parameter;
    if (fixture->fail || !input.list || input.width != 480 || input.height != 320 ||
        fixture->count == 128 || !id || id > AB_EXTRA_DRAW_MAXIMUM)
        return ab_fail(error, "Controlled extra-draw callback rejection");
    fixture->events[fixture->count++] = id | (fixture->has_z ? 256u : 0u);
    return id != fixture->remove ||
           ab_extra_draws_remove(fixture->draws,
                                 (AbSpan){(const uint8_t *)fixture->names[id - 1], 8},
                                 error);
}

static bool prepare(Fixture *fixture, ExtraDrawCase input, AbError *error) {
    *fixture = (Fixture){.remove = input.remove};
    if (!ab_extra_draws_open(32, 32768, &fixture->draws, error))
        return false;
    for (uint32_t index = 0; index < input.count; ++index) {
        extra_draw_name(index + 1, fixture->names[index]);
        fixture->functions[index] =
            (AbExtraDrawFunction){.context = fixture,
                                  .parameter = (void *)(uintptr_t)(index + 1),
                                  .draw = record,
                                  .z = input.z[index],
                                  .z_present = input.z_present[index],
                                  .before_blocks = input.before[index]};
        if (!ab_extra_draws_set(fixture->draws,
                                (AbSpan){(const uint8_t *)fixture->names[index], 8},
                                fixture->functions + index, error))
            return false;
    }
    return true;
}

static void publish_flag(void *context, bool present) {
    Fixture *fixture = context;
    fixture->has_z = present;
}

static bool frame(Fixture *fixture, bool *has_z, AbError *error) {
    AbDrawList list = {0};
    AbExtraDrawInput input = {.width = 480,
                              .height = 320,
                              .list = &list,
                              .state_context = fixture,
                              .publish_has_z = publish_flag};
    fixture->count = 0;
    if (!ab_extra_draws_begin(fixture->draws, input, has_z, error))
        return false;
    const float layers[] = {-2, 0, 1, 3, 7};
    for (uint32_t layer = 0; layer < 5; ++layer) {
        if (!ab_extra_draws_layer(fixture->draws, layers[layer], input, error) ||
            fixture->count == 128)
            return false;
        fixture->events[fixture->count++] = 100 + layer;
    }
    return ab_extra_draws_finish(fixture->draws, input, error);
}

static bool compare(FILE *file) {
    char line[80];
    if (!fgets(line, sizeof(line), file) ||
        strcmp(line, "AB_ANDROID_EXTRA_DRAW_V2 256 2\n"))
        return false;
    for (uint32_t sample = 0; sample < AB_EXTRA_DRAW_CASES; ++sample) {
        Fixture fixture;
        AbError error = {0};
        bool okay = prepare(&fixture, extra_draw_case(sample), &error);
        for (uint32_t index = 0; okay && index < 2; ++index) {
            bool has_z;
            uint32_t count, active;
            okay = frame(&fixture, &has_z, &error) &&
                   fscanf(file, "%" SCNx32 " %" SCNx32, &count, &active) == 2 &&
                   fgetc(file) == '\n' && count == fixture.count && active == has_z;
            for (uint32_t event = 0; okay && event < count; ++event) {
                uint32_t expected;
                okay = fscanf(file, "%" SCNx32, &expected) == 1 &&
                       fgetc(file) == (event + 1 == count ? '\n' : ' ') &&
                       expected == fixture.events[event];
            }
        }
        ab_extra_draws_close(fixture.draws);
        if (!okay) {
            fprintf(stderr, "Original extra-draw case %u differs: %s\n", sample,
                    error.message);
            return false;
        }
    }
    return fgets(line, sizeof(line), file) &&
           !strcmp(line, "AB extra-draw reference complete\n") && fgetc(file) == EOF &&
           !ferror(file);
}

static bool edges(void) {
    Fixture fixture;
    AbError error = {0};
    bool has_z = false;
    ExtraDrawCase input = {.count = 1, .before = {true}};
    if (!prepare(&fixture, input, &error))
        return false;
    bool okay = frame(&fixture, &has_z, &error) && !has_z && fixture.count == 6 &&
                fixture.events[0] == 1 && fixture.events[1] == 100;
    fixture.fail = true;
    okay = okay && !frame(&fixture, &has_z, &error) && !frame(&fixture, &has_z, &error);
    ab_extra_draws_close(fixture.draws);
    return okay;
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
    if (okay)
        puts("Original extra draws: 512 frames match registry traversal, before/late "
             "calls, numeric sort, layer flushing, self-removal and retained records.");
    return okay ? 0 : 1;
}
