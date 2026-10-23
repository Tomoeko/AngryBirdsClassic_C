#include "ab/classic_physics_phases.h"

#include <string.h>

static bool flush_birds(AbClassicPhysicsPhases *owner, AbError *error) {
    AbGameObjectCoreRegistry *registry =
        ab_classic_object_removal_registry(owner->config.physical);
    AbClassicLevelView level = ab_classic_level_view(owner->config.level);
    if (!registry || registry->world != level.world ||
        registry->bird_count > registry->bird_capacity ||
        (registry->bird_count && !registry->birds))
        return ab_fail(error, "Invalid shared physics-phase bird vector");
    AbClassicContacts *contacts = owner->config.contacts;
    if (!contacts || contacts->level.world != level.world ||
        contacts->count != level.bodies->count)
        return ab_fail(error, "Unbound actual physics-phase contact owner");
    for (size_t bird = 0; bird < registry->bird_count; ++bird) {
        const AbGameObjectCore *object = registry->birds[bird];
        if (!object || !object->name.data || !object->name.size)
            return ab_fail(error, "Missing shared physics-phase bird metadata");
        size_t index = 0;
        /* Destroyed body slots are reused by split children. A body index
         * alone can resolve to the removed parent instead of the live bird. */
        while (index < level.bodies->count &&
               (level.removed_objects[index] ||
                level.bodies->objects[index].physics.body != object->body ||
                level.bodies->objects[index].name.size != object->name.size ||
                memcmp(level.bodies->objects[index].name.data, object->name.data,
                       object->name.size)))
            ++index;
        if (index == level.bodies->count || level.removed_objects[index])
            return ab_fail(error, "Stale shared physics-phase bird membership");
        if (!ab_classic_contacts_flush(contacts, index, error))
            return false;
    }
    return true;
}

bool ab_classic_physics_phases_initialize(AbClassicPhysicsPhaseConfig config,
                                          AbClassicPhysicsPhases *output,
                                          AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!output || !level.data || !level.data->globals || !config.contacts ||
        !config.removals || !config.physical || !config.scripts)
        return ab_fail(error, "Invalid actual physics-phase bindings");
    AbClassicPhysicsPhases owner = {.config = config};
    if (!ab_water_effect(level.data->globals, &owner.water, error))
        return false;
    *output = owner;
    return true;
}

bool ab_classic_physics_phases_call(void *context, const AbClassicSimulationCall *call,
                                    AbError *error) {
    AbClassicPhysicsPhases *owner = context;
    if (!owner || !call || owner->calling)
        return ab_fail(error, "Invalid or reentrant actual physics phase");
    owner->calling = true;
    bool okay;
    switch (call->stage) {
        case AB_CLASSIC_SIMULATION_WATER:
            okay = !owner->water.enabled ||
                   (owner->config.water
                        ? owner->config.water(owner->config.context, &owner->water,
                                              call->seconds, error)
                        : ab_fail(error, "Missing actual water physics service"));
            break;
        case AB_CLASSIC_SIMULATION_SCRIPT_STEP:
            okay = ab_physics_script_step(owner->config.scripts, call->seconds, error);
            break;
        case AB_CLASSIC_SIMULATION_AFTER_STEP:
            okay = ab_classic_removals_run(owner->config.removals, error) &&
                   flush_birds(owner, error);
            break;
        default:
            okay = ab_fail(error, "Unknown actual physics phase");
            break;
    }
    owner->calling = false;
    return okay;
}
