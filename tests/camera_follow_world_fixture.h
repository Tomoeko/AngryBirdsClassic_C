#ifndef AB_TEST_CAMERA_FOLLOW_WORLD_FIXTURE_H
#define AB_TEST_CAMERA_FOLLOW_WORLD_FIXTURE_H

static bool owner_world_radius(void *context, float *output, AbError *error) {
#if !defined(AB_TEST_CAMERA_FOLLOW_PROPERTIES)
    (void)error;
#endif
    OwnerFixture *fixture = context;
    uint32_t pc = fixture->prediction_radii++ % 2u ? 73u : 58u;
    uint32_t *row = event(fixture, 10, pc, 115);
    CHECK(row);
    row[4] = fixture->prediction_bird;
#if defined(AB_TEST_CAMERA_FOLLOW_PROPERTIES)
    CHECK(fixture->prediction_bird == 1 || fixture->prediction_bird == 2);
    AbDataValue value = {0};
    CHECK(ab_level_property_get(&fixture->properties, fixture->prediction_bird - 1u,
                                (AbSpan){(const uint8_t *)"radius", 6}, &value,
                                error) &&
          value.kind == AB_DATA_NUMBER);
    memcpy(output, &value.bits, sizeof(*output));
#else
    *output = 0.25f + (float)(fixture->config[0] % 4u) * 0.125f;
#endif
    row[5] = bits(*output);
    return true;
}

static bool owner_world_print(void *context, size_t count, AbError *error) {
    (void)error;
    OwnerFixture *fixture = context;
    CHECK(count == fixture->prediction_rays);
    uint32_t *row = event(fixture, 11, 109, 115);
    CHECK(row);
    row[4] = bits((float)count);
    return true;
}

static bool owner_world_observe(void *context, AbVec2 first, AbVec2 second,
                                const AbLevelRaycast *hit, AbError *error) {
    (void)error;
    OwnerFixture *fixture = context;
    ++fixture->prediction_rays;
    uint32_t *row = event(fixture, 9, 19, 22);
    CHECK(row);
    row[4] = bits(first.x);
    row[5] = bits(first.y);
    row[6] = bits(second.x);
    row[7] = bits(second.y);
    row[8] = hit->hit ? 6u : 1u;
    row[9] = hit->hit;
    if (hit->hit) {
        CHECK(hit->name.kind == AB_DATA_NIL);
        row[10] = bits(hit->point.x);
        row[11] = bits(hit->point.y);
        row[12] = bits(hit->normal.x);
        row[13] = bits(hit->normal.y);
    }
    return true;
}

static bool owner_world_sequence(void *context, const char *name, AbError *error) {
    CHECK(!strcmp(name, "FollowShotBirdEndCameraSequence"));
    return start_end_sequence(context, error);
}

#if defined(AB_TEST_CAMERA_FOLLOW_PROPERTIES)
static bool owner_world_properties(OwnerFixture *fixture) {
    AbDataValue bird = {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"Bird", 4}};
    fixture->property_block = (AbDataEntry){
        bird, {.kind = AB_DATA_TABLE, .table = &fixture->property_definition}};
    fixture->property_blocks = (AbDataTable){&fixture->property_block, 1, 1, NULL};
    fixture->property_catalog_entries[0] = (AbDataEntry){
        {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"blocks", 6}},
        {.kind = AB_DATA_TABLE, .table = &fixture->property_blocks}};
    fixture->property_catalog_entries[1] = (AbDataEntry){
        {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"materials", 9}},
        {.kind = AB_DATA_TABLE, .table = &fixture->property_definition}};
    fixture->property_catalog =
        (AbDataTable){fixture->property_catalog_entries, 2, 2, NULL};
    AbLevelObject objects[2] = {0};
    for (uint32_t index = 0; index < 2; ++index) {
        objects[index].name = (AbSpan){(const uint8_t *)(index ? "bird2" : "bird1"), 5};
        objects[index].definition_name = bird.string;
        objects[index].physics.dimensions = (AbObjectDimensions){
            -1, -1, 0.25f + (float)(fixture->config[0] % 4u) * 0.125f};
    }
    AbLevelBodies bodies = {objects, 2, 2};
    fixture->properties = (AbLevelProperties){.objects = fixture->property_objects,
                                              .world_entries = fixture->property_world,
                                              .capacity = 2};
    return ab_level_properties_initialize(&fixture->properties, &bodies,
                                          &fixture->property_catalog, NULL);
}
#endif

static bool owner_world_open(OwnerFixture *fixture) {
#if defined(AB_TEST_CAMERA_FOLLOW_PROPERTIES)
    CHECK(owner_world_properties(fixture));
#endif
    CHECK(open_fixture(&fixture->world_query));
    for (uint32_t index = 0; index < WORLD_RAY_BODIES; ++index) {
        uint32_t row[WORLD_RAY_BODY_WORDS];
        world_raycast_body(fixture->config[0], index, row);
        CHECK(create_body(&fixture->world_query, row));
        fixture->world_query.world.storage.fixtures[index].user_data = 0;
    }
    fixture->world_query.world.locked = (fixture->config[0] & 16u) != 0;
    fixture->world_source =
        (AbBirdImpactWorld){&fixture->world_query.world, &fixture->world_names, NULL};
    fixture->world_services = (AbBirdImpactWorldHost){
        fixture, 2048, owner_world_radius, owner_world_print, owner_world_observe};
    fixture->gravity = owner_follow_number(fixture->config[16]);
    AbCameraFollowWorldConfig config = {&fixture->world_source,
                                        &fixture->world_services, &fixture->gravity,
                                        fixture, owner_world_sequence};
    return ab_camera_follow_world_initialize(&fixture->world_binding, &config, NULL);
}

static bool owner_world_predict(OwnerFixture *fixture, AbCameraFollowCall *call,
                                uint32_t *row, AbError *error) {
    fixture->prediction_bird = row[4];
    fixture->prediction_rays = fixture->prediction_radii = 0;
    AbCameraFollowHost *host = &fixture->world_binding.host;
    if (!host->query(host->context, call, error))
        return false;
    row[10] = call->prediction.point_present ? bits(call->prediction.point.x) : 0;
    row[11] = call->prediction.point_present ? bits(call->prediction.point.y) : 0;
    row[12] = bits(call->prediction.time);
    row[13] = call->prediction.point_present;
    fixture->environment.gravity = host->gravity;
    return true;
}

static bool owner_world_service(OwnerFixture *fixture, AbCameraFollowCall *call,
                                AbError *error) {
    AbCameraFollowHost *host = &fixture->world_binding.host;
    bool okay = host->query(host->context, call, error);
    fixture->environment.gravity = host->gravity;
    return okay;
}

#endif
