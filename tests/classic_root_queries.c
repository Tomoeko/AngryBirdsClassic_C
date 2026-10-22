#include "ab/classic_root_queries.h"
#include "root_query_menu.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Root query failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue flag(unsigned kind) {
    if (!kind)
        return (AbDataValue){0};
    return kind < 3 ? (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = kind == 2}
                    : (AbDataValue){.kind = AB_DATA_NUMBER};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_ROOT_QUERIES_V1 512\n"));
    }
    for (unsigned profile = 0; profile < 512; ++profile) {
        ReferenceRootQuery input = reference_root_query(profile);
        QueryMenu menu;
        CHECK(query_menu_prepare(input, &menu, NULL));
        bool safe, free_eagle;
        CHECK(ab_classic_safe_to_quit(menu.manager, &safe, NULL));
        AbDataEntry status_fields[3], statuses[3];
        AbDataTable status_records[3];
        for (unsigned index = 0; index < 3; ++index) {
            float key = (float)index + 1;
            AbDataValue numeric_key = {.kind = AB_DATA_NUMBER};
            memcpy(&numeric_key.bits, &key, sizeof(key));
            status_fields[index] =
                (AbDataEntry){text("enabled"), flag(index ? 1u : input.unlock)};
            status_records[index] = (AbDataTable){
                .entries = status_fields + index, .count = 1, .capacity = 1};
            statuses[index] = (AbDataEntry){numeric_key, table(status_records + index)};
        }
        AbDataTable status = {.entries = statuses, .count = 3, .capacity = 3};
        AbDataEntry free_field = {text("free"), flag(input.power == 2 ? 1u : 3u)};
        AbDataTable power = {.entries = &free_field, .count = 1, .capacity = 1};
        AbDataEntry power_field = {text("mightyPower"),
                                   input.power < 2 ? flag(input.power) : table(&power)};
        AbDataTable episode = {.entries = &power_field, .count = 1, .capacity = 1};
        AbDataEntry episodes[] = {{text("e"), table(&episode)},
                                  {flag(3), table(&episode)}};
        AbDataTable episode_table = {.entries = episodes, .count = 2, .capacity = 2};
        AbDataValue current = input.episode < 2    ? flag(input.episode)
                              : input.episode == 2 ? text("e")
                                                   : flag(3);
        AbDataValue origin = text(input.main_menu ? "MAIN_MENU" : "INGAME");
        AbClassicEagleQuery query = {&status, &episode_table, &current, &origin};
        CHECK(ab_classic_free_eagle(&query, &free_eagle, NULL));
        ab_ui_menu_manager_close(menu.manager);
        uint32_t values[] = {profile,       input.root,  input.children,  input.require,
                             input.episode, input.power, input.main_menu, input.unlock,
                             safe,          free_eagle};
        for (unsigned index = 0; reference && index < 10; ++index) {
            uint32_t expected;
            CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
            if (values[index] != expected) {
                fprintf(stderr, "Root query %u/%u: %08" PRIx32 " != %08" PRIx32 "\n",
                        profile, index, values[index], expected);
                return false;
            }
        }
    }
    if (reference) {
        char marker[80];
        CHECK(fscanf(reference, " %79[^\n]", marker) == 1 &&
              !strcmp(marker, "AB root queries reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    QueryMenu menu;
    CHECK(query_menu_prepare((ReferenceRootQuery){.root = 2, .children = 1}, &menu,
                             NULL));
    bool result = false;
    menu.menu.tree = NULL;
    CHECK(!ab_classic_safe_to_quit(menu.manager, &result, NULL) && !result);
    ab_ui_menu_manager_close(menu.manager);
    AbDataTable status = {0};
    AbDataValue current = flag(3), origin = {0};
    AbClassicEagleQuery query = {&status, NULL, &current, &origin};
    CHECK(!ab_classic_free_eagle(&query, &result, NULL) && !result);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference && fclose(reference))
        okay = false;
    if (okay)
        puts("Root queries: 512 original profiles, 5120 input/result words");
    return okay ? 0 : 1;
}
