#ifndef AB_TEST_BIRD_ROUTE_H
#define AB_TEST_BIRD_ROUTE_H

static bool bird_route_edges(void) {
    AbMuseumCollisionKind kind;
    CHECK(ab_museum_collision_kind(3, 1, &kind) &&
          kind == AB_MUSEUM_BIRD_PAIR_COLLISION);
    CHECK(ab_museum_collision_kind(3, 4, &kind) &&
          kind == AB_MUSEUM_FIRST_BIRD_COLLISION);
    CHECK(ab_museum_collision_kind(0, 1, &kind) &&
          kind == AB_MUSEUM_SECOND_BIRD_COLLISION);
    CHECK(ab_museum_collision_kind(10, 2, &kind) && kind == AB_MUSEUM_BLOCK_COLLISION);
    CHECK(!ab_museum_collision_kind(12, 0, &kind));
    CHECK(kind == AB_MUSEUM_BLOCK_COLLISION);
    CHECK(!ab_museum_collision_kind(0, 0, NULL));
    AbMuseumCallbackContact contact;
    CHECK(ab_museum_callback_contact(AB_MUSEUM_SECOND_BIRD_COLLISION, (AbVec2){1, 2},
                                     (AbVec2){3, 4}, &contact));
    CHECK(contact.normal.x == 3 && contact.normal.y == 4 && !contact.point_present);
    CHECK(ab_museum_callback_contact(AB_MUSEUM_BLOCK_COLLISION, (AbVec2){1, 2},
                                     (AbVec2){3, 4}, &contact));
    CHECK(contact.normal.x == 1 && contact.normal.y == 2 && contact.point_present &&
          contact.point.x == 3 && contact.point.y == 4);
    CHECK(!ab_museum_callback_contact((AbMuseumCollisionKind)4, (AbVec2){0},
                                      (AbVec2){0}, &contact));
    CHECK(!ab_museum_callback_contact(AB_MUSEUM_BLOCK_COLLISION, (AbVec2){NAN, 0},
                                      (AbVec2){0}, &contact));
    CHECK(contact.normal.x == 1 && contact.point_present);
    AbLegacyCollisionBody bodies[] = {{.velocity = {30, 40}, .mass = 2},
                                      {.velocity = {0, 20}, .mass = 100}};
    float force = 0;
    CHECK(ab_bird_pair_force(bodies, &force) && force == 10);
    bodies[1].velocity = bodies[0].velocity;
    CHECK(ab_bird_pair_force(bodies, &force) && force == 500);
    bodies[1].mass = -1;
    CHECK(!ab_bird_pair_force(bodies, &force) && force == 500);
    bodies[1].mass = FLT_MAX;
    CHECK(!ab_bird_pair_force(bodies, &force) && force == 500);
    bodies[1].mass = 1;
    bodies[0].velocity.x = NAN;
    CHECK(!ab_bird_pair_force(bodies, &force) && force == 500);
    CHECK(!ab_bird_pair_force(NULL, &force));
    CHECK(!ab_bird_pair_force(bodies, NULL));
    return true;
}

static AbBirdDamageInput route_damage_input(const uint32_t data[20]) {
    uint32_t flags = data[8];
    return (AbBirdDamageInput){
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
}

static bool compare_route_events(FILE *file, const uint32_t data[20],
                                 const AbBirdDamage *damage, size_t item, bool pair,
                                 bool reverse, float pair_force, size_t *calls) {
    bool immediate = damage->velocity_change == AB_BIRD_VELOCITY_IMMEDIATE;
    uint32_t expected[9], count;
    CHECK(read_words(file, &count, 1));
    CHECK(count == (pair ? 1u : immediate ? 3u : 2u));
    for (uint32_t event = 0; event < count; ++event) {
        uint32_t actual[9] = {0};
        if (pair || event + 1 == count) {
            actual[0] = 4;
            actual[1] = pair && reverse ? 2u : 1u;
            actual[2] = pair && reverse ? 1u : 2u;
            actual[3] = to_bits(pair ? pair_force : damage->collision_force);
            actual[4] = pair ? 0 : to_bits(damage->callback_damage);
            AbMuseumCallbackContact contact;
            AbMuseumCollisionKind kind = pair      ? AB_MUSEUM_BIRD_PAIR_COLLISION
                                         : reverse ? AB_MUSEUM_SECOND_BIRD_COLLISION
                                                   : AB_MUSEUM_FIRST_BIRD_COLLISION;
            CHECK(ab_museum_callback_contact(
                kind, (AbVec2){from_bits(data[9]), from_bits(data[10])},
                (AbVec2){from_bits(data[11]), from_bits(data[12])}, &contact));
            actual[5] = to_bits(contact.normal.x);
            actual[6] = to_bits(contact.normal.y);
            actual[7] = to_bits(contact.point.x);
            actual[8] = to_bits(contact.point.y);
            CHECK(contact.point_present == pair);
        } else if (event == 0) {
            actual[0] = 1;
            actual[1] = 2;
            actual[3] = to_bits(damage->joint_force);
        } else {
            actual[0] = 3;
            actual[1] = 1;
            actual[5] = to_bits(damage->velocity.x);
            actual[6] = to_bits(damage->velocity.y);
        }
        CHECK(read_words(file, expected, 9));
        CHECK(compare_words(actual, expected, 9, item, "enclosing bird callback"));
    }
    *calls += count;
    return true;
}

static bool native_bird_route_checks(FILE *file) {
    size_t branches[3] = {0}, calls = 0, deaths = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t data[20], expected[10];
        CHECK(read_words(file, data, 20));
        bool reverse = (item & 1) != 0;
        AbMuseumCollisionKind kind;
        CHECK(ab_museum_collision_kind(data[reverse ? 19 : 18], data[reverse ? 18 : 19],
                                       &kind));
        bool pair = kind == AB_MUSEUM_BIRD_PAIR_COLLISION;
        CHECK(pair || kind == (reverse ? AB_MUSEUM_SECOND_BIRD_COLLISION
                                       : AB_MUSEUM_FIRST_BIRD_COLLISION));
        ++branches[pair ? 2 : reverse ? 1 : 0];
        AbBirdDamageInput input = route_damage_input(data);
        AbBirdDamage damage = {0};
        float force = 0;
        if (pair) {
            AbLegacyCollisionBody bodies[2];
            bodies[reverse ? 1 : 0] =
                (AbLegacyCollisionBody){.velocity = input.velocity, .mass = input.mass};
            bodies[reverse ? 0 : 1] = (AbLegacyCollisionBody){
                .velocity = {from_bits(data[15]), from_bits(data[16])},
                .mass = from_bits(data[17])};
            CHECK(ab_bird_pair_force(bodies, &force));
            damage.remaining_strength =
                input.has_target_strength ? input.target_strength : 0;
        } else {
            CHECK(ab_bird_collision_damage(&input, &damage));
        }
        bool deferred = damage.velocity_change == AB_BIRD_VELOCITY_DEFERRED;
        uint32_t actual[] = {input.has_target_strength ? 3u : 0u,
                             to_bits(damage.remaining_strength),
                             damage.mark_dead,
                             1,
                             damage.disable_contact,
                             deferred ? 5u : 0u,
                             deferred ? 1u : 0u,
                             deferred ? to_bits(damage.velocity.x) : 0,
                             deferred ? to_bits(damage.velocity.y) : 0,
                             0};
        CHECK(read_words(file, expected, 10));
        CHECK(compare_words(actual, expected, 10, item, "enclosing bird state"));
        uint32_t types;
        CHECK(read_words(file, &types, 1));
        CHECK(types == (pair ? UINT32_C(0x333333) : UINT32_C(0x3333)));
        CHECK(compare_route_events(file, data, &damage, item, pair, reverse, force,
                                   &calls));
        deaths += damage.mark_dead;
    }
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB bird route reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 enclosing bird collisions match: %zu/%zu single-bird orders, "
           "%zu bird pairs, %zu lethal hits, %zu boundary calls and argument types.\n",
           branches[0], branches[1], branches[2], deaths, calls);
    return true;
}

#endif
