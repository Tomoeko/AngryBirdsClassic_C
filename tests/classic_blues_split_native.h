#ifndef AB_TEST_CLASSIC_BLUES_SPLIT_NATIVE_H
#define AB_TEST_CLASSIC_BLUES_SPLIT_NATIVE_H

#include "../tools/oracle/reference_blues_split.h"

typedef struct {
    PlayfieldTest *test;
    AbClassicPlayfieldView view;
    AbClassicBluesSplit *split;
    uint32_t stages[BS_MAX_STAGES];
    size_t count, initial_tracks, follows, parent, events, camera_ends;
    unsigned profile;
    bool okay;
} BluesNativeCheck;

static void blue_native_trace(void *context, unsigned stage) {
    BluesNativeCheck *check = context;
    if (check->count == BS_MAX_STAGES) {
        check->okay = false;
        return;
    }
    check->stages[check->count++] = stage;
    if (stage == AB_BLUES_SPLIT_AUDIO_PLAY)
        check->follows = ab_classic_trajectories_view(check->view.trajectories).tracks -
                         check->initial_tracks;
}

static bool blue_native_camera_done(void *context, AbCameraSystem *system,
                                    AbError *error) {
    (void)error;
    BluesNativeCheck *check = context;
    CHECK(ab_camera_system_view(system).sequence->current);
    ++check->camera_ends;
    if (check->profile >= 8 && (check->profile & 1u)) {
        AbClassicLevelView level = ab_classic_level_view(check->test->level);
        check->test->flying = (AbDataValue){
            .kind = AB_DATA_TABLE,
            .table = &level.properties->objects[check->parent + 1].physical};
    }
    return true;
}

static bool blue_native_event(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    BluesNativeCheck *check = context;
    AbClassicLevelView level = ab_classic_level_view(check->test->level);
    CHECK(bc_named(field(event, "id"), "EID_BIRD_SPECIALTY_TRIGGERED") &&
          !level.removed_objects[check->parent] && check->split->has_result &&
          check->split->last_result.objects[0] == check->parent + 1 &&
          ab_classic_trajectories_view(check->view.trajectories).tracks ==
              check->initial_tracks + 4);
    ++check->events;
    return true;
}

static bool blue_native_parent(PlayfieldTest *test, AbClassicPlayfieldView view,
                               unsigned profile, size_t *output, AbError *error) {
    BcValue input[BC_PARENT_FIELDS];
    bs_case(profile, input);
    AbGameCircleInput circle = {
        .name = span(bs_parent_names[profile]),
        .position = {from_bits(input[0].bits), from_bits(input[1].bits)},
        .radius = from_bits(input[2].bits),
        .density = from_bits(input[3].bits),
        .friction = from_bits(input[4].bits),
        .restitution = from_bits(input[5].bits),
        .z_order = from_bits(input[6].bits),
        .controllable = input[8].kind != 0 && (input[8].kind != 1 || input[8].bits)};
    size_t object;
    CHECK(ab_classic_level_create_circle(test->level, circle, &object, error) &&
          ab_classic_render_scene_create_object(view.scene, view.visuals, object,
                                                span("BIRD_BLUE"), circle.z_order,
                                                error));
    AbClassicLevelView level = ab_classic_level_view(test->level);
    AbDataValue physical = {.kind = AB_DATA_TABLE,
                            .table = &level.properties->objects[object].physical};
    AbObjectPropertyAccess access = {.physics_object = physical};
    for (size_t index = 0; index < BC_PARENT_FIELDS; ++index) {
        AbDataValue value =
            index == 25 ? (AbDataValue){.kind = AB_DATA_STRING, .string = circle.name}
                        : bc_value_c(test, input[index]);
        CHECK(ab_object_property_set(&access, span(bc_parent_fields[index]), value,
                                     error));
    }
    AbDataEntry flag = {{.kind = AB_DATA_STRING, .string = span("isCluster")},
                        {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable parameters = {&flag, 1, 1, NULL};
    AbDataEntry entry = {
        {.kind = AB_DATA_STRING, .string = span("MuseumBluesTrajectoryRenderer")},
        {.kind = AB_DATA_TABLE, .table = &parameters}};
    AbDataTable descriptor = {&entry, 1, 1, NULL};
    AbDataTableLayout layouts[2] = {
        {.table = &descriptor, .hash_capacity = 1, .no_nil_assignments = true},
        {.table = &parameters, .hash_capacity = 1, .no_nil_assignments = true}};
    layouts[0].next = layouts + 1;
    AbLuaData data = {.layouts = layouts};
    CHECK(ab_classic_behaviors_create_object(
              view.behaviors, object,
              (AbDataValue){.kind = AB_DATA_TABLE, .table = &descriptor}, &data,
              error) &&
          ab_classic_shots_register_circle(view.shots, object, error) &&
          ab_classic_camera_register_object(view.camera, object, error) &&
          ab_classic_post_physics_register_circle(test->blue_post, object, error) &&
          ab_classic_object_removal_register_circle(test->physical, object, error) &&
          ab_classic_birds_register_object(test->birds, object, error));
    access.physics_object = *ab_classic_birds_view(test->birds).birds_field;
    CHECK(ab_object_property_set(&access, circle.name, physical, error));
    *output = object;
    return true;
}

static bool blue_native_summary(BluesNativeCheck *check, AbDataTable *old_other,
                                const AbBirdCollisionHat *hat, size_t logs,
                                uint32_t output[BS_SUMMARY_WORDS], AbError *error) {
    PlayfieldTest *test = check->test;
    AbClassicLevelView level = ab_classic_level_view(test->level);
    AbClassicBluesSplitResult created = check->split->last_result;
    memset(output, 0, BS_SUMMARY_WORDS * sizeof(*output));
    output[0] = bits(created.direction.x);
    output[1] = bits(created.direction.y);
    AbClassicRenderSceneView scene = ab_classic_render_scene_view(check->view.scene);
    CHECK(scene.extra_count);
    AbVec2 puff = scene.extra_renderers[scene.extra_count - 1].pose.world_position;
    output[2] = bits(puff.x);
    output[3] = bits(puff.y);
    AbDataValue tutorials =
        field(ab_settings_statistics_root(test->statistics), "tutorials");
    AbDataValue help = field(field(tutorials.table, "BIRD_BLUE").table, "showHelp");
    output[4] = help.kind == AB_DATA_NUMBER ? 3u : (uint32_t)help.kind;
    output[5] = help.bits;
    uintptr_t handle;
    bool present;
    CHECK(ab_string_map_get(test->blue_hats, level.bodies->objects[check->parent].name,
                            &handle, &present, error));
    output[6] = !present;
    CHECK(
        ab_string_map_get(test->blue_hats, created.names[2], &handle, &present, error));
    output[7] = present && handle == (uintptr_t)hat;
    AbClassicBirdView birds = ab_classic_birds_view(test->birds);
    output[8] = birds.other_birds != old_other;
    output[10] = 1;
    for (size_t index = 0; index < 3; ++index) {
        AbDataValue key = {.kind = AB_DATA_NUMBER, .bits = bits((float)index + 1)};
        const AbDataValue *name = ab_data_get(birds.other_birds, key);
        if (name && name->kind != AB_DATA_NIL)
            ++output[9];
        if (index < 2)
            output[10] &= name && name->kind == AB_DATA_STRING &&
                          name->string.size == created.names[index].size &&
                          !memcmp(name->string.data, created.names[index].data,
                                  name->string.size);
        size_t object = created.objects[index];
        if (test->flying.table == &level.properties->objects[object].physical)
            output[11] = (uint32_t)index + 1;
        AbDataValue group = field(&level.properties->objects[object].behaviors,
                                  "MuseumBluesTrajectoryRenderer");
        CHECK(group.kind == AB_DATA_TABLE && group.table->count == 1);
        AbDataValue cluster =
            field(group.table->entries[0].value.table, "originalCluster");
        if (cluster.kind == AB_DATA_TABLE &&
            cluster.table == &level.properties->objects[check->parent].physical)
            output[12] = (uint32_t)index + 1;
    }
    output[13] = (uint32_t)check->camera_ends;
    CHECK(test->logs == logs + 2);
    output[14] = 1;
    for (size_t index = 0; index < check->count; ++index) {
        output[15] += check->stages[index] == AB_BLUES_SPLIT_AUDIO_NAME;
        output[16] += check->stages[index] == AB_BLUES_SPLIT_AUDIO_PLAY;
    }
    output[17] = (uint32_t)check->events;
    output[18] = level.removed_objects[check->parent];
    output[19] = (uint32_t)test->globals.specialty_available.kind;
    output[20] = test->globals.specialty_available.bits;
    output[21] = bc_named(test->globals.bird_specialty, "CLUSTER_BOMB");
    AbDataValue triggered =
        field(&level.properties->objects[check->parent].physical, "specialtyTriggered");
    output[22] = (uint32_t)triggered.kind;
    output[23] = triggered.bits;
    return true;
}

static bool check_blue_split_native(PlayfieldTest *test, AbClassicPlayfieldView view,
                                    FILE *reference, AbError *error) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), reference) &&
          !strcmp(marker, "AB_ANDROID_BLUES_SPLIT_V1 16 3\n"));
    AbClassicBluesSplit split;
    BluesNativeCheck check = {.test = test, .view = view, .split = &split};
    CHECK(ab_classic_blues_split_initialize(
        (AbClassicBluesSplitConfig){
            .playfield = view,
            .birds = test->birds,
            .physical = test->physical,
            .settings = test->statistics,
            .audio = test->audio,
            .hats = test->blue_hats,
            .teleporters = test->teleporters,
            .flying = &test->flying,
            .names = {test->blue_names, sizeof(test->blue_names)},
            .trace_context = &check,
            .trace = blue_native_trace},
        &split, error));
    AbUiFrameState notification_state = {0};
    AbUiFrameFields notification_fields = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span("notifications")}};
    AbUiInitBoundaries boundaries = {0};
    CHECK(ab_ui_frame_init(&notification_state, &notification_fields, &boundaries,
                           error));
    AbUiFrameNode notifications = {.state = &notification_state};
    AbClassicBirdSpecialty *specialty = NULL;
    CHECK(ab_classic_bird_specialty_open(
        (AbClassicBirdSpecialtyConfig){.shots = view.shots,
                                       .behaviors = view.behaviors,
                                       .audio = test->audio,
                                       .events = test->events,
                                       .notifications = &notifications,
                                       .flying = &test->flying,
                                       .red_available = &test->red_special,
                                       .notifications_capacity = BS_PROFILES,
                                       .storage_bytes = 16384,
                                       .settings = test->statistics,
                                       .blues = &split},
        &specialty, error));
    AbEventListener listener = {span("BluesNativeCheck"), &check, blue_native_event};
    AbDataValue event_id = {.kind = AB_DATA_STRING,
                            .string = span("EID_BIRD_SPECIALTY_TRIGGERED")};
    CHECK(ab_event_manager_add(test->events, event_id, &listener, error));
    AbBirdCollisionHat hats[BS_PROFILES];
    AbClassicBluesSplitResult results[BS_PROFILES];
    AbCameraSequenceStage wait = {
        .action = span("wait"), .duration = 5, .duration_present = true};
    AbCameraSequence pending = {&wait, 1};
    size_t stage_count = 0;
    for (unsigned profile = 0; profile < BS_PROFILES; ++profile) {
        /* These are independent prepared captures, without a device callback
         * draining audio between them. Release prior real mixer voices before
         * the next source call; actual per-call selection/playback stays bound. */
        int16_t scratch = 0;
        CHECK(ab_audio_mixer_all_action(test->mixer, AB_AUDIO_STOP, error) &&
              ab_audio_mixer_mix(test->mixer, 0, &scratch, 1, error) &&
              ab_audio_mixer_mix(test->mixer, 0, &scratch, 1, error));
        check.profile = profile;
        check.count = check.events = check.camera_ends = 0;
        check.okay = true;
        check.initial_tracks = ab_classic_trajectories_view(view.trajectories).tracks;
        CHECK(blue_native_parent(test, view, profile, &check.parent, error));
        AbClassicLevelView level = ab_classic_level_view(test->level);
        AbSpan tutorial_path[] = {span("tutorials"), span("BIRD_BLUE"),
                                  span("showHelp")};
        AbDataValue help = profile % 4 == 0   ? (AbDataValue){0}
                           : profile % 4 == 1 ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                           : profile % 4 == 2
                               ? (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}
                               : (AbDataValue){.kind = AB_DATA_NUMBER};
        CHECK(ab_settings_statistics_write(test->statistics, tutorial_path, 3, help,
                                           error));
        hats[profile].object_name = level.bodies->objects[check.parent].name;
        if (profile & 1u)
            CHECK(ab_string_map_set(test->blue_hats, hats[profile].object_name,
                                    (uintptr_t)(hats + profile), true, error));
        test->flying =
            (AbDataValue){.kind = AB_DATA_TABLE,
                          .table = &level.properties->objects[check.parent].physical};
        CHECK(ab_classic_birds_sync(test->birds, error) &&
              ab_classic_behaviors_launch(view.behaviors, check.parent, (AbVec2){0},
                                          error) &&
              ab_camera_system_start(ab_classic_camera_system(view.camera), &pending,
                                     blue_native_camera_done, &check, error));
        AbDataTable *old_other = ab_classic_birds_view(test->birds).other_birds;
        size_t logs = test->logs;
        bool triggered = ab_classic_bird_specialty_trigger_flying(specialty, 0, error);
        if (!triggered)
            fprintf(stderr, "Blues outer profile %u failed\n", profile);
        CHECK(triggered && check.okay && split.has_result && !split.failed);
        results[profile] = split.last_result;
        uint32_t header[3], expected_stages[BS_MAX_STAGES];
        CHECK(read_words(reference, header, 3) && header[0] == profile &&
              header[1] == check.count && header[2] == check.follows &&
              read_words(reference, expected_stages, check.count) &&
              bc_compare_c(check.stages, expected_stages, check.count, profile, 0,
                           "outer-stage", 0));
        stage_count += check.count;
        uint32_t summary[BS_SUMMARY_WORDS], expected_summary[BS_SUMMARY_WORDS];
        CHECK(blue_native_summary(&check, old_other, hats + profile, logs, summary,
                                  error) &&
              read_words(reference, expected_summary, BS_SUMMARY_WORDS) &&
              bc_compare_c(summary, expected_summary, BS_SUMMARY_WORDS, profile, 0,
                           "outer-summary", 0));
        for (unsigned child = 0; child < 3; ++child) {
            BluesChildCheck snapshot = {.test = test,
                                        .birds =
                                            ab_classic_birds_view(test->birds).birds,
                                        .object = split.last_result.objects[child],
                                        .parent_name = span(bs_parent_names[profile])};
            AbSpan name = split.last_result.names[child];
            CHECK(name.size < sizeof(snapshot.name));
            memcpy(snapshot.name, name.data, name.size);
            snapshot.name[name.size] = 0;
            uint32_t actual[BC_SNAPSHOT_WORDS], expected[BC_SNAPSHOT_WORDS];
            CHECK(bc_snapshot_c(&snapshot, actual) &&
                  read_words(reference, expected, BC_SNAPSHOT_WORDS) &&
                  bc_compare_c(actual, expected, BC_SNAPSHOT_WORDS, profile, child,
                               "outer-child", 0) &&
                  ab_classic_post_physics_register_circle(test->blue_post,
                                                          snapshot.object, error));
        }
        CHECK(ab_classic_render_scene_sync(view.scene, error) &&
              ab_classic_scene_sync_visuals(ab_classic_render_scene_objects(view.scene),
                                            view.visuals, error));
    }
    CHECK(fgetc(reference) == '\n' && fgets(marker, sizeof(marker), reference) &&
          !strcmp(marker, "AB Blues split reference complete\n") &&
          fgetc(reference) == EOF &&
          ab_event_manager_remove(test->events, event_id, &listener, error));
    for (unsigned profile = 0; profile < BS_PROFILES; ++profile)
        if (profile & 1u)
            CHECK(ab_string_map_set(test->blue_hats, results[profile].names[2], 0,
                                    false, error));
    ab_classic_bird_specialty_close(specialty);
    size_t words =
        BS_PROFILES * (3 + BS_SUMMARY_WORDS + 3 * BC_SNAPSHOT_WORDS) + stage_count;
    printf("Actual Blues outer:16 native profiles/48 children, %zu ordered calls/"
           "%zu typed words, numeric fields/bodies and live aliases match\n",
           stage_count, words);
    return true;
}

#endif
