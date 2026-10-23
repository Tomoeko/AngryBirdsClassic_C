#include "ab/classic_birds.h"
#include "classic_shots_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbDataEntry field;
    AbDataTable table;
} HatView;

struct AbClassicBirds {
    AbClassicBirdConfig config;
    AbClassicLevelView level;
    AbBirdRemoveState state;
    AbBirdRemoveHost host;
    AbDataValue current_published, flying_published, specialty_published;
    AbDataTable birds, hats, camera;
    AbDataTable *other_tables;
    size_t other_count, other_capacity;
    AbDataEntry *bird_entries, *other_entries, *hat_entries;
    HatView *hat_views;
    size_t count, capacity, bytes;
    bool calling, bound;
};

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static AbDataValue text(AbSpan name) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = name};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue field(const AbDataTable *value, const char *name) {
    const AbDataValue *found = ab_data_get(value, text(span(name)));
    return found ? *found : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool same_span(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || (first.data && second.data &&
                            !memcmp(first.data, second.data, first.size)));
}

static bool same_value(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    if (first.kind == AB_DATA_NIL)
        return true;
    if (first.kind == AB_DATA_STRING)
        return same_span(first.string, second.string);
    if (first.kind == AB_DATA_TABLE)
        return first.table == second.table;
    return first.bits == second.bits;
}

static bool index_of(AbClassicBirds *owner, AbDataValue value, bool script,
                     size_t *output, AbError *error) {
    for (size_t index = 0; value.kind == AB_DATA_TABLE && index < owner->count;
         ++index) {
        AbLevelPropertyObject *entry = owner->level.properties->objects + index;
        if (value.table == (script ? &entry->script : &entry->physical)) {
            *output = index;
            return true;
        }
    }
    return ab_fail(error, "Foreign owned bird lifecycle receiver");
}

static bool hat_views(AbClassicBirds *owner, AbError *error) {
    size_t cursor = 0;
    owner->hats.count = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(owner->config.hats, &cursor, &entry, error))
            return false;
        if (!entry.present)
            return true;
        const AbBirdCollisionHat *hat = (const AbBirdCollisionHat *)entry.value;
        if (!hat || (hat->object_name.size && !hat->object_name.data) ||
            owner->hats.count == owner->capacity)
            return ab_fail(error, "Unsupported owned bird hat registry");
        size_t index = owner->hats.count++;
        HatView *view = owner->hat_views + index;
        view->field = (AbDataEntry){text(span("hat")), text(hat->object_name)};
        view->table = (AbDataTable){&view->field, 1, 1, NULL};
        owner->hat_entries[index] = (AbDataEntry){text(entry.key), table(&view->table)};
    }
}

static bool flying_index(AbClassicBirds *owner, AbDataValue value, size_t *output,
                         AbError *error) {
    if (value.kind == AB_DATA_NIL) {
        *output = SIZE_MAX;
        return true;
    }
    return index_of(owner, value, false, output, error);
}

static bool refresh(AbClassicBirds *owner, AbError *error) {
    AbShotState *shot = ab_classic_shots_state(owner->config.shots);
    owner->state.current_bird_name = shot->current_bird_name
                                         ? text(span(shot->current_bird_name))
                                         : (AbDataValue){0};
    owner->state.flying_bird = *owner->config.flying_object;
    owner->state.specialty_available = *owner->config.specialty_available;
    owner->state.level_effect = *owner->config.level_effect;
    owner->current_published = owner->state.current_bird_name;
    owner->flying_published = owner->state.flying_bird;
    owner->specialty_published = owner->state.specialty_available;
    size_t index = SIZE_MAX;
    if (!flying_index(owner, owner->state.flying_bird, &index, error))
        return false;
    shot->flying =
        index == SIZE_MAX ? NULL : ab_classic_shots_bird(owner->config.shots, index);
    shot->specialty_available = truthy(owner->state.specialty_available);
    return ab_classic_camera_set_flying(owner->config.camera, index, error) &&
           hat_views(owner, error);
}

static bool publish_current(AbClassicBirds *owner, AbDataValue value, AbError *error) {
    AbShotState *shot = ab_classic_shots_state(owner->config.shots);
    if (value.kind == AB_DATA_NIL) {
        shot->current_bird_name = NULL;
        return true;
    }
    if (value.kind != AB_DATA_STRING)
        return ab_fail(error, "Unsupported owned current bird identity");
    for (size_t index = 0; index < owner->count; ++index) {
        AbShotBird *bird = ab_classic_shots_bird(owner->config.shots, index);
        if (bird && same_span(value.string, span(bird->name))) {
            shot->current_bird_name = bird->name;
            return true;
        }
    }
    return ab_fail(error, "Unknown owned current bird name");
}

static bool publish(AbClassicBirds *owner, AbError *error) {
    if (!same_value(owner->state.current_bird_name, owner->current_published) &&
        !publish_current(owner, owner->state.current_bird_name, error))
        return false;
    if (!same_value(owner->state.flying_bird, owner->flying_published))
        *owner->config.flying_object = owner->state.flying_bird;
    if (!same_value(owner->state.specialty_available, owner->specialty_published))
        *owner->config.specialty_available = owner->state.specialty_available;
    return refresh(owner, error);
}

static bool finish(AbClassicBirds *owner, bool okay, AbError *error) {
    return okay && refresh(owner, error);
}

static bool destroy(void *context, AbDataValue script, AbError *error) {
    AbClassicBirds *owner = context;
    size_t index = SIZE_MAX;
    return publish(owner, error) && index_of(owner, script, true, &index, error) &&
           finish(owner,
                  ab_classic_behaviors_destroy(owner->config.behaviors, index, error),
                  error);
}

static bool definition(void *context, AbDataValue name, AbDataValue *output,
                       AbError *error) {
    AbClassicBirds *owner = context;
    if (!publish(owner, error))
        return false;
    if (name.kind != AB_DATA_STRING)
        return ab_fail(error, "Invalid owned bird removal definition key");
    return ab_object_definition_lookup(&owner->level.properties->lookup, name.string,
                                       output, error);
}

static bool hook(void *context, AbDataValue name, AbDataValue object, AbError *error) {
    AbClassicBirds *owner = context;
    size_t index = SIZE_MAX;
    if (!publish(owner, error) || !index_of(owner, object, false, &index, error))
        return false;
    return owner->config.removed_hook
               ? finish(owner,
                        owner->config.removed_hook(owner->config.context, name, index,
                                                   error),
                        error)
               : ab_fail(error, "Missing actual named bird removal hook");
}

static bool particles(void *context, AbDataValue script, AbDataValue *output,
                      AbError *error) {
    AbClassicBirds *owner = context;
    size_t index = SIZE_MAX;
    return publish(owner, error) && index_of(owner, script, true, &index, error) &&
           ab_level_property_get(owner->level.properties, index, span("particles"),
                                 output, error);
}

static bool emit(void *context, AbDataValue name, AbDataValue descriptor, bool first,
                 bool second, AbError *error) {
    AbClassicBirds *owner = context;
    if (!publish(owner, error))
        return false;
    if (name.kind != AB_DATA_STRING)
        return ab_fail(error, "Invalid owned bird removal particle key");
    return finish(owner,
                  ab_classic_particles_removal(owner->config.particles, name.string,
                                               descriptor, first, second, error),
                  error);
}

static bool camera(void *context, AbDataValue receiver, AbDataValue object,
                   AbError *error) {
    AbClassicBirds *owner = context;
    size_t index = SIZE_MAX;
    if (!publish(owner, error) || !index_of(owner, object, false, &index, error))
        return false;
    if (receiver.kind != AB_DATA_TABLE || receiver.table != &owner->camera)
        return ab_fail(error, "Foreign owned bird removal camera");
    return finish(owner,
                  ab_classic_camera_remove_bird(owner->config.camera, index, error),
                  error);
}

static bool volume(void *context, AbDataValue receiver, AbDataValue object,
                   AbDataValue *output, AbError *error) {
    AbClassicBirds *owner = context;
    size_t index = SIZE_MAX;
    if (!publish(owner, error) || !index_of(owner, object, false, &index, error))
        return false;
    return owner->config.removal_volume
               ? finish(owner,
                        owner->config.removal_volume(owner->config.context, receiver,
                                                     index, output, error),
                        error)
               : ab_fail(error, "Missing actual bird removal volume policy");
}

static bool audio_name(void *context, AbDataValue name, AbDataValue *output,
                       AbError *error) {
    AbClassicBirds *owner = context;
    return publish(owner, error) &&
           ab_audio_bank_name_value(owner->config.audio, name, output, error);
}

static bool audio(void *context, AbAudioPlay request, AbError *error) {
    AbClassicBirds *owner = context;
    uint32_t handle = 0;
    return publish(owner, error) &&
           ab_audio_bank_play(owner->config.audio, request, &handle, error);
}

static bool remove_object(void *context, AbDataValue name, AbError *error) {
    AbClassicBirds *owner = context;
    if (!publish(owner, error))
        return false;
    if (name.kind != AB_DATA_STRING)
        return ab_fail(error, "Invalid owned physical bird removal key");
    return finish(
        owner,
        ab_classic_object_removal_call(owner->config.physical, name.string, error),
        error);
}

bool ab_classic_birds_open(AbClassicBirdConfig config, AbClassicBirds **output,
                           AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned birds output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    size_t count = level.bodies ? level.bodies->count : 0;
    size_t capacity = config.object_capacity ? config.object_capacity : count;
    size_t entries = config.other_entries ? config.other_entries : capacity;
    size_t row_bytes = sizeof(AbDataEntry) * 2 + sizeof(HatView);
    if (!count || !level.properties || !config.shots || !config.behaviors ||
        !config.camera || !config.physical || !config.particles || !config.audio ||
        !config.hats || !config.flying_object || !config.specialty_available ||
        !config.level_effect || !config.remove_speed_squared || !config.remove_time ||
        ab_classic_shots_level(config.shots) != config.level ||
        config.storage_bytes < sizeof(AbClassicBirds) || capacity < count ||
        capacity > level.bodies->capacity || !entries ||
        entries > UINT32_C(0x1000000) ||
        capacity > (config.storage_bytes - sizeof(AbClassicBirds)) / row_bytes ||
        (!config.other_generations && level.bird_count > (SIZE_MAX - 1) / 2))
        return ab_fail(error, "Invalid owned bird lifecycle bindings/storage");
    size_t remaining =
        config.storage_bytes - sizeof(AbClassicBirds) - capacity * row_bytes;
    /* Every queued launch replaces otherBirds, and every original Blues may
     * replace it again when splitting. Keep both fresh tables and old aliases. */
    size_t generations =
        config.other_generations ? config.other_generations : 2 * level.bird_count + 1;
    if (generations > remaining / sizeof(AbDataTable))
        return ab_fail(error, "Owned other-bird table generations exceed storage");
    remaining -= generations * sizeof(AbDataTable);
    if (generations > remaining / sizeof(AbDataEntry) / entries)
        return ab_fail(error, "Owned other-bird entry generations exceed storage");
    AbClassicBirds *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate owned bird lifecycle");
    owner->config = config;
    owner->level = level;
    owner->count = count;
    owner->capacity = capacity;
    owner->bytes = sizeof(*owner) + capacity * row_bytes +
                   generations * (sizeof(AbDataTable) + entries * sizeof(AbDataEntry));
    owner->other_capacity = generations;
    owner->other_count = 1;
    owner->bird_entries = calloc(capacity, sizeof(*owner->bird_entries));
    owner->other_entries = calloc(entries * generations, sizeof(*owner->other_entries));
    owner->other_tables = calloc(generations, sizeof(*owner->other_tables));
    owner->hat_entries = calloc(capacity, sizeof(*owner->hat_entries));
    owner->hat_views = calloc(capacity, sizeof(*owner->hat_views));
    if (!owner->bird_entries || !owner->other_entries || !owner->hat_entries ||
        !owner->hat_views || !owner->other_tables) {
        ab_classic_birds_close(owner);
        return ab_fail(error, "Cannot allocate owned bird lifecycle tables");
    }
    owner->birds = (AbDataTable){owner->bird_entries, 0, capacity, NULL};
    for (size_t index = 0; index < generations; ++index)
        owner->other_tables[index] =
            (AbDataTable){owner->other_entries + index * entries, 0, entries, NULL};
    owner->hats = (AbDataTable){owner->hat_entries, 0, capacity, NULL};
    for (size_t order = 0; order < level.bird_count; ++order) {
        size_t index = level.bird_order[order];
        if (index >= count) {
            ab_classic_birds_close(owner);
            return ab_fail(error, "Invalid owned initial bird order");
        }
        if (!level.removed_objects[index])
            owner->bird_entries[owner->birds.count++] =
                (AbDataEntry){text(level.bodies->objects[index].name),
                              table(&level.properties->objects[index].physical)};
    }
    owner->state = (AbBirdRemoveState){.objects = table(&level.properties->registry),
                                       .birds = table(&owner->birds),
                                       .other_birds = table(owner->other_tables),
                                       .hats = table(&owner->hats),
                                       .camera = table(&owner->camera)};
    owner->host =
        (AbBirdRemoveHost){owner,  destroy, definition, hook,  particles,    emit,
                           camera, volume,  audio_name, audio, remove_object};
    if (!ab_classic_shots_bind_birds(config.shots, owner, error)) {
        ab_classic_birds_close(owner);
        return false;
    }
    owner->bound = true;
    *output = owner;
    return true;
}

void ab_classic_birds_close(AbClassicBirds *owner) {
    if (owner) {
        if (owner->bound)
            ab_classic_shots_bind_birds(owner->config.shots, NULL, NULL);
        free(owner->other_tables);
        free(owner->hat_views);
        free(owner->hat_entries);
        free(owner->other_entries);
        free(owner->bird_entries);
        free(owner);
    }
}

size_t ab_classic_birds_bytes(const AbClassicBirds *owner) {
    return owner ? owner->bytes : 0;
}

AbClassicBirdView ab_classic_birds_view(AbClassicBirds *owner) {
    return owner ? (AbClassicBirdView){.birds = &owner->birds,
                                       .other_birds =
                                           owner->other_tables + owner->other_count - 1,
                                       .birds_field = &owner->state.birds,
                                       .other_birds_field = &owner->state.other_birds}
                 : (AbClassicBirdView){0};
}

bool ab_classic_birds_register_object(AbClassicBirds *owner, size_t object,
                                      AbError *error) {
    if (!owner || owner->calling || object != owner->count ||
        object >= owner->capacity || object >= owner->level.bodies->count ||
        object >= owner->level.properties->count || owner->level.world->failed ||
        owner->level.world->locked || owner->level.removed_objects[object] ||
        !ab_classic_shots_bird(owner->config.shots, object) ||
        object >= ab_classic_camera_view(owner->config.camera).object_count)
        return ab_fail(error, "Invalid new canonical bird identity");
    const AbLevelObject *source = owner->level.bodies->objects + object;
    AbGameObjectCoreRegistry *physical =
        ab_classic_object_removal_registry(owner->config.physical);
    uintptr_t handle = 0;
    bool present = false;
    if (!physical || physical->world != owner->level.world ||
        !ab_string_map_get(physical->objects, source->name, &handle, &present, error) ||
        !present || !handle)
        return ab_fail(error, "Unregistered native bird identity");
    ++owner->count;
    return true;
}

bool ab_classic_birds_sync(AbClassicBirds *owner, AbError *error) {
    return owner && !owner->calling
               ? refresh(owner, error)
               : ab_fail(error, "Invalid or reentrant bird synchronization");
}

bool ab_classic_birds_replace_other(AbClassicBirds *owner, const AbDataValue *names,
                                    size_t count, AbError *error) {
    if (!owner || owner->calling || owner->other_count == owner->other_capacity ||
        (count && !names) || count > owner->other_tables[owner->other_count].capacity)
        return ab_fail(error, "Invalid or exhausted other-bird table replacement");
    for (size_t index = 0; index < count; ++index)
        if (names[index].kind != AB_DATA_STRING ||
            (names[index].string.size && !names[index].string.data))
            return ab_fail(error, "Other-bird constructor requires string names");
    AbDataTable *next = owner->other_tables + owner->other_count;
    for (size_t index = 0; index < count; ++index)
        next->entries[index] = (AbDataEntry){number((float)(index + 1)), names[index]};
    next->count = count;
    owner->state.other_birds = table(next);
    ++owner->other_count;
    return true;
}

bool ab_classic_birds_reset_other(AbClassicBirds *owner, AbError *error) {
    return ab_classic_birds_replace_other(owner, NULL, 0, error);
}

static bool remove_bird(AbClassicBirds *owner, size_t object, AbDataValue play_sound,
                        AbError *error) {
    if (!refresh(owner, error))
        return false;
    bool okay =
        ab_bird_remove(table(&owner->level.properties->objects[object].physical),
                       play_sound, &owner->state, &owner->host, error);
    return publish(owner, error) && okay;
}

bool ab_classic_birds_remove(AbClassicBirds *owner, size_t object,
                             AbDataValue play_sound, AbError *error) {
    if (!owner || owner->calling || object >= owner->count)
        return ab_fail(error, "Invalid or reentrant owned bird removal");
    owner->calling = true;
    bool okay = remove_bird(owner, object, play_sound, error);
    owner->calling = false;
    return okay;
}

static bool numeric(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric owned bird retirement field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite owned bird retirement field");
}

static bool write_timer(AbClassicBirds *owner, size_t object, const char *name,
                        float value, AbError *error) {
    AbObjectPropertyAccess access = {
        .physics_object = table(&owner->level.properties->objects[object].physical)};
    return ab_object_property_set(&access, span(name), number(value), error);
}

static bool retirement(AbClassicBirds *owner, size_t index, float seconds, bool *alive,
                       AbError *error) {
    AbDataTable *object = &owner->level.properties->objects[index].physical;
    if (truthy(field(object, "isMightyBait")) ||
        truthy(field(object, "isMightyEagle"))) {
        *alive = true;
        return true;
    }
    AbDataValue shot = field(object, "shot");
    if (shot.kind != AB_DATA_BOOLEAN || !shot.bits) {
        *alive = true;
        return true;
    }
    AbDataValue definition;
    AbSpan name = owner->level.bodies->objects[index].name;
    if (!ab_object_definition_lookup(&owner->level.properties->lookup, name,
                                     &definition, error))
        return false;
    if (definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Missing owned bird retirement definition");
    if (truthy(field(definition.table, "manuallyRemovedAfterCollision"))) {
        *alive = true;
        return true;
    }
    AbBirdRetirementState state = {0};
    AbDataValue ready = field(object, "isReadyForRemoveTimer");
    if (ready.kind != AB_DATA_NIL) {
        if (!numeric(ready, &state.ready_timer, error))
            return false;
        state.ready_timer_present = true;
    }
    bool water_active = truthy(field(object, "waterTimerIsActive"));
    AbDataValue water = field(object, "waterTimer");
    if ((water_active && truthy(water)) ||
        (!water_active && water.kind != AB_DATA_NIL)) {
        if (!numeric(water, &state.water_timer, error))
            return false;
        state.water_timer_present = true;
    }
    float speed = 0;
    if (state.ready_timer_present) {
        float x = 0, y = 0;
        if (!numeric(field(object, "xVel"), &x, error) ||
            !numeric(field(object, "yVel"), &y, error))
            return false;
        speed = x * x + y * y;
    }
    AbBirdRetirementInput input = {
        .seconds = seconds,
        .speed_squared = speed,
        .speed_threshold = *owner->config.remove_speed_squared,
        .reset_timer = *owner->config.remove_time,
        .controllable = true,
        .shot_literal_true = true,
        .water_active = water_active,
        .outside_boundaries = truthy(field(object, "outsideBoundaries"))};
    AbBirdRetirementFrame frame;
    if (!ab_bird_retirement_frame(&state, &input, &frame))
        return ab_fail(error, "Unsupported owned bird retirement arithmetic");
    if (!write_timer(owner, index, "isReadyForRemoveTimer", frame.next.ready_timer,
                     error) ||
        (water_active &&
         !write_timer(owner, index, "waterTimer", frame.next.water_timer, error)))
        return false;
    if (frame.query_camera_target) {
        input.camera_targets_bird =
            ab_classic_camera_view(owner->config.camera).target_object == index;
        if (!ab_bird_retirement_frame(&state, &input, &frame))
            return ab_fail(error, "Invalid owned bird retirement camera gate");
    }
    if (frame.remove_bird && !remove_bird(owner, index, (AbDataValue){0}, error))
        return false;
    *alive = !frame.remove_bird;
    return true;
}

static bool bomb_timer(AbClassicBirds *owner, size_t object, float seconds, bool *alive,
                       AbError *error) {
    const AbDataTable *physical = &owner->level.properties->objects[object].physical;
    float timer = 0;
    if (!numeric(field(physical, "bombTimer"), &timer, error))
        return false;
    timer = timer - seconds;
    if (!write_timer(owner, object, "bombTimer", timer, error))
        return false;
    AbDataValue current = {0};
    if (timer < 1 && (!ab_object_definition_lookup(
                          &owner->level.properties->lookup,
                          owner->level.bodies->objects[object].name, &current, error) ||
                      current.kind != AB_DATA_TABLE))
        return ab_fail(error, "Missing current timed-bomb definition");
    if (timer < 0) {
        AbDataValue sound = {0};
        if (!ab_audio_bank_name_value(owner->config.audio,
                                      text(span("special_explosion")), &sound, error) ||
            !ab_classic_explosions_make(owner->config.explosions, object, current,
                                        sound, (AbDataValue){0}, error) ||
            !remove_bird(owner, object, (AbDataValue){0}, error))
            return false;
        *alive = false;
        return true;
    }
    if (timer < 1) {
        AbDataValue sprite =
            field(current.table, timer < 0.5f ? "spriteSpecial3" : "spriteSpecial2");
        AbObjectPropertyAccess access = {
            .physics_object =
                table(&owner->level.properties->objects[object].physical)};
        if (!ab_object_property_set(&access, span("damageSprite"), sprite, error))
            return false;
        if (sprite.kind != AB_DATA_STRING)
            return ab_fail(error, "Missing actual timed-bomb sprite");
        if (!ab_classic_visuals_set_sprite(owner->config.explosions->visuals, object,
                                           sprite.string, error))
            return false;
    }
    *alive = true;
    return true;
}

bool ab_classic_birds_update(AbClassicBirds *owner, size_t object, float seconds,
                             bool *alive, AbError *error) {
    if (!owner || !alive || owner->calling || object >= owner->count)
        return ab_fail(error, "Invalid or reentrant owned bird update");
    if (owner->level.removed_objects[object]) {
        *alive = false;
        return true;
    }
    const AbDataTable *physical = &owner->level.properties->objects[object].physical;
    if (!truthy(field(physical, "controllable"))) {
        *alive = true;
        return true;
    }
    owner->calling = true;
    AbClassicBirdUpdate kind = AB_CLASSIC_BIRD_BOMB_TIMER;
    bool special = true;
    if (field(physical, "bombTimer").kind != AB_DATA_NIL &&
        !truthy(field(physical, "disableTimedSpecialty")))
        kind = AB_CLASSIC_BIRD_BOMB_TIMER;
    else if (field(physical, "pinkTimer").kind != AB_DATA_NIL)
        kind = AB_CLASSIC_BIRD_PINK_TIMER;
    else if (truthy(field(physical, "superseed_timer")))
        kind = AB_CLASSIC_BIRD_SUPERSEED;
    else
        special = false;
    bool okay;
    if (special && kind == AB_CLASSIC_BIRD_BOMB_TIMER && owner->config.explosions)
        okay = bomb_timer(owner, object, seconds, alive, error);
    else if (special)
        okay = owner->config.specialty_update
                   ? owner->config.specialty_update(owner->config.context, kind, object,
                                                    seconds, alive, error)
                   : ab_fail(error, "Missing actual timed bird specialty continuation");
    else
        okay = retirement(owner, object, seconds, alive, error);
    owner->calling = false;
    return okay;
}
