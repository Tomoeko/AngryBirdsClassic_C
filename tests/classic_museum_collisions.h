#ifndef AB_TEST_CLASSIC_MUSEUM_COLLISIONS_H
#define AB_TEST_CLASSIC_MUSEUM_COLLISIONS_H

static bool museum_bird_boundary(void *context, const AbBirdCollisionNotifyInput *input,
                                 AbError *error) {
    BlockCollisionTest *test = context;
    CHECK(input->contact.force > 0 && input->contact.normal.x == 2 &&
          input->contact.normal.y == 3 && input->point_present == test->bird_pair);
    AbSpan bird_name = test->test->level.bodies->objects[test->test->bird].name;
    CHECK(input->contact.first.size == bird_name.size &&
          !memcmp(input->contact.first.data, bird_name.data, bird_name.size));
    if (test->bird_pair) {
        CHECK(input->contact.damage == 0 && input->contact.point.x == 4 &&
              input->contact.point.y == 5);
    } else {
        CHECK(input->contact.damage == 4 &&
              field(&test->test->level.properties->objects[test->pig].physical,
                    "strength")
                      .bits == bits(0));
    }
    ++test->bird_calls;
    return ab_fail(error, "Expected complete bird callback boundary");
}

static bool museum_numeric_boundaries(BlockCollisionTest *test,
                                      AbClassicMuseumCollisionConfig config,
                                      AbError *error) {
    ShotTest *shot = test->test;
    CHECK(shot->level.bird_count >= 2);
    size_t bird = shot->bird, other = shot->level.bird_order[0];
    if (other == bird)
        other = shot->level.bird_order[1];
    AbDataTable *bird_table = &shot->level.properties->objects[bird].physical;
    AbDataTable *target = &shot->level.properties->objects[test->pig].physical;
    AbDataValue cached_x = field(bird_table, "xVel"),
                cached_y = field(bird_table, "yVel");
    AbDataValue collided = field(bird_table, "hasCollided");
    AbDataValue pig_strength = field(target, "strength");
    float health_before = shot->level.character_objects[test->pig].goal.strength;
    CHECK(ab_level_property_set(
              shot->level.properties, bird, span("xVel"),
              (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(1000)}, error) &&
          ab_level_property_set(shot->level.properties, bird, span("yVel"),
                                (AbDataValue){.kind = AB_DATA_NUMBER}, error));
    /* These sessions deliberately stop at the required full bird callback.
     * A recording rejection verifies the preceding real numeric effects and
     * parameter mapping without substituting successful gameplay effects. */
    AbClassicMuseumCollisions owner;
    config.bird = museum_bird_boundary;
    CHECK(ab_classic_museum_collisions_open(config, &owner, error));
    AbGameContactCall call = {.first = shot->level.bodies->objects[test->pig].name,
                              .second = shot->level.bodies->objects[bird].name,
                              .normal = {7, 8},
                              .point = {2, 3}};
    AbError expected = {0};
    CHECK(!ab_classic_museum_collisions_contact(&owner, &call, &expected) &&
          strstr(expected.message, "Expected complete bird callback") &&
          test->bird_calls == 1 && !call.response.should_disable_contact &&
          call.response.new_collision_parameters.kind == AB_DATA_NIL &&
          field(bird_table, "hasCollided").kind == collided.kind &&
          field(bird_table, "hasCollided").bits == collided.bits);
    uintptr_t value = 0;
    bool present = false;
    CHECK(ab_string_map_get(config.dead_blocks, call.first, &value, &present, error) &&
          present && value == (uintptr_t)&config.physical_values[test->pig] &&
          ((const AbDataValue *)value)->table == target &&
          ab_string_map_set(config.dead_blocks, call.first, 0, false, error) &&
          ab_level_property_set(shot->level.properties, test->pig, span("strength"),
                                pig_strength, error));
    shot->level.character_objects[test->pig].goal.strength = health_before;
    /* Independent pair session: neither primary health nor a score is applied,
     * and the original normal and point both reach the callback. */
    test->bird_pair = true;
    CHECK(ab_classic_museum_collisions_open(config, &owner, error));
    call = (AbGameContactCall){.first = shot->level.bodies->objects[bird].name,
                               .second = shot->level.bodies->objects[other].name,
                               .normal = {2, 3},
                               .point = {4, 5}};
    expected = (AbError){0};
    CHECK(!ab_classic_museum_collisions_contact(&owner, &call, &expected) &&
          strstr(expected.message, "Expected complete bird callback") &&
          test->bird_calls == 2 &&
          field(target, "strength").bits == pig_strength.bits &&
          field(bird_table, "hasCollided").kind == collided.kind &&
          field(bird_table, "hasCollided").bits == collided.bits &&
          ab_level_property_set(shot->level.properties, bird, span("xVel"), cached_x,
                                error) &&
          ab_level_property_set(shot->level.properties, bird, span("yVel"), cached_y,
                                error));
    test->bird_pair = false;
    return true;
}

#endif
