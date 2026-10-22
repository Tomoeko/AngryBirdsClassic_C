#include "ab/physics_geometry.h"
#include "ab/physics_contact.h"
#include "ab/physics_body.h"
#include "ab/physics_solver.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(AbManifold) == 64 && offsetof(AbManifold, point_count) == 60,
               "Native manifold comparison requires the observed field layout");
_Static_assert(sizeof(AbWorldManifold) == 32,
               "Native world manifold comparison requires eight float fields");

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t to_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool circle_checks(void) {
    AbCircle circle = {2.0f, {0.0f, 0.0f}};
    AbRigidTransform identity = {{0, 0}, 0, 1};
    CHECK(ab_circle_contains(circle, identity, (AbVec2){2, 0}));
    CHECK(!ab_circle_contains(circle, identity, (AbVec2){from_bits(0x40000001), 0}));
    CHECK(!ab_circle_contains(circle, identity, (AbVec2){2, 1}));
    AbAabb bounds = ab_circle_bounds(circle, identity);
    CHECK(bounds.lower.x == -2 && bounds.lower.y == -2 && bounds.upper.x == 2 &&
          bounds.upper.y == 2);
    circle = (AbCircle){1, {2, 3}};
    AbRigidTransform rotated = {{10, -4}, 1, 0};
    bounds = ab_circle_bounds(circle, rotated);
    CHECK(bounds.lower.x == 6 && bounds.lower.y == -3 && bounds.upper.x == 8 &&
          bounds.upper.y == -1);
    CHECK(ab_circle_contains(circle, rotated, (AbVec2){7, -2}));
    CHECK(!ab_circle_contains(circle, rotated, (AbVec2){9, -2}));
    AbMass mass = ab_circle_mass(circle, 0);
    CHECK(mass.mass == 0 && mass.inertia == 0 && mass.center.x == 2 &&
          mass.center.y == 3);
    circle = (AbCircle){0, {2, 3}};
    CHECK(ab_circle_contains(circle, identity, (AbVec2){2, 3}));
    CHECK(!ab_circle_contains(circle, identity, (AbVec2){2, 4}));
    bounds = ab_circle_bounds(circle, identity);
    CHECK(bounds.lower.x == bounds.upper.x && bounds.lower.y == bounds.upper.y);
    return true;
}

static bool polygon_checks(void) {
    AbPolygon box = ab_polygon_box(2, 3, 0.25f);
    AbRigidTransform identity = {{0, 0}, 0, 1};
    CHECK(box.count == 4 && box.radius == 0.25f);
    CHECK(ab_polygon_contains(&box, identity, (AbVec2){2, 3}));
    CHECK(!ab_polygon_contains(&box, identity, (AbVec2){2.125f, 0}));
    AbAabb bounds = ab_polygon_bounds(&box, identity);
    CHECK(bounds.lower.x == -2.25f && bounds.lower.y == -3.25f &&
          bounds.upper.x == 2.25f && bounds.upper.y == 3.25f);
    AbMass mass = ab_polygon_mass(&box, 0.5f);
    CHECK(mass.mass == 12 && mass.center.x == 0 && mass.center.y == 0 &&
          mass.inertia == 52);
    for (size_t index = 0; index < box.count; ++index) {
        box.vertices[index].x += 10;
        box.vertices[index].y -= 4;
    }
    mass = ab_polygon_mass(&box, 0.5f);
    CHECK(mass.mass == 12 && mass.center.x == 10 && mass.center.y == -4 &&
          mass.inertia == 1444);
    return true;
}

static bool polygon_build_checks(void) {
    AbPolygon polygon;
    CHECK(ab_polygon_build(NULL, 0, 0, &polygon));
    CHECK(polygon.count == 4 && polygon.centroid.x == 0 && polygon.centroid.y == 0 &&
          polygon.vertices[0].x == -1 && polygon.vertices[0].y == -1);
    AbVec2 triangle[] = {{0, 0}, {0, 1}, {1, 0}};
    CHECK(ab_polygon_build(triangle, 3, 0.25f, &polygon));
    CHECK(polygon.count == 3 && polygon.radius == 0.25f);
    CHECK(polygon.vertices[0].x == 1 && polygon.vertices[0].y == 0);
    CHECK(to_bits(polygon.centroid.x) == 0x3eaaaaab &&
          to_bits(polygon.centroid.y) == 0x3eaaaaab);
    triangle[2].x = 0.15625f;
    CHECK(ab_polygon_build(triangle, 3, 0, &polygon));
    CHECK(polygon.count == 4 && polygon.vertices[0].x == -1);
    triangle[2].x = 0.25f;
    CHECK(ab_polygon_build(triangle, 3, 0, &polygon));
    CHECK(polygon.count == 3);
    CHECK(ab_polygon_build(polygon.vertices, polygon.count, polygon.radius, &polygon));
    CHECK(polygon.count == 3 && polygon.vertices[0].x == 0.25f);
    AbVec2 limited[9] = {0};
    for (size_t index = 0; index < 8; ++index) {
        limited[index].x = (float)index;
    }
    limited[8] = (AbVec2){0, 1};
    CHECK(ab_polygon_build(limited, 9, 0, &polygon));
    CHECK(polygon.count == 4 && polygon.vertices[0].x == -1);
    CHECK(!ab_polygon_build(NULL, 3, 0, &polygon) && polygon.count == 0);
    CHECK(!ab_polygon_build(triangle, 3, -1, &polygon) && polygon.count == 0);
    CHECK(!ab_polygon_build(triangle, SIZE_MAX, 0, &polygon) && polygon.count == 0);
    CHECK(!ab_polygon_build(triangle, 3, 0, NULL));
    triangle[0].x = from_bits(0x7fc00000);
    CHECK(!ab_polygon_build(triangle, 3, 0, &polygon) && polygon.count == 0);
    return true;
}

static bool body_checks(void) {
    AbBodyMassState body = {.type = AB_BODY_DYNAMIC,
                            .transform = {{10, -5}, 0, 1},
                            .linear_velocity = {3, 4},
                            .angular_velocity = 2};
    AbMass mass = {2, {1, 3}, 30};
    ab_body_mass_set(&body, mass, false);
    CHECK(body.mass == 2 && body.inverse_mass == 0.5f && body.inertia == 10 &&
          body.inverse_inertia == 0.1f);
    CHECK(body.center.x == 11 && body.center.y == -2 && body.previous_center.x == 11 &&
          body.previous_center.y == -2);
    CHECK(body.local_center.x == 1 && body.local_center.y == 3);
    CHECK(body.linear_velocity.x == 7 && body.linear_velocity.y == 26 &&
          body.angular_velocity == 2);
    body.fixed_rotation = true;
    ab_body_mass_set(&body, mass, false);
    CHECK(body.inertia == 0 && body.inverse_inertia == 0);
    mass = (AbMass){8, {8, 9}, 80};
    ab_body_mass_set(&body, mass, true);
    CHECK(body.mass == 2 && body.center.x == 11 && body.linear_velocity.x == 7);
    body.type = AB_BODY_KINEMATIC;
    ab_body_mass_set(&body, mass, false);
    CHECK(body.mass == 2 && body.center.x == 11);
    body.type = AB_BODY_DYNAMIC;
    body.fixed_rotation = false;
    ab_body_mass_set(&body, (AbMass){-1, {0, 0}, 0}, false);
    CHECK(body.mass == 1 && body.inverse_mass == 1 && body.inertia == 0 &&
          body.inverse_inertia == 0);
    return true;
}

static bool body_reset_checks(void) {
    AbBodyMassState body = {.type = AB_BODY_DYNAMIC,
                            .transform = {{10, -5}, 0, 1},
                            .linear_velocity = {3, 4},
                            .angular_velocity = 2,
                            .previous_angle = 7,
                            .angle = 8};
    AbFixtureMass fixtures[] = {
        {1, {2, {-1, 0}, 4}},
        {0, {from_bits(0x7fc00000), {0, 0}, from_bits(0x7fc00000)}},
        {1, {2, {1, 0}, 4}},
    };
    ab_body_mass_reset(&body, fixtures, 3);
    CHECK(body.mass == 4 && body.inverse_mass == 0.25f && body.inertia == 8 &&
          body.inverse_inertia == 0.125f);
    CHECK(body.local_center.x == 0 && body.local_center.y == 0 && body.center.x == 10 &&
          body.center.y == -5);
    CHECK(body.linear_velocity.x == 13 && body.linear_velocity.y == 24);
    CHECK(body.previous_angle == 7 && body.angle == 8);
    body.type = AB_BODY_KINEMATIC;
    ab_body_mass_reset(&body, NULL, 0);
    CHECK(body.mass == 0 && body.inverse_mass == 0 && body.inertia == 0 &&
          body.inverse_inertia == 0 && body.previous_angle == 8 && body.angle == 8);
    CHECK(body.linear_velocity.x == 13 && body.linear_velocity.y == 24);
    body.type = AB_BODY_DYNAMIC;
    ab_body_mass_reset(&body, NULL, 0);
    CHECK(body.mass == 1 && body.inverse_mass == 1 && body.inertia == 0);
    return true;
}

static bool ray_checks(void) {
    AbRigidTransform identity = {{0, 0}, 0, 1};
    AbRay ray = {{-2, 0}, {2, 0}, 1};
    AbCircle circle = {1, {0, 0}};
    AbRayHit hit = {{12, 34}, 56};
    CHECK(ab_circle_raycast(circle, identity, ray, &hit));
    CHECK(hit.fraction == 0.25f && hit.normal.x == -1 && hit.normal.y == 0);
    hit = (AbRayHit){{12, 34}, 56};
    ray.max_fraction = 0.2f;
    CHECK(!ab_circle_raycast(circle, identity, ray, &hit));
    CHECK(hit.normal.x == 12 && hit.normal.y == 34 && hit.fraction == 56);
    ray = (AbRay){{0, 0}, {2, 0}, 1};
    CHECK(!ab_circle_raycast(circle, identity, ray, &hit));
    AbPolygon box = ab_polygon_box(2, 3, 0.25f);
    ray = (AbRay){{-4, 0}, {4, 0}, 1};
    CHECK(ab_polygon_raycast(&box, identity, ray, &hit));
    CHECK(hit.fraction == 0.25f && hit.normal.x == -1 && hit.normal.y == 0);
    hit = (AbRayHit){{12, 34}, 56};
    ray = (AbRay){{-4, 4}, {4, 4}, 1};
    CHECK(!ab_polygon_raycast(&box, identity, ray, &hit));
    CHECK(hit.normal.x == 12 && hit.normal.y == 34 && hit.fraction == 56);
    ray = (AbRay){{0, 0}, {4, 0}, 1};
    CHECK(!ab_polygon_raycast(&box, identity, ray, &hit));
    ray = (AbRay){{-4, 0}, {-4, 0}, 1};
    CHECK(!ab_polygon_raycast(&box, identity, ray, &hit));
    return true;
}

static bool contact_checks(void) {
    AbRigidTransform identity = {{0, 0}, 0, 1};
    AbCircle circle = {1, {0, 0}};
    AbManifold manifold = {.points = {{.normal_impulse = 12, .tangent_impulse = 34}},
                           .kind = AB_CONTACT_FACE_SECOND};
    AbRigidTransform touching = {{2, 0}, 0, 1};
    ab_collide_circles(&manifold, circle, identity, circle, touching);
    CHECK(manifold.point_count == 1 && manifold.kind == AB_CONTACT_CIRCLES);
    CHECK(manifold.points[0].normal_impulse == 12 &&
          manifold.points[0].tangent_impulse == 34);
    CHECK(manifold.points[0].feature_id == 0 && manifold.local_normal.x == 0 &&
          manifold.local_normal.y == 0);
    touching.position.x = from_bits(0x40000001);
    ab_collide_circles(&manifold, circle, identity, circle, touching);
    CHECK(manifold.point_count == 0);

    AbPolygon box = ab_polygon_box(2, 3, 0);
    circle.radius = 0.5f;
    touching.position = (AbVec2){2.5f, 0};
    ab_collide_polygon_circle(&manifold, &box, identity, circle, touching);
    CHECK(manifold.point_count == 1 && manifold.kind == AB_CONTACT_FACE_FIRST);
    CHECK(manifold.local_normal.x == 1 && manifold.local_normal.y == 0 &&
          manifold.local_point.x == 2 && manifold.local_point.y == 0);
    touching.position = (AbVec2){2.25f, 3.25f};
    ab_collide_polygon_circle(&manifold, &box, identity, circle, touching);
    CHECK(manifold.point_count == 1 && manifold.local_point.x == 2 &&
          manifold.local_point.y == 3);
    CHECK(manifold.local_normal.x > 0.7071f && manifold.local_normal.x < 0.7072f &&
          manifold.local_normal.x == manifold.local_normal.y);
    touching.position = (AbVec2){2.5f, 3.5f};
    ab_collide_polygon_circle(&manifold, &box, identity, circle, touching);
    CHECK(manifold.point_count == 0 && manifold.points[0].normal_impulse == 12 &&
          manifold.points[0].tangent_impulse == 34);
    touching.position = (AbVec2){4, 0};
    ab_collide_polygons(&manifold, &box, identity, &box, touching);
    CHECK(manifold.point_count == 2 && manifold.kind == AB_CONTACT_FACE_FIRST);
    CHECK(manifold.local_normal.x == 1 &&
          to_bits(manifold.local_normal.y) == 0x80000000);
    CHECK(manifold.local_point.x == 2 && manifold.local_point.y == 0);
    CHECK(manifold.points[0].local_point.x == -2 &&
          manifold.points[1].local_point.x == -2);
    CHECK(manifold.points[0].local_point.y == 3 &&
          manifold.points[1].local_point.y == -3);
    CHECK(manifold.points[0].feature_id == 0x10301 &&
          manifold.points[1].feature_id == 0x10001);
    CHECK(manifold.points[0].normal_impulse == 12 &&
          manifold.points[0].tangent_impulse == 34);
    touching.position.x = from_bits(0x40800001);
    ab_collide_polygons(&manifold, &box, identity, &box, touching);
    CHECK(manifold.point_count == 0);
    return true;
}

static bool compare_native_case(const uint32_t words[19], size_t index) {
    AbCircle circle = {from_bits(words[0]), {from_bits(words[1]), from_bits(words[2])}};
    AbRigidTransform transform = {{from_bits(words[4]), from_bits(words[5])},
                                  from_bits(words[6]),
                                  from_bits(words[7])};
    AbVec2 point = {from_bits(words[8]), from_bits(words[9])};
    AbMass mass = ab_circle_mass(circle, from_bits(words[3]));
    AbAabb bounds = ab_circle_bounds(circle, transform);
    const uint32_t actual[] = {
        to_bits(mass.mass),
        to_bits(mass.center.x),
        to_bits(mass.center.y),
        to_bits(mass.inertia),
        to_bits(bounds.lower.x),
        to_bits(bounds.lower.y),
        to_bits(bounds.upper.x),
        to_bits(bounds.upper.y),
        ab_circle_contains(circle, transform, point) ? 1u : 0u,
    };
    for (size_t field = 0; field < 9; ++field) {
        if (actual[field] != words[10 + field]) {
            fprintf(stderr,
                    "Native circle case %zu field %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    index, field, actual[field], words[10 + field]);
            return false;
        }
    }
    return true;
}

static bool native_circles(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_CIRCLES_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[19];
        for (size_t word = 0; word < 19; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        CHECK(compare_native_case(words, index));
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool compare_native_polygon(const uint32_t *words, size_t index) {
    AbPolygon polygon = {.count = words[0], .radius = from_bits(words[1])};
    AbRigidTransform transform = {{from_bits(words[3]), from_bits(words[4])},
                                  from_bits(words[5]),
                                  from_bits(words[6])};
    AbVec2 point = {from_bits(words[7]), from_bits(words[8])};
    for (size_t vertex = 0; vertex < polygon.count; ++vertex) {
        polygon.vertices[vertex] = (AbVec2){from_bits(words[9 + vertex * 2]),
                                            from_bits(words[10 + vertex * 2])};
        size_t normal = 9 + polygon.count * 2 + vertex * 2;
        polygon.normals[vertex] =
            (AbVec2){from_bits(words[normal]), from_bits(words[normal + 1])};
    }
    if (index < 2048) {
        AbPolygon box = ab_polygon_box(polygon.vertices[1].x, polygon.vertices[2].y,
                                       polygon.radius);
        CHECK(box.count == polygon.count);
        for (size_t vertex = 0; vertex < box.count; ++vertex) {
            CHECK(to_bits(box.vertices[vertex].x) ==
                  to_bits(polygon.vertices[vertex].x));
            CHECK(to_bits(box.vertices[vertex].y) ==
                  to_bits(polygon.vertices[vertex].y));
            CHECK(to_bits(box.normals[vertex].x) == to_bits(polygon.normals[vertex].x));
            CHECK(to_bits(box.normals[vertex].y) == to_bits(polygon.normals[vertex].y));
        }
    }
    AbMass mass = ab_polygon_mass(&polygon, from_bits(words[2]));
    AbAabb bounds = ab_polygon_bounds(&polygon, transform);
    const uint32_t actual[] = {
        to_bits(mass.mass),
        to_bits(mass.center.x),
        to_bits(mass.center.y),
        to_bits(mass.inertia),
        to_bits(bounds.lower.x),
        to_bits(bounds.lower.y),
        to_bits(bounds.upper.x),
        to_bits(bounds.upper.y),
        ab_polygon_contains(&polygon, transform, point) ? 1u : 0u,
    };
    size_t result = 9 + polygon.count * 4;
    for (size_t field = 0; field < 9; ++field) {
        if (actual[field] != words[result + field]) {
            fprintf(stderr,
                    "Native polygon case %zu field %zu: %08" PRIx32 " != %08" PRIx32
                    "\n",
                    index, field, actual[field], words[result + field]);
            return false;
        }
    }
    return true;
}

static bool native_polygons(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_POLYGONS_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[50];
        CHECK(fscanf(file, "%8" SCNx32, words) == 1);
        CHECK(words[0] >= 3 && words[0] <= AB_POLYGON_VERTICES);
        size_t count = 18 + (size_t)words[0] * 4;
        for (size_t word = 1; word < count; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        CHECK(compare_native_polygon(words, index));
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static AbManifold initial_manifold(size_t index) {
    uint32_t words[16];
    for (uint32_t field = 0; field < 16; ++field) {
        words[field] = 0x3e800000u + (field << 12) + ((uint32_t)index & 255);
    }
    words[14] = AB_CONTACT_FACE_SECOND;
    words[15] = 2;
    AbManifold manifold;
    memcpy(&manifold, words, sizeof(manifold));
    return manifold;
}

static bool compare_manifold(const AbManifold *manifold, const uint32_t expected[16],
                             size_t index, const char *kind) {
    uint32_t actual[16];
    memcpy(actual, manifold, sizeof(actual));
    for (size_t field = 0; field < 16; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Native %s contact %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    kind, index, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool compare_world(const AbManifold *manifold, AbRigidTransform first_transform,
                          float first_radius, AbRigidTransform second_transform,
                          float second_radius, const uint32_t expected[8],
                          size_t index) {
    uint32_t words[8];
    for (uint32_t field = 0; field < 8; ++field) {
        words[field] = 0x3d000000u + (field << 12) + ((uint32_t)index & 255);
    }
    AbWorldManifold world;
    memcpy(&world, words, sizeof(world));
    ab_world_manifold(&world, manifold, first_transform, first_radius, second_transform,
                      second_radius);
    memcpy(words, &world, sizeof(words));
    for (size_t field = 0; field < 8; ++field) {
        if (words[field] != expected[field]) {
            fprintf(stderr,
                    "Native world manifold %zu kind %d field %zu: %08" PRIx32
                    " != %08" PRIx32 "\n",
                    index, manifold->kind, field, words[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool native_circle_contacts(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_CIRCLE_CONTACTS_V2 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[38];
        for (size_t word = 0; word < 38; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        AbCircle first = {from_bits(words[0]),
                          {from_bits(words[1]), from_bits(words[2])}};
        AbCircle second = {from_bits(words[7]),
                           {from_bits(words[8]), from_bits(words[9])}};
        AbRigidTransform first_transform = {{from_bits(words[3]), from_bits(words[4])},
                                            from_bits(words[5]),
                                            from_bits(words[6])};
        AbRigidTransform second_transform = {
            {from_bits(words[10]), from_bits(words[11])},
            from_bits(words[12]),
            from_bits(words[13])};
        AbManifold manifold = initial_manifold(index);
        ab_collide_circles(&manifold, first, first_transform, second, second_transform);
        CHECK(compare_manifold(&manifold, words + 14, index, "circle-circle"));
        CHECK(compare_world(&manifold, first_transform, first.radius, second_transform,
                            second.radius, words + 30, index));
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool native_polygon_circle_contacts(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_POLYGON_CIRCLE_CONTACTS_V2 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[69];
        CHECK(fscanf(file, "%8" SCNx32, words) == 1);
        CHECK(words[0] >= 3 && words[0] <= AB_POLYGON_VERTICES);
        size_t count = 37 + (size_t)words[0] * 4;
        for (size_t word = 1; word < count; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        AbPolygon polygon = {.count = words[0], .radius = from_bits(words[1])};
        for (size_t vertex = 0; vertex < polygon.count; ++vertex) {
            polygon.vertices[vertex] = (AbVec2){from_bits(words[13 + vertex * 2]),
                                                from_bits(words[14 + vertex * 2])};
            size_t normal = 13 + polygon.count * 2 + vertex * 2;
            polygon.normals[vertex] =
                (AbVec2){from_bits(words[normal]), from_bits(words[normal + 1])};
        }
        AbRigidTransform polygon_transform = {
            {from_bits(words[2]), from_bits(words[3])},
            from_bits(words[4]),
            from_bits(words[5])};
        AbRigidTransform circle_transform = {
            {from_bits(words[9]), from_bits(words[10])},
            from_bits(words[11]),
            from_bits(words[12])};
        AbCircle circle = {from_bits(words[6]),
                           {from_bits(words[7]), from_bits(words[8])}};
        AbManifold manifold = initial_manifold(index);
        ab_collide_polygon_circle(&manifold, &polygon, polygon_transform, circle,
                                  circle_transform);
        size_t result = 13 + polygon.count * 4;
        CHECK(compare_manifold(&manifold, words + result, index, "polygon-circle"));
        CHECK(compare_world(&manifold, polygon_transform, polygon.radius,
                            circle_transform, circle.radius, words + result + 16,
                            index));
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool native_polygon_contacts(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_POLYGON_CONTACTS_V2 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[100];
        CHECK(fscanf(file, "%8" SCNx32 " %8" SCNx32, words, words + 1) == 2);
        CHECK(words[0] >= 3 && words[0] <= AB_POLYGON_VERTICES && words[1] >= 3 &&
              words[1] <= AB_POLYGON_VERTICES);
        size_t count = 36 + ((size_t)words[0] + words[1]) * 4;
        for (size_t word = 2; word < count; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        AbPolygon polygons[2] = {0};
        size_t cursor = 12;
        for (size_t shape = 0; shape < 2; ++shape) {
            AbPolygon *polygon = polygons + shape;
            polygon->count = words[shape];
            polygon->radius = from_bits(words[2 + shape]);
            for (size_t vertex = 0; vertex < polygon->count; ++vertex) {
                polygon->vertices[vertex] =
                    (AbVec2){from_bits(words[cursor + vertex * 2]),
                             from_bits(words[cursor + vertex * 2 + 1])};
                size_t normal = cursor + polygon->count * 2 + vertex * 2;
                polygon->normals[vertex] =
                    (AbVec2){from_bits(words[normal]), from_bits(words[normal + 1])};
            }
            cursor += polygon->count * 4;
        }
        AbRigidTransform first_transform = {{from_bits(words[4]), from_bits(words[5])},
                                            from_bits(words[6]),
                                            from_bits(words[7])};
        AbRigidTransform second_transform = {{from_bits(words[8]), from_bits(words[9])},
                                             from_bits(words[10]),
                                             from_bits(words[11])};
        AbManifold manifold = initial_manifold(index);
        ab_collide_polygons(&manifold, polygons, first_transform, polygons + 1,
                            second_transform);
        CHECK(compare_manifold(&manifold, words + cursor, index, "polygon-polygon"));
        CHECK(compare_world(&manifold, first_transform, polygons[0].radius,
                            second_transform, polygons[1].radius, words + cursor + 16,
                            index));
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool native_polygon_build(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_POLYGON_BUILD_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[61];
        CHECK(fscanf(file, "%8" SCNx32 " %8" SCNx32, words, words + 1) == 2);
        CHECK(words[0] <= 12);
        size_t output = 2 + (size_t)words[0] * 2;
        for (size_t word = 2; word <= output; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        CHECK(words[output] >= 3 && words[output] <= AB_POLYGON_VERTICES);
        size_t count = output + 3 + (size_t)words[output] * 4;
        for (size_t word = output + 1; word < count; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        AbVec2 input[12];
        for (size_t vertex = 0; vertex < words[0]; ++vertex) {
            input[vertex] = (AbVec2){from_bits(words[2 + vertex * 2]),
                                     from_bits(words[3 + vertex * 2])};
        }
        AbPolygon polygon;
        CHECK(ab_polygon_build(input, words[0], from_bits(words[1]), &polygon));
        if (polygon.count != words[output] ||
            to_bits(polygon.centroid.x) != words[output + 1] ||
            to_bits(polygon.centroid.y) != words[output + 2]) {
            fprintf(stderr, "Native polygon build %zu centroid/count mismatch\n",
                    index);
            return false;
        }
        CHECK(to_bits(polygon.radius) == words[1]);
        for (size_t vertex = 0; vertex < polygon.count; ++vertex) {
            size_t position = output + 3 + vertex * 2;
            size_t normal = output + 3 + polygon.count * 2 + vertex * 2;
            if (to_bits(polygon.vertices[vertex].x) != words[position] ||
                to_bits(polygon.vertices[vertex].y) != words[position + 1] ||
                to_bits(polygon.normals[vertex].x) != words[normal] ||
                to_bits(polygon.normals[vertex].y) != words[normal + 1]) {
                fprintf(stderr, "Native polygon build %zu vertex/normal %zu mismatch\n",
                        index, vertex);
                return false;
            }
        }
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool compare_ray(bool hit, const AbRayHit *output, const uint32_t expected[4],
                        size_t index, const char *kind) {
    const uint32_t actual[] = {hit ? 1u : 0u, to_bits(output->normal.x),
                               to_bits(output->normal.y), to_bits(output->fraction)};
    for (size_t field = 0; field < 4; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Native %s ray %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    kind, index, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static AbRayHit initial_ray_output(size_t index) {
    uint32_t words[3];
    for (uint32_t field = 0; field < 3; ++field) {
        words[field] = 0x3d000000u + (field << 12) + ((uint32_t)index & 255);
    }
    return (AbRayHit){{from_bits(words[0]), from_bits(words[1])}, from_bits(words[2])};
}

static bool native_circle_rays(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_CIRCLE_RAYS_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[16];
        for (size_t word = 0; word < 16; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        AbCircle circle = {from_bits(words[0]),
                           {from_bits(words[1]), from_bits(words[2])}};
        AbRigidTransform transform = {{from_bits(words[3]), from_bits(words[4])},
                                      from_bits(words[5]),
                                      from_bits(words[6])};
        AbRay ray = {{from_bits(words[7]), from_bits(words[8])},
                     {from_bits(words[9]), from_bits(words[10])},
                     from_bits(words[11])};
        AbRayHit output = initial_ray_output(index);
        bool hit = ab_circle_raycast(circle, transform, ray, &output);
        CHECK(compare_ray(hit, &output, words + 12, index, "circle"));
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool native_polygon_rays(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_POLYGON_RAYS_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[47];
        CHECK(fscanf(file, "%8" SCNx32, words) == 1);
        CHECK(words[0] >= 3 && words[0] <= AB_POLYGON_VERTICES);
        size_t count = 15 + (size_t)words[0] * 4;
        for (size_t word = 1; word < count; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        AbPolygon polygon = {.count = words[0], .radius = from_bits(words[1])};
        for (size_t vertex = 0; vertex < polygon.count; ++vertex) {
            polygon.vertices[vertex] = (AbVec2){from_bits(words[11 + vertex * 2]),
                                                from_bits(words[12 + vertex * 2])};
            size_t normal = 11 + polygon.count * 2 + vertex * 2;
            polygon.normals[vertex] =
                (AbVec2){from_bits(words[normal]), from_bits(words[normal + 1])};
        }
        AbRigidTransform transform = {{from_bits(words[2]), from_bits(words[3])},
                                      from_bits(words[4]),
                                      from_bits(words[5])};
        AbRay ray = {{from_bits(words[6]), from_bits(words[7])},
                     {from_bits(words[8]), from_bits(words[9])},
                     from_bits(words[10])};
        AbRayHit output = initial_ray_output(index);
        bool hit = ab_polygon_raycast(&polygon, transform, ray, &output);
        CHECK(compare_ray(hit, &output, words + 11 + polygon.count * 4, index,
                          "polygon"));
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool native_body_mass(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_BODY_MASS_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[36];
        for (size_t word = 0; word < 36; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        CHECK(words[0] <= 2 && words[1] <= 1 && words[2] <= 1);
        AbBodyMassState body = {
            .type = (AbBodyType)words[0],
            .fixed_rotation = words[1] != 0,
            .transform = {{from_bits(words[7]), from_bits(words[8])},
                          from_bits(words[9]),
                          from_bits(words[10])},
            .local_center = {from_bits(words[11]), from_bits(words[12])},
            .previous_center = {from_bits(words[13]), from_bits(words[14])},
            .center = {from_bits(words[15]), from_bits(words[16])},
            .linear_velocity = {from_bits(words[17]), from_bits(words[18])},
            .angular_velocity = from_bits(words[19]),
            .mass = from_bits(words[20]),
            .inverse_mass = from_bits(words[21]),
            .inertia = from_bits(words[22]),
            .inverse_inertia = from_bits(words[23]),
        };
        AbMass mass = {from_bits(words[3]),
                       {from_bits(words[4]), from_bits(words[5])},
                       from_bits(words[6])};
        ab_body_mass_set(&body, mass, words[2] != 0);
        CHECK((uint32_t)body.type == words[0] &&
              body.fixed_rotation == (words[1] != 0));
        CHECK(to_bits(body.angular_velocity) == words[19]);
        CHECK(to_bits(body.transform.position.x) == words[7] &&
              to_bits(body.transform.position.y) == words[8] &&
              to_bits(body.transform.sine) == words[9] &&
              to_bits(body.transform.cosine) == words[10]);
        const uint32_t actual[] = {
            to_bits(body.mass),
            to_bits(body.inverse_mass),
            to_bits(body.inertia),
            to_bits(body.inverse_inertia),
            to_bits(body.local_center.x),
            to_bits(body.local_center.y),
            to_bits(body.previous_center.x),
            to_bits(body.previous_center.y),
            to_bits(body.center.x),
            to_bits(body.center.y),
            to_bits(body.linear_velocity.x),
            to_bits(body.linear_velocity.y),
        };
        for (size_t field = 0; field < 12; ++field) {
            if (actual[field] != words[24 + field]) {
                fprintf(stderr,
                        "Native body mass %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        index, field, actual[field], words[24 + field]);
                return false;
            }
        }
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool native_body_reset(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_BODY_RESET_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[56];
        for (size_t word = 0; word < 3; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        CHECK(words[0] <= 2 && words[1] <= 1 && words[2] <= 4);
        size_t output = 22 + (size_t)words[2] * 5;
        for (size_t word = 3; word < output + 14; ++word) {
            CHECK(fscanf(file, "%8" SCNx32, words + word) == 1);
        }
        AbBodyMassState body = {
            .type = (AbBodyType)words[0],
            .fixed_rotation = words[1] != 0,
            .transform = {{from_bits(words[3]), from_bits(words[4])},
                          from_bits(words[5]),
                          from_bits(words[6])},
            .local_center = {from_bits(words[7]), from_bits(words[8])},
            .previous_center = {from_bits(words[9]), from_bits(words[10])},
            .center = {from_bits(words[11]), from_bits(words[12])},
            .linear_velocity = {from_bits(words[13]), from_bits(words[14])},
            .angular_velocity = from_bits(words[15]),
            .mass = from_bits(words[16]),
            .inverse_mass = from_bits(words[17]),
            .inertia = from_bits(words[18]),
            .inverse_inertia = from_bits(words[19]),
            .previous_angle = from_bits(words[20]),
            .angle = from_bits(words[21]),
        };
        AbFixtureMass fixtures[4];
        for (size_t fixture = 0; fixture < words[2]; ++fixture) {
            size_t start = 22 + fixture * 5;
            fixtures[fixture] = (AbFixtureMass){
                from_bits(words[start]),
                {from_bits(words[start + 1]),
                 {from_bits(words[start + 2]), from_bits(words[start + 3])},
                 from_bits(words[start + 4])}};
        }
        ab_body_mass_reset(&body, fixtures, words[2]);
        CHECK((uint32_t)body.type == words[0] &&
              body.fixed_rotation == (words[1] != 0));
        CHECK(to_bits(body.angular_velocity) == words[15]);
        CHECK(to_bits(body.transform.position.x) == words[3] &&
              to_bits(body.transform.position.y) == words[4] &&
              to_bits(body.transform.sine) == words[5] &&
              to_bits(body.transform.cosine) == words[6]);
        const uint32_t actual[] = {
            to_bits(body.mass),
            to_bits(body.inverse_mass),
            to_bits(body.inertia),
            to_bits(body.inverse_inertia),
            to_bits(body.local_center.x),
            to_bits(body.local_center.y),
            to_bits(body.previous_center.x),
            to_bits(body.previous_center.y),
            to_bits(body.center.x),
            to_bits(body.center.y),
            to_bits(body.linear_velocity.x),
            to_bits(body.linear_velocity.y),
            to_bits(body.previous_angle),
            to_bits(body.angle),
        };
        for (size_t field = 0; field < 14; ++field) {
            if (actual[field] != words[output + field]) {
                fprintf(stderr,
                        "Native body reset %zu field %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        index, field, actual[field], words[output + field]);
                return false;
            }
        }
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool warm_start_checks(void) {
    AbVelocity velocities[] = {{{0, 0}, 0}, {{0, 0}, 0}};
    AbVelocityContact contacts[2] = {{
        .points = {{{1, 2}, {-1, 3}, 2, 1, 0, 0, 0}},
        .normal = {1, 0},
        .first_inverse_mass = 1,
        .second_inverse_mass = 0.5f,
        .first_inverse_inertia = 2,
        .second_inverse_inertia = 1,
        .first_body = 0,
        .second_body = 1,
        .point_count = 1,
    }};
    CHECK(ab_contacts_warm_start(velocities, 2, contacts, 1));
    CHECK(velocities[0].linear.x == -2 && velocities[0].linear.y == 1 &&
          velocities[0].angular == 10 && velocities[1].linear.x == 1 &&
          velocities[1].linear.y == -0.5f && velocities[1].angular == -5);
    AbVelocity original[2];
    memcpy(original, velocities, sizeof(original));
    contacts[1].second_body = 2;
    CHECK(!ab_contacts_warm_start(velocities, 2, contacts, 2));
    CHECK(memcmp(original, velocities, sizeof(original)) == 0);
    contacts[0].point_count = 3;
    CHECK(!ab_contacts_warm_start(velocities, 2, contacts, 1));
    CHECK(ab_contacts_warm_start(NULL, 0, NULL, 0));
    CHECK(!ab_contacts_warm_start(NULL, 0, contacts, 1));
    return true;
}

static bool native_warm_start(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_WARM_START_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[109];
        CHECK(fscanf(file, "%8" SCNx32, words) == 1 && words[0] <= 4);
        size_t output = 13 + (size_t)words[0] * 21;
        for (size_t field = 1; field < output + 12; ++field) {
            CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
        }
        AbVelocity velocities[4];
        AbVelocityContact contacts[4] = {0};
        for (size_t body = 0; body < 4; ++body) {
            size_t start = 1 + body * 3;
            velocities[body] =
                (AbVelocity){{from_bits(words[start]), from_bits(words[start + 1])},
                             from_bits(words[start + 2])};
        }
        for (size_t contact = 0; contact < words[0]; ++contact) {
            size_t start = 13 + contact * 21;
            AbVelocityContact *current = contacts + contact;
            CHECK(words[start] <= 2 && words[start + 1] < 4 && words[start + 2] < 4);
            current->point_count = words[start];
            current->first_body = words[start + 1];
            current->second_body = words[start + 2];
            current->first_inverse_mass = from_bits(words[start + 3]);
            current->second_inverse_mass = from_bits(words[start + 4]);
            current->first_inverse_inertia = from_bits(words[start + 5]);
            current->second_inverse_inertia = from_bits(words[start + 6]);
            current->normal =
                (AbVec2){from_bits(words[start + 7]), from_bits(words[start + 8])};
            for (size_t point = 0; point < 2; ++point) {
                size_t offset = start + 9 + point * 6;
                current->points[point] = (AbVelocityPoint){
                    {from_bits(words[offset]), from_bits(words[offset + 1])},
                    {from_bits(words[offset + 2]), from_bits(words[offset + 3])},
                    from_bits(words[offset + 4]),
                    from_bits(words[offset + 5]),
                    0,
                    0,
                    0};
            }
        }
        CHECK(ab_contacts_warm_start(velocities, 4, contacts, words[0]));
        for (size_t body = 0; body < 4; ++body) {
            const uint32_t actual[] = {to_bits(velocities[body].linear.x),
                                       to_bits(velocities[body].linear.y),
                                       to_bits(velocities[body].angular)};
            for (size_t field = 0; field < 3; ++field) {
                if (actual[field] != words[output + body * 3 + field]) {
                    fprintf(stderr,
                            "Native warm start %zu body %zu field %zu: "
                            "%08" PRIx32 " != %08" PRIx32 "\n",
                            index, body, field, actual[field],
                            words[output + body * 3 + field]);
                    return false;
                }
            }
        }
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool velocity_solver_checks(void) {
    AbVelocity velocities[] = {{{1, 0}, 0}, {{-1, 0}, 0}};
    AbVelocityContact contact = {
        .points = {{.normal_mass = 0.5f}},
        .normal = {1, 0},
        .first_inverse_mass = 1,
        .second_inverse_mass = 1,
        .first_body = 0,
        .second_body = 1,
        .point_count = 1,
    };
    CHECK(ab_contacts_solve_velocity(velocities, 2, &contact, 1));
    CHECK(velocities[0].linear.x == 0 && velocities[1].linear.x == 0 &&
          contact.points[0].normal_impulse == 1);
    CHECK(ab_contacts_solve_velocity(velocities, 2, &contact, 1));
    CHECK(velocities[0].linear.x == 0 && velocities[1].linear.x == 0 &&
          contact.points[0].normal_impulse == 1);
    AbVelocity original[2];
    memcpy(original, velocities, sizeof(original));
    contact.point_count = 0;
    CHECK(!ab_contacts_solve_velocity(velocities, 2, &contact, 1));
    CHECK(memcmp(original, velocities, sizeof(original)) == 0);
    CHECK(ab_contacts_solve_velocity(NULL, 0, NULL, 0));
    return true;
}

static bool native_velocity_solver(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_VELOCITY_SOLVER_V1 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[68];
        for (size_t field = 0; field < 68; ++field) {
            CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
        }
        CHECK(words[0] >= 1 && words[0] <= 10);
        const uint32_t *input = words + 13;
        CHECK(input[28] < 4 && input[29] < 4 && input[37] >= 1 && input[37] <= 2);
        AbVelocityContact contact = {
            .normal = {from_bits(input[18]), from_bits(input[19])},
            .first_inverse_mass = from_bits(input[30]),
            .second_inverse_mass = from_bits(input[31]),
            .first_inverse_inertia = from_bits(input[32]),
            .second_inverse_inertia = from_bits(input[33]),
            .friction = from_bits(input[34]),
            .tangent_speed = from_bits(input[36]),
            .normal_matrix = {from_bits(input[24]), from_bits(input[25]),
                              from_bits(input[27])},
            .normal_inverse = {from_bits(input[20]), from_bits(input[21]),
                               from_bits(input[23])},
            .first_body = input[28],
            .second_body = input[29],
            .point_count = input[37],
        };
        for (size_t point = 0; point < 2; ++point) {
            const uint32_t *data = input + point * 9;
            contact.points[point] =
                (AbVelocityPoint){{from_bits(data[0]), from_bits(data[1])},
                                  {from_bits(data[2]), from_bits(data[3])},
                                  from_bits(data[4]),
                                  from_bits(data[5]),
                                  from_bits(data[6]),
                                  from_bits(data[7]),
                                  from_bits(data[8])};
        }
        AbVelocity velocities[4];
        for (size_t body = 0; body < 4; ++body) {
            const uint32_t *data = words + 1 + body * 3;
            velocities[body] = (AbVelocity){{from_bits(data[0]), from_bits(data[1])},
                                            from_bits(data[2])};
        }
        for (size_t iteration = 0; iteration < words[0]; ++iteration) {
            CHECK(ab_contacts_solve_velocity(velocities, 4, &contact, 1));
        }
        uint32_t actual[16];
        for (size_t body = 0; body < 4; ++body) {
            actual[body * 3] = to_bits(velocities[body].linear.x);
            actual[body * 3 + 1] = to_bits(velocities[body].linear.y);
            actual[body * 3 + 2] = to_bits(velocities[body].angular);
        }
        actual[12] = to_bits(contact.points[0].normal_impulse);
        actual[13] = to_bits(contact.points[0].tangent_impulse);
        actual[14] = to_bits(contact.points[1].normal_impulse);
        actual[15] = to_bits(contact.points[1].tangent_impulse);
        for (size_t field = 0; field < 16; ++field) {
            if (actual[field] != words[52 + field]) {
                fprintf(stderr,
                        "Native velocity solve %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        index, field, actual[field], words[52 + field]);
                return false;
            }
        }
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool velocity_initialize_checks(void) {
    AbVelocityContact contact = {
        .points = {{.normal_impulse = 5, .tangent_impulse = 7}},
        .first_inverse_mass = 1,
        .second_inverse_mass = 1,
        .point_count = 1,
    };
    AbManifold manifold = {.kind = AB_CONTACT_CIRCLES, .point_count = 1};
    AbSolverFrame first = {{0, 0}, {{0, 0}, 0, 1}, {{1, 0}, 0}};
    AbSolverFrame second = {{2, 0}, {{2, 0}, 0, 1}, {{-1, 0}, 0}};
    CHECK(
        ab_contact_velocity_initialize(&contact, &manifold, first, 1, second, 1, 0.5f));
    CHECK(contact.normal.x == 1 && contact.normal.y == 0 &&
          contact.points[0].normal_mass == 0.5f &&
          contact.points[0].tangent_mass == 0.5f &&
          contact.points[0].velocity_bias == 1 &&
          contact.points[0].normal_impulse == 5 &&
          contact.points[0].tangent_impulse == 7);
    AbVelocityContact original = contact;
    manifold.point_count = 0;
    CHECK(!ab_contact_velocity_initialize(&contact, &manifold, first, 1, second, 1,
                                          0.5f));
    CHECK(memcmp(&original, &contact, sizeof(contact)) == 0);
    return true;
}

static AbSolverFrame native_solver_frame(const uint32_t *position,
                                         const uint32_t *velocity,
                                         const uint32_t *local_center,
                                         const uint32_t *rotation) {
    AbSolverFrame frame = {
        .center = {from_bits(position[0]), from_bits(position[1])},
        .transform = {.sine = from_bits(rotation[0]), .cosine = from_bits(rotation[1])},
        .velocity = {{from_bits(velocity[0]), from_bits(velocity[1])},
                     from_bits(velocity[2])},
    };
    float local_x = from_bits(local_center[0]);
    float local_y = from_bits(local_center[1]);
    float rotated_x = frame.transform.cosine * local_x - frame.transform.sine * local_y;
    float rotated_y = frame.transform.cosine * local_y + local_x * frame.transform.sine;
    frame.transform.position =
        (AbVec2){frame.center.x - rotated_x, frame.center.y - rotated_y};
    return frame;
}

static bool native_velocity_initialize(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_VELOCITY_INITIALIZE_V2 4096\n") == 0);
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[132];
        for (size_t field = 0; field < 132; ++field) {
            CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
        }
        const uint32_t *position = words + 12;
        const uint32_t *input = words + 50;
        CHECK(input[37] >= 1 && input[37] <= 2);
        AbManifold manifold;
        memcpy(&manifold, words + 34, sizeof(manifold));
        AbVelocityContact contact = {
            .normal = {from_bits(input[18]), from_bits(input[19])},
            .first_inverse_mass = from_bits(input[30]),
            .second_inverse_mass = from_bits(input[31]),
            .first_inverse_inertia = from_bits(input[32]),
            .second_inverse_inertia = from_bits(input[33]),
            .friction = from_bits(input[34]),
            .tangent_speed = from_bits(input[36]),
            .normal_matrix = {from_bits(input[24]), from_bits(input[25]),
                              from_bits(input[27])},
            .normal_inverse = {from_bits(input[20]), from_bits(input[21]),
                               from_bits(input[23])},
            .first_body = input[28],
            .second_body = input[29],
            .point_count = input[37],
        };
        for (size_t point = 0; point < 2; ++point) {
            const uint32_t *data = input + point * 9;
            contact.points[point] =
                (AbVelocityPoint){{from_bits(data[0]), from_bits(data[1])},
                                  {from_bits(data[2]), from_bits(data[3])},
                                  from_bits(data[4]),
                                  from_bits(data[5]),
                                  from_bits(data[6]),
                                  from_bits(data[7]),
                                  from_bits(data[8])};
        }
        AbSolverFrame first =
            native_solver_frame(words, words + 6, position + 12, words + 128);
        AbSolverFrame second =
            native_solver_frame(words + 3, words + 9, position + 14, words + 130);
        CHECK(ab_contact_velocity_initialize(
            &contact, &manifold, first, from_bits(position[19]), second,
            from_bits(position[20]), from_bits(input[35])));
        uint32_t actual[39];
        memcpy(actual, input, sizeof(actual));
        for (size_t point = 0; point < 2; ++point) {
            const AbVelocityPoint *current = contact.points + point;
            const uint32_t fields[] = {
                to_bits(current->first_offset.x),  to_bits(current->first_offset.y),
                to_bits(current->second_offset.x), to_bits(current->second_offset.y),
                to_bits(current->normal_impulse),  to_bits(current->tangent_impulse),
                to_bits(current->normal_mass),     to_bits(current->tangent_mass),
                to_bits(current->velocity_bias)};
            memcpy(actual + point * 9, fields, sizeof(fields));
        }
        actual[18] = to_bits(contact.normal.x);
        actual[19] = to_bits(contact.normal.y);
        actual[20] = to_bits(contact.normal_inverse.xx);
        actual[21] = to_bits(contact.normal_inverse.xy);
        actual[22] = to_bits(contact.normal_inverse.xy);
        actual[23] = to_bits(contact.normal_inverse.yy);
        actual[24] = to_bits(contact.normal_matrix.xx);
        actual[25] = to_bits(contact.normal_matrix.xy);
        actual[26] = to_bits(contact.normal_matrix.xy);
        actual[27] = to_bits(contact.normal_matrix.yy);
        actual[37] = (uint32_t)contact.point_count;
        for (size_t field = 0; field < 39; ++field) {
            if (actual[field] != words[89 + field]) {
                fprintf(stderr,
                        "Native velocity initialization %zu field %zu: "
                        "%08" PRIx32 " != %08" PRIx32 "\n",
                        index, field, actual[field], words[89 + field]);
                return false;
            }
        }
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool position_point_checks(void) {
    AbPositionContact contact = {
        .local_normal = {1, 0}, .kind = AB_CONTACT_FACE_FIRST, .point_count = 1};
    AbPositionBody bodies[2] = {
        {.center = {0, 0}, .transform = {{0, 0}, 0, 1}, .inverse_mass = 1},
        {.center = {-0.1f, 0}, .transform = {{-0.1f, 0}, 0, 1}, .inverse_mass = 1},
    };
    float minimum = 0;
    CHECK(ab_contact_position_point(&contact, 0, &bodies[0], &bodies[1], &minimum));
    CHECK(minimum == -0.1f && bodies[0].center.x < 0 && bodies[1].center.x > -0.1f);
    CHECK(bodies[0].angle == 0 && bodies[1].angle == 0 &&
          bodies[0].transform.position.x == 0 &&
          bodies[1].transform.position.x == -0.1f);
    AbPositionBody original[2];
    memcpy(original, bodies, sizeof(original));
    CHECK(!ab_contact_position_point(&contact, 1, &bodies[0], &bodies[1], &minimum));
    CHECK(memcmp(original, bodies, sizeof(original)) == 0 && minimum == -0.1f);
    return true;
}

static bool native_position_point(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    CHECK(strcmp(header, "AB_ARM_POSITION_POINT_V1 4096\n") == 0);
    const uint32_t zero_velocity[3] = {0};
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[39];
        for (size_t field = 0; field < 39; ++field) {
            CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
        }
        const uint32_t *constraint = words + 6;
        CHECK(constraint[18] <= 2 && constraint[21] == 1 && words[38] <= 1);
        AbSolverFrame first_frame =
            native_solver_frame(words, zero_velocity, constraint + 12, words + 28);
        AbSolverFrame second_frame =
            native_solver_frame(words + 3, zero_velocity, constraint + 14, words + 30);
        AbPositionBody first = {first_frame.center, from_bits(words[2]),
                                first_frame.transform, from_bits(constraint[10]),
                                from_bits(constraint[16])};
        AbPositionBody second = {second_frame.center, from_bits(words[5]),
                                 second_frame.transform, from_bits(constraint[11]),
                                 from_bits(constraint[17])};
        AbPositionContact contact = {
            .local_points = {{from_bits(constraint[0]), from_bits(constraint[1])},
                             {from_bits(constraint[2]), from_bits(constraint[3])}},
            .local_normal = {from_bits(constraint[4]), from_bits(constraint[5])},
            .local_point = {from_bits(constraint[6]), from_bits(constraint[7])},
            .first_radius = from_bits(constraint[19]),
            .second_radius = from_bits(constraint[20]),
            .kind = (AbContactKind)constraint[18],
            .point_count = 1,
        };
        float minimum = 0;
        CHECK(ab_contact_position_point(&contact, 0, &first, &second, &minimum));
        const uint32_t actual[] = {to_bits(first.center.x),    to_bits(first.center.y),
                                   to_bits(first.angle),       to_bits(second.center.x),
                                   to_bits(second.center.y),   to_bits(second.angle),
                                   minimum >= -0.15f ? 1u : 0u};
        for (size_t field = 0; field < 7; ++field) {
            if (actual[field] != words[32 + field]) {
                fprintf(stderr,
                        "Native position point %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        index, field, actual[field], words[32 + field]);
                return false;
            }
        }
        CHECK(fgetc(file) == '\n');
    }
    return true;
}

static bool position_pass_checks(void) {
    AbPositionState positions[] = {{{0, 0}, 0}, {{-0.1f, 0}, 0}};
    AbPositionConstraint constraints[2] = {{
        .geometry = {.local_normal = {1, 0},
                     .kind = AB_CONTACT_FACE_FIRST,
                     .point_count = 1},
        .first_body = 0,
        .second_body = 1,
        .first_inverse_mass = 1,
        .second_inverse_mass = 1,
    }};
    CHECK(ab_contacts_solve_position(positions, 2, constraints, 1) ==
          AB_POSITION_RESOLVED);
    CHECK(positions[0].center.x < 0 && positions[1].center.x > -0.1f);
    AbPositionState original[2];
    memcpy(original, positions, sizeof(original));
    constraints[1].second_body = 2;
    CHECK(ab_contacts_solve_position(positions, 2, constraints, 2) ==
          AB_POSITION_INVALID);
    CHECK(memcmp(original, positions, sizeof(original)) == 0);
    CHECK(ab_contacts_solve_position(NULL, 0, NULL, 0) == AB_POSITION_RESOLVED);
    CHECK(ab_contacts_solve_position(NULL, 0, constraints, 1) == AB_POSITION_INVALID);
    positions[0] = (AbPositionState){{0, 0}, 0};
    positions[1] = (AbPositionState){{-0.1f, 0}, 0};
    CHECK(ab_contacts_solve_toi_position(positions, 2, constraints, 1, 0, 0) ==
          AB_POSITION_UNRESOLVED);
    CHECK(positions[0].center.x < 0 && positions[1].center.x == -0.1f);
    memcpy(original, positions, sizeof(original));
    CHECK(ab_contacts_solve_toi_position(positions, 2, constraints, 1, 2, 0) ==
          AB_POSITION_INVALID);
    CHECK(memcmp(original, positions, sizeof(original)) == 0);
    CHECK(ab_contacts_solve_toi_position(NULL, 0, NULL, 0, SIZE_MAX, SIZE_MAX) ==
          AB_POSITION_RESOLVED);
    return true;
}

static bool native_position_passes(FILE *file) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file) != NULL);
    bool toi = strcmp(header, "AB_ANDROID_TOI_POSITION_V1 4096\n") == 0;
    CHECK(toi || strcmp(header, "AB_ANDROID_POSITION_PASSES_V1 4096\n") == 0);
    size_t iterations = 0;
    for (size_t index = 0; index < 4096; ++index) {
        uint32_t words[234];
        CHECK(fscanf(file, "%8" SCNx32 " %8" SCNx32, words, words + 1) == 2);
        CHECK(words[0] <= 4 && words[1] >= 1 && words[1] <= 10);
        size_t position_offset = toi ? 4 : 2;
        size_t constraint_offset = position_offset + 12;
        size_t output = constraint_offset + (size_t)words[0] * 22;
        size_t count = output + (size_t)words[1] * 13;
        for (size_t field = 2; field < count; ++field) {
            CHECK(fscanf(file, "%8" SCNx32, words + field) == 1);
        }
        CHECK(!toi || (words[2] < 4 && words[3] < 4));
        AbPositionState positions[4];
        AbPositionConstraint constraints[4];
        for (size_t body = 0; body < 4; ++body) {
            const uint32_t *current = words + position_offset + body * 3;
            positions[body] = (AbPositionState){
                {from_bits(current[0]), from_bits(current[1])}, from_bits(current[2])};
        }
        for (size_t contact = 0; contact < words[0]; ++contact) {
            const uint32_t *current = words + constraint_offset + contact * 22;
            CHECK(current[8] < 4 && current[9] < 4 && current[18] <= 2 &&
                  current[21] <= 2);
            constraints[contact] = (AbPositionConstraint){
                .geometry =
                    {
                        .local_points = {{from_bits(current[0]), from_bits(current[1])},
                                         {from_bits(current[2]),
                                          from_bits(current[3])}},
                        .local_normal = {from_bits(current[4]), from_bits(current[5])},
                        .local_point = {from_bits(current[6]), from_bits(current[7])},
                        .first_radius = from_bits(current[19]),
                        .second_radius = from_bits(current[20]),
                        .kind = (AbContactKind)current[18],
                        .point_count = current[21],
                    },
                .first_body = current[8],
                .second_body = current[9],
                .first_local_center = {from_bits(current[12]), from_bits(current[13])},
                .second_local_center = {from_bits(current[14]), from_bits(current[15])},
                .first_inverse_mass = from_bits(current[10]),
                .second_inverse_mass = from_bits(current[11]),
                .first_inverse_inertia = from_bits(current[16]),
                .second_inverse_inertia = from_bits(current[17]),
            };
        }
        for (size_t iteration = 0; iteration < words[1]; ++iteration) {
            AbPositionResult result =
                toi ? ab_contacts_solve_toi_position(positions, 4, constraints,
                                                     words[0], words[2], words[3])
                    : ab_contacts_solve_position(positions, 4, constraints, words[0]);
            CHECK(result != AB_POSITION_INVALID);
            uint32_t actual[13];
            for (size_t body = 0; body < 4; ++body) {
                actual[body * 3] = to_bits(positions[body].center.x);
                actual[body * 3 + 1] = to_bits(positions[body].center.y);
                actual[body * 3 + 2] = to_bits(positions[body].angle);
            }
            actual[12] = result == AB_POSITION_RESOLVED ? 1u : 0u;
            for (size_t field = 0; field < 13; ++field) {
                if (actual[field] != words[output + field]) {
                    fprintf(stderr,
                            "Android position pass %zu iteration %zu field "
                            "%zu: %08" PRIx32 " != %08" PRIx32 "\n",
                            index, iteration, field, actual[field],
                            words[output + field]);
                    return false;
                }
            }
            output += 13;
            ++iterations;
        }
        CHECK(fgetc(file) == '\n');
    }
    if (toi) {
        CHECK(fgets(header, sizeof(header), file));
        CHECK(strcmp(header, "AB TOI position reference complete\n") == 0);
    }
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 Android %sposition-pass cases match all %zu iterations.\n",
           toi ? "TOI " : "", iterations);
    return true;
}

int main(int argc, char **argv) {
    bool toi_position = argc == 3 && strcmp(argv[1], "--toi-position") == 0;
    if (argc > 2 && !toi_position) {
        fprintf(stderr, "Usage: ab_physics_tests [NATIVE_GEOMETRY_TRACE | "
                        "--toi-position NATIVE_TOI_TRACE]\n");
        return 2;
    }
    if (!circle_checks() || !polygon_checks() || !polygon_build_checks() ||
        !contact_checks() || !ray_checks() || !body_checks() || !body_reset_checks() ||
        !warm_start_checks() || !velocity_solver_checks() ||
        !velocity_initialize_checks() || !position_point_checks() ||
        !position_pass_checks()) {
        return 1;
    }
    if (argc >= 2) {
        FILE *file = fopen(argv[toi_position ? 2 : 1], "rb");
        if (!file) {
            fprintf(stderr, "Cannot open native geometry trace\n");
            return 1;
        }
        bool okay;
        if (toi_position) {
            okay = native_position_passes(file);
        } else {
            okay = native_circles(file) && native_polygons(file) &&
                   native_circle_contacts(file) &&
                   native_polygon_circle_contacts(file) &&
                   native_polygon_contacts(file) && native_polygon_build(file) &&
                   native_circle_rays(file) && native_polygon_rays(file) &&
                   native_body_mass(file) && native_body_reset(file) &&
                   native_warm_start(file) && native_velocity_solver(file) &&
                   native_velocity_initialize(file) && native_position_point(file);
            if (okay && !toi_position) {
                int next = fgetc(file);
                if (next == EOF) {
                    okay = !ferror(file);
                } else {
                    okay = ungetc(next, file) != EOF && native_position_passes(file);
                }
            }
        }
        if (fclose(file) != 0) {
            okay = false;
        }
        if (!okay) {
            return 1;
        }
        if (!toi_position) {
            puts("All 4096 circle and 4096 polygon cases match the pinned ARMv7 "
                 "functions bit for bit, plus 12288 contact/world manifolds, 4096 "
                 "hulls, 8192 rays, 4096 mass overrides, 4096 mass resets, and 4096 "
                 "warm starts, 4096 velocity solves, 4096 initializations, and 4096 "
                 "position corrections.");
        }
    }
    return 0;
}
