#ifndef AB_TEST_ISLAND_REPORTS_H
#define AB_TEST_ISLAND_REPORTS_H

#include "ab/physics_world_solve.h"

typedef struct {
    const AbBodyMotion *bodies;
    const AbWorldSolveBody *world_bodies;
    const uint32_t (*original)[64];
    size_t body_count;
    uint32_t records[8][62];
    size_t count;
    bool invalid;
} IslandReports;

static void record_impulse(void *context, size_t contact,
                           const AbContactImpulse *impulse) {
    IslandReports *reports = context;
    if (reports->count >= 8 || reports->body_count > 7 || impulse->count > 2) {
        reports->invalid = true;
        return;
    }
    uint32_t *record = reports->records[reports->count++];
    record[0] = (uint32_t)contact;
    record[1] = (uint32_t)impulse->count;
    for (size_t point = 0; point < 2; ++point) {
        record[2 + point] =
            point < impulse->count ? to_bits(impulse->normal[point]) : 0;
        record[4 + point] =
            point < impulse->count ? to_bits(impulse->tangent[point]) : 0;
    }
    for (size_t body = 0; body < reports->body_count; ++body) {
        const AbBodyMotion *motion = reports->world_bodies
                                         ? &reports->world_bodies[body].motion
                                         : reports->bodies + body;
        uint32_t flags =
            (reports->original[body][1] & ~UINT32_C(2)) | (motion->awake ? 2u : 0u);
        if (reports->world_bodies) {
            flags =
                (flags & ~UINT32_C(1)) | (reports->world_bodies[body].island ? 1u : 0u);
        }
        const uint32_t fields[] = {
            flags,
            to_bits(motion->sleep_time),
            to_bits(motion->mass.center.x),
            to_bits(motion->mass.center.y),
            to_bits(motion->mass.angle),
            to_bits(motion->mass.linear_velocity.x),
            to_bits(motion->mass.linear_velocity.y),
            to_bits(motion->mass.angular_velocity),
        };
        memcpy(record + 6 + body * 8, fields, sizeof(fields));
    }
}

static bool compare_reports(FILE *file, const IslandReports *reports, size_t sample,
                            size_t *total) {
    uint32_t count;
    CHECK(read_words(file, &count, 1));
    CHECK(!reports->invalid && count == reports->count);
    for (size_t record = 0; record < count; ++record) {
        uint32_t expected[62];
        size_t fields = 6 + reports->body_count * 8;
        CHECK(read_words(file, expected, fields));
        for (size_t field = 0; field < fields; ++field) {
            if (reports->records[record][field] != expected[field]) {
                fprintf(stderr,
                        "Post-solve sample %zu report %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        sample, record, field, reports->records[record][field],
                        expected[field]);
                return false;
            }
        }
    }
    *total += count;
    return true;
}

#endif
