#ifndef AB_TEST_CLASSIC_CONTACTS_H
#define AB_TEST_CLASSIC_CONTACTS_H

#include "ab/classic_contacts.h"

typedef struct {
    AbClassicContacts *contacts;
    AbDataEntry velocity_entries[3], glue_entries[2];
    AbDataTable velocity, glue;
    size_t begins, ends, specials;
} ClassicContactCheck;

typedef struct {
    AbClassicContacts *contacts;
    AbDataEntry response_entries[3];
    AbDataTable response;
    size_t begins;
} DynamicContactCheck;

static bool dynamic_contact_begin(void *context, AbGameContactCall *call,
                                  AbError *error) {
    (void)error;
    DynamicContactCheck *check = context;
    CHECK(check->contacts->level.world->locked &&
          !ab_classic_contacts_register_object(check->contacts, 4, NULL) &&
          call->first.size == 12 && call->second.size == 12 &&
          !memcmp(call->first.data, "Bird_clone_", 11) &&
          !memcmp(call->second.data, "Bird_clone_", 11) &&
          call->first.data[11] != call->second.data[11]);
    check->response_entries[0] =
        (AbDataEntry){string("birdName"), string("Bird_clone_b")};
    check->response_entries[1] =
        (AbDataEntry){string("afterCollisionVelocityX"), number(1)};
    check->response_entries[2] =
        (AbDataEntry){string("afterCollisionVelocityY"), number(-2)};
    check->response = (AbDataTable){check->response_entries, 3, 3, NULL};
    call->response.should_disable_contact = true;
    call->response.new_collision_parameters = table(&check->response);
    ++check->begins;
    return true;
}

static bool classic_contact_begin(void *context, AbGameContactCall *call,
                                  AbError *error) {
    (void)error;
    ClassicContactCheck *check = context;
    CHECK(check->contacts->level.world->locked &&
          !ab_classic_contacts_close(check->contacts, NULL) &&
          !ab_classic_contacts_flush(check->contacts, 0, NULL) &&
          !call->response.should_disable_contact && isfinite(call->normal.x) &&
          isfinite(call->point.y));
    ++check->begins;
    check->velocity_entries[0] = (AbDataEntry){string("birdName"), string("Bird_1")};
    check->velocity_entries[1] =
        (AbDataEntry){string("afterCollisionVelocityX"), number(0.25f)};
    check->velocity_entries[2] =
        (AbDataEntry){string("afterCollisionVelocityY"), number(-0.5f)};
    check->velocity = (AbDataTable){check->velocity_entries, 3, 3, NULL};
    check->glue_entries[0] = (AbDataEntry){number(1), string("Bird_1")};
    check->glue_entries[1] = (AbDataEntry){number(2), string("ground")};
    check->glue = (AbDataTable){check->glue_entries, 2, 2, NULL};
    call->response.should_disable_contact = true;
    call->response.new_collision_parameters = table(&check->velocity);
    call->response.glue_contact_pair = table(&check->glue);
    return true;
}

static bool classic_contact_end(void *context, AbSpan first, AbSpan second,
                                AbError *error) {
    (void)error;
    ClassicContactCheck *check = context;
    CHECK(first.size == 6 && second.size == 6 &&
          memcmp(first.data, second.data, 6) != 0);
    ++check->ends;
    return true;
}

static bool classic_contact_special(void *context,
                                    const AbGameContactDispatchCall *call,
                                    AbError *error) {
    (void)error;
    ClassicContactCheck *check = context;
    CHECK(call->stage == AB_GAME_CONTACT_BEGIN && call->handler == 77 &&
          call->contact < check->contacts->level.world->contact_count &&
          !call->previous);
    ++check->specials;
    return true;
}

static bool classic_contacts_checks(AbClassicLevel *level, bool static_flush) {
    AbClassicLevelView view = ab_classic_level_view(level);
    CHECK(view.bodies->count == 2);
    size_t bird = view.bodies->objects[0].physics.body;
    size_t ground = view.bodies->objects[1].physics.body;
    CHECK(ab_world_set_body_type(view.world, bird, AB_BODY_DYNAMIC, NULL) &&
          ab_world_set_transform(view.world, ground, (AbVec2){0, 10}, 0, NULL) &&
          ab_world_set_transform(view.world, bird, (AbVec2){0, 4.75f}, 0, NULL));
    AbGameContactRoutes routes;
    CHECK(ab_game_contact_routes_initialize(&routes));
    AbClassicContactObject objects[2] = {{.velocity = {{-0.0f, 0.5f}, true}},
                                         {.script_table_present = true}};
    AbClassicGluePair pairs[2];
    AbClassicGlueQueue queue = {pairs, 0, 2};
    AbClassicContacts contacts = {0};
    ClassicContactCheck check = {.contacts = &contacts};
    AbClassicContactsConfig config = {.routes = &routes,
                                      .glue = &queue,
                                      .context = &check,
                                      .collision = classic_contact_begin,
                                      .end = classic_contact_end,
                                      .special = classic_contact_special};
    AbClassicContacts before = contacts;
    AbWorldListener previous = view.world->listener;
    CHECK(!ab_classic_contacts_open(level, objects, 1, config, &contacts, NULL) &&
          memcmp(&before, &contacts, sizeof(before)) == 0 &&
          view.world->listener.context == previous.context &&
          ab_classic_contacts_open(level, objects, 2, config, &contacts, NULL) &&
          objects[0].velocity.pending && signbit(objects[0].velocity.velocity.x) &&
          !objects[0].script_table_present);
    before = contacts;
    CHECK(!ab_classic_contacts_open(level, objects, 2, config, &contacts, NULL) &&
          memcmp(&before, &contacts, sizeof(before)) == 0 &&
          !ab_classic_contacts_flush(&contacts, 2, NULL));
    objects[0].velocity.pending = false;
    CHECK(ab_world_step(view.world, 1.0f / 30, 8, 3, NULL) && !check.begins &&
          !objects[0].velocity.pending && !queue.count &&
          ab_world_set_transform(view.world, bird, (AbVec2){0, 0}, 0, NULL) &&
          ab_world_step(view.world, 0, 8, 3, NULL) && !check.ends);
    objects[0].script_table_present = true;
    CHECK(ab_world_set_transform(view.world, bird, (AbVec2){0, 4.75f}, 0, NULL) &&
          ab_world_step(view.world, 1.0f / 30, 8, 3, NULL) && check.begins == 1 &&
          objects[0].velocity.pending && objects[0].velocity.velocity.x == 0.25f &&
          objects[0].velocity.velocity.y == -0.5f && !objects[1].velocity.pending &&
          queue.count == 1 && pairs[0].first_body == bird &&
          pairs[0].second_body == ground);
    if (static_flush) {
        CHECK(ab_world_set_body_type(view.world, bird, AB_BODY_STATIC, NULL));
    }
    CHECK(ab_classic_contacts_flush(&contacts, 0, NULL) &&
          !objects[0].velocity.pending &&
          view.world->storage.bodies[bird].motion.mass.linear_velocity.x ==
              (static_flush ? 0 : 0.25f) &&
          ab_classic_contacts_flush(&contacts, 1, NULL));
    CHECK(ab_world_set_body_type(view.world, bird, AB_BODY_DYNAMIC, NULL));
    contacts.listener.config.begin_disabled = true;
    CHECK(ab_world_set_transform(view.world, bird, (AbVec2){0, 0}, 0, NULL) &&
          ab_world_step(view.world, 0, 8, 3, NULL) && check.ends == 1);
    contacts.listener.config.begin_disabled = false;
    CHECK(ab_game_contact_routes_set(&routes, AB_GAME_CONTACT_BEGIN, AB_COLLIDER_BIRD,
                                     AB_COLLIDER_BASIC, 77) &&
          ab_world_set_transform(view.world, bird, (AbVec2){0, 4.75f}, 0, NULL) &&
          ab_world_step(view.world, 1.0f / 30, 8, 3, NULL) && check.specials == 1 &&
          check.begins == 1 && !objects[0].velocity.pending && queue.count == 1 &&
          ab_classic_contacts_close(&contacts, NULL) && !contacts.listener.world &&
          view.world->listener.context == previous.context &&
          view.world->listener.filter == previous.filter &&
          ab_classic_contacts_close(&contacts, NULL) &&
          !ab_classic_contacts_flush(&contacts, 0, NULL));
    return true;
}

#endif
