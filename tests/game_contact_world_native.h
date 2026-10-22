#ifndef AB_TEST_GAME_CONTACT_WORLD_NATIVE_H
#define AB_TEST_GAME_CONTACT_WORLD_NATIVE_H

#include "game_contact_world_cases.h"

#include <inttypes.h>

typedef struct {
    Host state;
    uint32_t input[8], sample, frame, event_count, events[64][7];
    size_t compared_words;
} NativeWorldHost;

static float world_number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool native_world_object(void *context, uintptr_t object,
                                AbGameContactWorldObject *output, AbError *error) {
    (void)error;
    NativeWorldHost *host = context;
    CHECK(object == 1 || object == 2);
    *output = (AbGameContactWorldObject){
        .collision = {AB_COLLIDER_BASIC, false},
        .script = {text(object == 1 ? "first" : "second").string,
                   (host->input[0] &
                    ((uint32_t)CONTACT_WORLD_FIRST_SCRIPT << (object - 1))) != 0}};
    return true;
}

static bool native_world_event(NativeWorldHost *host, uint32_t kind, AbSpan first,
                               AbSpan second, AbVec2 normal, AbVec2 point) {
    size_t first_index, second_index;
    CHECK(host->event_count < 64 && name_index(first, &first_index) &&
          name_index(second, &second_index));
    uint32_t *event = host->events[host->event_count++];
    event[0] = kind;
    event[1] = (uint32_t)first_index + 1;
    event[2] = (uint32_t)second_index + 1;
    event[3] = contact_world_bits(normal.x);
    event[4] = contact_world_bits(normal.y);
    event[5] = contact_world_bits(point.x);
    event[6] = contact_world_bits(point.y);
    return true;
}

static bool native_world_collision(void *context, AbGameContactCall *call,
                                   AbError *error) {
    (void)error;
    NativeWorldHost *host = context;
    CHECK(!call->response.should_disable_contact &&
          call->response.new_collision_parameters.kind == AB_DATA_NIL &&
          call->response.glue_contact_pair.kind == AB_DATA_NIL &&
          native_world_event(host, 1, call->first, call->second, call->normal,
                             call->point));
    ++host->state.begins;
    call->response.should_disable_contact =
        (host->input[0] & CONTACT_WORLD_DISABLE) != 0;
    if (host->input[0] & CONTACT_WORLD_OFFER) {
        uint32_t velocity[2];
        game_contact_world_proposal(host->sample, host->frame, velocity);
        Host *state = &host->state;
        state->velocity_entries[0] = (AbDataEntry){text("birdName"), text("second")};
        state->velocity_entries[1] =
            (AbDataEntry){text("afterCollisionVelocityX"),
                          {.kind = AB_DATA_NUMBER, .bits = velocity[0]}};
        state->velocity_entries[2] =
            (AbDataEntry){text("afterCollisionVelocityY"),
                          {.kind = AB_DATA_NUMBER, .bits = velocity[1]}};
        state->velocity = (AbDataTable){state->velocity_entries, 3, 3, NULL};
        state->glue_entries[0] = (AbDataEntry){
            {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)}, text("first")};
        state->glue_entries[1] = (AbDataEntry){
            {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)}, text("second")};
        state->glue = (AbDataTable){state->glue_entries, 2, 2, NULL};
        call->response.new_collision_parameters =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &state->velocity};
        call->response.glue_contact_pair =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &state->glue};
    }
    return true;
}

static bool native_world_velocity(void *context, AbSpan name,
                                  AbDeferredVelocity **output, AbError *error) {
    NativeWorldHost *host = context;
    return velocity_lookup(&host->state, name, output, error);
}

static bool native_world_end(void *context, AbSpan first, AbSpan second,
                             AbError *error) {
    (void)error;
    NativeWorldHost *host = context;
    CHECK(native_world_event(host, 2, first, second, (AbVec2){0}, (AbVec2){0}));
    ++host->state.ends;
    return true;
}

static bool native_world_compare(FILE *file, NativeWorldHost *host, const char *section,
                                 const uint32_t *actual, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[field] != expected) {
            fprintf(stderr,
                    "World case %" PRIu32 " frame %" PRIu32 " %s[%zu]: "
                    "C %08" PRIx32 ", native %08" PRIx32 "\n",
                    host->sample, host->frame, section, field, actual[field], expected);
            return false;
        }
    }
    CHECK(fgetc(file) == '\n');
    host->compared_words += count;
    return true;
}

static bool native_world_snapshot(FILE *file, NativeWorldHost *host,
                                  const AbPhysicsWorld *world,
                                  const AbClassicGlueQueue *queue) {
    const Host *state = &host->state;
    uint32_t summary[] = {host->sample,
                          host->frame,
                          host->event_count,
                          (uint32_t)state->begins,
                          (uint32_t)state->ends,
                          (uint32_t)state->posts,
                          (uint32_t)queue->count,
                          state->pending[0].pending,
                          contact_world_bits(state->pending[0].velocity.x),
                          contact_world_bits(state->pending[0].velocity.y),
                          state->pending[1].pending,
                          contact_world_bits(state->pending[1].velocity.x),
                          contact_world_bits(state->pending[1].velocity.y),
                          (uint32_t)world->contact_count};
    CHECK(native_world_compare(file, host, "summary", summary, 14));
    for (size_t index = 0; index < 2; ++index) {
        const AbBodyMotion *body = &world->storage.bodies[index].motion;
        const AbBodyMassState *mass = &body->mass;
        uint32_t values[] = {mass->type,
                             body->awake,
                             contact_world_bits(mass->transform.position.x),
                             contact_world_bits(mass->transform.position.y),
                             contact_world_bits(mass->transform.sine),
                             contact_world_bits(mass->transform.cosine),
                             contact_world_bits(mass->linear_velocity.x),
                             contact_world_bits(mass->linear_velocity.y),
                             contact_world_bits(mass->angular_velocity),
                             contact_world_bits(mass->mass),
                             contact_world_bits(mass->inverse_mass),
                             contact_world_bits(mass->inertia),
                             contact_world_bits(mass->inverse_inertia),
                             contact_world_bits(body->sleep_time),
                             contact_world_bits(mass->previous_angle),
                             contact_world_bits(mass->angle),
                             contact_world_bits(mass->previous_center.x),
                             contact_world_bits(mass->previous_center.y),
                             contact_world_bits(mass->center.x),
                             contact_world_bits(mass->center.y)};
        CHECK(
            native_world_compare(file, host, index ? "dynamic" : "static", values, 20));
    }
    CHECK(world->contact_count <= 1);
    uint32_t contact[12] = {0};
    if (world->contact_count) {
        const AbContactState *contact_state = &world->storage.contacts[0].state;
        const AbManifold *manifold = &contact_state->manifold;
        contact[0] = contact_state->flags;
        contact[1] = manifold->point_count;
        CHECK(manifold->point_count <= 1);
        if (manifold->point_count) {
            contact[2] = manifold->kind;
            contact[3] = contact_world_bits(manifold->local_normal.x);
            contact[4] = contact_world_bits(manifold->local_normal.y);
            contact[5] = contact_world_bits(manifold->local_point.x);
            contact[6] = contact_world_bits(manifold->local_point.y);
            contact[7] = contact_world_bits(manifold->points[0].local_point.x);
            contact[8] = contact_world_bits(manifold->points[0].local_point.y);
            contact[9] = contact_world_bits(manifold->points[0].normal_impulse);
            contact[10] = contact_world_bits(manifold->points[0].tangent_impulse);
            contact[11] = manifold->points[0].feature_id;
        }
    }
    CHECK(native_world_compare(file, host, "contact", contact, 12));
    for (size_t index = 0; index < host->event_count; ++index) {
        CHECK(native_world_compare(file, host, "event", host->events[index], 7));
    }
    for (size_t index = 0; index < queue->count; ++index) {
        uint32_t pair[] = {(uint32_t)queue->pairs[index].first_body,
                           (uint32_t)queue->pairs[index].second_body};
        CHECK(native_world_compare(file, host, "glue", pair, 2));
    }
    return true;
}

static bool native_world_case(FILE *file, uint32_t sample, size_t *words) {
    WorldStorage memory;
    AbWorldStorage storage = prepare_storage(&memory, 2, 2);
    AbPhysicsWorld world = {0};
    AbGameContactWorld owner = {0};
    NativeWorldHost host = {.state = {.world = &world, .owner = &owner},
                            .sample = sample};
    game_contact_world_case(sample, host.input);
    CHECK(native_world_compare(file, &host, "input", host.input, 8));
    uint32_t flags = host.input[0];
    for (size_t index = 0; index < 2; ++index) {
        AbBodyDefinition definition = {
            .type = index ? AB_BODY_DYNAMIC : AB_BODY_STATIC,
            .position = {index ? world_number(host.input[3]) : 0,
                         index ? world_number(host.input[4]) : 0},
            .linear_velocity = {index ? world_number(host.input[5]) : 0,
                                index ? world_number(host.input[6]) : 0},
            .active = true,
            .awake = true,
            .gravity_scale = 1};
        CHECK(ab_body_initialize(&definition, &memory.bodies[index].motion));
        memory.fixtures[index] = (AbWorldFixture){
            .shape = {.kind = AB_SHAPE_CIRCLE,
                      .data.circle = {world_number(host.input[1 + index]), {0, 0}}},
            .body = index,
            .density = index ? 1 : 0,
            .filter = {1, flags & CONTACT_WORLD_MASKED ? 0 : UINT16_MAX, 0},
            .user_data = flags & ((uint32_t)CONTACT_WORLD_FIRST_OBJECT << index)
                             ? index + 1
                             : 0};
    }
    CHECK(ab_world_open(&world, default_definition(), storage, NULL));
    world.listener = (AbWorldListener){.context = &host.state,
                                       .post_solve = previous_post,
                                       .destroy_fixture = previous_destroy};
    AbClassicGluePair pairs[64];
    AbClassicGlueQueue queue = {pairs, 0, 64};
    AbGameContactRoutes routes;
    CHECK(ab_game_contact_routes_initialize(&routes));
    AbGameContactWorldConfig config = {
        .routes = &routes,
        .glue = &queue,
        .object_context = &host,
        .object = native_world_object,
        .basic = {&host, native_world_collision, native_world_velocity, body_lookup},
        .end = native_world_end,
        .begin_disabled = (flags & CONTACT_WORLD_BEGIN_DISABLED) != 0};
    CHECK(ab_game_contact_world_open(&world, config, &owner, NULL));
    bool okay = true;
    for (uint32_t frame = 0; frame <= 16; ++frame) {
        host.frame = frame;
        host.event_count = 0;
        if (frame == 16) {
            owner.config.begin_disabled = true;
            okay = ab_world_set_transform(&world, 1, (AbVec2){10, 0}, 0, NULL);
        }
        if (!okay || !ab_world_step(&world, frame == 16 ? 0 : 1.0f / 30, 8, 3, NULL) ||
            !native_world_snapshot(file, &host, &world, &queue)) {
            okay = false;
            break;
        }
    }
    if (okay) {
        okay = ab_world_destroy_body(&world, 1, NULL);
        uint32_t final[] = {(uint32_t)host.state.ends, (uint32_t)host.state.destroyed,
                            (uint32_t)world.body_count};
        okay = okay && native_world_compare(file, &host, "destroy", final, 3);
    }
    okay = ab_game_contact_world_close(&owner, NULL) && okay;
    ab_world_close(&world);
    *words += host.compared_words;
    return okay;
}

static bool native_world_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_GAME_CONTACT_WORLD_V1 64 16\n") == 0);
    size_t words = 0;
    for (uint32_t sample = 0; sample < 64; ++sample) {
        CHECK(native_world_case(file, sample, &words));
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB game contact world reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 64 native game-contact worlds match over 1,088 steps: %zu words "
           "of body/contact state, callback geometry/order and retained responses.\n",
           words);
    return true;
}

#endif
