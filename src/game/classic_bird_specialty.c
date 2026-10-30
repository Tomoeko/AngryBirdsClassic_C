#include "ab/classic_bird_specialty.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataEntry entry;
    AbDataTable table;
} SpecialtyEvent;

struct AbClassicBirdSpecialty {
    AbClassicBirdSpecialtyConfig config;
    AbClassicLevelView level;
    SpecialtyEvent *events;
    size_t used, bytes;
    bool calling, failed;
};

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

static bool named(AbDataValue value, const char *name) {
    return value.kind == AB_DATA_STRING && value.string.size == strlen(name) &&
           (!value.string.size ||
            (value.string.data && !memcmp(value.string.data, name, value.string.size)));
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool definition(AbClassicBirdSpecialty *owner, size_t object,
                       AbDataValue *output, AbError *error) {
    return ab_object_definition_lookup(&owner->level.properties->lookup,
                                       owner->level.bodies->objects[object].name,
                                       output, error) &&
           (output->kind == AB_DATA_TABLE ||
            ab_fail(error, "Missing ordinary specialty definition"));
}

static bool red_callback(AbClassicBirdSpecialty *owner, size_t object, AbError *error) {
    AbClassicBirdSpecialtyConfig *config = &owner->config;
    AbSpan path = span("redBirdFingerTutorial");
    AbUiFrameNode *tutorial = NULL;
    if (!ab_ui_frame_find_child(config->notifications, &path, 1, &tutorial, error))
        return false;
    AbDataTable *physical = &owner->level.properties->objects[object].physical;
    if (tutorial || truthy(field(physical, "forcedTargeting")))
        return ab_fail(error, "Active red tutorial/forced targeting is not bound");
    if (truthy(*config->red_available) && !truthy(field(physical, "inBubble")) &&
        !truthy(field(physical, "vacuumed")))
        return ab_fail(error, "Active red specialty is not bound");
    AbDataValue current = {0}, selected;
    if (!definition(owner, object, &current, error) ||
        !ab_audio_bank_name_value(config->audio, field(current.table, "specialSound"),
                                  &selected, error))
        return false;
    uint32_t handle;
    return ab_audio_bank_play(
        config->audio,
        (AbAudioPlay){.name = selected,
                      .volume = {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)},
                      .loop = {.kind = AB_DATA_BOOLEAN}},
        &handle, error);
}

static bool notify(AbClassicBirdSpecialty *owner, AbError *error) {
    if (owner->used == owner->config.notifications_capacity)
        return ab_fail(error, "Specialty notification capacity exhausted");
    SpecialtyEvent *event = owner->events + owner->used++;
    event->entry = (AbDataEntry){text("id"), text("EID_BIRD_SPECIALTY_TRIGGERED")};
    event->table = (AbDataTable){&event->entry, 1, 1, NULL};
    return ab_event_manager_notify(owner->config.events, &event->table, error);
}

static bool trigger(AbClassicBirdSpecialty *owner, AbDataValue bird, float seconds,
                    AbError *error) {
    (void)seconds;
    AbClassicBirdSpecialtyConfig *config = &owner->config;
    AbClassicGameGlobals *globals = ab_classic_shots_globals(config->shots);
    globals->specialty_available = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    ab_classic_shots_state(config->shots)->specialty_available = false;
    if (bird.kind == AB_DATA_NIL)
        return true;
    size_t object = 0;
    while (object < owner->level.bodies->count &&
           (bird.kind != AB_DATA_TABLE ||
            bird.table != &owner->level.properties->objects[object].physical))
        ++object;
    if (object == owner->level.bodies->count || owner->level.removed_objects[object])
        return ab_fail(error, "Unknown or removed specialty bird identity");
    AbDataValue source = {0};
    if (!definition(owner, object, &source, error))
        return false;
    AbDataValue selected = field(source.table, "specialty");
    globals->bird_specialty = selected;
    if (!ab_level_property_set(
            owner->level.properties, object, span("specialtyTriggered"),
            (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error))
        return false;
    if (!named(selected, "BUBBLES") &&
        !ab_level_property_set(owner->level.properties, object, span("inBubble"),
                               (AbDataValue){.kind = AB_DATA_BOOLEAN}, error))
        return false;
    if (truthy(field(source.table, "hasExtraTutorial")) &&
        !ab_settings_statistics_extra_tutorial(config->settings,
                                               field(source.table, "sprite"), error))
        return false;
    bool bomb = named(selected, "BOMB");
    bool split = named(selected, "CLUSTER_BOMB");
    if (bomb) {
        AbDataValue current = {0}, sound = {0};
        if (!ab_settings_statistics_extra_tutorial(
                config->settings, field(source.table, "sprite"), error) ||
            !definition(owner, object, &current, error) ||
            !ab_audio_bank_name_value(
                config->audio, field(current.table, "specialSound"), &sound, error) ||
            !ab_classic_explosions_make(config->explosions, object, source, sound,
                                        (AbDataValue){0}, error))
            return false;
    }
    if (split) {
        AbClassicBluesSplitResult created;
        if (!ab_classic_blues_split_create(config->blues, object, &created, error))
            return false;
        if (config->blues_created &&
            !config->blues_created(config->blues_context, &created, error))
            return false;
    }
    if (named(selected, "BOOST") &&
        !ab_classic_bird_boost(&config->boost, object, source.table, error))
        return false;
    if (named(selected, "BOOMERANG") &&
        !ab_classic_boomerang_trigger(config->boomerang, object, source.table, error))
        return false;
    if (named(selected, "GRENADE") &&
        !ab_classic_eggs_trigger(config->eggs, object, source.table, error))
        return false;
    if (named(selected, "BUBBLES") && !truthy(field(bird.table, "inBubble"))) {
        if (!config->bubbles)
            return ab_fail(error, "Active BUBBLES specialty branch is not bound");
        if (!ab_settings_statistics_extra_tutorial(
                config->settings, field(source.table, "sprite"), error) ||
            !config->bubbles(config->bubbles_context, object, error))
            return false;
    }
    AbDataValue hook = field(source.table, "onTriggerSpecialty");
    if (truthy(hook)) {
        if (named(hook, "triggerRedBirdSpecialty")) {
            if (!red_callback(owner, object, error))
                return false;
        } else if (config->hook) {
            if (!config->hook(config->hook_context, hook, object, error))
                return false;
        } else {
            return hook.kind == AB_DATA_STRING
                       ? ab_fail(error, "Unresolved ordinary specialty hook: %.*s",
                                 (int)(hook.string.size < 128 ? hook.string.size : 128),
                                 hook.string.data)
                       : ab_fail(error, "Unresolved ordinary specialty hook type %u",
                                 (unsigned)hook.kind);
        }
    }
    if (!notify(owner, error))
        return false;
    AbDataValue script = field(bird.table, "gameObjectScript");
    if (script.kind != AB_DATA_TABLE ||
        script.table != &owner->level.properties->objects[object].script)
        return ab_fail(error, "Rebound specialty GameObject script");
    return ab_classic_behaviors_specialty(config->behaviors, object, error) &&
           (!(bomb || split) ||
            ab_classic_birds_remove(bomb ? config->birds : config->blues->config.birds,
                                    object, (AbDataValue){0}, error));
}

bool ab_classic_bird_specialty_open(AbClassicBirdSpecialtyConfig config,
                                    AbClassicBirdSpecialty **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing ordinary bird-specialty output");
    *output = NULL;
    AbClassicLevelView level =
        ab_classic_level_view(ab_classic_shots_level(config.shots));
    if (!level.bodies || !level.properties || !ab_classic_shots_globals(config.shots) ||
        !config.behaviors || !config.audio || !config.events || !config.notifications ||
        !config.flying || !config.red_available ||
        (config.blues &&
         (!config.blues->initialized ||
          config.blues->config.playfield.shots != config.shots ||
          config.blues->config.playfield.behaviors != config.behaviors ||
          config.blues->config.flying != config.flying)) ||
        config.storage_bytes < sizeof(AbClassicBirdSpecialty) ||
        config.notifications_capacity >
            (config.storage_bytes - sizeof(AbClassicBirdSpecialty)) /
                sizeof(SpecialtyEvent))
        return ab_fail(error, "Invalid ordinary bird-specialty bindings");
    AbClassicBirdSpecialty *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate ordinary bird specialty");
    owner->config = config;
    owner->level = level;
    owner->bytes =
        sizeof(*owner) + config.notifications_capacity * sizeof(SpecialtyEvent);
    if (config.notifications_capacity)
        owner->events = calloc(config.notifications_capacity, sizeof(*owner->events));
    if (config.notifications_capacity && !owner->events) {
        ab_classic_bird_specialty_close(owner);
        return ab_fail(error, "Cannot allocate retained specialty notifications");
    }
    *output = owner;
    return true;
}

void ab_classic_bird_specialty_close(AbClassicBirdSpecialty *owner) {
    if (owner) {
        free(owner->events);
        free(owner);
    }
}

size_t ab_classic_bird_specialty_bytes(const AbClassicBirdSpecialty *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_bird_specialty_trigger(AbClassicBirdSpecialty *owner, AbDataValue bird,
                                       float seconds, AbError *error) {
    if (!owner || owner->calling || owner->failed || owner->level.world->locked ||
        owner->level.world->failed)
        return ab_fail(error, "Invalid, failed or reentrant ordinary bird specialty");
    owner->calling = true;
    bool okay = trigger(owner, bird, seconds, error);
    owner->calling = false;
    owner->failed = !okay;
    return okay;
}

bool ab_classic_bird_specialty_trigger_flying(AbClassicBirdSpecialty *owner,
                                              float seconds, AbError *error) {
    return owner ? ab_classic_bird_specialty_trigger(owner, *owner->config.flying,
                                                     seconds, error)
                 : ab_fail(error, "Missing ordinary flying-specialty binding");
}
