#include "ab/classic_root_queries.h"
#include "root_query_menu.h"
#include "../tools/oracle/reference_museum_pause.h"

#include <inttypes.h>
#include <stdio.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Museum pause failed at %d: %s\n", __LINE__, #condition);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbUiPointerValue show_flag(unsigned kind) {
    if (kind < 4)
        return query_ui_flag(kind);
    if (kind == 4)
        return (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                  .number_bits = UINT32_C(0x3f800000)};
    if (kind < 7)
        return (AbUiPointerValue){
            .kind = AB_UI_POINTER_TEXT,
            .text = {(const uint8_t *)"true", kind == 5 ? 4u : 0u}};
    return (AbUiPointerValue){.kind = AB_UI_POINTER_OBJECT, .object = 7};
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_MUSEUM_PAUSE_V1 256\n"));
    }
    for (unsigned profile = 0; profile < 256; ++profile) {
        ReferenceMuseumPause input = reference_museum_pause(profile);
        const unsigned roots[] = {1, 5, 2, 8};
        const char *names[] = {"pausePage"};
        QueryMenu menu;
        CHECK(query_menu_prepare_named(
            (ReferenceRootQuery){.root = roots[input.root], .children = input.child},
            names, &menu, NULL));
        if (input.child && input.root) {
            menu.frames[1].fields.pause_page_show = show_flag(input.show);
            menu.frames[1].fields.visible = query_ui_flag(input.visible);
        }
        bool paused;
        AbDataValue level;
        CHECK(ab_classic_pause_menu_visible(menu.manager, &paused, NULL) &&
              ab_classic_museum_in_level(menu.manager, &level, NULL) &&
              (level.kind == AB_DATA_NIL || level.kind == AB_DATA_BOOLEAN));
        uint32_t row[] = {profile, paused, level.kind == AB_DATA_BOOLEAN ? 1u : 0u,
                          level.bits};
        for (unsigned index = 0; reference && index < 6; ++index) {
            uint32_t expected;
            CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
            if (index < 4 && expected != row[index]) {
                fprintf(stderr, "Museum pause %u/%u: %08" PRIx32 " != %08" PRIx32 "\n",
                        profile, index, row[index], expected);
                return false;
            }
            /* Pure getter counts are diagnostic source branch evidence. */
        }
        ab_ui_menu_manager_close(menu.manager);
    }
    if (reference) {
        char marker[64];
        CHECK(fscanf(reference, " %63[^\n]", marker) == 1 &&
              !strcmp(marker, "AB Museum pause reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    return true;
}

static bool edges(void) {
    AbUiFrameState own = {.fields = {.pause_page_show = show_flag(2),
                                     .notifications_frame_pause = show_flag(2),
                                     .require_input = show_flag(3)}};
    AbUiFrameFields arguments = {.name = show_flag(5), .pause_page_show = show_flag(1)};
    CHECK(ab_ui_frame_init(&own, &arguments, &(AbUiInitBoundaries){0}, NULL) &&
          own.fields.pause_page_show.kind == AB_UI_POINTER_BOOLEAN &&
          own.fields.pause_page_show.boolean &&
          own.fields.notifications_frame_pause.boolean &&
          own.fields.require_input.kind == AB_UI_POINTER_NUMBER);
    bool paused = true;
    AbDataValue result = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(ab_classic_pause_menu_visible(NULL, &paused, NULL) && !paused &&
          !ab_classic_pause_menu_visible(NULL, NULL, NULL) &&
          !ab_classic_museum_in_level(NULL, &result, NULL) && result.bits == 1);
    QueryMenu menu;
    CHECK(query_menu_prepare((ReferenceRootQuery){.root = 5}, &menu, NULL));
    menu.frames[0].child_count = 1;
    paused = true;
    CHECK(!ab_classic_pause_menu_visible(menu.manager, &paused, NULL) && paused &&
          !ab_classic_museum_in_level(menu.manager, &result, NULL) && result.bits == 1);
    ab_ui_menu_manager_close(menu.manager);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference)
        okay = fclose(reference) == 0 && okay;
    if (!okay)
        return 1;
    puts(reference ? "Museum pause queries match 256 original profiles"
                   : "Museum pause query field and failure checks passed");
    return 0;
}
