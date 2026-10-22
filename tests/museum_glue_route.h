#ifndef AB_TEST_MUSEUM_GLUE_ROUTE_H
#define AB_TEST_MUSEUM_GLUE_ROUTE_H

#include "museum_glue_route_cases.h"

typedef struct {
    GlueHost basic;
    uint32_t input[9];
    AbDataValue dead[2];
} GlueRouteHost;

static bool route_collider(void *context, AbSpan name, AbDataValue *output,
                           AbError *error) {
    GlueRouteHost *host = context;
    uint32_t id = name_id(name);
    CHECK(id == 1 || id == 2);
    if (!event(&host->basic, 1, name, error)) {
        return false;
    }
    *output = number((float)host->input[id]);
    return true;
}

static bool route_object(void *context, AbSpan name, AbMuseumGlueObject **output,
                         AbError *error) {
    GlueRouteHost *host = context;
    return object(&host->basic, name, output, error);
}

static bool route_velocity(void *context, AbSpan name, AbVec2 value, AbError *error) {
    GlueRouteHost *host = context;
    return velocity(&host->basic, name, value, error);
}

static AbMuseumGlueHost route_callbacks(GlueRouteHost *host) {
    return (AbMuseumGlueHost){host, route_collider, route_object, route_velocity};
}

static void route_prepare(GlueRouteHost *host, uint32_t sample,
                          AbMuseumGlueRoute *route, AbMuseumGlueResult *response) {
    *host = (GlueRouteHost){0};
    prepare(&host->basic, 0);
    host->basic.input[0] = sample;
    museum_glue_route_case(sample, host->input);
    for (size_t index = 0; index < 3; ++index) {
        host->basic.objects[index] =
            (AbMuseumGlueObject){.name = name_span(index),
                                 .plunger_name = name_span(2),
                                 .grabber = scalar(index < 2 ? host->input[8] : 0)};
    }
    *route = (AbMuseumGlueRoute){
        .objects = {host->basic.objects, host->basic.objects + 1},
        .colliders = {number((float)host->input[1]), number((float)host->input[2])}};
    for (size_t index = 0; index < 2; ++index) {
        host->dead[index] = scalar(host->input[3 + index]);
        if (host->input[7] & (1u << index)) {
            host->dead[index] = scalar(2);
        }
        route->dead[index] = host->dead[index];
    }
    *response = (AbMuseumGlueResult){.disable_contact = host->input[5] != 0,
                                     .has_pair = host->input[5] != 0,
                                     .first = name_span(0),
                                     .second = name_span(1)};
    if (host->input[1] == 1 || host->input[1] == 3 || host->input[2] == 1 ||
        host->input[2] == 3) {
        response->disable_contact = host->input[6] != 0;
    }
}

static bool native_route_checks(FILE *file) {
    char line[100];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_MUSEUM_GLUE_ROUTE_V1 8192\n") == 0);
    size_t words = 0, new_pairs = 0, cleared = 0, suffix_calls = 0;
    for (uint32_t sample = 0; sample < 8192; ++sample) {
        GlueRouteHost host;
        AbMuseumGlueRoute route;
        AbMuseumGlueResult response;
        route_prepare(&host, sample, &route, &response);
        bool before_disable = response.disable_contact;
        CHECK(compare(file, &host.basic, "route input", host.input, 9) &&
              ab_museum_glue_after_damage(&route, route_callbacks(&host), &response,
                                          NULL));
        uint32_t result[] = {1, response.disable_contact, response.has_pair ? 5u : 0u,
                             response.has_pair ? name_id(response.first) : 0,
                             response.has_pair ? name_id(response.second) : 0};
        CHECK(compare(file, &host.basic, "route response", result, 5));
        new_pairs += host.basic.count == 3;
        cleared += before_disable && !response.disable_contact;
        suffix_calls += host.basic.count;
        for (size_t index = 0; index < 3; ++index) {
            const AbMuseumGlueObject *entry = host.basic.objects + index;
            uint32_t fields[7] = {name_id(entry->name)};
            value_words(entry->grabber, fields + 1);
            value_words(entry->grabbed_egg, fields + 3);
            value_words(entry->glued_to_block, fields + 5);
            CHECK(compare(file, &host.basic, "route object", fields, 7));
        }
        uint32_t dead[4];
        value_words(host.dead[0], dead);
        value_words(host.dead[1], dead + 2);
        CHECK(compare(file, &host.basic, "route dead", dead, 4));
        uint32_t count = 5u + host.basic.count;
        CHECK(compare(file, &host.basic, "route count", &count, 1));
        /* Original object/definition lookups and controlled primary helpers are
         * captured preconditions, not implementations of primary damage here. */
        static const uint32_t lookups[][4] = {
            {1, 1, 0, 0}, {1, 2, 0, 0}, {2, 1, 0, 0}, {2, 2, 0, 0}};
        for (size_t index = 0; index < 4; ++index) {
            CHECK(compare(file, &host.basic, "route precondition", lookups[index], 4));
        }
        bool first_bird = host.input[1] == 1 || host.input[1] == 3;
        bool second_bird = host.input[2] == 1 || host.input[2] == 3;
        uint32_t primary[] = {first_bird || second_bird ? 3u : 4u,
                              second_bird ? 2u : 1u, second_bird ? 1u : 2u, 6};
        CHECK(compare(file, &host.basic, "route primary", primary, 4));
        for (size_t index = 0; index < host.basic.count; ++index) {
            uint32_t event_words[4];
            memcpy(event_words, host.basic.events[index], sizeof(event_words));
            static const uint32_t kinds[] = {0, 2, 1, 5};
            CHECK(event_words[0] < 4);
            event_words[0] = kinds[event_words[0]];
            CHECK(compare(file, &host.basic, "route helper", event_words, 4));
        }
        words += host.basic.words;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB museum glue route reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 8,192 original post-damage glue routes match: %zu words, %zu new "
           "pairs, %zu cleared disable decisions and %zu helper host calls.\n",
           words, new_pairs, cleared, suffix_calls);
    return true;
}

static bool route_edge_checks(void) {
    GlueRouteHost host;
    AbMuseumGlueRoute route;
    AbMuseumGlueResult response;
    route_prepare(&host, 130, &route, &response);
    CHECK(
        host.input[1] == 10 && host.input[2] == 10 && response.disable_contact &&
        response.has_pair &&
        ab_museum_glue_after_damage(&route, route_callbacks(&host), &response, NULL) &&
        !response.disable_contact && !response.has_pair && host.basic.count == 1 &&
        host.basic.events[0][1] == 2);
    route.dead[0] = number(0);
    route.objects[0] = route.objects[1] = NULL;
    response = (AbMuseumGlueResult){.disable_contact = true, .first = name_span(5)};
    AbMuseumGlueResult before = response;
    CHECK(ab_museum_glue_after_damage(&route, (AbMuseumGlueHost){0}, &response, NULL) &&
          memcmp(&before, &response, sizeof(before)) == 0 &&
          !ab_museum_glue_after_damage(NULL, (AbMuseumGlueHost){0}, &response, NULL) &&
          memcmp(&before, &response, sizeof(before)) == 0);
    route.dead[0] = scalar(1);
    route.colliders[0] = scalar(4);
    route.colliders[1] = scalar(2);
    CHECK(ab_museum_glue_after_damage(&route, (AbMuseumGlueHost){0}, &response, NULL) &&
          memcmp(&before, &response, sizeof(before)) == 0);
    return true;
}

#endif
