#include "ab/classic_world_runtime.h"

#include <stdlib.h>

struct AbClassicWorldRuntime {
    AbClassicWorldRuntimeConfig config;
    AbClassicPlayfieldView playfield;
    AbClassicWorldRuntimeView view;
    AbClassicMuseumCollisions museum;
    AbClassicContactObject *contacts;
    AbDataValue *physical_values;
    AbBirdCollisionScoreEntry *bird_scores;
    float *collision_entries, *destruction_entries;
    AbBlockCollisionScores block_scores;
    AbBlockRemovalScores destruction_scores;
    AbBirdCollisionGrenades grenades;
    float bird_bonus;
    size_t count, capacity;
};

static bool reserve(size_t count, size_t element, size_t *remaining, AbError *error) {
    if (!element || count > *remaining / element)
        return ab_fail(error, "World runtime exceeds its retained storage bound");
    *remaining -= count * element;
    return true;
}

static bool bindings(AbClassicWorldRuntimeConfig *config,
                     AbClassicPlayfieldView playfield, AbError *error) {
    if (!playfield.level || !playfield.visuals || !playfield.behaviors ||
        !playfield.scene || !config->particles || !config->audio || !config->settings ||
        !config->events || !config->globals || !config->random ||
        !config->physics_enabled ||
        ab_classic_particles_level(config->particles) != playfield.level ||
        ab_classic_shots_globals(playfield.shots) != config->globals ||
        config->blocks.level || config->blocks.visuals || config->blocks.behaviors ||
        config->blocks.audio || config->blocks.script_objects ||
        config->blocks.script_object_count || config->blocks.scores ||
        config->blocks.dead_blocks || config->birds.level || config->birds.visuals ||
        config->birds.behaviors || config->birds.audio ||
        config->birds.floating_scores || config->birds.script_objects ||
        config->birds.script_object_count || config->birds.hats ||
        config->birds.grenades || config->birds.bird_scores ||
        config->birds.bird_score_count || config->birds.scores ||
        config->birds.root_scores ||
        (config->birds.specialty_available &&
         config->birds.specialty_available != &config->globals->specialty_available) ||
        config->dynamics.level || config->dynamics.visuals || config->dynamics.scene ||
        config->dynamics.behaviors || config->dynamics.audio ||
        config->dynamics.settings || config->dynamics.events ||
        config->dynamics.globals || config->dynamics.random ||
        config->dynamics.physical.teleporters || config->dynamics.removals.particles ||
        config->dynamics.removals.floating || config->dynamics.removals.scores ||
        config->dynamics.removals.destruction_scores ||
        config->dynamics.removals.dead_blocks || config->dynamics.contact_objects ||
        config->dynamics.contact_count || config->dynamics.scripts ||
        config->dynamics.contacts.collision || config->museum.level ||
        config->museum.blocks || config->museum.dead_blocks ||
        config->museum.physical_values || config->museum.physical_capacity ||
        config->museum.bird)
        return ab_fail(error, "Invalid or conflicting fresh world runtime bindings");
    return true;
}

static bool contact(void *context, AbGameContactCall *call, AbError *error) {
    return ab_classic_museum_collisions_contact(
        &((AbClassicWorldRuntime *)context)->museum, call, error);
}

static bool end_contact(void *context, AbSpan first, AbSpan second, AbError *error) {
    AbClassicContactsConfig *source =
        &((AbClassicWorldRuntime *)context)->config.dynamics.contacts;
    return source->end ? source->end(source->context, first, second, error)
                       : ab_fail(error, "Missing selected end-contact method");
}

static bool special_contact(void *context, const AbGameContactDispatchCall *call,
                            AbError *error) {
    AbClassicContactsConfig *source =
        &((AbClassicWorldRuntime *)context)->config.dynamics.contacts;
    return source->special ? source->special(source->context, call, error)
                           : ab_fail(error, "Missing selected special-contact method");
}

static bool bird(void *context, const AbBirdCollisionNotifyInput *input,
                 AbError *error) {
    AbClassicWorldRuntime *runtime = context;
    return ab_classic_bird_collisions_call(runtime->view.birds, input, error);
}

static bool break_joint(void *context, AbSpan name, AbError *error) {
    AbClassicWorldRuntime *runtime = context;
    AbClassicMuseumCollisionConfig *source = &runtime->config.museum;
    return source->break_joint
               ? source->break_joint(source->context, name, error)
               : ab_fail(error, "Missing actual deferred joint-destruction owner");
}

static bool glue(void *context, AbGameContactCall *call, AbError *error) {
    AbClassicWorldRuntime *runtime = context;
    AbClassicMuseumCollisionConfig *source = &runtime->config.museum;
    return source->glue ? source->glue(source->context, call, error)
                        : ab_fail(error, "Missing actual Museum glue handler");
}

static bool allocate(AbClassicWorldRuntime *runtime, size_t count, AbError *error) {
    AbClassicWorldRuntimeConfig *config = &runtime->config;
    size_t capacity = runtime->capacity;
    size_t remaining = config->storage_bytes;
    const size_t budgets[] = {sizeof(*runtime),
                              config->blocks.storage_bytes,
                              config->birds.storage_bytes,
                              config->dynamics.storage_bytes,
                              config->script_bytes,
                              config->floating.storage_bytes};
    for (size_t index = 0; index < sizeof(budgets) / sizeof(*budgets); ++index)
        if (!reserve(1, budgets[index], &remaining, error))
            return false;
    if (!reserve(3, config->map_bytes, &remaining, error) ||
        !reserve(capacity, sizeof(*runtime->contacts), &remaining, error) ||
        !reserve(capacity, sizeof(*runtime->physical_values), &remaining, error) ||
        !reserve(capacity, sizeof(*runtime->bird_scores), &remaining, error) ||
        !reserve(config->score_entries, 2 * sizeof(float), &remaining, error))
        return false;
    runtime->contacts = calloc(capacity, sizeof(*runtime->contacts));
    runtime->physical_values = calloc(capacity, sizeof(*runtime->physical_values));
    runtime->bird_scores = calloc(capacity, sizeof(*runtime->bird_scores));
    if (config->score_entries) {
        runtime->collision_entries = calloc(config->score_entries, sizeof(float));
        runtime->destruction_entries = calloc(config->score_entries, sizeof(float));
    }
    if ((count &&
         (!runtime->contacts || !runtime->physical_values || !runtime->bird_scores)) ||
        (config->score_entries &&
         (!runtime->collision_entries || !runtime->destruction_entries)))
        return ab_fail(error, "Cannot allocate fresh world runtime arrays");
    runtime->block_scores = (AbBlockCollisionScores){
        .entries = runtime->collision_entries, .capacity = config->score_entries};
    runtime->destruction_scores = (AbBlockRemovalScores){
        .entries = runtime->destruction_entries, .capacity = config->score_entries};
    runtime->grenades.present = true;
    runtime->view.grenades = &runtime->grenades;
    runtime->view.block_scores = &runtime->block_scores;
    runtime->view.destruction_scores = &runtime->destruction_scores;
    runtime->view.bird_bonus = &runtime->bird_bonus;
    AbStringMap **maps[] = {&runtime->view.dead_blocks, &runtime->view.hats,
                            &runtime->view.teleporters};
    for (size_t index = 0; index < sizeof(maps) / sizeof(*maps); ++index)
        if (!ab_string_map_open(0, config->map_nodes, config->map_bytes, maps[index],
                                error))
            return false;
    return ab_floating_scores_open(config->floating, &runtime->view.floating, error) &&
           ab_classic_scores_open(
               (AbClassicScoreConfig){.level = runtime->playfield.level,
                                      .blocks = &runtime->block_scores,
                                      .birds = &runtime->bird_bonus,
                                      .objects = runtime->bird_scores,
                                      .count = count,
                                      .object_capacity = capacity,
                                      .storage_bytes = remaining},
               &runtime->view.scores, error);
}

static bool open_collisions(AbClassicWorldRuntime *runtime, AbError *error) {
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicBlockScriptView scripts =
        ab_classic_block_scripts_view(runtime->view.scripts);
    AbClassicBlockCollisionConfig blocks = runtime->config.blocks;
    blocks.level = playfield.level;
    blocks.visuals = playfield.visuals;
    blocks.behaviors = playfield.behaviors;
    blocks.audio = runtime->config.audio;
    blocks.script_objects = scripts.objects;
    blocks.script_object_count = scripts.count;
    blocks.scores = &runtime->block_scores;
    blocks.dead_blocks = runtime->view.dead_blocks;
    blocks.object_capacity = runtime->capacity;
    if (!ab_classic_block_collisions_open(blocks, &runtime->view.blocks, error))
        return false;
    AbClassicBirdCollisionConfig birds = runtime->config.birds;
    birds.level = playfield.level;
    birds.visuals = playfield.visuals;
    birds.behaviors = playfield.behaviors;
    birds.audio = runtime->config.audio;
    birds.floating_scores = runtime->view.floating;
    birds.script_objects = scripts.objects;
    birds.script_object_count = scripts.count;
    birds.hats = runtime->view.hats;
    birds.grenades = &runtime->grenades;
    birds.bird_scores = runtime->bird_scores;
    birds.bird_score_count = scripts.count;
    birds.scores = &runtime->block_scores;
    birds.root_scores = runtime->view.scores;
    birds.object_capacity = runtime->capacity;
    birds.specialty_available = &runtime->config.globals->specialty_available;
    if (!ab_classic_bird_collisions_open(birds, &runtime->view.birds, error))
        return false;
    AbClassicMuseumCollisionConfig museum = runtime->config.museum;
    museum.level = playfield.level;
    museum.blocks = runtime->view.blocks;
    museum.dead_blocks = runtime->view.dead_blocks;
    museum.physical_values = runtime->physical_values;
    museum.physical_capacity = runtime->capacity;
    museum.context = runtime;
    museum.bird = bird;
    museum.break_joint = break_joint;
    museum.glue = glue;
    return ab_classic_museum_collisions_open(museum, &runtime->museum, error);
}

static bool open_dynamics(AbClassicWorldRuntime *runtime, AbError *error) {
    AbClassicWorldRuntimeConfig *config = &runtime->config;
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicLevelView level = ab_classic_level_view(playfield.level);
    for (size_t index = 0; index < level.bodies->count; ++index) {
        AbDataValue key = {.kind = AB_DATA_STRING,
                           .string = level.bodies->objects[index].name};
        const AbDataValue *value = ab_data_get(&level.properties->world, key);
        runtime->contacts[index].script_table_present =
            value && value->kind != AB_DATA_NIL &&
            (value->kind != AB_DATA_BOOLEAN || value->bits);
    }
    AbClassicDynamicsConfig dynamics = config->dynamics;
    dynamics.level = playfield.level;
    dynamics.visuals = playfield.visuals;
    dynamics.scene = playfield.scene;
    dynamics.behaviors = playfield.behaviors;
    dynamics.audio = config->audio;
    dynamics.settings = config->settings;
    dynamics.events = config->events;
    dynamics.globals = config->globals;
    dynamics.random = config->random;
    dynamics.physical.teleporters = runtime->view.teleporters;
    dynamics.physical.object_capacity = runtime->capacity;
    dynamics.simulation.limits.object_capacity = runtime->capacity;
    dynamics.removals.object_capacity = runtime->capacity;
    dynamics.contacts.object_capacity = runtime->capacity;
    dynamics.removals.particles = config->particles;
    dynamics.removals.floating = runtime->view.floating;
    dynamics.removals.scores = &runtime->block_scores;
    dynamics.removals.destruction_scores = &runtime->destruction_scores;
    dynamics.removals.dead_blocks = runtime->view.dead_blocks;
    dynamics.contacts.collision = contact;
    dynamics.contacts.end = end_contact;
    dynamics.contacts.special = special_contact;
    dynamics.contacts.context = runtime;
    dynamics.contact_objects = runtime->contacts;
    dynamics.contact_count = level.bodies->count;
    dynamics.scripts = ab_classic_block_scripts_view(runtime->view.scripts).physics;
    return ab_classic_dynamics_open(dynamics, &runtime->view.dynamics, error);
}

bool ab_classic_world_runtime_open(AbClassicWorldRuntimeConfig config,
                                   AbClassicWorldRuntime **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing fresh world runtime output");
    *output = NULL;
    AbClassicPlayfieldView playfield = ab_classic_playfield_view(config.playfield);
    if (!bindings(&config, playfield, error))
        return false;
    AbClassicWorldRuntime *runtime = calloc(1, sizeof(*runtime));
    if (!runtime)
        return ab_fail(error, "Cannot allocate fresh world runtime");
    runtime->config = config;
    runtime->playfield = playfield;
    runtime->view.physics_enabled = config.physics_enabled;
    runtime->view.flying = config.birds.flying_object;
    AbClassicLevelView level = ab_classic_level_view(playfield.level);
    runtime->count = level.bodies->count;
    runtime->capacity =
        config.object_capacity ? config.object_capacity : runtime->count;
    runtime->view.object_capacity = runtime->capacity;
    if (runtime->capacity < runtime->count ||
        runtime->capacity > level.bodies->capacity) {
        ab_classic_world_runtime_close(&runtime, NULL);
        return ab_fail(error, "Invalid reserved world object capacity");
    }
    if (!allocate(runtime, level.bodies->count, error) ||
        !ab_classic_block_scripts_open(
            (AbClassicBlockScriptConfig){.level = playfield.level,
                                         .physics_enabled = config.physics_enabled,
                                         .storage_bytes = config.script_bytes,
                                         .object_capacity = runtime->capacity,
                                         .context = config.script_context,
                                         .level_step = config.level_physics},
            &runtime->view.scripts, error) ||
        !open_collisions(runtime, error) || !open_dynamics(runtime, error)) {
        ab_classic_world_runtime_close(&runtime, NULL);
        return false;
    }
    runtime->view.physical_values = runtime->physical_values;
    runtime->view.physical_count = ab_classic_level_view(playfield.level).bodies->count;
    *output = runtime;
    return true;
}

bool ab_classic_world_runtime_register_circle(AbClassicWorldRuntime *runtime,
                                              size_t object, AbError *error) {
    AbClassicLevelView level = runtime ? ab_classic_level_view(runtime->playfield.level)
                                       : (AbClassicLevelView){0};
    if (!runtime || object != runtime->count || object >= runtime->capacity ||
        object >= level.bodies->count || level.world->failed || level.world->locked)
        return ab_fail(error, "Invalid new world runtime circle");
    AbDataValue value = {.kind = AB_DATA_TABLE,
                         .table = &level.properties->objects[object].physical};
    runtime->physical_values[object] = value;
    runtime->contacts[object].script_table_present = true;
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(runtime->view.dynamics);
    if (!ab_classic_block_scripts_register_circle(runtime->view.scripts, object,
                                                  error) ||
        !ab_classic_scores_register_object(runtime->view.scores, object, error) ||
        !ab_classic_block_collisions_register_object(runtime->view.blocks, object,
                                                     error) ||
        !ab_classic_bird_collisions_register_object(runtime->view.birds, object,
                                                    error) ||
        !ab_classic_simulation_register_circle(dynamics.simulation, object, error) ||
        !ab_classic_removals_register_object(dynamics.removals, object, error) ||
        !ab_classic_contacts_register_object(dynamics.contacts, object, error))
        return false;
    ++runtime->count;
    runtime->view.physical_count = runtime->count;
    return true;
}

bool ab_classic_world_runtime_close(AbClassicWorldRuntime **pointer, AbError *error) {
    if (!pointer || !*pointer)
        return true;
    AbClassicWorldRuntime *runtime = *pointer;
    if (!ab_classic_dynamics_close(&runtime->view.dynamics, error))
        return false;
    ab_string_map_close(runtime->view.dead_blocks);
    ab_string_map_close(runtime->view.hats);
    ab_string_map_close(runtime->view.teleporters);
    ab_classic_bird_collisions_close(runtime->view.birds);
    ab_classic_block_collisions_close(runtime->view.blocks);
    ab_classic_block_scripts_close(runtime->view.scripts);
    ab_classic_scores_close(runtime->view.scores);
    ab_floating_scores_close(runtime->view.floating);
    free(runtime->destruction_entries);
    free(runtime->collision_entries);
    free(runtime->bird_scores);
    free(runtime->physical_values);
    free(runtime->contacts);
    free(runtime);
    *pointer = NULL;
    return true;
}

AbClassicWorldRuntimeView
ab_classic_world_runtime_view(AbClassicWorldRuntime *runtime) {
    return runtime ? runtime->view : (AbClassicWorldRuntimeView){0};
}
