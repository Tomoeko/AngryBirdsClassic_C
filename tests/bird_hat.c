#include "ab/bird_hat.h"
#include "bird_hat_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Hat check failed at %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    BirdHatCase input;
    AbBirdHat record;
    AbDataValue counter;
    AbDataEntry bird_fields[5], physical_fields[4], offset_fields[2], hat_fields[2];
    AbDataTable bird, physical, offset, definition;
    uint32_t output[AB_BIRD_HAT_WORDS], events, prints;
    bool present, fail_impulse;
} Fixture;

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(value)};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool equal(AbSpan value, const char *wanted) {
    return value.size == strlen(wanted) && !memcmp(value.data, wanted, value.size);
}

static uint32_t *event(Fixture *fixture, uint32_t code) {
    if (fixture->events >= 11)
        return NULL;
    uint32_t *output = fixture->output + 49 + fixture->events++ * 7;
    output[0] = code;
    return output;
}

static bool name_bytes(uint32_t *output, AbSpan name) {
    CHECK(name.size < 64);
    for (size_t index = 0; index < name.size; ++index)
        output[index / 4] |= (uint32_t)name.data[index] << ((index % 4) * 8);
    return true;
}

static bool create_object(void *context, AbSpan definition, AbSpan name,
                          AbVec2 position, float density, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t *output = event(fixture, 1);
    CHECK(output && equal(definition, "block_a") &&
          name_bytes(fixture->output + 16, name));
    output[1] = 1;
    output[2] = number(position.x).bits;
    output[3] = number(position.y).bits;
    output[4] = number(density).bits;
    fixture->physical_fields[0].value = number(position.x);
    fixture->physical_fields[1].value = number(position.y);
    return true;
}

static bool set_static(void *context, AbSpan name, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(name.size && event(fixture, 2));
    return true;
}

static bool set_defence(void *context, AbSpan name, float value, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(name.size);
    fixture->physical_fields[3].value = number(value);
    return true;
}

static bool create_joint(void *context, AbSpan name, AbSpan bird, AbSpan hat,
                         AbError *error) {
    (void)error;
    Fixture *fixture = context;
    uint32_t *output = event(fixture, 3);
    CHECK(output && equal(bird, "bird") && hat.size &&
          name_bytes(fixture->output + 32, name));
    output[1] = 2;
    output[2] = 2;
    if (fixture->input.change_counter)
        fixture->counter = number(25.5f);
    if (fixture->input.change_parameters) {
        fixture->hat_fields[0].value = text("block_b");
        fixture->offset_fields[0].value = number(fixture->input.dx + 0.03125f);
        fixture->offset_fields[1].value = number(fixture->input.dy - 0.0625f);
    }
    return true;
}

static bool publish(void *context, AbSpan bird, AbBirdHat *record, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(equal(bird, "bird") && record == &fixture->record);
    fixture->present = true;
    return true;
}

static bool print_release(void *context, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(event(fixture, 4));
    if (!fixture->prints++ && fixture->input.remove_on_print)
        fixture->present = false;
    return true;
}

static bool hat(void *context, AbSpan bird, AbBirdHat **output, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(equal(bird, "bird"));
    *output = fixture->present ? &fixture->record : NULL;
    return true;
}

static bool destroy_joint(void *context, AbSpan name, AbError *error) {
    (void)error;
    CHECK(name.size && event(context, 5));
    return true;
}

static bool set_damping(void *context, AbSpan name, float value, AbError *error) {
    (void)error;
    uint32_t *output = event(context, 6);
    CHECK(name.size && output);
    output[1] = number(value).bits;
    return true;
}

static bool set_dynamic(void *context, AbSpan name, AbError *error) {
    (void)error;
    Fixture *fixture = context;
    CHECK(name.size && event(context, 7));
    if (fixture->input.change_physics) {
        fixture->bird_fields[4].value = number(fixture->input.velocity * -2);
        fixture->physical_fields[0].value = number(fixture->input.x + 3.25f);
        fixture->physical_fields[1].value = number(fixture->input.y - 5.5f);
    }
    return true;
}

static bool object(void *context, AbSpan name, const AbDataTable **output,
                   AbError *error) {
    (void)error;
    Fixture *fixture = context;
    *output = equal(name, "bird") ? &fixture->bird : &fixture->physical;
    return true;
}

static bool impulse(void *context, AbSpan name, AbVec2 value, AbVec2 point,
                    AbError *error) {
    Fixture *fixture = context;
    uint32_t *output = event(fixture, 8);
    CHECK(output && name.size);
    output[1] = number(value.x).bits;
    output[2] = number(value.y).bits;
    output[3] = number(point.x).bits;
    output[4] = number(point.y).bits;
    return !fixture->fail_impulse || ab_fail(error, "Controlled impulse failure");
}

static AbDataValue destroyed(uint32_t kind) {
    if (kind == 1 || kind == 2)
        return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2};
    return kind == 3 ? number(0) : kind == 4 ? text("") : (AbDataValue){0};
}

static void scalar(AbDataValue value, uint32_t *output) {
    output[0] = value.kind == AB_DATA_NIL       ? 0
                : value.kind == AB_DATA_BOOLEAN ? 1
                : value.kind == AB_DATA_NUMBER  ? 3
                                                : 4;
    output[1] =
        value.kind == AB_DATA_BOOLEAN || value.kind == AB_DATA_NUMBER ? value.bits : 0;
}

static bool run(uint32_t sample, uint32_t output[AB_BIRD_HAT_WORDS]) {
    Fixture fixture = {.input = bird_hat_case(sample)};
    BirdHatCase input = fixture.input;
    fixture.counter = input.counter_kind == 0   ? (AbDataValue){0}
                      : input.counter_kind == 1 ? destroyed(1)
                                                : number(input.counter);
    AbDataEntry bird_fields[] = {{text("name"), text("bird")},
                                 {text("definition"), text("birddef")},
                                 {text("x"), number(input.x)},
                                 {text("y"), number(input.y)},
                                 {text("xVel"), number(input.velocity)}};
    AbDataEntry physical_fields[] = {{text("x"), number(0)},
                                     {text("y"), number(0)},
                                     {text("height"), number(input.height)},
                                     {text("defence"), number(0)}};
    AbDataEntry offset_fields[] = {{text("x"), number(input.dx)},
                                   {text("y"), number(input.dy)}};
    memcpy(fixture.bird_fields, bird_fields, sizeof(bird_fields));
    memcpy(fixture.physical_fields, physical_fields, sizeof(physical_fields));
    memcpy(fixture.offset_fields, offset_fields, sizeof(offset_fields));
    fixture.bird = (AbDataTable){fixture.bird_fields, 5, 5, NULL};
    fixture.physical = (AbDataTable){fixture.physical_fields, 4, 4, NULL};
    fixture.offset = (AbDataTable){fixture.offset_fields, 2, 2, NULL};
    AbDataEntry parameter = {text("birddef"), input.absent ? destroyed(sample & 1)
                                                           : table(&fixture.offset)};
    AbDataTable parameters = {&parameter, 1, 1, NULL};
    fixture.hat_fields[0] = (AbDataEntry){text("block"), text("block_a")};
    fixture.hat_fields[1] = (AbDataEntry){text("params"), table(&parameters)};
    fixture.definition = (AbDataTable){fixture.hat_fields, 2, 2, NULL};
    AbDataEntry hat_entry = {text("selected"), table(&fixture.definition)};
    AbDataTable hats = {&hat_entry, 1, 1, NULL};
    AbBirdHatHost host = {&fixture,      create_object, set_static,    set_defence,
                          create_joint,  publish,       print_release, hat,
                          destroy_joint, set_damping,   set_dynamic,   object,
                          impulse};
    bool created = true;
    CHECK(ab_bird_hat_create(&hats, &fixture.bird, span("selected"), &fixture.counter,
                             &fixture.record, &created, host, NULL) &&
          created == !input.absent);
    fixture.record.joint_destroyed = destroyed(input.destroyed_kind);
    CHECK(ab_bird_hat_release(span("bird"), host, NULL) &&
          ab_bird_hat_release(span("bird"), host, NULL));
    scalar(fixture.counter, fixture.output);
    fixture.output[2] = fixture.present;
    if (created) {
        fixture.output[3] = number(fixture.record.offset.x).bits;
        fixture.output[4] = number(fixture.record.offset.y).bits;
        fixture.output[5] = equal(fixture.record.definition, "block_a") ? 1 : 2;
        scalar(fixture.record.joint_destroyed, fixture.output + 6);
    }
    fixture.output[8] = fixture.physical_fields[3].value.bits;
    fixture.output[48] = fixture.events;
    memcpy(output, fixture.output, sizeof(fixture.output));
    if (!sample) {
        fixture.events = fixture.prints = 0;
        fixture.record.joint_destroyed = (AbDataValue){0};
        fixture.fail_impulse = true;
        CHECK(!ab_bird_hat_release(span("bird"), host, NULL) && fixture.events == 5 &&
              fixture.record.joint_destroyed.kind == AB_DATA_NIL);
        bool untouched = true;
        CHECK(!ab_bird_hat_create(&hats, &fixture.bird, span("selected"), NULL,
                                  &fixture.record, &untouched, host, NULL) &&
              untouched);
        AbBirdHat fresh = {0};
        AbDataTable invalid = {.count = 1, .capacity = 1};
        CHECK(!ab_bird_hat_create(&invalid, &fixture.bird, span("selected"),
                                  &fixture.counter, &fresh, &untouched, host, NULL) &&
              untouched);
    }
    return true;
}

static bool cases(FILE *reference) {
    if (reference) {
        char header[80];
        if (!fgets(header, sizeof(header), reference) ||
            strcmp(header, "AB_ANDROID_BIRD_HAT_V1 1024\n"))
            return false;
    }
    for (uint32_t sample = 0; sample < AB_BIRD_HAT_CASES; ++sample) {
        uint32_t output[AB_BIRD_HAT_WORDS];
        if (!run(sample, output))
            return false;
        for (size_t word = 0; reference && word < AB_BIRD_HAT_WORDS; ++word) {
            uint32_t expected = 0;
            if (fscanf(reference, "%" SCNx32, &expected) != 1 ||
                output[word] != expected) {
                fprintf(stderr,
                        "Hat case %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        sample, word, output[word], expected);
                return false;
            }
        }
        if (reference && fgetc(reference) != '\n')
            return false;
    }
    if (reference) {
        char completion[80];
        if (!fgets(completion, sizeof(completion), reference) ||
            strcmp(completion, "AB bird-hat reference complete\n") ||
            fgetc(reference) != EOF || ferror(reference))
            return false;
    }
    puts("Hat construction, source callbacks, release and retained state pass");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 1;
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc == 2 && !reference)
        return 1;
    bool okay = cases(reference);
    if (reference && fclose(reference))
        okay = false;
    return okay ? 0 : 1;
}
