#include "ab/classic_level.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool same(AbDataValue value, AbDataValue expected) {
    return value.kind == expected.kind &&
           (value.kind == AB_DATA_STRING
                ? value.string.size == expected.string.size &&
                      !memcmp(value.string.data, expected.string.data,
                              value.string.size)
                : value.bits == expected.bits);
}

static bool numeric(AbDataValue value, float expected) {
    float actual = 0;
    memcpy(&actual, &value.bits, sizeof(actual));
    return value.kind == AB_DATA_NUMBER && actual == expected;
}

static bool construct(AbClassicLevel *level, const AbDataTable *hats, AbError *error) {
    const char *const names[] = {"PartyHat_1", "PartyHat_2", "PartyHat_3", "PartyHat_4",
                                 "PartyHat_blue"};
    const char *const definitions[] = {"PartyHat_1", "PartyHat_2", "PartyHat_5",
                                       "PartyHat_6", "PartyHat_blue"};
    /* These names remain alive until the borrowing level closes in main. */
    static const char *const generated[] = {"generated_hat_1", "generated_hat_2",
                                            "generated_hat_3", "generated_hat_4",
                                            "generated_hat_5"};
    for (size_t index = 0; index < 5; ++index) {
        AbDataValue selected = field(hats, names[index]);
        if (selected.kind != AB_DATA_TABLE ||
            !same(field(selected.table, "block"), text(definitions[index])))
            return ab_fail(error, "Missing actual hat catalog mapping");
        AbSpan name = text(generated[index]).string;
        AbSpan definition = field(selected.table, "block").string;
        AbVec2 position = {(float)index * 4, -5};
        size_t object = SIZE_MAX;
        if (!ab_classic_level_create_defined_box(level, name, definition, position,
                                                 0.001f, &object, error))
            return false;
        AbClassicLevelView view = ab_classic_level_view(level);
        const AbLevelObject *created = view.bodies->objects + object;
        const AbWorldFixture *fixture =
            view.world->storage.fixtures + created->physics.fixture;
        const AbDataTable *physical = &view.properties->objects[object].physical;
        const AbDataValue *published =
            ab_data_get(&view.properties->dynamic, text(generated[index]));
        if (object != index + 1 || !published || published->table != physical ||
            !same(field(physical, "definition"), text(definitions[index])) ||
            !numeric(field(physical, "density"), 0.001f) ||
            !numeric(field(physical, "strength"), 10) ||
            !numeric(field(physical, "defence"), 20) ||
            !numeric(field(physical, "height"), 1.7f) ||
            !numeric(field(physical, "width"), index == 1 ? 1.6f : 1.5f) ||
            field(physical, "gameObjectScript").kind != AB_DATA_NIL ||
            view.initial_sprites[object].sprite.kind != AB_DATA_STRING ||
            fixture->shape.kind != AB_SHAPE_POLYGON ||
            fixture->shape.data.polygon.count != 4 || fixture->density != 0.001f ||
            fixture->friction != 1 || fixture->restitution != 0 ||
            fixture->user_data != (uintptr_t)created ||
            view.world->storage.bodies[created->physics.body].motion.mass.type !=
                AB_BODY_DYNAMIC)
            return ab_fail(error,
                           "Defined hat box has incorrect physical/catalog state");
        if (!ab_level_properties_bind_script(view.properties, object, error))
            return false;
        AbDataValue script = field(physical, "gameObjectScript");
        if (script.kind != AB_DATA_TABLE ||
            script.table != &view.properties->objects[object].script)
            return ab_fail(error, "Defined box lost its canonical GameObject identity");
        size_t unchanged = 99;
        if (ab_classic_level_create_defined_box(level, name, definition, position,
                                                0.001f, &unchanged, NULL) ||
            unchanged != 99 || view.bodies->count != index + 2)
            return ab_fail(error, "Defined box duplicate changed its world");
    }
    AbClassicLevelView view = ab_classic_level_view(level);
    return (view.bodies->count == 6 && view.world->body_count == 6 &&
            view.properties->dynamic.count == 5 && view.bird_count == 0) ||
           ab_fail(error, "Defined box construction changed native bird membership");
}

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbClassicLevelSource *source = NULL;
    AbClassicLevel *level = NULL;
    AbClassicLevelSourceView catalog = {0};
    AbError error = {0};
    AbDataTable empty = {0};
    AbDataEntry entries[] = {
        {text("world"), {.kind = AB_DATA_TABLE, .table = &empty}},
        {text("joints"), {.kind = AB_DATA_TABLE, .table = &empty}}};
    AbDataTable globals = {entries, 2, 2, NULL};
    AbDataTableLayout layout = {.table = &empty, .no_nil_assignments = true};
    AbLuaData data = {.globals = &globals, .layouts = &layout};
    bool okay =
        ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
        ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
        ab_classic_level_source_open(&archive, 16 * 1024 * 1024, &source, &error) &&
        ab_classic_level_source_view(source, &archive, &catalog, &error);
    if (okay)
        okay = ab_classic_level_open(
            &(AbLevelBodySource){.level = &data,
                                 .catalog = catalog.catalog->globals,
                                 .placement_scale = 1,
                                 .physics_scale = 0.05f,
                                 .ground_position = {0, 10}},
            (AbWorldDefinition){.gravity = {0, 20},
                                .broadphase_limits = {64, 128, 128},
                                .maximum_translation = 2,
                                .maximum_translation_squared = 4,
                                .clear_forces = true},
            (AbClassicLevelLimits){
                .objects = 8, .contacts = 32, .storage_bytes = 2 * 1024 * 1024},
            false, &level, &error);
    AbDataValue hats =
        okay ? field(catalog.catalog->globals, "hats") : (AbDataValue){0};
    okay = okay && hats.kind == AB_DATA_TABLE && construct(level, hats.table, &error);
    if (okay)
        puts("All five actual hat definitions create real canonical box bodies");
    else
        fprintf(stderr, "%s\n", error.message);
    ab_classic_level_close(level);
    ab_classic_level_source_close(source);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
