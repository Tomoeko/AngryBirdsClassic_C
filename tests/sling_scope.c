#include "ab/sling_scope.h"
#include "sling_scope_cases.h"

#include <stdio.h>
#include <string.h>

static int compare(FILE *file, const float *values, size_t count, size_t item,
                   size_t dot) {
    for (size_t field = 0; field < count; ++field) {
        unsigned expected = 0;
        uint32_t actual;
        memcpy(&actual, values + field, sizeof(actual));
        if (fscanf(file, "%x", &expected) != 1 || expected != actual) {
            fprintf(stderr,
                    "Original Sling Scope mismatch: case %zu dot %zu field %zu: "
                    "expected %08x got %08x\n",
                    item, dot, field, expected, actual);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    FILE *reference = NULL;
    if (argc == 2) {
        reference = fopen(argv[1], "r");
        char header[96];
        if (!reference || !fgets(header, sizeof(header), reference) ||
            strcmp(header, "AB_ANDROID_SLING_SCOPE_V1 256\n")) {
            fputs("Missing original Sling Scope corpus\n", stderr);
            if (reference)
                fclose(reference);
            return 1;
        }
    } else if (argc != 1) {
        return 2;
    }
    AbError error = {0};
    int status = 1;
    size_t dots = 0;
    for (size_t item = 0; item < AB_SLING_SCOPE_CASES; ++item) {
        AbSlingScopeCase row = ab_sling_scope_case(item);
        AbSlingScopeInput input = {
            .velocity = {row.vx, row.vy},
            .position = {row.x, row.y},
            .timer = row.timer,
            .gravity_y = row.gravity,
            .vertical_force = row.force,
            .mass = row.mass,
            .maximum_translation = 2,
            .physics_to_world = 20,
            .length = row.length_kind == 1 ? row.length : 3,
            .alpha_factor = row.alpha_kind == 1 ? row.alpha : 1,
            .alpha_decay = row.decay_present && row.decay,
            .body_present = row.body != 0,
            .render = {.translation = {row.tx, row.ty}, .scale = {row.sx, row.sy}}};
        AbSlingScopePlan plan;
        if (!ab_sling_scope_plan(&input, &plan, &error)) {
            fprintf(stderr, "%zu: %s\n", item, error.message);
            goto close_reference;
        }
        if (reference) {
            unsigned expected_item, expected_count;
            if (fscanf(reference, "%x %x", &expected_item, &expected_count) != 2 ||
                expected_item != item || expected_count != plan.count) {
                fprintf(stderr,
                        "Original Sling Scope dot count differs at %zu: actual %zu\n",
                        item, plan.count);
                goto close_reference;
            }
        }
        for (size_t dot = 0; dot < plan.count; ++dot) {
            const AbSlingScopeDot *value = plan.dots + dot;
            float x = value->position.x - (float)(int32_t)((float)row.pivot_x + 0.5f);
            float y = value->position.y - (float)(int32_t)((float)row.pivot_y + 0.5f);
            const float words[] = {value->render.translation.x,
                                   value->render.translation.y,
                                   value->render.scale.x,
                                   value->render.scale.y,
                                   value->alpha,
                                   x,
                                   y,
                                   16,
                                   16};
            if (reference && !compare(reference, words, 9, item, dot))
                goto close_reference;
            ++dots;
        }
        const float final[] = {
            plan.final_render.translation.x, plan.final_render.translation.y,
            plan.final_render.scale.x, plan.final_render.scale.y, plan.final_alpha};
        if (reference && !compare(reference, final, 5, item, SIZE_MAX))
            goto close_reference;
    }
    if (reference) {
        char marker[96];
        if (fgetc(reference) != '\n' || !fgets(marker, sizeof(marker), reference) ||
            strcmp(marker, "AB Sling Scope reference complete\n") ||
            fgetc(reference) != EOF || ferror(reference)) {
            fputs("Incomplete original Sling Scope corpus\n", stderr);
            goto close_reference;
        }
    }
    printf("Sling Scope: %zu dots across 256 native "
           "speed/force/gravity/phase/alpha/scale cases.\n",
           dots);
    status = 0;
close_reference:
    if (reference && fclose(reference))
        status = 1;
    return status;
}
