#ifndef AB_TEST_CLASSIC_CAMERA_LEVEL_H
#define AB_TEST_CLASSIC_CAMERA_LEVEL_H

#include "ab/classic_level.h"

#include <string.h>

typedef struct {
    AbDataEntry definition_entries[7], block_entries[2], catalog_entries[3];
    AbDataEntry placement_entries[2][5], world_entries[2], global_entries[2];
    AbDataTable definition, empty, blocks, catalog;
    AbDataTable placements[2], world, globals;
    AbDataTableLayout layout;
    AbLuaData data;
} ClassicCameraLevelInput;

static AbDataValue classic_camera_text(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue classic_camera_number(float number) {
    AbDataValue value = {.kind = AB_DATA_NUMBER};
    memcpy(&value.bits, &number, sizeof(number));
    return value;
}

static AbDataValue classic_camera_table(AbDataTable *table) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static bool classic_camera_open_level(ClassicCameraLevelInput *input,
                                      AbClassicLevel **output) {
    *input = (ClassicCameraLevelInput){0};
    const char *const names[] = {"type",        "radius",   "density",     "friction",
                                 "restitution", "collider", "controllable"};
    const AbDataValue values[] = {classic_camera_text("circle"),
                                  classic_camera_number(0.5f),
                                  classic_camera_number(1),
                                  classic_camera_number(0.3f),
                                  classic_camera_number(0),
                                  classic_camera_number(3),
                                  {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    for (size_t index = 0; index < 7; ++index)
        input->definition_entries[index] =
            (AbDataEntry){classic_camera_text(names[index]), values[index]};
    input->definition = (AbDataTable){input->definition_entries, 7, 7, NULL};
    input->block_entries[0] = (AbDataEntry){classic_camera_text("Bird"),
                                            classic_camera_table(&input->definition)};
    input->block_entries[1] = (AbDataEntry){classic_camera_text("Ground"),
                                            classic_camera_table(&input->empty)};
    input->blocks = (AbDataTable){input->block_entries, 2, 2, NULL};
    input->catalog_entries[0] = (AbDataEntry){classic_camera_text("blocks"),
                                              classic_camera_table(&input->blocks)};
    input->catalog_entries[1] = (AbDataEntry){classic_camera_text("materials"),
                                              classic_camera_table(&input->empty)};
    input->catalog_entries[2] = (AbDataEntry){classic_camera_text("collider_types"),
                                              classic_camera_table(&input->empty)};
    input->catalog = (AbDataTable){input->catalog_entries, 3, 3, NULL};
    for (size_t index = 0; index < 2; ++index) {
        const char *name = index ? "bird2" : "bird1";
        AbDataEntry *entries = input->placement_entries[index];
        entries[0] =
            (AbDataEntry){classic_camera_text("name"), classic_camera_text(name)};
        entries[1] = (AbDataEntry){classic_camera_text("definition"),
                                   classic_camera_text("Bird")};
        entries[2] = (AbDataEntry){classic_camera_text("x"),
                                   classic_camera_number(index ? 5 : 1)};
        entries[3] = (AbDataEntry){classic_camera_text("y"),
                                   classic_camera_number(index ? 3 : 2)};
        entries[4] =
            (AbDataEntry){classic_camera_text("angle"), classic_camera_number(0)};
        input->placements[index] = (AbDataTable){entries, 5, 5, NULL};
        input->world_entries[index] = (AbDataEntry){
            classic_camera_text(name), classic_camera_table(input->placements + index)};
    }
    input->world = (AbDataTable){input->world_entries, 2, 2, NULL};
    input->global_entries[0] = (AbDataEntry){classic_camera_text("world"),
                                             classic_camera_table(&input->world)};
    input->global_entries[1] = (AbDataEntry){classic_camera_text("joints"),
                                             classic_camera_table(&input->empty)};
    input->globals = (AbDataTable){input->global_entries, 2, 2, NULL};
    input->layout = (AbDataTableLayout){
        .table = &input->world, .hash_capacity = 2, .no_nil_assignments = true};
    input->data = (AbLuaData){.globals = &input->globals, .layouts = &input->layout};
    AbLevelBodySource source = {.level = &input->data,
                                .catalog = &input->catalog,
                                .placement_scale = 1,
                                .physics_scale = 0.05f,
                                .ground_position = {0, 10}};
    AbWorldDefinition world = {.gravity = {0, 20},
                               .broadphase_limits = {64, 128, 128},
                               .maximum_translation = 2,
                               .maximum_translation_squared = 4,
                               .clear_forces = true,
                               .continuous = true};
    AbClassicLevelLimits limits = {
        .objects = 3, .contacts = 16, .storage_bytes = 65536};
    return ab_classic_level_open(&source, world, limits, false, output, NULL);
}

#endif
