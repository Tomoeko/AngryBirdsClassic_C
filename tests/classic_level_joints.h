#ifndef AB_TEST_CLASSIC_LEVEL_JOINTS_H
#define AB_TEST_CLASSIC_LEVEL_JOINTS_H

static bool level_joint_owners(const AbLevelBodySource *original,
                               AbWorldDefinition physics) {
    const char *const names[] = {"rope", "weld", "hinge", "slider", "glue"};
    AbDataEntry fields[5][10], entries[5], globals[8];
    AbDataTable definitions[5];
    for (size_t index = 0; index < 5; ++index) {
        const AbDataEntry values[] = {
            {string("type"), number((float)(index + 1))},
            {string("name"), string(names[index])},
            {string("end1"), string("Bird_1")},
            {string("end2"), string(index == 4 ? "missing" : "ground")},
            {string("coordType"), number(2)},
            {string("x1"), number(0)},
            {string("y1"), number(0)},
            {string("x2"), number(0)},
            {string("y2"), number(0)},
            {string("worldAxisX"), number(1)},
        };
        memcpy(fields[index], values, sizeof(values));
        definitions[index] = (AbDataTable){fields[index], 10, 10, NULL};
        entries[index] =
            (AbDataEntry){string(names[index]), table(definitions + index)};
    }
    AbDataTable joint_table = {entries, 5, 5, NULL};
    CHECK(original->level->globals->count <= 8);
    size_t count = original->level->globals->count;
    memcpy(globals, original->level->globals->entries, count * sizeof(*globals));
    bool replaced = false;
    for (size_t index = 0; index < count; ++index) {
        if (globals[index].key.kind == AB_DATA_STRING &&
            globals[index].key.string.size == 6 &&
            memcmp(globals[index].key.string.data, "joints", 6) == 0) {
            globals[index].value = table(&joint_table);
            replaced = true;
        }
    }
    CHECK(replaced);
    AbDataTable global_table = {globals, count, count, NULL};
    AbDataTableLayout joint_layout = {.table = &joint_table,
                                      .hash_capacity = 8,
                                      .no_nil_assignments = true,
                                      .next = original->level->layouts};
    AbLuaData data = *original->level;
    data.globals = &global_table;
    data.layouts = &joint_layout;
    AbLevelBodySource source = *original;
    source.level = &data;
    AbLevelJointRequirements required = {77, 88, 99};
    CHECK(!ab_level_joint_requirements(NULL, &required, NULL) &&
          required.immediate == 77 &&
          ab_level_joint_requirements(&data, &required, NULL) &&
          required.immediate == 4 && required.deferred == 1 &&
          required.hash_capacity == 8);
    AbClassicLevelLimits limits = {
        .objects = 4, .contacts = 16, .storage_bytes = 65536};
    AbClassicLevel *owner = NULL;
    joint_layout.no_nil_assignments = false;
    CHECK(!ab_classic_level_open(&source, physics, limits, false, &owner, NULL) &&
          !owner);
    joint_layout.no_nil_assignments = true;
    limits.joints = 3;
    CHECK(!ab_classic_level_open(&source, physics, limits, false, &owner, NULL) &&
          !owner);
    limits.joints = SIZE_MAX;
    CHECK(!ab_classic_level_open(&source, physics, limits, false, &owner, NULL) &&
          !owner);
    limits.joints = 0;
    fields[1][3].value = string("missing");
    CHECK(!ab_classic_level_open(&source, physics, limits, false, &owner, NULL) &&
          !owner);
    fields[1][3].value = string("ground");
    size_t indices[5];
    AbStringOrderNode nodes[8];
    AbStringOrderWorkspace order = {indices, 5, nodes, 8};
    CHECK(ab_data_string_order(&joint_table, 8, &order, NULL));
    for (size_t queued = 0; queued < 2; ++queued) {
        CHECK(
            ab_classic_level_open(&source, physics, limits, queued != 0, &owner, NULL));
        AbClassicLevelView view = ab_classic_level_view(owner);
        CHECK(view.joints && view.world->joint_count == 4 &&
              view.world->storage.joint_capacity == 5 &&
              view.joints->joint_count == 4 && view.joints->glue_count == 1 &&
              view.world->storage.bodies[0].motion.mass.angle == (queued ? 0 : 0.5f));
        size_t immediate = 0;
        for (size_t cursor = 0; cursor < 5; ++cursor) {
            size_t entry = indices[cursor];
            const AbClassicJointRecord *record =
                entry == 4 ? view.joints->storage.glue
                           : view.joints->storage.joints + immediate++;
            CHECK(record->definition.type == (AbClassicJointType)(entry + 1));
            if (entry == 4) {
                CHECK(record->world_joint == SIZE_MAX);
                continue;
            }
            const AbJointConstraint *joint =
                &view.world->storage.joints[record->world_joint].constraint;
            CHECK(joint->kind == (AbJointKind)entry &&
                  view.world->joint_order[3 - (immediate - 1)] == record->world_joint);
            if (entry == 1) {
                CHECK(joint->definition.weld.reference_angle == -0.5f);
            } else if (entry == 2) {
                CHECK(joint->definition.revolute.reference_angle == -0.5f);
            } else if (entry == 3) {
                CHECK(joint->definition.prismatic.reference_angle == -0.5f);
            }
        }
        CHECK(!ab_level_joints_create(view.joints, &data, view.bodies, &order, NULL));
        const AbClassicGluePair pair = {view.bodies->objects[0].physics.body,
                                        view.bodies->objects[1].physics.body};
        size_t pairs = 1;
        CHECK(ab_classic_joints_glue(view.joints, &pair, &pairs, NULL) && !pairs &&
              view.world->joint_count == 1 && !view.joints->joint_count &&
              view.joints->glue_count == 1 &&
              ab_world_step(view.world, 1.0f / 30, 8, 3, NULL));
        ab_classic_level_close(owner);
        owner = NULL;
    }
    /* onLoad reward pruning keeps original constructor entries/layout while
     * reducing live joint requirements, including an entirely pruned table. */
    entries[0].value = (AbDataValue){0};
    entries[3].value = (AbDataValue){0};
    entries[4].value = (AbDataValue){0};
    CHECK(ab_classic_level_open(&source, physics, limits, false, &owner, NULL));
    AbClassicLevelView pruned = ab_classic_level_view(owner);
    CHECK(pruned.joints->joint_count == 2 && !pruned.joints->glue_count &&
          pruned.world->joint_count == 2);
    ab_classic_level_close(owner);
    owner = NULL;
    entries[1].value = (AbDataValue){0};
    entries[2].value = (AbDataValue){0};
    CHECK(ab_classic_level_open(&source, physics, limits, false, &owner, NULL));
    pruned = ab_classic_level_view(owner);
    CHECK(!pruned.joints->joint_count && !pruned.joints->glue_count &&
          !pruned.world->joint_count);
    ab_classic_level_close(owner);
    return true;
}

#endif
