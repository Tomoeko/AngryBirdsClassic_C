#include "ab/classic_blues_split.h"
#include "ab/coordinates.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static void trace(AbClassicBluesSplit *owner, unsigned stage) {
    if (owner->config.trace)
        owner->config.trace(owner->config.trace_context, stage);
}

static void child_trace(void *context, AbClassicBluesChildStage stage,
                        const AbDataValue *arguments, size_t count) {
    (void)arguments;
    (void)count;
    trace(context, (unsigned)stage);
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    float number;
    memcpy(&number, &value.bits, sizeof(number));
    if (value.kind != AB_DATA_NUMBER || !isfinite(number))
        return ab_fail(error, "Unsupported Blues split number");
    *output = number;
    return true;
}

static bool object_name(const AbDataTable *physical, AbSpan *output, AbError *error) {
    AbDataValue name = field(physical, "name");
    if (name.kind != AB_DATA_STRING || (name.string.size && !name.string.data))
        return ab_fail(error, "Unsupported Blues split object name");
    *output = name.string;
    return true;
}

/* utils.lua10029 uses separate float arithmetic, a strict threshold on both
 * components and the original math.sqrt's double input/output conversion. */
static bool direction(const AbDataTable *parent, AbVec2 *output, AbError *error) {
    float x = 0, y = 0, epsilon;
    uint32_t threshold = UINT32_C(0x358637bd);
    memcpy(&epsilon, &threshold, sizeof(epsilon));
    if (!numeric(field(parent, "yVel"), &x, error) ||
        !numeric(field(parent, "xVel"), &y, error))
        return false;
    y = -y;
    if (fabsf(x) < epsilon && fabsf(y) < epsilon) {
        *output = (AbVec2){0};
        return true;
    }
    float xx = x * x, yy = y * y;
    float squared = xx + yy;
    float length = (float)sqrt((double)squared);
    AbVec2 normalized = {x / length, y / length};
    if (!isfinite(squared) || length == 0 || !isfinite(normalized.x) ||
        !isfinite(normalized.y))
        return ab_fail(error, "Unsupported Blues split normalization");
    *output = normalized;
    return true;
}

static bool puff(AbClassicBluesSplit *owner, AbDataTable *parent, AbError *error) {
    trace(owner, AB_BLUES_SPLIT_PUFF);
    AbVec2 physics, world;
    if (!numeric(field(parent, "x"), &physics.x, error) ||
        !numeric(field(parent, "y"), &physics.y, error) ||
        !ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD, physics, &world))
        return ab_fail(error, "Unsupported Blues split puff position");
    return ab_classic_trajectories_puff(
        owner->config.playfield.trajectories, world,
        (AbDataValue){.kind = AB_DATA_TABLE, .table = parent}, (AbDataValue){0},
        (AbDataValue){0}, error);
}

static bool register_child(AbClassicBluesSplit *owner, size_t object, AbError *error) {
    AbClassicPlayfieldView *view = &owner->config.playfield;
    return ab_classic_shots_register_circle(view->shots, object, error) &&
           ab_classic_camera_register_object(view->camera, object, error) &&
           ab_classic_object_removal_register_circle(owner->config.physical, object,
                                                     error) &&
           ab_classic_birds_register_object(owner->config.birds, object, error);
}

static bool inherit_hat(AbClassicBluesSplit *owner, AbDataTable *parent, AbSpan child,
                        AbError *error) {
    AbSpan name;
    uintptr_t hat;
    bool present;
    if (!object_name(parent, &name, error) ||
        !ab_string_map_get(owner->config.hats, name, &hat, &present, error))
        return false;
    if (!present)
        return true;
    /* Values are actual table handles, hence truthy even for an empty hat.
     * Preserve the record's identity; the source does not clone its fields. */
    if (!hat)
        return ab_fail(error, "Invalid actual Blues hat handle");
    return ab_string_map_set(owner->config.hats, child, hat, true, error) &&
           object_name(parent, &name, error) &&
           ab_string_map_set(owner->config.hats, name, 0, false, error);
}

static bool inherit_teleportation(AbClassicBluesSplit *owner, AbDataTable *parent,
                                  AbError *error) {
    trace(owner, AB_BLUES_SPLIT_TELEPORT);
    AbSpan name;
    uintptr_t teleporter;
    bool present;
    if (!object_name(parent, &name, error) ||
        !ab_string_map_get(owner->config.teleporters, name, &teleporter, &present,
                           error))
        return false;
    /* Native GameLua::inheritTeleportation325bf8 returns before reading its
     * child array if the parent map key is absent. Active virtual clone+map
     * publication requires its real implementation, never an empty fallback. */
    return !present ||
           ab_fail(error, "Active Blues teleportation inheritance is not bound");
}

static bool flying_child(AbClassicBluesSplit *owner, AbDataTable *parent,
                         const AbClassicBluesSplitResult *created, AbError *error) {
    AbClassicPlayfieldView *view = &owner->config.playfield;
    AbClassicLevelView level = ab_classic_level_view(view->level);
    AbDataValue child;
    if (!ab_object_world_lookup(
            &level.properties->lookup,
            (AbDataValue){.kind = AB_DATA_STRING, .string = created->names[2]}, &child,
            error))
        return false;
    *owner->config.flying = child;
    if (!ab_classic_birds_sync(owner->config.birds, error) ||
        child.kind != AB_DATA_TABLE ||
        child.table != &level.properties->objects[created->objects[2]].physical ||
        field(child.table, "gameObjectScript").kind != AB_DATA_TABLE ||
        field(child.table, "gameObjectScript").table !=
            &level.properties->objects[created->objects[2]].script)
        return ab_fail(error, "Invalid Blues flying child camera binding");
    trace(owner, AB_BLUES_SPLIT_CAMERA);
    if (!ab_classic_behaviors_add(view->behaviors, created->objects[2],
                                  span("MuseumBluesCameraFollowBehavior"),
                                  (AbDataValue){0}, error))
        return ab_fail(error, "Invalid Blues flying child camera binding");
    trace(owner, AB_BLUES_SPLIT_BEHAVIOR);
    /* Camera init may run an actual sequence-end callback. The next source
     * expression reads flyingBird again before GameObject.getBehavior. */
    child = *owner->config.flying;
    size_t current = 0;
    while (current < level.bodies->count &&
           (child.kind != AB_DATA_TABLE ||
            child.table != &level.properties->objects[current].physical))
        ++current;
    if (current == level.bodies->count || level.removed_objects[current] ||
        field(child.table, "gameObjectScript").kind != AB_DATA_TABLE ||
        field(child.table, "gameObjectScript").table !=
            &level.properties->objects[current].script)
        return ab_fail(error, "Rebound Blues flying child script");
    /* GameObject.getBehavior returns the first actual instance in this group. */
    AbDataValue group = field(&level.properties->objects[current].behaviors,
                              "MuseumBluesTrajectoryRenderer");
    const AbDataValue *behavior =
        group.kind == AB_DATA_TABLE
            ? ab_data_get(group.table, (AbDataValue){.kind = AB_DATA_NUMBER,
                                                     .bits = UINT32_C(0x3f800000)})
            : NULL;
    if (!behavior || behavior->kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing actual Blues child trajectory behavior");
    AbObjectPropertyAccess access = {.physics_object = *behavior};
    return ab_object_property_set(&access, span("originalCluster"),
                                  (AbDataValue){.kind = AB_DATA_TABLE, .table = parent},
                                  error);
}

static bool special_audio(AbClassicBluesSplit *owner, AbDataTable *parent,
                          AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner->config.playfield.level);
    AbDataValue definitions = field(level.catalog, "blocks");
    const AbDataValue *definition =
        definitions.kind == AB_DATA_TABLE
            ? ab_data_get(definitions.table, field(parent, "definition"))
            : NULL;
    if (!definition || definition->kind != AB_DATA_TABLE)
        return ab_fail(error, "Unsupported Blues split audio definition");
    AbDataValue selected;
    uint32_t handle;
    trace(owner, AB_BLUES_SPLIT_AUDIO_NAME);
    if (!ab_audio_bank_name_value(owner->config.audio,
                                  field(definition->table, "specialSound"), &selected,
                                  error))
        return false;
    trace(owner, AB_BLUES_SPLIT_AUDIO_PLAY);
    return ab_audio_bank_play(
        owner->config.audio,
        (AbAudioPlay){.name = selected,
                      .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                      .loop = {.kind = AB_DATA_BOOLEAN}},
        &handle, error);
}

static bool create(AbClassicBluesSplit *owner, size_t object,
                   AbClassicBluesSplitResult *output, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner->config.playfield.level);
    AbDataTable *parent = &level.properties->objects[object].physical;
    AbDataValue definition = {0};
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     level.bodies->objects[object].name, &definition,
                                     error) ||
        definition.kind != AB_DATA_TABLE)
        return false;
    trace(owner, AB_BLUES_SPLIT_TUTORIAL);
    if (!ab_settings_statistics_extra_tutorial(
            owner->config.settings, field(definition.table, "sprite"), error) ||
        !puff(owner, parent, error))
        return false;
    AbVec2 normalized;
    trace(owner, AB_BLUES_SPLIT_NORMALIZE);
    if (!direction(parent, &normalized, error))
        return false;
    const AbVec2 offsets[] = {{-normalized.x, -normalized.y}, {0, 0}, normalized};
    static const char *const suffixes[] = {"a", "b", "c"};
    AbClassicBluesSplitResult created = {.direction = normalized};
    for (size_t index = 0; index < 3; ++index)
        if (!ab_classic_blues_child_create(&owner->child, object, span(suffixes[index]),
                                           offsets[index], created.objects + index,
                                           created.names + index, error) ||
            !register_child(owner, created.objects[index], error))
            return false;
    if (!inherit_hat(owner, parent, created.names[2], error) ||
        !inherit_teleportation(owner, parent, error))
        return false;
    const AbDataValue names[] = {{.kind = AB_DATA_STRING, .string = created.names[0]},
                                 {.kind = AB_DATA_STRING, .string = created.names[1]}};
    if (!ab_classic_birds_replace_other(owner->config.birds, names, 2, error) ||
        !flying_child(owner, parent, &created, error) ||
        !special_audio(owner, parent, error))
        return false;
    *output = created;
    owner->last_result = created;
    owner->has_result = true;
    return true;
}

bool ab_classic_blues_split_initialize(AbClassicBluesSplitConfig config,
                                       AbClassicBluesSplit *output, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(config.playfield.level);
    AbGameObjectCoreRegistry *physical =
        ab_classic_object_removal_registry(config.physical);
    if (!output || !level.bodies || !level.properties || !config.birds || !physical ||
        physical->world != level.world || !config.settings || !config.audio ||
        !config.hats || !config.teleporters || !config.flying ||
        !config.playfield.trajectories || !config.playfield.camera ||
        !config.playfield.shots)
        return ab_fail(error, "Invalid actual Blues split bindings");
    AbClassicBluesSplit next = {.config = config, .initialized = true};
    if (!ab_classic_blues_child_initialize(
            (AbClassicBluesChildConfig){
                .playfield = config.playfield,
                .birds = ab_classic_birds_view(config.birds).birds_field,
                .names = config.names,
                .trace_context = output,
                .trace = child_trace},
            &next.child, error))
        return false;
    *output = next;
    return true;
}

bool ab_classic_blues_split_create(AbClassicBluesSplit *owner, size_t parent,
                                   AbClassicBluesSplitResult *output, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(
        owner && owner->initialized ? owner->config.playfield.level : NULL);
    if (!owner || !owner->initialized || owner->calling || owner->failed || !output ||
        !level.bodies || parent >= level.bodies->count ||
        level.removed_objects[parent] || level.world->locked || level.world->failed)
        return ab_fail(error, "Invalid, failed or reentrant actual Blues split");
    owner->calling = true;
    bool okay = create(owner, parent, output, error);
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}
