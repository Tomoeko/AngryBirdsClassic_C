#include "ab/game_contact_world.h"
#include "world_storage.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Contact world failed at %d: %s\n", __LINE__, #condition); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbPhysicsWorld *world;
    AbGameContactWorld *owner;
    AbDeferredVelocity pending[2];
    AbDataEntry velocity_entries[3], glue_entries[2];
    AbDataTable velocity, glue;
    size_t lookups, begins, ends, filters, posts, destroyed, close_rejections;
    bool disable, offer, fail, scripts;
} Host;

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static bool object_lookup(void *context, uintptr_t object,
                          AbGameContactWorldObject *output, AbError *error) {
    (void)error;
    Host *host = context;
    CHECK(object == 1 || object == 2);
    ++host->lookups;
    *output = (AbGameContactWorldObject){
        .collision = {AB_COLLIDER_BASIC, false},
        .script = {text(object == 1 ? "first" : "second").string, host->scripts}};
    return true;
}

static bool name_index(AbSpan name, size_t *output) {
    if (name.size == 5 && memcmp(name.data, "first", 5) == 0) {
        *output = 0;
        return true;
    }
    if (name.size == 6 && memcmp(name.data, "second", 6) == 0) {
        *output = 1;
        return true;
    }
    return false;
}

static bool velocity_lookup(void *context, AbSpan name, AbDeferredVelocity **output,
                            AbError *error) {
    (void)error;
    Host *host = context;
    size_t index;
    CHECK(name_index(name, &index));
    *output = host->pending + index;
    return true;
}

static bool body_lookup(void *context, AbSpan name, size_t *output, AbError *error) {
    (void)context;
    (void)error;
    return name_index(name, output);
}

static bool collision(void *context, AbGameContactCall *call, AbError *error) {
    Host *host = context;
    CHECK(call->first.size == 5 && call->second.size == 6 && call->normal.x == 1 &&
          call->normal.y == 0 && call->point.x == 1 && call->point.y == 0 &&
          !call->response.should_disable_contact && host->world->locked &&
          !ab_game_contact_world_close(host->owner, NULL));
    ++host->close_rejections;
    ++host->begins;
    if (host->fail) {
        return ab_fail(error, "Controlled game-contact world failure");
    }
    call->response.should_disable_contact = host->disable;
    if (host->offer) {
        host->velocity_entries[0] = (AbDataEntry){text("birdName"), text("second")};
        host->velocity_entries[1] =
            (AbDataEntry){text("afterCollisionVelocityX"),
                          {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3e800000)}};
        host->velocity_entries[2] =
            (AbDataEntry){text("afterCollisionVelocityY"),
                          {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f000000)}};
        host->velocity = (AbDataTable){host->velocity_entries, 3, 3, NULL};
        host->glue_entries[0] = (AbDataEntry){
            {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)}, text("first")};
        host->glue_entries[1] = (AbDataEntry){
            {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x40000000)}, text("second")};
        host->glue = (AbDataTable){host->glue_entries, 2, 2, NULL};
        call->response.new_collision_parameters =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &host->velocity};
        call->response.glue_contact_pair =
            (AbDataValue){.kind = AB_DATA_TABLE, .table = &host->glue};
    }
    return true;
}

static bool ended(void *context, AbSpan first, AbSpan second, AbError *error) {
    (void)error;
    Host *host = context;
    CHECK(first.size == 5 && second.size == 6);
    ++host->ends;
    return true;
}

static bool previous_filter(void *context, const AbPhysicsWorld *world, size_t first,
                            size_t second) {
    Host *host = context;
    CHECK(world == host->world && !ab_game_contact_world_close(host->owner, NULL));
    ++host->filters;
    ++host->close_rejections;
    return ab_fixture_filters_collide(world->storage.fixtures[first].filter,
                                      world->storage.fixtures[second].filter);
}

static void previous_post(void *context, AbPhysicsWorld *world, size_t contact,
                          const AbContactImpulse *impulse) {
    Host *host = context;
    if (world == host->world && contact < world->contact_count && impulse) {
        ++host->posts;
    }
}

static void previous_destroy(void *context, AbPhysicsWorld *world, size_t fixture) {
    Host *host = context;
    if (world == host->world && fixture < world->fixture_slot_count &&
        !ab_game_contact_world_close(host->owner, NULL)) {
        ++host->destroyed;
        ++host->close_rejections;
    }
}

static bool open_world(AbPhysicsWorld *world, WorldStorage *memory, bool masked) {
    AbWorldStorage storage = prepare_storage(memory, 2, 2);
    for (size_t index = 0; index < 2; ++index) {
        AbBodyDefinition definition = {.type = index ? AB_BODY_DYNAMIC : AB_BODY_STATIC,
                                       .position = {(float)index * 2, 0},
                                       .linear_velocity = {index ? -1 : 0, 0},
                                       .active = true,
                                       .awake = true,
                                       .gravity_scale = 1};
        CHECK(ab_body_initialize(&definition, &memory->bodies[index].motion));
        memory->fixtures[index] = (AbWorldFixture){
            .shape = {.kind = AB_SHAPE_CIRCLE, .data.circle = {1, {0, 0}}},
            .body = index,
            .density = index ? 1 : 0,
            .filter = {1, masked ? 0 : UINT16_MAX, 0},
            .user_data = index + 1};
    }
    return ab_world_open(world, default_definition(), storage, NULL);
}

static bool world_case(unsigned profile) {
    WorldStorage memory;
    AbPhysicsWorld world = {0};
    CHECK(open_world(&world, &memory, profile == 3));
    AbGameContactRoutes routes;
    CHECK(ab_game_contact_routes_initialize(&routes));
    AbGameContactWorld owner = {0};
    Host host = {.world = &world,
                 .owner = &owner,
                 .disable = profile == 0,
                 .offer = profile == 0,
                 .fail = profile == 2,
                 .scripts = profile != 4};
    AbClassicGluePair pairs[4];
    AbClassicGlueQueue queue = {pairs, 0, 4};
    if (profile != 3) {
        world.listener = (AbWorldListener){.context = &host,
                                           .filter = previous_filter,
                                           .post_solve = previous_post,
                                           .destroy_fixture = previous_destroy};
    }
    AbWorldListener previous = world.listener;
    AbGameContactWorldConfig config = {
        .routes = &routes,
        .glue = &queue,
        .object_context = &host,
        .object = object_lookup,
        .basic = {&host, collision, velocity_lookup, body_lookup},
        .end = ended};
    CHECK(ab_game_contact_world_open(&world, config, &owner, NULL));
    AbGameContactWorld rejected = {0};
    CHECK(!ab_game_contact_world_open(&world, config, &rejected, NULL) &&
          !rejected.world && owner.world == &world);
    bool okay = ab_world_step(&world, 1.0f / 30, 8, 3, NULL);
    if (profile == 2) {
        CHECK(!okay && owner.failed && world.failed && host.begins == 1 &&
              strstr(owner.error.message, "Controlled") && !world.locked &&
              memory.bodies[1].motion.mass.transform.position.x == 2 &&
              !host.pending[1].pending && !queue.count);
    } else {
        CHECK(okay && !owner.failed && !world.failed);
        if (profile == 3) {
            CHECK(!world.contact_count && !host.lookups && !host.begins &&
                  !world.listener.filter);
        } else if (profile == 4) {
            CHECK(host.lookups && !host.begins && !host.pending[1].pending &&
                  !queue.count);
        } else if (profile == 0) {
            CHECK(host.begins == 1 && world.contact_count == 1 &&
                  !(memory.contacts[0].state.flags & AB_CONTACT_ENABLED) &&
                  memory.bodies[1].motion.mass.linear_velocity.x == -1 &&
                  host.pending[1].pending && host.pending[1].velocity.x == 0.25f &&
                  host.pending[1].velocity.y == 0.5f && queue.count == 1 &&
                  pairs[0].first_body == 0 && pairs[0].second_body == 1 && !host.posts);
            owner.config.begin_disabled = true;
            CHECK(ab_world_set_transform(&world, 1, (AbVec2){10, 0}, 0, NULL) &&
                  ab_world_step(&world, 0, 8, 3, NULL) && host.ends == 1 &&
                  host.pending[1].pending && queue.count == 1);
        } else {
            CHECK(host.begins == 1 && host.posts > 0 && !queue.count &&
                  !host.pending[1].pending);
        }
        if (profile != 3) {
            CHECK(host.filters > 0 && ab_world_destroy_body(&world, 1, NULL) &&
                  host.destroyed == 1);
        }
    }
    CHECK(ab_game_contact_world_close(&owner, NULL) && !owner.world &&
          world.listener.context == previous.context &&
          world.listener.filter == previous.filter &&
          world.listener.post_solve == previous.post_solve &&
          world.listener.destroy_fixture == previous.destroy_fixture &&
          !world.listener.begin && !world.listener.end && !world.listener.pre_solve &&
          ab_game_contact_world_close(&owner, NULL));
    ab_world_close(&world);
    return true;
}

#include "game_contact_world_native.h"

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    for (unsigned profile = 0; profile < 5; ++profile) {
        if (!world_case(profile)) {
            return 1;
        }
    }
    puts("World listener integration preserves geometry, deferred/glue responses, "
         "end dispatch, masks and previous callback contexts; host errors stop "
         "physics.");
    FILE *file = argc == 2 ? fopen(argv[1], "rb") : NULL;
    bool okay = argc != 2 || (file && native_world_checks(file));
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
