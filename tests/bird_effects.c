#include "ab/bird_collision.h"
#include "bird_effects_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Bird effects failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbBirdCollisionObject second;
    AbBirdCollisionEffectDefinition definitions[2];
    AbBirdCollisionEffectBird bird, alternate;
    AbBirdCollisionParticleQuery query;
    AbBirdCollisionEffectHost host;
    AbBirdCollisionEffectInput contact;
    AbDataTable tables[2];
    AbDataEntry collision;
    AbDataValue property;
    uint32_t input[24], events[6][36], count, name_calls, bird_binding, script_kind;
    bool fail_audio, fail_particles, mutate_input;
} Fixture;

static const char *const strings[] = {"sound", "changed"};

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static float number(uint32_t word) {
    float output;
    memcpy(&output, &word, sizeof(output));
    return output;
}

static AbDataValue scalar(Fixture *fixture, uint32_t kind) {
    switch (kind) {
        case 0:
            return (AbDataValue){0};
        case 1:
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
        case 3:
            return (AbDataValue){.kind = AB_DATA_NUMBER};
        case 4:
            return text("sound");
        default:
            return (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables};
    }
}

static void value_words(Fixture *fixture, AbDataValue value, uint32_t output[2]) {
    static const uint32_t types[] = {0, 1, 3, 4, 5};
    output[0] = types[value.kind];
    output[1] = 0;
    if (value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER) {
        output[1] = value.bits;
    } else if (value.kind == AB_DATA_STRING) {
        output[1] = value.string.size == strlen(strings[1]) &&
                            !memcmp(value.string.data, strings[1], value.string.size)
                        ? 2u
                        : 1u;
    } else if (value.kind == AB_DATA_TABLE) {
        output[1] = (uint32_t)(value.table - fixture->tables) + 201u;
    }
}

static bool audio_first(void *context, AbAudioPlay request, AbError *error);
static bool audio_second(void *context, AbAudioPlay request, AbError *error);

static void snapshot(Fixture *fixture, uint32_t output[22]) {
    const AbDataValue fields[] = {fixture->bird.mass,
                                  fixture->bird.x_velocity,
                                  fixture->bird.y_velocity,
                                  fixture->definitions[0].collision_sound,
                                  fixture->definitions[1].collision_sound,
                                  fixture->definitions[1].damage_sound,
                                  fixture->collision.value};
    for (size_t field = 0; field < 7; ++field) {
        value_words(fixture, fields[field], output + field * 2u);
    }
    output[14] = fixture->script_kind;
    output[15] = fixture->bird_binding;
    output[16] = !fixture->host.audio                 ? 0u
                 : fixture->host.audio == audio_first ? 1u
                                                      : 2u;
    value_words(fixture, fixture->second.object.sensor, output + 17);
    output[19] = fixture->name_calls;
    value_words(fixture, fixture->property, output + 20);
}

static uint32_t *record(Fixture *fixture, uint32_t kind, uint32_t pc, uint32_t variant,
                        uint32_t argc) {
    if (fixture->count >= 6) {
        return NULL;
    }
    uint32_t *event = fixture->events[fixture->count++];
    memset(event, 0, 36 * sizeof(*event));
    event[0] = kind;
    event[1] = pc;
    event[2] = variant;
    event[3] = argc;
    snapshot(fixture, event + 14);
    return event;
}

static bool lookup(void *context, AbSpan name, AbBirdCollisionEffectBird **output,
                   AbError *error) {
    Fixture *fixture = context;
    (void)error;
    CHECK(name.size == 5 && !memcmp(name.data, "first", 5));
    *output = fixture->bird_binding == 1   ? &fixture->bird
              : fixture->bird_binding == 2 ? &fixture->alternate
                                           : NULL;
    return true;
}

static bool audio_name(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 1, 0, 0, 1);
    CHECK(event);
    value_words(fixture, sound, event + 4);
    if (!fixture->name_calls++) {
        uint32_t mutation = fixture->input[15];
        if (mutation & 1u) {
            fixture->host.audio = audio_second;
        }
        if (mutation & 2u) {
            fixture->bird.mass =
                (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[22]};
        }
        if (mutation & 4u) {
            fixture->definitions[0].collision_sound = text("changed");
        }
        if (mutation & 8u) {
            fixture->bird.script = NULL;
            fixture->script_kind = 0;
        }
        if (mutation & 16u) {
            fixture->bird_binding = 2;
        }
        if (mutation & 512u) {
            fixture->second.object.sensor = scalar(fixture, 2);
        }
    }
    if (fixture->mutate_input) {
        fixture->contact.first = span("changed");
        fixture->contact.force = -99;
        fixture->contact.damage = -99;
    }
    *output = scalar(fixture, fixture->input[14]);
    return true;
}

static bool audio(void *context, AbAudioPlay request, uint32_t variant,
                  AbError *error) {
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 2, 0, variant, 4);
    CHECK(event);
    const AbDataValue values[] = {request.name, request.volume, request.loop,
                                  request.channel};
    for (size_t field = 0; field < 4; ++field) {
        value_words(fixture, values[field], event + 4 + field * 2u);
    }
    return fixture->fail_audio ? ab_fail(error, "Injected bird effect audio failure")
                               : true;
}

static bool audio_first(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 1, error);
}

static bool audio_second(void *context, AbAudioPlay request, AbError *error) {
    return audio(context, request, 2, error);
}

static bool property(void *context, AbDataValue *output, AbError *error) {
    Fixture *fixture = context;
    (void)error;
    uint32_t *event = record(fixture, 3, 896, 0, 2);
    CHECK(event);
    event[4] = 301;
    if (fixture->input[15] & 32u) {
        fixture->bird.mass =
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = fixture->input[22]};
    }
    if (fixture->input[15] & 64u) {
        fixture->collision.value = scalar(fixture, 1);
    }
    *output = fixture->property;
    return true;
}

static bool particles(void *context, AbSpan first, AbDataValue descriptor,
                      AbError *error) {
    Fixture *fixture = context;
    uint32_t *event = record(fixture, 4, 906, 0, 4);
    CHECK(event && first.size == 5 && !memcmp(first.data, "first", 5));
    event[4] = 1;
    value_words(fixture, descriptor, event + 5);
    if (fixture->input[15] & 128u) {
        fixture->definitions[0].collision_sound = scalar(fixture, 1);
    }
    if (fixture->input[15] & 256u) {
        fixture->bird.mass = scalar(fixture, 3);
    }
    return fixture->fail_particles ? ab_fail(error, "Injected bird particle failure")
                                   : true;
}

static void prepare(Fixture *fixture, uint32_t sample) {
    memset(fixture, 0, sizeof(*fixture));
    bird_effects_case(sample, fixture->input);
    fixture->second.object.sensor = scalar(fixture, fixture->input[1]);
    fixture->bird_binding = fixture->input[3];
    static const char *const materials[] = {"immovable", "staticGround", "OTHER"};
    uint32_t material = fixture->input[4];
    fixture->definitions[1].material_name =
        material >= 3 ? text(materials[material - 3u]) : scalar(fixture, material);
    AbDataValue *numbers[] = {&fixture->bird.x_velocity, &fixture->bird.y_velocity,
                              &fixture->bird.mass};
    for (size_t field = 0; field < 3; ++field) {
        *numbers[field] =
            fixture->input[5 + field] == 2
                ? (AbDataValue){.kind = AB_DATA_NUMBER,
                                .bits = fixture->input[field < 2 ? 19 + field : 18]}
                : scalar(fixture, fixture->input[5 + field]);
    }
    fixture->script_kind = fixture->input[8];
    fixture->query = (AbBirdCollisionParticleQuery){
        fixture, fixture->script_kind == 1 ? property : NULL};
    fixture->bird.script = fixture->script_kind ? &fixture->query : NULL;
    fixture->collision =
        (AbDataEntry){text("collision"), scalar(fixture, fixture->input[10])};
    fixture->tables[1] = (AbDataTable){&fixture->collision, 1, 1, NULL};
    fixture->property =
        fixture->input[9] == 5
            ? (AbDataValue){.kind = AB_DATA_TABLE, .table = fixture->tables + 1}
            : scalar(fixture, fixture->input[9]);
    fixture->definitions[0].collision_sound = scalar(fixture, fixture->input[11]);
    fixture->definitions[1].collision_sound = scalar(fixture, fixture->input[12]);
    fixture->definitions[1].damage_sound = scalar(fixture, fixture->input[13]);
    fixture->host = (AbBirdCollisionEffectHost){fixture,
                                                lookup,
                                                number(fixture->input[21]),
                                                audio_name,
                                                fixture->input[23] ? audio_first : NULL,
                                                particles};
    fixture->contact = (AbBirdCollisionEffectInput){span("first"),
                                                    span("second"),
                                                    fixture->input[2] != 0,
                                                    number(fixture->input[16]),
                                                    number(fixture->input[17]),
                                                    &fixture->second,
                                                    fixture->definitions,
                                                    fixture->definitions + 1};
}

typedef struct {
    Fixture *fixture;
    AbBirdCollisionObject first;
    AbBlockCollisionDefinition definitions[2];
    AbBirdCollisionScoreEntry score_entry;
    AbDamageSpriteNames unchanged;
    AbDataTable catalog;
} Composition;

static bool composition_object(void *context, AbSpan name,
                               AbBirdCollisionObject **output, AbError *error) {
    Composition *composition = context;
    (void)error;
    *output = name.size == 5 && !memcmp(name.data, "first", 5)
                  ? &composition->first
                  : &composition->fixture->second;
    return true;
}

static bool composition_definition(void *context, AbSpan name,
                                   AbBlockCollisionDefinition **output,
                                   AbError *error) {
    Composition *composition = context;
    (void)error;
    *output = composition->definitions + (name.size == 5 ? 0 : 1);
    return true;
}

static bool composition_scripts(void *context, const AbBlockCollisionNotice *notice,
                                AbError *error) {
    (void)context;
    (void)notice;
    (void)error;
    return true;
}

static bool composition_hat(void *context, AbSpan name, AbBirdCollisionHat **output,
                            AbError *error) {
    (void)context;
    (void)name;
    (void)error;
    *output = NULL;
    return true;
}

static bool composition_score_entry(void *context, AbSpan name,
                                    AbBirdCollisionScoreEntry **output,
                                    AbError *error) {
    Composition *composition = context;
    (void)name;
    (void)error;
    *output = &composition->score_entry;
    return true;
}

static bool composition_score(void *context, float damage, float *output,
                              AbError *error) {
    (void)context;
    (void)damage;
    (void)error;
    *output = 0;
    return true;
}

static bool composition_floating(void *context,
                                 const AbBirdCollisionFloatingScore *score,
                                 AbError *error) {
    (void)context;
    (void)score;
    (void)error;
    return true;
}

static bool composition_damage(void *context, AbBirdCollisionObject *object,
                               AbDataValue blocks, AbDamageSpriteNames **output,
                               AbError *error) {
    Composition *composition = context;
    (void)blocks;
    (void)error;
    CHECK(object == &composition->fixture->second);
    *output = &composition->unchanged;
    return true;
}

static AbBirdCollisionEffectDefinition *
composition_effect_definition(void *context, AbBlockCollisionDefinition *definition) {
    Composition *composition = context;
    return composition->fixture->definitions +
           (definition == composition->definitions ? 0 : 1);
}

static bool invoke(Fixture *fixture, bool composed, AbError *error) {
    if (!composed) {
        return ab_bird_collision_effects(&fixture->contact, &fixture->host, error);
    }
    Composition composition = {.fixture = fixture,
                               .score_entry = {.present = true},
                               .unchanged = {.sprite = text("old2")}};
    composition.first.object.name = span("first");
    fixture->second.object.name = span("second");
    fixture->second.object.damage_sprite = text("old2");
    AbBirdCollisionObject marker = {.object = {.name = span("first")}};
    AbBirdCollisionObject *items[] = {&marker};
    AbBirdCollisionGrenades grenades = {items, fixture->input[2] ? 0u : 1u, 1, true};
    float score = 0;
    AbBirdCollisionPreludeState prelude_state = {
        .floating_score_scaling = 1, .collision_scores = &score, .capacity = 1};
    AbBirdCollisionSpriteState sprite_state = {0};
    AbBirdCollisionNotifyHost notifications = {.context = &composition,
                                               .object = composition_object,
                                               .definition = composition_definition,
                                               .scripts = composition_scripts};
    AbBirdCollisionPreludeHost prelude = {.context = &composition,
                                          .hat = composition_hat,
                                          .score_entry = composition_score_entry,
                                          .score = composition_score,
                                          .floating = composition_floating};
    AbBirdCollisionDamageHost damage = {
        .context = &composition,
        .block_table = {.kind = AB_DATA_TABLE, .table = &composition.catalog},
        .sprites = composition_damage};
    AbBirdCollisionHost hosts = {.notifications = &notifications,
                                 .prelude = &prelude,
                                 .effects = &fixture->host,
                                 .damage = &damage,
                                 .definition_context = &composition,
                                 .effect_definition = composition_effect_definition};
    AbBirdCollisionNotifyInput input = {.contact = {.first = span("first"),
                                                    .second = span("second"),
                                                    .force = fixture->contact.force,
                                                    .damage = fixture->contact.damage}};
    bool success = ab_bird_collision_callback(
        &input, (AbBirdCollisionState){&prelude_state, &sprite_state, &grenades},
        &hosts, error);
    CHECK(!prelude_state.specialty_available_binding);
    return success;
}

static bool edges(void) {
    Fixture fixture;
    prepare(&fixture, 1);
    fixture.second.object.sensor = scalar(&fixture, 2);
    CHECK(ab_bird_collision_effects(&fixture.contact, NULL, NULL));
    fixture.second.object.sensor = scalar(&fixture, 0);
    fixture.contact.first_present = false;
    CHECK(ab_bird_collision_effects(&fixture.contact, NULL, NULL));
    prepare(&fixture, 1);
    fixture.input[15] = 1;
    fixture.contact.force = 10;
    fixture.host.threshold = 0;
    fixture.definitions[1].collision_sound = scalar(&fixture, 1);
    fixture.fail_audio = true;
    CHECK(!ab_bird_collision_effects(&fixture.contact, &fixture.host, NULL));
    CHECK(fixture.count == 2 && fixture.events[0][4] == 1 &&
          fixture.events[1][2] == 1 && fixture.host.audio == audio_second);
    prepare(&fixture, 1);
    fixture.input[15] = 0;
    fixture.contact.force = 10;
    fixture.contact.damage = 0;
    fixture.host.threshold = 0;
    fixture.definitions[1].collision_sound = text("sound");
    fixture.bird.script = NULL;
    fixture.bird.mass = scalar(&fixture, 3);
    fixture.definitions[0].collision_sound = text("sound");
    fixture.mutate_input = true;
    CHECK(ab_bird_collision_effects(&fixture.contact, &fixture.host, NULL));
    CHECK(fixture.count == 4 && fixture.events[3][7] == UINT32_C(0x3f800000));
    prepare(&fixture, 1);
    fixture.input[15] = 0;
    fixture.contact.force = fixture.contact.damage = 0;
    fixture.host.threshold = -1;
    fixture.definitions[1].material_name = text("OTHER");
    fixture.definitions[1].collision_sound = (AbDataValue){0};
    fixture.definitions[0].collision_sound = text("sound");
    fixture.bird.script = NULL;
    fixture.bird.mass = scalar(&fixture, 3);
    CHECK(ab_bird_collision_effects(&fixture.contact, &fixture.host, NULL));
    CHECK(fixture.count == 2 && fixture.events[1][7] == UINT32_C(0x7fc00000));
    CHECK(!ab_bird_collision_effects(NULL, NULL, NULL));
    return true;
}

static bool words(FILE *file, const uint32_t *actual, size_t count, uint32_t sample,
                  const char *label, size_t *matched) {
    for (size_t field = 0; field < count; ++field) {
        uint32_t expected = 0;
        if (fscanf(file, " %" SCNx32, &expected) != 1 || expected != actual[field]) {
            fprintf(stderr, "Bird effect sample %u %s[%zu]: C=%08x original=%08x\n",
                    sample, label, field, actual[field], expected);
            return false;
        }
        ++*matched;
    }
    return true;
}

static bool reference(FILE *file, bool composed) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          !strcmp(line, "AB_ANDROID_BIRD_EFFECTS_V1 4096\n"));
    size_t matched = 0, failures = 0, counts[5] = {0};
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        Fixture fixture;
        prepare(&fixture, sample);
        CHECK(words(file, fixture.input, 24, sample, "input", &matched));
        uint32_t failed = !invoke(&fixture, composed, NULL), state[22];
        CHECK(words(file, &failed, 1, sample, "result", &matched));
        snapshot(&fixture, state);
        CHECK(words(file, state, 22, sample, "state", &matched) &&
              words(file, &fixture.count, 1, sample, "count", &matched));
        for (uint32_t event = 0; event < fixture.count; ++event) {
            CHECK(words(file, fixture.events[event], 36, sample, "event", &matched));
            ++counts[fixture.events[event][0]];
        }
        failures += failed;
    }
    CHECK(fscanf(file, " %95[^\n]", line) == 1 &&
          !strcmp(line, "AB bird effects reference complete") &&
          fscanf(file, " %1s", line) == EOF);
    printf(
        "All 4,096 original bird contact-effect phases match: %zu words, %zu failures, "
        "%zu audio queries, %zu audio calls, %zu properties, %zu particles.\n",
        matched, failures, counts[1], counts[2], counts[3], counts[4]);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edges() ? 0 : 1;
    }
    if (argc != 2 && (argc != 3 || strcmp(argv[2], "--callback"))) {
        return 2;
    }
    FILE *file = fopen(argv[1], "r");
    bool okay = file && reference(file, argc == 3);
    if (file) {
        fclose(file);
    }
    return okay ? 0 : 1;
}
