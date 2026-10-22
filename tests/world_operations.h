#ifndef AB_TEST_WORLD_OPERATIONS_H
#define AB_TEST_WORLD_OPERATIONS_H

static bool compare_fixtures(FILE *file, const AbPhysicsWorld *world, size_t scene,
                             size_t frame) {
    for (size_t index = 0; index < world->fixture_count; ++index) {
        uint32_t expected[19];
        CHECK(read_words(file, expected, 19));
        const AbWorldFixture *fixture = NULL;
        for (size_t cursor = 0; cursor < world->fixture_count; ++cursor) {
            const AbWorldFixture *candidate =
                world->storage.fixtures + world->fixture_order[cursor];
            if (candidate->user_data == expected[0]) {
                CHECK(!fixture);
                fixture = candidate;
            }
        }
        CHECK(fixture);
        uint32_t actual[19] = {
            (uint32_t)fixture->user_data,
            (uint32_t)world->storage.bodies[fixture->body].motion.user_data,
            to_bits(fixture->density),
            to_bits(fixture->friction),
            to_bits(fixture->restitution),
            fixture->filter.category,
            fixture->filter.mask,
            (uint16_t)fixture->filter.group,
            fixture->sensor,
            fixture->proxy.active,
            UINT32_C(0xffffffff),
        };
        if (fixture->proxy.active) {
            actual[10] = (uint32_t)fixture->proxy.proxy;
            AbAabb bounds = fixture->proxy.bounds;
            AbAabb fat = world->broadphase.tree.nodes[fixture->proxy.proxy].bounds;
            const float coordinates[] = {
                bounds.lower.x, bounds.lower.y, bounds.upper.x, bounds.upper.y,
                fat.lower.x,    fat.lower.y,    fat.upper.x,    fat.upper.y,
            };
            for (size_t field = 0; field < 8; ++field) {
                actual[11 + field] = to_bits(coordinates[field]);
            }
        }
        CHECK(compare_words(actual, expected, 19, scene, frame, "fixture", index));
    }
    return true;
}

static bool read_world_operation(FILE *file, AbPhysicsWorld *world, size_t counts[7]) {
    uint32_t input[5];
    CHECK(read_words(file, input, 5));
    CHECK(input[0] <= 6 && input[1] && input[1] <= world->storage.body_count);
    ++counts[input[0]];
    size_t body = world->storage.body_count - input[1];
    size_t fixture = input[1] - 1;
    AbError error = {0};
    bool okay = true;
    switch (input[0]) {
        case 0:
            break;
        case 1:
            okay = ab_world_set_transform(
                world, body, (AbVec2){from_bits(input[2]), from_bits(input[3])},
                from_bits(input[4]), &error);
            break;
        case 2:
            okay = ab_world_set_active(world, body, input[2] != 0, &error);
            break;
        case 3:
            okay = ab_world_set_body_type(world, body, (AbBodyType)input[2], &error);
            break;
        case 4:
            okay = ab_world_set_fixed_rotation(world, body, input[2] != 0, &error);
            break;
        case 5: {
            uint16_t group_bits = (uint16_t)input[4];
            int16_t group;
            memcpy(&group, &group_bits, sizeof(group));
            AbCollisionFilter filter = {(uint16_t)input[2], (uint16_t)input[3], group};
            okay = ab_world_set_fixture_filter(world, fixture, filter, &error);
            break;
        }
        case 6:
            okay = ab_world_set_fixture_sensor(world, fixture, input[2] != 0);
            break;
        default:
            return false;
    }
    if (!okay) {
        fprintf(stderr, "World operation %u: %s\n", input[0], error.message);
    }
    return okay;
}

#endif
