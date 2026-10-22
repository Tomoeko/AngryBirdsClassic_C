#ifndef AB_TEST_LEVEL_FLIGHT_H
#define AB_TEST_LEVEL_FLIGHT_H

static bool frame_impact_after_step(void *context, AbPhysicsWorld *world,
                                    AbError *error) {
    (void)error;
    return impact_after_step(context, world);
}

static void sync_idle_frame_poses(LevelIdle *idle, AbGameFrame *frame,
                                  bool from_frame) {
    if (from_frame) {
        idle->history_index = frame->history_index;
    } else {
        frame->history_index = idle->history_index;
    }
    for (size_t item = 0; item < 2; ++item) {
        AbGameFrameObject *object = frame->objects + idle->indices[item];
        if (from_frame) {
            memcpy(idle->history[item], object->history, sizeof(object->history));
            idle->rendered[item] = object->rendered;
        } else {
            memcpy(object->history, idle->history[item], sizeof(object->history));
            object->rendered = idle->rendered[item];
        }
    }
}

static bool flight_queue_poses(FILE *file, AbPhysicsWorld *world,
                               const NativeObjectBody records[23], bool impacts,
                               const LevelReload *reload, bool generated_idle,
                               bool results, const LevelResults *ending,
                               AbGameFrame *pose_frame) {
    bool controller_enabled = ending->controller_enabled;
    for (size_t item = 0; item < 2; ++item) {
        uint32_t input[6] = {0};
        CHECK(read_body_words(file, input,
                              results   ? 6
                              : impacts ? 5
                                        : 4) &&
              input[0] < 23 && input[5] <= 1);
        CHECK(!results || input[5] == controller_enabled);
        const AbObjectBody *object = &records[input[0]].instance;
        CHECK(!records[input[0]].leading_bird &&
              object->collider.bits == UINT32_C(0x40400000) &&
              (input[4] == AB_BODY_STATIC || input[4] == AB_BODY_DYNAMIC));
        AbError error = {0};
        if ((results && (!controller_enabled ||
                         (ending->completed && input[0] == ending->current_object))) ||
            generated_idle ||
            (reload && reload->started && input[0] == reload->object_index)) {
            const AbBodyMassState *body =
                &world->storage.bodies[object->body].motion.mass;
            uint32_t actual[] = {number(body->transform.position.x).bits,
                                 number(body->transform.position.y).bits,
                                 number(body->angle).bits, (uint32_t)body->type};
            CHECK(compare_level_words(actual, input + 1, 4, 0, "reload", "pose", 0));
            continue;
        }
        CHECK(
            ab_world_set_body_type(world, object->body, (AbBodyType)input[4], &error));
        CHECK(ab_world_set_transform(world, object->body,
                                     (AbVec2){from_bits(input[1]), from_bits(input[2])},
                                     from_bits(input[3]), &error));
        if (pose_frame) {
            /* The queued pose is an explicit controller/setPosition input.
             * Its recovered setter resets both history samples and rendering. */
            CHECK(input[0] < pose_frame->count &&
                  ab_game_frame_object_reset(world, object->body, 3,
                                             pose_frame->objects + input[0], &error));
        }
        if (impacts && input[4] == AB_BODY_DYNAMIC &&
            (!results || controller_enabled)) {
            CHECK(ab_body_velocity_set(&world->storage.bodies[object->body].motion,
                                       (AbVec2){0}));
        }
    }
    return true;
}

static bool replay_level_flight_scoped(FILE *file, AbPhysicsWorld *world,
                                       const NativeObjectBody records[23],
                                       const uint32_t controls[8],
                                       const AbDataTable *catalog,
                                       const AbLevelObject objects[23],
                                       LevelImpactResources *resources,
                                       LevelCamera *camera) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    bool generated_queue = strcmp(marker, "AB_LEVEL_NATIVE_RESULTS_V2 96 23\n") == 0;
    bool generated_idle =
        generated_queue || strcmp(marker, "AB_LEVEL_NATIVE_IMPACT_V3 24 23\n") == 0;
    bool generated_reload =
        generated_idle || strcmp(marker, "AB_LEVEL_NATIVE_IMPACT_V2 24 23\n") == 0;
    bool results =
        generated_queue || strcmp(marker, "AB_LEVEL_NATIVE_RESULTS_V1 96 23\n") == 0;
    bool retry_prefix =
        results || strcmp(marker, "AB_LEVEL_NATIVE_RETRY_V1 38 23\n") == 0;
    bool scene_frames = strcmp(marker, "AB_LEVEL_NATIVE_SCENE_V1 24 23\n") == 0;
    bool trajectory_frames =
        scene_frames || strcmp(marker, "AB_LEVEL_NATIVE_TRAJECTORY_V1 24 23\n") == 0;
    bool camera_frames =
        trajectory_frames || strcmp(marker, "AB_LEVEL_NATIVE_CAMERA_V1 24 23\n") == 0;
    CHECK(!camera_frames || camera);
    bool impacts = camera_frames || retry_prefix || generated_reload ||
                   strcmp(marker, "AB_LEVEL_NATIVE_IMPACT_V1 24 23\n") == 0;
    CHECK(impacts || strcmp(marker, "AB_LEVEL_NATIVE_FLIGHT_V1 12 23\n") == 0);
    size_t frame_count = results ? 96 : retry_prefix ? 38 : impacts ? 24 : 12;
    LevelBoundsScope bounds_scope = impacts ? LEVEL_IMPACT_BOUNDS : LEVEL_SKIP_BIRD_FAT;
    const AbObjectBody *bird = NULL;
    for (size_t index = 0; index < 23; ++index) {
        if (records[index].leading_bird) {
            CHECK(!bird);
            bird = &records[index].instance;
        }
    }
    CHECK(bird);
    AbError error = {0};
    AbVec2 resting = world->storage.bodies[bird->body].motion.mass.transform.position;
    for (size_t step = 0; step < 512; ++step) {
        CHECK(ab_world_step(world, from_bits(controls[2]), controls[3], controls[4],
                            &error));
        CHECK(ab_slingshot_hold_body(world, bird, resting, &error));
    }
    for (size_t index = 0; index < 23; ++index) {
        const AbBodyMotion *motion =
            &world->storage.bodies[records[index].instance.body].motion;
        CHECK(records[index].leading_bird || motion->mass.type != AB_BODY_DYNAMIC ||
              !motion->awake);
    }
    uint32_t launch[8], expected_impulse[2];
    CHECK(read_body_words(file, launch, 8) &&
          read_body_words(file, expected_impulse, 2));
    AbSlingLaunchInput input = {
        .rest = {from_bits(launch[0]), from_bits(launch[1])},
        .rubber_band = {from_bits(launch[2]), from_bits(launch[3])},
        .launch_force = from_bits(launch[4]),
        .physics_scale = from_bits(launch[5]),
        .slingshot_power = from_bits(launch[6]),
        .bird_mass = world->storage.bodies[bird->body].motion.mass.mass,
        .bird_present = true};
    AbSlingStretch measured;
    CHECK(ab_slingshot_measure_stretch(input.rest, input.rubber_band,
                                       from_bits(launch[7]), &measured));
    input.stretch = measured.factor;
    AbVec2 impulse;
    CHECK(ab_slingshot_launch_impulse(&input, NULL, &impulse));
    if (number(impulse.x).bits != expected_impulse[0] ||
        number(impulse.y).bits != expected_impulse[1]) {
        fprintf(stderr,
                "Flight impulse %08" PRIx32 " %08" PRIx32 " != %08" PRIx32 " %08" PRIx32
                "; mass %08" PRIx32 "\n",
                number(impulse.x).bits, number(impulse.y).bits, expected_impulse[0],
                expected_impulse[1], number(input.bird_mass).bits);
    }
    CHECK(number(impulse.x).bits == expected_impulse[0] &&
          number(impulse.y).bits == expected_impulse[1]);
    CHECK(ab_slingshot_hold_body(world, bird, input.rubber_band, &error));
    CHECK(ab_body_impulse_apply(&world->storage.bodies[bird->body].motion, impulse,
                                &input.rubber_band));
    LevelImpact impact = {0};
    if (impacts) {
        CHECK(impact_initialize(file, world, records, catalog, &impact, resources));
    }
    if (camera_frames) {
        camera->compare_trajectory = trajectory_frames;
        camera->compare_scene = scene_frames;
        CHECK(level_camera_initialize(file, camera, resources->owner));
        impact.projection.context = camera;
        impact.projection.bird_collision = level_camera_collision;
    }
    LevelReload reload = {0};
    if (generated_reload) {
        CHECK(initialize_level_reload(file, objects, input.rest, &reload));
    }
    LevelIdle idle = {0};
    if (generated_idle) {
        CHECK(initialize_level_idle(file, world, records, catalog, generated_queue,
                                    &idle));
    }
    LevelResults ending = {0};
    if (results) {
        CHECK(initialize_level_results(file, &impact, objects, input.rest, &ending));
    }
    AbGameFrameObject poses[23];
    AbGameFrame update = {.objects = poses,
                          .count = 23,
                          .removed_objects = impacts ? impact.removal.removed : NULL};
    AbGameFrameControls stepping = {.velocity_iterations = controls[3],
                                    .position_iterations = controls[4],
                                    .maximum_steps = 32,
                                    .context = &impact,
                                    .after_step =
                                        impacts ? frame_impact_after_step : NULL};
    CHECK(number(0x1.111112p-5f).bits == controls[2]);
    for (size_t index = 0; index < 23; ++index) {
        const AbObjectBody *object = &records[index].instance;
        CHECK(ab_game_frame_object_reset(world, object->body,
                                         (uint32_t)from_bits(object->collider.bits),
                                         poses + index, &error));
    }
    if (scene_frames) {
        AbClassicLevelView level = ab_classic_level_view(resources->owner);
        memcpy(update.objects, level.frame->objects, sizeof(poses));
        update.history_index = level.frame->history_index;
    }
    if (generated_queue) {
        sync_idle_frame_poses(&idle, &update, false);
    }
    uint32_t total_steps = 0;
    for (size_t frame = 0; frame < frame_count; ++frame) {
        uint32_t elapsed[7], reload_expected[10] = {0};
        CHECK(read_body_words(file, elapsed, 7) && elapsed[1] <= 1);
        uint32_t ending_expected[12];
        if (results) {
            CHECK(read_body_words(file, ending_expected, 12));
            CHECK(results_state_matches(&ending, world, ending_expected, frame));
        }
        if (generated_reload) {
            CHECK(read_body_words(file, reload_expected, 10));
            CHECK(reload_state_matches(&reload, reload_expected, frame));
        }
        if (!frame) {
            update.clock = (AbFrameClock){from_bits(elapsed[3]), from_bits(elapsed[4])};
        }
        CHECK(number(update.clock.time).bits == elapsed[3] &&
              number(update.clock.accumulator).bits == elapsed[4]);
        CHECK(flight_queue_poses(file, world, records, impacts,
                                 generated_reload ? &reload : NULL, generated_idle,
                                 results, &ending, scene_frames ? &update : NULL));
        CHECK(compare_level_snapshot(file, world, records, frame, "flight before",
                                     bounds_scope));
        if (impacts) {
            CHECK(impact_lua_snapshot(file, world, &impact, frame));
        }
        if (generated_idle) {
            CHECK(idle_state_matches(file, &idle, frame));
        }
        if (generated_queue) {
            CHECK(idle_history_matches(file, &idle, frame));
        }
        AbFrameClockInput timing = {.seconds = from_bits(elapsed[0]),
                                    .speed_factor = from_bits(elapsed[2]),
                                    .speed_factor_present = true,
                                    .physics_enabled = elapsed[1] != 0};
        AbFrameClockPlan plan;
        CHECK(ab_game_frame_advance(&update, world, &timing, &stepping, &plan, &error));
        CHECK(number(plan.next.time).bits == elapsed[5] &&
              number(plan.next.accumulator).bits == elapsed[6]);
        if (generated_queue) {
            sync_idle_frame_poses(&idle, &update, true);
        }
        total_steps += plan.steps;
        if (impacts) {
            impact_sync_velocity(world, &impact);
        }
        if (results) {
            if (generated_queue) {
                ending.initialization.current_bird_index =
                    reload.state.current_bird_index;
            }
            CHECK(advance_level_results(&ending, world, timing.seconds));
            CHECK(results_state_matches(&ending, world, ending_expected + 6, frame));
        }
        bool reload_started_before = reload.started;
        if (generated_reload) {
            if (!results || ending.controller_enabled) {
                CHECK(advance_level_reload(&reload, world, records, timing.seconds,
                                           results && ending.completed));
            }
            CHECK(reload_state_matches(&reload, reload_expected + 5, frame));
        }
        if (generated_idle) {
            CHECK(advance_level_idle(&idle, world, records, &reload,
                                     reload.started && !reload_started_before,
                                     timing.seconds, plan.interpolation,
                                     results && ending.completed,
                                     !results || ending.controller_enabled));
        }
        if (generated_queue) {
            sync_idle_frame_poses(&idle, &update, false);
        }
        CHECK(flight_queue_poses(file, world, records, impacts,
                                 generated_reload ? &reload : NULL, generated_idle,
                                 results, &ending, scene_frames ? &update : NULL));
        CHECK(compare_level_snapshot(file, world, records, frame, "flight after",
                                     bounds_scope));
        if (impacts) {
            CHECK(impact_lua_snapshot(file, world, &impact, frame));
        }
        if (generated_idle) {
            CHECK(idle_state_matches(file, &idle, frame));
        }
        if (generated_queue) {
            CHECK(idle_history_matches(file, &idle, frame));
        }
        if (camera_frames) {
            CHECK(level_camera_advance(file, camera, &update, timing.seconds, frame));
        }
    }
    CHECK(!generated_idle || idle.draw_index == idle.draw_count);
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, impacts ? "AB level native impact complete\n"
                                 : "AB level native flight complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    if (camera_frames) {
        printf("All 25 live first-level camera boundaries match alongside physics: "
               "%zu predictions, %zu rays, %zu sequence loads. Initial camera "
               "position, launch elapsed time, queued poses and activity/specialty "
               "flags are inputs; viewport readback and full frame scheduling remain "
               "separate.\n",
               camera->predictions, camera->rays, camera->loads);
        if (trajectory_frames) {
            CHECK(camera->trajectory_removes == 1 && camera->trajectory_keeps == 1 &&
                  !camera->trajectory.count && camera->collision_notifications &&
                  camera->point_commands &&
                  ab_classic_render_scene_view(camera->render_scene).extra_count ==
                      camera->point_count);
            printf("All 25 trajectory/behavior states and %zu retained point positions "
                   "match through real C launch, collision and follow updates. "
                   "Actual APK trail resources bind into ordered shared draw "
                   "commands.\n",
                   camera->point_count);
            printf("Built %zu visible trail commands across all captured boundaries.\n",
                   camera->point_commands);
            if (scene_frames)
                puts("All 25 native scene counters, ordered IDs/classes, memberships "
                     "and renderer pose/scale/visibility fields match. Starting "
                     "render caches and queued controller poses are inputs; full "
                     "interactive scheduling and original GPU pixels remain separate.");
            else
                puts("Native scene counter/membership and GPU pixels are outside "
                     "this recording's comparison.");
        }
    }
    if (results) {
        CHECK(ending.bird_removals == 1 && ending.bonuses == 2 &&
              ending.notifications == 1 && ending.ending_events == 1 &&
              !ending.controller_enabled);
        puts("Restricted scene completion, retirement and bonuses match all native "
             "boundaries; one additional bird removal is generated. Ordinary "
             "configuration and camera identity are inputs. Ending event handoff "
             "uses this capture's next-update dispatch boundary.");
    }
    if (impacts) {
        printf("All %zu native impact frames match: %u physics steps, %zu C "
               "callbacks, %zu removals, score %.0f. Initial logical scalars and "
               "%s are inputs; bird fat bounds excluded.\n",
               frame_count, total_steps, impact.callbacks, impact.removals,
               (double)impact.score,
               generated_idle ? "inferred idle seeds/draws" : "remaining idle poses");
        if (generated_reload) {
            puts("Next bird's timer, source-data selection, transfer, body-type "
                 "completion and held motion match; reload poses are outputs.");
        }
        if (generated_queue) {
            puts("All queued poses, full jump state, both history slots and rendered "
                 "poses match from captured initial states and inferred random "
                 "boundary inputs. The original RNG remains open.");
        } else if (generated_idle) {
            puts("All queued poses are generated from explicit/inferred initial jump "
                 "seeds and inferred jump RNG boundary draws.");
        }
        return true;
    }
    printf("All 12 pre-impact native flight frames and %u physics steps match, "
           "including 23 bodies, ordered contacts and fixture bounds. Queued-bird "
           "poses and initial clock are inputs; bird fat bounds excluded.\n",
           total_steps);
    return true;
}

static bool replay_level_flight_camera(FILE *file, AbPhysicsWorld *world,
                                       const NativeObjectBody records[23],
                                       const uint32_t controls[8],
                                       const AbDataTable *catalog,
                                       const AbLevelObject objects[23],
                                       AbClassicLevel *owner, LevelCamera *camera) {
    LevelImpactResources resources = {.owner = owner};
    AbWorldListener previous_listener = world->listener;
    bool okay = replay_level_flight_scoped(file, world, records, controls, catalog,
                                           objects, &resources, camera);
    world->listener = previous_listener;
    impact_removal_resources_close(&resources);
    return okay;
}

static bool replay_level_flight(FILE *file, AbPhysicsWorld *world,
                                const NativeObjectBody records[23],
                                const uint32_t controls[8], const AbDataTable *catalog,
                                const AbLevelObject objects[23],
                                AbClassicLevel *owner) {
    return replay_level_flight_camera(file, world, records, controls, catalog, objects,
                                      owner, NULL);
}

#endif
