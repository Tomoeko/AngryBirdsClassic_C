#ifndef AB_TEST_BIRD_DAMAGE_H
#define AB_TEST_BIRD_DAMAGE_H

static bool bird_edge_checks(void) {
    AbBirdDamageInput input = {.velocity = {30, 40},
                               .mass = 2,
                               .target_strength = 5,
                               .target_defence = 3,
                               .damage_multiplier = 2,
                               .velocity_multiplier = 0.5f,
                               .velocity_modifier = 0.5f,
                               .has_target_strength = true,
                               .bird_script_present = true,
                               .target_script_present = true};
    AbBirdDamage result;
    CHECK(ab_bird_collision_damage(&input, &result));
    CHECK(result.joint_force == 10 && result.collision_force == 20 &&
          result.remaining_strength == 0 && result.callback_damage == 5 &&
          result.mark_dead && result.disable_contact &&
          result.velocity_change == AB_BIRD_VELOCITY_DEFERRED &&
          result.velocity.x == 5.625f && result.velocity.y == 7.5f);
    input.legacy_velocity = true;
    input.velocity_modifier = 0;
    CHECK(ab_bird_collision_damage(&input, &result));
    CHECK(result.velocity_change == AB_BIRD_VELOCITY_IMMEDIATE &&
          result.velocity.x == 0 && result.velocity.y == 0);
    input.target_strength = 100;
    CHECK(ab_bird_collision_damage(&input, &result));
    CHECK(result.remaining_strength == 83 && result.callback_damage == 17 &&
          !result.disable_contact &&
          result.velocity_change == AB_BIRD_VELOCITY_UNCHANGED);
    input.target_defence = 30;
    CHECK(ab_bird_collision_damage(&input, &result));
    CHECK(result.remaining_strength == 100 && result.callback_damage == -10);
    input.target_defence = 3;
    input.target_ignores_damage = true;
    CHECK(ab_bird_collision_damage(&input, &result));
    CHECK(result.remaining_strength == 100 && result.callback_damage == 0);
    input.bird_script_present = false;
    CHECK(ab_bird_collision_damage(&input, &result));
    CHECK(result.remaining_strength == 83 && result.callback_damage == 17);
    input.bird_script_present = true;
    input.target_script_present = false;
    AbBirdDamage previous = result;
    CHECK(!ab_bird_collision_damage(&input, &result));
    CHECK(memcmp(&previous, &result, sizeof(result)) == 0);
    input.has_target_strength = false;
    input.target_strength = NAN;
    CHECK(ab_bird_collision_damage(&input, &result));
    CHECK(result.remaining_strength == 0 && result.callback_damage == 0);
    previous = result;
    input.mass = -1;
    CHECK(!ab_bird_collision_damage(&input, &result));
    CHECK(!ab_bird_collision_damage(NULL, &result));
    CHECK(!ab_bird_collision_damage(&input, NULL));
    CHECK(memcmp(&previous, &result, sizeof(result)) == 0);
    return true;
}

static bool native_bird_checks(FILE *file) {
    size_t deaths = 0, immediate = 0, deferred = 0, calls = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[15], expected[9];
        CHECK(read_words(file, data, 15));
        uint32_t flags = data[8];
        AbBirdDamageInput input = {
            .velocity = {from_bits(data[0]), from_bits(data[1])},
            .mass = from_bits(data[2]),
            .target_strength = from_bits(data[3]),
            .target_defence = flags & 256 ? 0 : from_bits(data[4]),
            .damage_multiplier = flags & 32 ? 1 : from_bits(data[5]),
            .velocity_multiplier = flags & 64 ? 1 : from_bits(data[6]),
            .velocity_modifier = flags & 128 || !(flags & 2) ? 1 : from_bits(data[7]),
            .has_target_strength = !(flags & 8),
            .bird_script_present = (flags & 2) != 0,
            .target_script_present = (flags & 4) != 0,
            .target_ignores_damage = (flags & 16) != 0,
            .legacy_velocity = (flags & 1) != 0,
        };
        AbBirdDamage result;
        CHECK(ab_bird_collision_damage(&input, &result));
        bool velocity_now = result.velocity_change == AB_BIRD_VELOCITY_IMMEDIATE;
        bool velocity_later = result.velocity_change == AB_BIRD_VELOCITY_DEFERRED;
        const uint32_t summary[] = {
            input.has_target_strength ? 3u : 0u,
            to_bits(result.remaining_strength),
            result.mark_dead,
            result.disable_contact,
            velocity_later ? 5u : 0u,
            velocity_later ? to_bits(result.velocity.x) : 0,
            velocity_later ? to_bits(result.velocity.y) : 0,
        };
        CHECK(read_words(file, expected, 7));
        CHECK(compare_words(summary, expected, 7, item, "bird strength/return"));
        uint32_t count;
        CHECK(read_words(file, &count, 1) && count == (velocity_now ? 3u : 2u));
        for (uint32_t event = 0; event < count; ++event) {
            uint32_t record[9] = {0};
            if (event == 0) {
                record[0] = 1;
                record[1] = 2;
                record[3] = to_bits(result.joint_force);
            } else if (event + 1 == count) {
                record[0] = 4;
                record[1] = 1;
                record[2] = 2;
                record[3] = to_bits(result.collision_force);
                record[4] = to_bits(result.callback_damage);
                memcpy(record + 5, data + 11, 4 * sizeof(*data));
            } else {
                record[0] = 3;
                record[1] = 1;
                record[5] = to_bits(result.velocity.x);
                record[6] = to_bits(result.velocity.y);
            }
            CHECK(read_words(file, expected, 9));
            CHECK(compare_words(record, expected, 9, item, "bird boundary callback"));
        }
        deaths += result.mark_dead;
        immediate += velocity_now;
        deferred += velocity_later;
        calls += count;
    }
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB bird damage reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original-Lua bird impacts match: %zu lethal/disabled contacts, "
           "%zu immediate and %zu deferred velocities, %zu boundary calls.\n",
           deaths, immediate, deferred, calls);
    return true;
}

#endif
