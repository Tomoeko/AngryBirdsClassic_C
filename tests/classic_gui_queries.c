#include "ab/classic_root_queries.h"
#include "root_query_menu.h"
#include "../tools/oracle/reference_gui_queries.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "GUI query failed at %d: %s\n", __LINE__, #condition);     \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool setup(ReferenceGuiQuery input, QueryMenu *menu,
                  AbUiFrameNode notifications[2], AbUiFrameState states[2],
                  AbUiFrameNode **children) {
    static const unsigned roots[] = {1, 5, 2, 8};
    const char *names[] = {"pausePage"};
    CHECK(query_menu_prepare_named(
        (ReferenceRootQuery){.root = roots[input.root], .children = input.pause != 0},
        names, menu, NULL));
    if (input.pause)
        menu->frames[1].fields.visible = query_ui_flag(input.pause);
    states[1].fields.name = (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT,
                                               .text = {(const uint8_t *)"notice", 6}};
    states[1].fields.visible = query_ui_flag(input.notification_visible);
    states[1].fields.active = query_ui_flag(1);
    states[1].fields.notifications_frame_pause =
        query_ui_flag(input.notification_pause);
    notifications[0] =
        (AbUiFrameNode){.state = states, .children = children, .capacity = 1};
    notifications[1] = (AbUiFrameNode){.state = states + 1};
    return ab_ui_frame_add_child(notifications, notifications + 1, 0, false, NULL);
}

static bool corpus(FILE *reference) {
    if (reference) {
        char header[64];
        CHECK(fgets(header, sizeof(header), reference) &&
              !strcmp(header, "AB_ANDROID_GUI_QUERIES_V1 256\n"));
    }
    unsigned active_results = 0, pausing_results = 0;
    for (unsigned profile = 0; profile < 256; ++profile) {
        ReferenceGuiQuery input = reference_gui_query(profile);
        QueryMenu menu;
        AbUiFrameNode notifications[2] = {0}, *children[1];
        AbUiFrameState states[2] = {0};
        CHECK(setup(input, &menu, notifications, states, children));
        AbDataValue ingame;
        bool pausing = false;
        CHECK(
            ab_classic_level_start_ingame(menu.manager, notifications, &ingame, NULL) &&
            ab_classic_notifications_pausing(notifications, &pausing, NULL));
        CHECK(ingame.kind == AB_DATA_NIL || ingame.kind == AB_DATA_BOOLEAN);
        uint32_t actual[] = {profile, ingame.kind == AB_DATA_BOOLEAN ? 1u : 0u,
                             ingame.bits, pausing};
        for (unsigned field = 0; reference && field < 6; ++field) {
            uint32_t expected;
            CHECK(fscanf(reference, "%" SCNx32, &expected) == 1);
            if (field < 4 && expected != actual[field]) {
                fprintf(stderr, "GUI query %u/%u: %08" PRIx32 " != %08" PRIx32 "\n",
                        profile, field, actual[field], expected);
                return false;
            }
            /* Original getter counts are diagnostic source branch evidence.
             * Builtin pure C getters need not duplicate repeated table reads. */
            if (field >= 4)
                CHECK(expected <= 2);
        }
        active_results += ingame.kind == AB_DATA_BOOLEAN && ingame.bits;
        pausing_results += pausing;
        ab_ui_menu_manager_close(menu.manager);
    }
    if (reference) {
        char marker[80];
        CHECK(fscanf(reference, " %79[^\n]", marker) == 1 &&
              !strcmp(marker, "AB GUI queries reference complete"));
        int byte;
        do {
            byte = fgetc(reference);
        } while (byte == '\n' || byte == '\r');
        CHECK(byte == EOF && !ferror(reference));
    }
    printf("GUI queries: 256 original profiles, %u in-game/%u pausing results\n",
           active_results, pausing_results);
    return true;
}

static bool edges(void) {
    AbDataValue result = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    CHECK(!ab_classic_level_start_ingame(NULL, NULL, &result, NULL) &&
          result.bits == 1);
    QueryMenu menu;
    AbUiFrameNode notifications[2] = {0}, *children[1];
    AbUiFrameState states[2] = {0};
    CHECK(setup((ReferenceGuiQuery){.root = 1, .pause = 1}, &menu, notifications,
                states, children) &&
          !ab_classic_level_start_ingame(menu.manager, NULL, &result, NULL) &&
          result.bits == 1);
    menu.frames[1].fields.visible = query_ui_flag(3);
    CHECK(ab_classic_level_start_ingame(menu.manager, NULL, &result, NULL) &&
          result.kind == AB_DATA_BOOLEAN && !result.bits);
    ab_ui_menu_manager_close(menu.manager);
    bool paused = true;
    CHECK(!ab_classic_notifications_pausing(NULL, &paused, NULL) && paused);
    return true;
}

int main(int argc, char **argv) {
    FILE *reference = argc == 2 ? fopen(argv[1], "rb") : NULL;
    if (argc > 2 || (argc == 2 && !reference))
        return 2;
    bool okay = edges() && corpus(reference);
    if (reference && fclose(reference))
        okay = false;
    return okay ? 0 : 1;
}
