#include "ab/physics_contact.h"

#include "internal.h"

#include <float.h>
#include <math.h>

typedef struct {
    float distance;
    size_t edge;
} SeparatingEdge;

typedef struct {
    AbVec2 point;
    uint32_t feature;
} ClipPoint;

static AbVec2 inverse_vector(AbVec2 vector, AbRigidTransform transform) {
    /* This path adds a negative product, unlike the circle query's subtraction. */
    return (AbVec2){vector.y * transform.sine + transform.cosine * vector.x,
                    vector.y * transform.cosine + vector.x * -transform.sine};
}

static AbRigidTransform relative_transform(AbRigidTransform first,
                                           AbRigidTransform second) {
    AbVec2 delta = {first.position.x - second.position.x,
                    first.position.y - second.position.y};
    return (AbRigidTransform){
        inverse_vector(delta, second),
        second.cosine * first.sine - second.sine * first.cosine,
        first.sine * second.sine + second.cosine * first.cosine,
    };
}

static SeparatingEdge separating_edge(const AbPolygon *first,
                                      AbRigidTransform first_transform,
                                      const AbPolygon *second,
                                      AbRigidTransform second_transform) {
    AbRigidTransform relative = relative_transform(first_transform, second_transform);
    SeparatingEdge result = {-FLT_MAX, 0};
    for (size_t edge = 0; edge < first->count; ++edge) {
        AbVec2 normal = ab_physics_rotate(first->normals[edge], relative);
        AbVec2 vertex = ab_physics_point(first->vertices[edge], relative);
        float nearest = FLT_MAX;
        for (size_t index = 0; index < second->count; ++index) {
            float distance = normal.y * (second->vertices[index].y - vertex.y);
            distance = distance + normal.x * (second->vertices[index].x - vertex.x);
            if (distance < nearest) {
                nearest = distance;
            }
        }
        if (nearest > result.distance) {
            result = (SeparatingEdge){nearest, edge};
        }
    }
    return result;
}

static void incident_edge(ClipPoint output[2], const AbPolygon *reference,
                          AbRigidTransform reference_transform, size_t edge,
                          const AbPolygon *incident,
                          AbRigidTransform incident_transform) {
    AbVec2 normal =
        inverse_vector(ab_physics_rotate(reference->normals[edge], reference_transform),
                       incident_transform);
    size_t selected = 0;
    float nearest = FLT_MAX;
    for (size_t index = 0; index < incident->count; ++index) {
        float distance = normal.y * incident->normals[index].y;
        distance = distance + normal.x * incident->normals[index].x;
        if (distance < nearest) {
            nearest = distance;
            selected = index;
        }
    }
    size_t next = selected + 1 == incident->count ? 0 : selected + 1;
    output[0] =
        (ClipPoint){ab_physics_point(incident->vertices[selected], incident_transform),
                    (uint32_t)edge | (uint32_t)selected << 8 | UINT32_C(0x10000)};
    output[1] =
        (ClipPoint){ab_physics_point(incident->vertices[next], incident_transform),
                    (uint32_t)edge | (uint32_t)next << 8 | UINT32_C(0x10000)};
}

static size_t clip_segment(ClipPoint output[2], const ClipPoint input[2], AbVec2 normal,
                           float offset, size_t vertex) {
    float first = normal.y * input[0].point.y + normal.x * input[0].point.x;
    float second = normal.y * input[1].point.y + normal.x * input[1].point.x;
    first = first - offset;
    second = second - offset;
    size_t count = 0;
    if (first <= 0) {
        output[count++] = input[0];
    }
    if (second <= 0) {
        output[count++] = input[1];
    }
    if (first * second < 0) {
        float fraction = first / (first - second);
        AbVec2 delta = {input[1].point.x - input[0].point.x,
                        input[1].point.y - input[0].point.y};
        AbVec2 point = {input[0].point.x + fraction * delta.x,
                        input[0].point.y + fraction * delta.y};
        uint32_t feature = (uint32_t)vertex | (input[0].feature & UINT32_C(0xff00)) |
                           UINT32_C(0x1000000);
        output[count++] = (ClipPoint){point, feature};
    }
    return count;
}

static uint32_t reverse_feature(uint32_t feature) {
    return (feature & UINT32_C(0x00ff00ff)) << 8 |
           (feature & UINT32_C(0xff00ff00)) >> 8;
}

void ab_collide_polygons(AbManifold *output, const AbPolygon *first,
                         AbRigidTransform first_transform, const AbPolygon *second,
                         AbRigidTransform second_transform) {
    output->point_count = 0;
    float radius = first->radius + second->radius;
    SeparatingEdge first_edge =
        separating_edge(first, first_transform, second, second_transform);
    if (first_edge.distance > radius) {
        return;
    }
    SeparatingEdge second_edge =
        separating_edge(second, second_transform, first, first_transform);
    if (second_edge.distance > radius) {
        return;
    }
    bool reverse = second_edge.distance > first_edge.distance + 0x1.47ae16p-8f;
    const AbPolygon *reference = reverse ? second : first;
    const AbPolygon *incident = reverse ? first : second;
    AbRigidTransform reference_transform = reverse ? second_transform : first_transform;
    AbRigidTransform incident_transform = reverse ? first_transform : second_transform;
    size_t edge = reverse ? second_edge.edge : first_edge.edge;
    output->kind = reverse ? AB_CONTACT_FACE_SECOND : AB_CONTACT_FACE_FIRST;
    ClipPoint incident_points[2];
    incident_edge(incident_points, reference, reference_transform, edge, incident,
                  incident_transform);

    size_t next = edge + 1 == reference->count ? 0 : edge + 1;
    AbVec2 first_vertex = reference->vertices[edge];
    AbVec2 second_vertex = reference->vertices[next];
    AbVec2 tangent = {second_vertex.x - first_vertex.x,
                      second_vertex.y - first_vertex.y};
    float length = sqrtf(tangent.y * tangent.y + tangent.x * tangent.x);
    if (length >= FLT_EPSILON) {
        float inverse = 1.0f / length;
        tangent.x = tangent.x * inverse;
        tangent.y = tangent.y * inverse;
    }
    AbVec2 world_tangent = ab_physics_rotate(tangent, reference_transform);
    AbVec2 world_first = ab_physics_point(first_vertex, reference_transform);
    AbVec2 world_second = ab_physics_point(second_vertex, reference_transform);
    float first_offset =
        world_tangent.y * world_first.y + world_tangent.x * world_first.x;
    first_offset = radius - first_offset;
    float second_offset =
        world_tangent.y * world_second.y + world_tangent.x * world_second.x;
    second_offset = radius + second_offset;
    ClipPoint clipped_first[2], clipped_second[2];
    AbVec2 negative_tangent = {-world_tangent.x, -world_tangent.y};
    if (clip_segment(clipped_first, incident_points, negative_tangent, first_offset,
                     edge) < 2 ||
        clip_segment(clipped_second, clipped_first, world_tangent, second_offset,
                     next) < 2) {
        return;
    }
    output->local_normal = (AbVec2){tangent.y, -tangent.x};
    output->local_point = (AbVec2){(second_vertex.x + first_vertex.x) * 0.5f,
                                   (second_vertex.y + first_vertex.y) * 0.5f};
    AbVec2 normal = {world_tangent.y, -world_tangent.x};
    float front_offset = normal.y * world_first.y + normal.x * world_first.x;
    for (size_t index = 0; index < 2; ++index) {
        AbVec2 point = clipped_second[index].point;
        float separation = normal.y * point.y + normal.x * point.x;
        separation = separation - front_offset;
        if (separation <= radius) {
            AbContactPoint *contact = output->points + output->point_count++;
            AbVec2 delta = {point.x - incident_transform.position.x,
                            point.y - incident_transform.position.y};
            contact->local_point = inverse_vector(delta, incident_transform);
            contact->feature_id = reverse
                                      ? reverse_feature(clipped_second[index].feature)
                                      : clipped_second[index].feature;
        }
    }
}
