#ifndef AB_TEST_CAMERA_FOLLOW_END_SEQUENCE_H
#define AB_TEST_CAMERA_FOLLOW_END_SEQUENCE_H

static bool start_camera_sequence(OwnerFixture *fixture, bool follow, AbError *error) {
    const AbCameraSequence *source = fixture->end_template;
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
    if (follow)
        source = fixture->start_template;
#endif
    CHECK(source && source->count == (follow ? 1u : 3u));
    uint32_t *row = event(fixture, 4, 9, 19);
    CHECK(row);
    row[4] = follow ? 2u : 1u;
    row[6] = 1;
    row = event(fixture, 7, 15, 19);
    CHECK(row);
    row[4] = follow ? 3u : 1u;
    row = event(fixture, 7, 3, 24);
    CHECK(row);
    row[4] = 2;
    memcpy(fixture->end_stages, source->stages,
           source->count * sizeof(*fixture->end_stages));
    fixture->end_sequence = (AbCameraSequence){fixture->end_stages, source->count};
    return ab_camera_system_start(fixture->system, &fixture->end_sequence, NULL, NULL,
                                  error);
}

static bool start_end_sequence(OwnerFixture *fixture, AbError *error) {
    return start_camera_sequence(fixture, false, error);
}

#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
static bool shot_start_resource(void *context, const char *name, AbError *error) {
    CHECK(!strcmp(name, "FollowShotBirdCameraSequence"));
    return start_camera_sequence(context, true, error);
}

static bool shot_cancel_sequence(void *context, AbError *error) {
    OwnerFixture *fixture = context;
    return ab_camera_system_cancel_sequence(fixture->system, error);
}

static bool start_shot_behavior(OwnerFixture *fixture) {
    AbBirdCameraBehaviorHost host = {fixture, shot_start_resource,
                                     shot_cancel_sequence};
    return fixture->config[0] % 3u == 2 ? ab_bird_camera_blues_initialize(&host, NULL)
                                        : ab_bird_camera_on_launch(&host, NULL);
}

static bool resolve_shot_target(void *context, AbDataValue target,
                                const AbCameraFollowBird **bird,
                                AbCameraFollowHost **environment, AbError *error) {
    (void)error;
    OwnerFixture *fixture = context;
    CHECK(target.kind == AB_DATA_STRING && target.string.size == 10 &&
          !memcmp(target.string.data, "flyingBird", 10));
    uint32_t *row = event(fixture, 8, 10, 48);
    CHECK(row && fixture->flying >= 1 && fixture->flying <= 2);
    row[6] = 1;
#if defined(AB_TEST_CAMERA_FOLLOW_PROPERTIES)
    AbDataValue found = {0};
    CHECK(ab_object_world_lookup(&fixture->properties.lookup, target, &found, error) &&
          found.kind == AB_DATA_NIL);
#endif
    *bird = fixture->birds + (fixture->flying - 1u);
    *environment = &fixture->environment;
    return true;
}
#endif

static void end_sequence_snapshot(const OwnerFixture *fixture, uint32_t output[80]) {
    const AbCameraSequenceManager *manager =
        ab_camera_system_view(fixture->system).sequence;
    output[64] = manager->current != NULL;
    output[65] = owner_follow_word((float)manager->stage_index);
    output[66] = owner_follow_word(manager->timer);
    output[67] = manager->stage_timer_present;
    output[68] =
        manager->stage_timer_present ? owner_follow_word(manager->stage_timer) : 0;
    output[69] = manager->done_callback;
    if (fixture->end_sequence.stages) {
#if defined(AB_TEST_CAMERA_SHOT_SEQUENCE)
        output[62] = (uint32_t)fixture->end_sequence.count;
#endif
        for (size_t index = 0; index < fixture->end_sequence.count; ++index) {
            const AbCameraSequenceStage *stage = fixture->end_stages + index;
            size_t first = 70 + index * 3;
            output[first] = stage->duration_present;
            output[first + 1] =
                stage->duration_present ? owner_follow_word(stage->duration) : 0;
            output[first + 2] = (uint32_t)stage->condition;
        }
    }
    output[79] = fixture->counts[7];
}

#endif
