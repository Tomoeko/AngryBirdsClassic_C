#include "ab/bird_collision.h"
#include "ab/damage_particles.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird callback composition failed at %d: %s\n", __LINE__,  \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionObject objects[4];
    AbBlockCollisionDefinition notification[2];
    AbBirdCollisionSpriteDefinition definitions[3];
    AbBirdCollisionEffectDefinition effects[3];
    AbBirdCollisionHat hat;
    AbBirdCollisionScoreEntry score;
    AbBirdCollisionPreludeState prelude_state;
    AbBirdCollisionSpriteState sprite_state;
    AbBirdCollisionGrenades grenades;
    AbBirdCollisionObject *grenade_items[2];
    AbBirdCollisionNotifyHost notifications;
    AbBirdCollisionPreludeHost prelude;
    AbBirdCollisionSpriteHost sprites;
    AbBirdCollisionSpecialtyHost specialty;
    AbBirdCollisionProjectileHost projectiles;
    AbBirdCollisionEffectHost effect_host;
    AbBirdCollisionDamageHost damage;
    AbBirdCollisionHost host;
    AbBirdCollisionNotifyInput input;
    AbDamageSpriteNames damage_names;
    AbDataTable catalog;
    AbDataTable particle_descriptor, particle_raw;
    AbDataEntry particle_fields[2], particle_entry;
    AbDataValue saved_available;
    float collision_score;
    uint32_t flags, trace[32], trace_count;
    unsigned notifications_run, sprite_calls, releases, mighty_calls, audio_calls,
        damage_queries, particle_calls, cooldowns;
} Fixture;

static bool mark(Fixture *fixture, uint32_t kind) {
    CHECK(fixture->trace_count < 32);
    fixture->trace[fixture->trace_count++] = kind;
    return true;
}

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue text(const char *string) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(string)};
}

static bool named(AbSpan value, const char *wanted) {
    return value.data && value.size == strlen(wanted) &&
           !memcmp(value.data, wanted, value.size);
}

static AbDataValue boolean(bool value) {
    return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = value};
}

static bool object(void *context, AbSpan name, AbBirdCollisionObject **output,
                   AbError *error) {
    Fixture *fixture = context;
    (void)error;
    *output = named(name, "first") ? fixture->objects
              : named(name, "second")
                  ? fixture->objects + (fixture->notifications_run ? 2 : 1)
              : named(name, "hat") ? fixture->objects + 3
                                   : NULL;
    return true;
}

static bool definition(void *context, AbSpan name, AbBlockCollisionDefinition **output,
                       AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 1));
    (void)error;
    *output = fixture->notification + (named(name, "first") ? 0 : 1);
    return true;
}

static bool sprite_definition(void *context, AbSpan name,
                              AbBirdCollisionSpriteDefinition **output,
                              AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 1));
    (void)error;
    *output = fixture->definitions + (named(name, "first")         ? 0
                                      : fixture->notifications_run ? 2
                                                                   : 1);
    return true;
}

static bool notification(void *context, const AbBlockCollisionNotice *notice,
                         AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 2));
    (void)error;
    CHECK(fixture->notifications_run < 2);
    CHECK(notice->first == &fixture->objects[fixture->notifications_run].object);
    ++fixture->notifications_run;
    return true;
}

static bool hat(void *context, AbSpan name, AbBirdCollisionHat **output,
                AbError *error) {
    Fixture *fixture = context;
    (void)error;
    *output = named(name, "first") ? &fixture->hat : NULL;
    return true;
}

static bool score_entry(void *context, AbSpan name, AbBirdCollisionScoreEntry **output,
                        AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(named(name, "first"));
    *output = &fixture->score;
    return true;
}

static bool score(void *context, float damage, float *output, AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 3));
    (void)error;
    CHECK(damage == 11);
    *output = 17;
    return true;
}

static bool floating(void *context, const AbBirdCollisionFloatingScore *request,
                     AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 4));
    (void)error;
    CHECK(request->score == 17 && request->position.x == 2 && request->position.y == 3);
    CHECK(fixture->objects[3].defence.kind == AB_DATA_NUMBER &&
          !fixture->objects[3].defence.bits);
    return true;
}

static bool cooldown(void *context, AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 5));
    (void)error;
    CHECK(fixture->prelude_state.specialty_available_binding ==
          &fixture->sprite_state.bird_specialty_available);
    CHECK(fixture->sprite_state.bird_specialty_available.bits);
    fixture->sprite_state.bird_specialty_available = boolean(false);
    ++fixture->cooldowns;
    return true;
}

static bool mighty(void *context, AbSpan name, AbSpan other, AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 6));
    (void)error;
    CHECK(named(name, "first") && named(other, "second"));
    CHECK(fixture->score.score == 17 && fixture->prelude_state.count == 1);
    ++fixture->mighty_calls;
    return true;
}

static bool sprite(void *context, AbSpan name, AbDataValue value, AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 7));
    (void)error;
    if (named(name, "second")) {
        CHECK(named(fixture->objects[2].sprite.string, "normal"));
        CHECK(fixture->objects[1].sprite.kind == AB_DATA_NIL);
    }
    CHECK(value.kind == AB_DATA_STRING);
    ++fixture->sprite_calls;
    return true;
}

static bool release(void *context, AbSpan name, AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 8));
    (void)error;
    CHECK(named(name, "first") && fixture->objects[0].collision.bits);
    CHECK(fixture->objects[0].bomb_timer.bits == UINT32_C(0x3fc00000));
    CHECK(fixture->sprite_state.bird_specialty_available.bits);
    ++fixture->releases;
    return true;
}

static bool bird(void *context, AbSpan name, AbBirdCollisionEffectBird **output,
                 AbError *error) {
    (void)context;
    (void)error;
    CHECK(named(name, "first"));
    *output = NULL;
    return true;
}

static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 9));
    (void)error;
    CHECK(sound.kind == AB_DATA_STRING);
    CHECK(named(sound.string, fixture->audio_calls ? "bird" : "damage"));
    *output = sound;
    return true;
}

static bool audio(void *context, AbAudioPlay request, AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 10));
    (void)error;
    CHECK(request.volume.bits == UINT32_C(0x3f333333));
    CHECK(fixture->objects[1].boomerang_active.kind == AB_DATA_BOOLEAN &&
          !fixture->objects[1].boomerang_active.bits);
    ++fixture->audio_calls;
    return true;
}

static AbBirdCollisionEffectDefinition *
effect_definition(void *context, AbBlockCollisionDefinition *cached) {
    Fixture *fixture = context;
    return fixture->effects + (cached == fixture->notification ? 0 : 1);
}

static bool damage_sprites(void *context, AbBirdCollisionObject *cached,
                           AbDataValue blocks, AbDamageSpriteNames **output,
                           AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 11));
    (void)blocks;
    (void)error;
    CHECK(cached == fixture->objects + 1);
    ++fixture->damage_queries;
    *output = &fixture->damage_names;
    return true;
}

static bool particle_transform(void *context, AbVec2 point, AbVec2 *output,
                               AbError *error) {
    (void)context;
    (void)error;
    CHECK(point.x == 0 && point.y == 0);
    *output = point;
    return true;
}

static bool particle_emit(void *context, const AbAmbientParticleEmission *emission,
                          AbError *error) {
    Fixture *fixture = context;
    CHECK(mark(fixture, 12));
    (void)error;
    CHECK(named(emission->particles, "dust") && emission->amount == 1 &&
          emission->position.x == 0 && emission->position.y == 0);
    ++fixture->particle_calls;
    return true;
}

static bool damage_particles(void *context, AbBirdCollisionObject *cached,
                             AbError *error) {
    Fixture *fixture = context;
    CHECK(cached == fixture->objects + 1 &&
          named(cached->object.damage_sprite.string, "damaged"));
    CHECK(named(cached->object.blink_sprite.string, "blink") &&
          named(cached->object.smile_sprite.string, "smile"));
    return ab_damage_state_particles(
        &fixture->particle_raw, &cached->position,
        (AbAmbientParticleHost){fixture, particle_transform, particle_emit}, error);
}

static void prepare(Fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    fixture->objects[0].object.name = span("first");
    fixture->objects[1].object.name = fixture->objects[2].object.name = span("second");
    fixture->objects[2].object.sensor = boolean(true);
    fixture->objects[3].object.name = span("hat");
    fixture->objects[0].position = (AbVec2){2, 3};
    fixture->objects[0].shot = fixture->objects[1].shot = boolean(true);
    fixture->objects[0].controllable = boolean(true);
    fixture->objects[1].controllable = (AbDataValue){.kind = AB_DATA_NUMBER};
    fixture->objects[1].boomerang_active = boolean(true);
    fixture->objects[0].mighty_bait = boolean(true);
    fixture->objects[0].collision_function = (AbBirdCollisionFunction){fixture, mighty};
    fixture->definitions[0].specialty = text("BOMB");
    fixture->definitions[0].sprite_collision = text("normal");
    fixture->definitions[0].sprite_special1 = text("special");
    fixture->definitions[1].specialty = text("OTHER");
    fixture->definitions[2].specialty = text("BOOMERANG");
    fixture->definitions[2].sprite_collision = text("normal");
    fixture->effects[0].collision_sound = text("bird");
    fixture->effects[1].collision_sound = text("target");
    fixture->effects[1].damage_sound = text("damage");
    fixture->effects[2].collision_sound = fixture->effects[2].damage_sound =
        text("replacement");
    fixture->hat.object_name = span("hat");
    fixture->saved_available = boolean(false);
    fixture->prelude_state = (AbBirdCollisionPreludeState){
        .delta_based_slingshot = boolean(true),
        .bird_specialty_available = boolean(false),
        .floating_score_scaling = 1,
        .collision_scores = &fixture->collision_score,
        .capacity = 1,
        .specialty_available_binding = &fixture->saved_available};
    fixture->sprite_state =
        (AbBirdCollisionSpriteState){boolean(true), fixture->objects};
    fixture->grenades = (AbBirdCollisionGrenades){fixture->grenade_items, 0, 2, true};
    fixture->notifications = (AbBirdCollisionNotifyHost){.context = fixture,
                                                         .object = object,
                                                         .definition = definition,
                                                         .scripts = notification};
    fixture->prelude = (AbBirdCollisionPreludeHost){
        fixture, hat, object, score_entry, score, floating, cooldown, NULL};
    fixture->sprites =
        (AbBirdCollisionSpriteHost){fixture, sprite_definition, object, sprite};
    fixture->specialty = (AbBirdCollisionSpecialtyHost){.context = fixture,
                                                        .definition = sprite_definition,
                                                        .object = object,
                                                        .sprite = sprite,
                                                        .hat = hat,
                                                        .release_hat = release};
    fixture->projectiles = (AbBirdCollisionProjectileHost){fixture, sprite_definition};
    fixture->effect_host =
        (AbBirdCollisionEffectHost){fixture, bird, 0, audio_name, audio, NULL};
    fixture->damage_names =
        (AbDamageSpriteNames){text("damaged"), text("blink"), text("smile"), {0}};
    fixture->particle_fields[0] = (AbDataEntry){text("name"), text("dust")};
    fixture->particle_fields[1] = (AbDataEntry){
        text("amount"), {.kind = AB_DATA_NUMBER, .bits = UINT32_C(0x3f800000)}};
    fixture->particle_descriptor = (AbDataTable){fixture->particle_fields, 2, 2, NULL};
    fixture->particle_entry =
        (AbDataEntry){text("damageStateChangedParticles"),
                      {.kind = AB_DATA_TABLE, .table = &fixture->particle_descriptor}};
    fixture->particle_raw = (AbDataTable){&fixture->particle_entry, 1, 1, NULL};
    fixture->damage =
        (AbBirdCollisionDamageHost){fixture,
                                    {.kind = AB_DATA_TABLE, .table = &fixture->catalog},
                                    damage_sprites,
                                    damage_particles};
    fixture->host =
        (AbBirdCollisionHost){&fixture->notifications, &fixture->prelude,
                              &fixture->sprites,       &fixture->specialty,
                              &fixture->projectiles,   &fixture->effect_host,
                              &fixture->damage,        fixture,
                              effect_definition};
    fixture->input = (AbBirdCollisionNotifyInput){
        .contact = {span("first"), span("second"), 5, 11, {1, 0}, {0, 0}}};
}

static bool invoke(Fixture *fixture, AbError *error) {
    bool success = ab_bird_collision_callback(
        &fixture->input,
        (AbBirdCollisionState){&fixture->prelude_state, &fixture->sprite_state,
                               &fixture->grenades},
        &fixture->host, error);
    CHECK(fixture->prelude_state.specialty_available_binding ==
          &fixture->saved_available);
    return success;
}

static bool checks(void) {
    Fixture fixture;
    prepare(&fixture);
    AbError error = {0};
    CHECK(invoke(&fixture, &error));
    CHECK(fixture.notifications_run == 2 && fixture.cooldowns == 1 &&
          fixture.mighty_calls == 1);
    CHECK(fixture.sprite_calls == 3 && fixture.releases == 1 &&
          fixture.audio_calls == 2);
    CHECK(fixture.damage_queries == 1 && fixture.particle_calls == 1);
    CHECK(fixture.objects[0].has_collided.bits && fixture.objects[1].has_collided.bits);
    CHECK(fixture.objects[1].collision.bits &&
          fixture.sprite_state.bird_specialty_available.bits);
    prepare(&fixture);
    fixture.host.projectiles = NULL;
    CHECK(!invoke(&fixture, &error));
    CHECK(fixture.sprite_calls == 3 && fixture.releases == 1 && !fixture.audio_calls &&
          !fixture.damage_queries);
    CHECK(fixture.objects[1].boomerang_active.bits);
    prepare(&fixture);
    fixture.grenades.count = 1;
    fixture.grenade_items[0] = fixture.objects + 1;
    CHECK(invoke(&fixture, &error));
    CHECK(!fixture.audio_calls && !fixture.damage_queries && fixture.releases == 1);
    prepare(&fixture);
    fixture.grenades.count = 1;
    fixture.grenade_items[0] = fixture.objects;
    CHECK(invoke(&fixture, &error));
    CHECK(!fixture.audio_calls && fixture.damage_queries == 1 &&
          fixture.objects[0].explode.bits);
    prepare(&fixture);
    fixture.objects[0].projectile_feather = boolean(true);
    CHECK(ab_bird_collision_callback(&fixture.input, (AbBirdCollisionState){0},
                                     &fixture.host, &error));
    CHECK(!fixture.notifications_run && !fixture.cooldowns && !fixture.sprite_calls);
    CHECK(!ab_bird_collision_callback(NULL, (AbBirdCollisionState){0}, NULL, &error));
    return true;
}

static uint32_t bits(float value) {
    uint32_t output;
    memcpy(&output, &value, sizeof(output));
    return output;
}

static void value_words(AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    static const char *const strings[] = {"normal", "special", "damaged", "blink",
                                          "smile"};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_NUMBER || value.kind == AB_DATA_BOOLEAN) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        for (uint32_t id = 0; id < 5; ++id) {
            if (named(value.string, strings[id])) {
                output[1] = id + 1u;
                break;
            }
        }
    }
}

static void snapshot(Fixture *fixture, uint32_t output[128]) {
    for (size_t id = 0; id < 4; ++id) {
        AbBirdCollisionObject *object = fixture->objects + id;
        const AbDataValue fields[] = {object->shot,
                                      object->has_collided,
                                      object->collision,
                                      object->boomerang_active,
                                      object->explode,
                                      object->bomb_timer,
                                      object->defence,
                                      object->strength,
                                      object->sprite,
                                      object->object.damage_sprite,
                                      object->object.blink_sprite,
                                      object->object.smile_sprite,
                                      object->object.sensor,
                                      object->controllable};
        for (size_t field = 0; field < 14; ++field) {
            value_words(fields[field], output + id * 28u + field * 2u);
        }
    }
    const uint32_t counts[] = {fixture->notifications_run, fixture->sprite_calls,
                               fixture->releases,          fixture->mighty_calls,
                               fixture->audio_calls,       fixture->damage_queries,
                               fixture->particle_calls,    fixture->cooldowns};
    memcpy(output + 112, counts, sizeof(counts));
    output[120] = fixture->score.present;
    output[121] = bits(fixture->score.score);
    output[122] = (uint32_t)fixture->prelude_state.count;
    output[123] = bits(fixture->collision_score);
    value_words(fixture->sprite_state.bird_specialty_available, output + 124);
    output[126] = output[127] = fixture->notifications_run ? 3u : 2u;
}

static void apply_flags(Fixture *fixture, uint32_t flags) {
    fixture->flags = flags;
    if (flags & 1u) {
        fixture->objects[0].projectile_feather = boolean(true);
    }
    if (flags & 2u) {
        fixture->hat.object_name = span("second");
    }
    if (flags & 4u) {
        fixture->objects[1].object.sensor = boolean(true);
    }
    if (flags & 8u) {
        fixture->grenade_items[fixture->grenades.count++] = fixture->objects;
    }
    if (flags & 16u) {
        fixture->grenade_items[fixture->grenades.count++] = fixture->objects + 1;
    }
    if (flags & 32u) {
        fixture->host.projectiles = NULL;
    }
    if (flags & 64u) {
        fixture->grenades.present = false;
    }
    if (flags & 128u) {
        fixture->objects[0].has_collided = fixture->objects[1].has_collided =
            boolean(true);
    }
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Bird callback sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BIRD_COLLISION_V1 256\n"));
    size_t matched = 0, failures = 0;
    for (uint32_t sample = 0; sample < 256; ++sample) {
        Fixture fixture;
        prepare(&fixture);
        apply_flags(&fixture, sample);
        CHECK(words(file, &sample, 1, sample, "flags", &matched));
        AbError error = {0};
        uint32_t failed = !invoke(&fixture, &error), state[128];
        CHECK(words(file, &failed, 1, sample, "result", &matched));
        snapshot(&fixture, state);
        CHECK(
            words(file, state, 128, sample, "state", &matched) &&
            words(file, &fixture.trace_count, 1, sample, "trace count", &matched) &&
            words(file, fixture.trace, fixture.trace_count, sample, "trace", &matched));
        failures += failed;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB bird collision reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf(
        "All 256 original coupled bird callbacks match: %zu words and %zu failures.\n",
        matched, failures);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    bool okay = file && reference(file);
    if (file) {
        fclose(file);
    }
    return okay ? 0 : 1;
}
