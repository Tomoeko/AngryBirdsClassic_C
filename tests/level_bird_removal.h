#ifndef AB_TEST_LEVEL_BIRD_REMOVAL_H
#define AB_TEST_LEVEL_BIRD_REMOVAL_H

/* The replay compares native numeric/body/controller outputs. These script,
 * camera, presentation and audio receivers remain controlled inputs. Particle
 * descriptors use the real first-party definition/material property lookup. */
static size_t impact_bird_script(const LevelImpact *impact, AbDataValue script) {
    for (size_t index = 0; script.kind == AB_DATA_TABLE && index < 23; ++index) {
        if (script.table == impact->removal.scripts + index) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool impact_bird_destroy(void *context, AbDataValue script, AbError *error) {
    LevelImpact *impact = context;
    size_t index = impact_bird_script(impact, script);
    CHECK(index < 23);
    return impact_removal_notice(impact, impact->removal.values[index], error);
}

static bool impact_bird_definition(void *context, AbDataValue name, AbDataValue *output,
                                   AbError *error) {
    CHECK(name.kind == AB_DATA_STRING);
    return impact_removal_definition(context, name.string, output, error);
}

static bool impact_bird_hook(void *context, AbDataValue key, AbDataValue object,
                             AbError *error) {
    CHECK(key.kind == AB_DATA_STRING);
    return impact_removal_notice(context, object, error);
}

static bool impact_bird_particles(void *context, AbDataValue script,
                                  AbDataValue *output, AbError *error) {
    LevelImpact *impact = context;
    size_t index = impact_bird_script(impact, script);
    CHECK(index < 23);
    return impact_removal_property(impact, impact->removal.values[index],
                                   span("particles"), output, error);
}

static bool impact_bird_emit(void *context, AbDataValue name, AbDataValue descriptor,
                             bool first, bool second, AbError *error) {
    CHECK(name.kind == AB_DATA_STRING);
    return impact_removal_new_particles(context, name.string, descriptor, first, second,
                                        error);
}

static bool impact_bird_camera(void *context, AbDataValue receiver, AbDataValue object,
                               AbError *error) {
    LevelImpact *impact = context;
    CHECK(receiver.kind == AB_DATA_TABLE && receiver.table == &impact->removal.camera);
    return impact_removal_notice(context, object, error);
}

static bool impact_bird_volume(void *context, AbDataValue receiver, AbDataValue object,
                               AbDataValue *output, AbError *error) {
    LevelImpact *impact = context;
    CHECK(receiver.kind == AB_DATA_TABLE && receiver.table == &impact->removal.effect);
    CHECK(impact_removal_notice(context, object, error));
    *output = number(ab_level_effect_base_bird_volume());
    return true;
}

static bool impact_bird_physical(void *context, AbDataValue name, AbError *error) {
    CHECK(name.kind == AB_DATA_STRING);
    return impact_removal_physical(context, name.string, error);
}

static bool impact_bird_open(LevelImpact *impact) {
    LevelImpactRemoval *removal = &impact->removal;
    removal->bird_table = (AbDataTable){removal->bird_fields, 0, 23, NULL};
    removal->other_bird_table = (AbDataTable){removal->other_bird_fields, 0, 23, NULL};
    for (size_t index = 0; index < 23; ++index) {
        if (removal->physical[index].collider == AB_COLLIDER_BIRD ||
            removal->physical[index].collider == AB_COLLIDER_EAGLE) {
            removal->bird_fields[removal->bird_table.count++] = (AbDataEntry){
                string(impact->records[index].name), removal->values[index]};
        }
    }
    removal->bird_state = (AbBirdRemoveState){
        .objects = removal->state.objects,
        .birds = impact_removal_table(&removal->bird_table),
        .other_birds = impact_removal_table(&removal->other_bird_table),
        .hats = impact_removal_table(&removal->hats),
        .camera = impact_removal_table(&removal->camera),
        .level_effect = impact_removal_table(&removal->effect)};
    removal->bird_host = (AbBirdRemoveHost){impact,
                                            impact_bird_destroy,
                                            impact_bird_definition,
                                            impact_bird_hook,
                                            impact_bird_particles,
                                            impact_bird_emit,
                                            impact_bird_camera,
                                            impact_bird_volume,
                                            impact_removal_audio_name,
                                            impact_removal_audio,
                                            impact_bird_physical};
    return true;
}

static bool impact_remove_bird(LevelImpact *impact, size_t index, size_t current,
                               AbError *error) {
    CHECK(index < 23 && current < 23);
    LevelImpactRemoval *removal = &impact->removal;
    removal->bird_state.objects = removal->state.objects;
    removal->bird_state.current_bird_name =
        removal->body_fields[current][IMPACT_NAME].value;
    removal->bird_state.flying_bird = removal->values[index];
    if (!ab_bird_remove(removal->values[index], (AbDataValue){0}, &removal->bird_state,
                        &removal->bird_host, error)) {
        return false;
    }
    const AbDataValue *bird =
        data_field(&removal->bird_table, impact->records[index].name);
    CHECK(bird && bird->kind == AB_DATA_NIL && removal->removed[index]);
    return true;
}

#endif
