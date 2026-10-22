#include "ab/classic_dynamics.h"
#include "ab/classic_audio.h"
#include "ab/level_effect.h"

#include <stdlib.h>

struct AbClassicDynamics {
    AbClassicDynamicsConfig config;
    AbClassicDynamicsView view;
    AbGameContactRoutes routes;
    AbClassicContacts contacts;
    AbClassicPhysicsPhases phases;
    size_t bytes;
};

static bool force(void *context, size_t object, const AbObjectForceCall *call,
                  AbError *error) {
    AbClassicSimulationConfig *source =
        &((AbClassicDynamics *)context)->config.simulation;
    return source->force ? source->force(source->context, object, call, error)
                         : ab_fail(error, "Missing dynamics force service");
}

static bool phase(void *context, const AbClassicSimulationCall *call, AbError *error) {
    return ab_classic_physics_phases_call(&((AbClassicDynamics *)context)->phases, call,
                                          error);
}

static bool rolling(void *context, AbRollingAudioCall *call, AbError *error) {
    return ab_classic_audio_bank_rolling(((AbClassicDynamics *)context)->config.audio,
                                         call, error);
}

static bool destroyed_sound(void *context, size_t object, AbDataValue definition,
                            AbDataValue *output, AbError *error) {
    AbClassicDynamics *owner = context;
    if (owner->config.effect == AB_CLASSIC_DYNAMICS_BASE_EFFECT)
        return ab_level_effect_base_sound(AB_LEVEL_EFFECT_DESTROYED_SOUND, definition,
                                          output, error);
    AbClassicRemovalConfig *source = &owner->config.removals;
    return source->destroyed_sound
               ? source->destroyed_sound(source->context, object, definition, output,
                                         error)
               : ab_fail(error, "Missing selected custom destroyed-sound method");
}

static bool update_destroyed(void *context, size_t object, AbError *error) {
    return ab_classic_destruction_statistics_call(
        ((AbClassicDynamics *)context)->view.statistics, object, error);
}

static bool remove_pig(void *context, AbSpan name, AbError *error) {
    return ab_classic_goals_after_removal(((AbClassicDynamics *)context)->view.goals,
                                          name, error);
}

static bool rocket(void *context, AbDataValue name, AbError *error) {
    AbClassicRemovalConfig *source = &((AbClassicDynamics *)context)->config.removals;
    return source->rocket ? source->rocket(source->context, name, error)
                          : ab_fail(error, "Missing dynamics rocket service");
}

static bool explosion(void *context, AbDataValue object, AbDataValue definition,
                      AbDataValue sound, AbError *error) {
    AbClassicRemovalConfig *source = &((AbClassicDynamics *)context)->config.removals;
    return source->explosion
               ? source->explosion(source->context, object, definition, sound, error)
               : ab_fail(error, "Missing dynamics explosion service");
}

static bool gift(void *context, AbDataValue level, AbDataValue x, AbDataValue y,
                 AbError *error) {
    AbClassicRemovalConfig *source = &((AbClassicDynamics *)context)->config.removals;
    return source->collect_gift
               ? source->collect_gift(source->context, level, x, y, error)
               : ab_fail(error, "Missing dynamics gift service");
}

static bool functions(void *context, size_t object, AbUiPointerValue *output,
                      AbError *error) {
    AbClassicRemovalConfig *source = &((AbClassicDynamics *)context)->config.removals;
    return source->functions
               ? source->functions(source->context, object, output, error)
               : ab_fail(error, "Missing dynamics removal-function lookup");
}

static bool call(void *context, AbUiPointerValue function, size_t object,
                 AbError *error) {
    AbClassicRemovalConfig *source = &((AbClassicDynamics *)context)->config.removals;
    return source->call ? source->call(source->context, function, object, error)
                        : ab_fail(error, "Missing dynamics removal-function service");
}

static bool budget(AbClassicDynamicsConfig *config, size_t *output, AbError *error) {
    const size_t parts[] = {
        sizeof(AbClassicDynamics),        config->simulation.limits.storage_bytes,
        config->physical.storage_bytes,   config->goals.storage_bytes,
        config->statistics.storage_bytes, config->removals.storage_bytes};
    size_t bytes = 0;
    for (size_t index = 0; index < sizeof(parts) / sizeof(*parts); ++index) {
        if (!parts[index] || bytes > config->storage_bytes ||
            parts[index] > config->storage_bytes - bytes)
            return ab_fail(error, "Invalid or exhausted dynamics byte budget");
        bytes += parts[index];
    }
    *output = bytes;
    return true;
}

static bool bindings(AbClassicDynamicsConfig *config, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(config->level);
    if (!level.world || !level.bodies || !config->visuals || !config->behaviors ||
        ab_classic_visuals_view(config->visuals).level != config->level ||
        ab_classic_render_scene_level(config->scene) != config->level ||
        !config->audio || !config->settings || !config->events || !config->globals ||
        !config->random || !config->scripts || !config->contact_objects ||
        config->contact_count != level.bodies->count || config->simulation.phase ||
        config->simulation.activity.has_awake || config->simulation.activity.moving ||
        config->simulation.activity.above_tolerance || config->simulation.rolling ||
        config->physical.rolling_levels || config->contacts.routes ||
        config->contacts.glue || config->removals.update_destroyed ||
        config->removals.remove_pig ||
        (config->effect == AB_CLASSIC_DYNAMICS_BASE_EFFECT &&
         config->removals.destroyed_sound) ||
        (config->effect != AB_CLASSIC_DYNAMICS_BASE_EFFECT &&
         config->effect != AB_CLASSIC_DYNAMICS_CUSTOM_EFFECT))
        return ab_fail(error, "Invalid or conflicting dynamics bindings");
    return true;
}

static bool open_simulation(AbClassicDynamics *owner, AbError *error) {
    AbClassicDynamicsConfig *config = &owner->config;
    AbClassicSimulationConfig simulation = config->simulation;
    simulation.random = config->random;
    simulation.phase = phase;
    simulation.force = force;
    simulation.rolling = rolling;
    simulation.context = owner;
    simulation.activity = (AbClassicActivityBindings){
        &config->globals->has_awake_objects, &config->globals->has_moving_objects,
        &config->globals->has_moving_objects_above_tolerance};
    if (!ab_classic_simulation_open(config->level, simulation, &owner->view.simulation,
                                    error))
        return false;
    AbClassicObjectRemovalConfig physical = config->physical;
    physical.level = config->level;
    physical.visuals = config->visuals;
    physical.scene = config->scene;
    physical.rolling_levels =
        ab_classic_simulation_view(owner->view.simulation).post.rolling_levels;
    return ab_classic_object_removal_open(physical, &owner->view.physical, error);
}

static bool open_goals(AbClassicDynamics *owner, AbError *error) {
    AbClassicDynamicsConfig *config = &owner->config;
    AbClassicGameGlobals *globals = config->globals;
    AbClassicGoalConfig goals = config->goals;
    goals.level = config->level;
    goals.events = config->events;
    goals.time = &globals->time;
    goals.score = &globals->score;
    goals.last_score_time = &globals->last_score_time;
    goals.old_score = &globals->old_score;
    if (!ab_classic_goals_open(goals, &owner->view.goals, error))
        return false;
    AbClassicDestructionStatisticsConfig statistics = config->statistics;
    statistics.level = config->level;
    statistics.settings = config->settings;
    statistics.events = config->events;
    return ab_classic_destruction_statistics_open(statistics, &owner->view.statistics,
                                                  error);
}

static bool open_removals(AbClassicDynamics *owner, AbError *error) {
    AbClassicDynamicsConfig *config = &owner->config;
    AbClassicRemovalConfig removals = config->removals;
    removals.level = config->level;
    removals.behaviors = config->behaviors;
    removals.physical = owner->view.physical;
    removals.audio = config->audio;
    removals.level_goals = ab_classic_goals_map(owner->view.goals);
    removals.random = config->random;
    removals.destroyed_sound = destroyed_sound;
    removals.update_destroyed = update_destroyed;
    removals.remove_pig = remove_pig;
    removals.rocket = rocket;
    removals.explosion = explosion;
    removals.collect_gift = gift;
    removals.functions = functions;
    removals.call = call;
    removals.context = owner;
    return ab_classic_removals_open(removals, &owner->view.removals, error);
}

static bool open_contacts(AbClassicDynamics *owner, AbError *error) {
    AbClassicDynamicsConfig *config = &owner->config;
    if (!ab_game_contact_routes_initialize(&owner->routes) ||
        !ab_classic_physics_phases_initialize(
            (AbClassicPhysicsPhaseConfig){.level = config->level,
                                          .contacts = &owner->contacts,
                                          .removals = owner->view.removals,
                                          .physical = owner->view.physical,
                                          .scripts = config->scripts,
                                          .context = config->water_context,
                                          .water = config->water},
            &owner->phases, error))
        return false;
    AbClassicContactsConfig contacts = config->contacts;
    contacts.routes = &owner->routes;
    contacts.glue = ab_classic_simulation_view(owner->view.simulation).glue;
    if (!ab_classic_contacts_open(config->level, config->contact_objects,
                                  config->contact_count, contacts, &owner->contacts,
                                  error))
        return false;
    owner->view.contacts = &owner->contacts;
    owner->view.phases = &owner->phases;
    return true;
}

bool ab_classic_dynamics_open(AbClassicDynamicsConfig config,
                              AbClassicDynamics **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing dynamics output");
    *output = NULL;
    size_t bytes = 0;
    if (!bindings(&config, error) || !budget(&config, &bytes, error))
        return false;
    AbClassicDynamics *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate dynamics");
    owner->config = config;
    owner->bytes = bytes;
    if (!open_simulation(owner, error) || !open_goals(owner, error) ||
        !open_removals(owner, error) || !open_contacts(owner, error)) {
        ab_classic_dynamics_close(&owner, NULL);
        return false;
    }
    *output = owner;
    return true;
}

bool ab_classic_dynamics_close(AbClassicDynamics **handle, AbError *error) {
    if (!handle || !*handle)
        return true;
    AbClassicDynamics *owner = *handle;
    if (owner->phases.calling)
        return ab_fail(error, "Cannot close active dynamics physics phases");
    if (!ab_classic_contacts_close(&owner->contacts, error))
        return false;
    ab_classic_removals_close(owner->view.removals);
    ab_classic_destruction_statistics_close(owner->view.statistics);
    ab_classic_goals_close(owner->view.goals);
    ab_classic_object_removal_close(owner->view.physical);
    ab_classic_simulation_close(owner->view.simulation);
    free(owner);
    *handle = NULL;
    return true;
}

size_t ab_classic_dynamics_bytes(const AbClassicDynamics *owner) {
    return owner ? owner->bytes : 0;
}

AbClassicDynamicsView ab_classic_dynamics_view(AbClassicDynamics *owner) {
    return owner ? owner->view : (AbClassicDynamicsView){0};
}
