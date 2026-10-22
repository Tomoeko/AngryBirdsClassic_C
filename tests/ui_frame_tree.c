#include "ab/ui_frame_tree.h"

#include "../tools/oracle/ui_frame_tree_cases.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Frame tree failed at %d: %s\n", __LINE__, #condition);    \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbUiFrameState states[8];
    AbUiFrameNode nodes[8];
    AbUiFrameNode *children[8][8];
    AbUiFrameLifecycle lifecycle;
    uint32_t events[16][10];
    size_t count, fail_at;
} Model;

typedef struct {
    size_t commands[13], events, stale_parent_snapshots, shared_memberships;
} Counts;

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static uint32_t id(const Model *model, const AbUiFrameNode *node) {
    if (!node) {
        return 0;
    }
    for (uint32_t index = 0; index < 8; ++index) {
        if (node == model->nodes + index) {
            return index + 1;
        }
    }
    return UINT32_MAX;
}

static bool decode(const uint32_t words[2], AbUiPointerValue *output) {
    CHECK(words[0] <= AB_UI_POINTER_FUNCTION);
    *output = (AbUiPointerValue){.kind = (AbUiPointerKind)words[0]};
    switch (output->kind) {
        case AB_UI_POINTER_NONE:
            CHECK(words[1] == 0);
            break;
        case AB_UI_POINTER_BOOLEAN:
            CHECK(words[1] < 2);
            output->boolean = words[1] != 0;
            break;
        case AB_UI_POINTER_NUMBER:
            output->number_bits = words[1];
            break;
        case AB_UI_POINTER_TEXT:
            CHECK(words[1] < 2);
            output->text = span(ui_tree_texts[words[1]]);
            break;
        case AB_UI_POINTER_OBJECT:
            output->object = words[1];
            break;
        case AB_UI_POINTER_FUNCTION:
            CHECK(words[1] == 0);
            output->function = 0;
            break;
    }
    return true;
}

static bool encode(AbUiPointerValue value, uint32_t output[2]) {
    CHECK(ab_ui_pointer_value_valid(&value));
    output[0] = (uint32_t)value.kind;
    output[1] = 0;
    if (value.kind == AB_UI_POINTER_BOOLEAN) {
        output[1] = value.boolean;
    } else if (value.kind == AB_UI_POINTER_NUMBER) {
        output[1] = value.number_bits;
    } else if (value.kind == AB_UI_POINTER_OBJECT) {
        CHECK(value.object <= UINT32_MAX);
        output[1] = (uint32_t)value.object;
    } else if (value.kind == AB_UI_POINTER_FUNCTION) {
        CHECK(value.function == 0);
    } else if (value.kind == AB_UI_POINTER_TEXT) {
        for (uint32_t index = 0; index < 2; ++index) {
            AbSpan candidate = span(ui_tree_texts[index]);
            if (candidate.size == value.text.size &&
                (!candidate.size ||
                 memcmp(candidate.data, value.text.data, candidate.size) == 0)) {
                output[1] = index;
                return true;
            }
        }
        CHECK(false);
    }
    return true;
}

static size_t membership(const AbUiFrameNode *parent, const AbUiFrameNode *child) {
    if (parent) {
        for (size_t index = 0; index < parent->state->child_count; ++index) {
            if (parent->children[index] == child) {
                return index + 1;
            }
        }
    }
    return 0;
}

static bool method(void *context, AbUiFrameNode *node, AbUiFrameMethod requested,
                   AbError *error) {
    (void)error;
    Model *model = context;
    CHECK(model->count < 16 && id(model, node) >= 1 && id(model, node) <= 8);
    uint32_t *event = model->events[model->count++];
    memset(event, 0, sizeof(model->events[0]));
    event[0] = (uint32_t)requested + 1;
    event[1] = id(model, node);
    CHECK(encode(node->entered, event + 2));
    event[4] = id(model, node->parent);
    event[5] = (uint32_t)node->state->child_count;
    CHECK(encode(model->lifecycle.entry_counts, event + 6));
    event[8] = node->parent ? (uint32_t)node->parent->state->child_count : 0;
    event[9] = (uint32_t)membership(node->parent, node);
    if (model->fail_at && model->fail_at == model->count) {
        return false;
    }
    if (requested != AB_UI_FRAME_LAYOUT) {
        node->entered = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                           .boolean = requested == AB_UI_FRAME_ENTRY};
    }
    return true;
}

static bool reset(Model *model, size_t node) {
    AbUiFrameFields arguments = {
        .name = {.kind = AB_UI_POINTER_TEXT, .text = span(ui_tree_names[node])}};
    AbUiInitBoundaries boundaries = {0};
    return ab_ui_frame_init(model->states + node, &arguments, &boundaries, NULL);
}

static bool initialize(Model *model) {
    memset(model, 0, sizeof(*model));
    for (size_t index = 0; index < 8; ++index) {
        CHECK(reset(model, index));
        model->nodes[index] = (AbUiFrameNode){.state = model->states + index,
                                              .children = model->children[index],
                                              .capacity = 8,
                                              .context = model,
                                              .method = method};
    }
    return true;
}

static bool edges(void) {
    Model model;
    CHECK(initialize(&model));
    AbUiFrameNode *a = model.nodes, *b = model.nodes + 1, *child = model.nodes + 2;
    CHECK(ab_ui_frame_add_child(a, child, 0, false, NULL) && child->parent == a &&
          a->state->child_count == 1 && a->state->lookup_count == 1 && !model.count);
    CHECK(ab_ui_frame_enter(a, &model.lifecycle, NULL) && a->entered.boolean &&
          child->entered.boolean &&
          model.lifecycle.entry_counts.number_bits == 0x3f800000 && model.count == 1 &&
          model.events[0][6] == AB_UI_POINTER_NUMBER &&
          model.events[0][7] == 0x3f800000);
    model.count = 0;
    CHECK(ab_ui_frame_add_child(b, child, 1, true, NULL) && child->parent == b &&
          membership(a, child) == 1 && membership(b, child) == 1);
    CHECK(ab_ui_frame_remove_child(a, child, NULL) && child->parent == b &&
          a->state->child_count == 0 && !child->entered.boolean && model.count == 1 &&
          model.events[0][4] == 2 && model.events[0][9] == 1);
    CHECK(ab_ui_frame_remove_self(child, NULL) && child->parent == b &&
          !b->state->child_count && !ab_ui_frame_remove_self(child, NULL));
    model.count = 0;
    a->entered = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER};
    CHECK(ab_ui_frame_add_child(a, child, 1, true, NULL) && model.count == 2 &&
          model.events[0][0] == 1 && model.events[1][0] == 3);
    model.count = 0;
    CHECK(!ab_ui_frame_add_child(a, child, 1, true, NULL) && !model.count &&
          a->state->child_count == 1);
    a->entered = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN};
    CHECK(ab_ui_frame_clear_children(a, NULL) && !a->state->child_count &&
          child->entered.boolean && child->parent == a && !model.count);
    CHECK(ab_ui_frame_exit(a, &model.lifecycle, NULL) &&
          model.lifecycle.entry_counts.number_bits == 0);
    CHECK(ab_ui_frame_exit(a, &model.lifecycle, NULL) &&
          model.lifecycle.entry_counts.number_bits == 0xbf800000);
    model.count = 0;
    a->entered = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN, .boolean = true};
    child->entered = (AbUiPointerValue){0};
    model.fail_at = 1;
    CHECK(!ab_ui_frame_add_child(a, child, 1, true, NULL) &&
          a->state->child_count == 1 && child->parent == a &&
          child->entered.kind == AB_UI_POINTER_NONE && model.count == 1);
    model.count = model.fail_at = 0;
    CHECK(reset(&model, 0) && !a->state->child_count && child->parent == a &&
          a->entered.boolean);
    AbUiFrameNode *output = child;
    CHECK(!ab_ui_frame_pop_child(a, &output, NULL) && output == child);
    AbSpan path[] = {span("missing")};
    CHECK(ab_ui_frame_find_child(a, path, 1, &output, NULL) && !output);
    CHECK(!ab_ui_frame_find_child(a, path, 0, &output, NULL));
    puts("Frame hierarchy order/lookup, shared membership, retained parents, lifecycle "
         "counter, truthy gates, reset and failed-callback prefix checks pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool run(Model *model, const uint32_t command[8], AbUiFrameNode **returned) {
    CHECK(command[0] < 13 && command[1] < 8);
    AbUiFrameNode *node = model->nodes + command[1];
    *returned = NULL;
    model->count = 0;
    switch (command[0]) {
        case UI_TREE_ADD:
            CHECK(command[2] < 8 && command[3] < 2);
            return ab_ui_frame_add_child(node, model->nodes + command[2], command[4],
                                         command[3] != 0, NULL);
        case UI_TREE_REMOVE:
            CHECK(command[2] < 8);
            return ab_ui_frame_remove_child(node, model->nodes + command[2], NULL);
        case UI_TREE_CLEAR:
            return ab_ui_frame_clear_children(node, NULL);
        case UI_TREE_POP:
            return ab_ui_frame_pop_child(node, returned, NULL);
        case UI_TREE_REMOVE_SELF:
            return ab_ui_frame_remove_self(node, NULL);
        case UI_TREE_ENTER:
            return ab_ui_frame_enter(node, &model->lifecycle, NULL);
        case UI_TREE_EXIT:
            return ab_ui_frame_exit(node, &model->lifecycle, NULL);
        case UI_TREE_LAYOUT:
            return ab_ui_frame_layout_children(node, NULL);
        case UI_TREE_CHILD: {
            CHECK(command[7] >= 1 && command[7] <= 4);
            AbSpan path[4];
            for (size_t index = 0; index < command[7]; ++index) {
                CHECK(command[2 + index] < 8);
                path[index] = span(ui_tree_names[command[2 + index]]);
            }
            return ab_ui_frame_find_child(node, path, command[7], returned, NULL);
        }
        case UI_TREE_PARENT:
            *returned = node->parent;
            return true;
        case UI_TREE_SET_ENTERED:
            return decode(command + 5, &node->entered);
        case UI_TREE_SET_COUNTER:
            return decode(command + 5, &model->lifecycle.entry_counts);
        case UI_TREE_RESET:
            return reset(model, command[1]);
    }
    return false;
}

static bool compare_snapshot(FILE *file, Model *model, Counts *counts) {
    uint32_t actual_counter[2], expected_counter[2];
    CHECK(encode(model->lifecycle.entry_counts, actual_counter) &&
          words(file, expected_counter, 2) &&
          memcmp(actual_counter, expected_counter, sizeof(actual_counter)) == 0);
    for (size_t index = 0; index < 8; ++index) {
        const AbUiFrameNode *node = model->nodes + index;
        uint32_t actual[4], expected[4];
        CHECK(encode(node->entered, actual));
        actual[2] = id(model, node->parent);
        actual[3] = (uint32_t)node->state->child_count;
        CHECK(words(file, expected, 4) &&
              memcmp(actual, expected, sizeof(actual)) == 0 &&
              node->state->child_count == node->state->lookup_count && actual[3] <= 8);
        if (actual[3]) {
            uint32_t order[8];
            CHECK(words(file, order, actual[3]));
            for (size_t child = 0; child < actual[3]; ++child) {
                CHECK(order[child] == id(model, node->children[child]));
            }
        }
        uint32_t lookup[16];
        CHECK(words(file, lookup, 16));
        for (size_t child = 0; child < 8; ++child) {
            AbUiFrameNode *found;
            AbSpan requested = span(ui_tree_names[child]);
            CHECK(ab_ui_frame_find_child(model->nodes + index, &requested, 1, &found,
                                         NULL) &&
                  lookup[child * 2] == id(model, found) &&
                  lookup[child * 2 + 1] == membership(node, model->nodes + child));
        }
        counts->stale_parent_snapshots +=
            node->parent && !membership(node->parent, node);
        size_t memberships = 0;
        for (size_t parent = 0; parent < 8; ++parent) {
            memberships += membership(model->nodes + parent, node) != 0;
        }
        counts->shared_memberships += memberships > 1;
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_UI_FRAME_TREE_V2 32 128\n") == 0);
    Counts counts = {0};
    for (size_t instance = 0; instance < 32; ++instance) {
        Model model;
        CHECK(initialize(&model));
        for (size_t step = 0; step < 128; ++step) {
            uint32_t command[8], metadata[2];
            CHECK(words(file, command, 8));
            AbUiFrameNode *returned;
            if (!run(&model, command, &returned)) {
                fprintf(stderr, "Frame tree command %zu/%zu op %u failed\n", instance,
                        step, command[0]);
                return false;
            }
            CHECK(words(file, metadata, 2) && metadata[0] == id(&model, returned) &&
                  metadata[1] == model.count);
            for (size_t event = 0; event < model.count; ++event) {
                uint32_t expected[10];
                CHECK(words(file, expected, 10) &&
                      memcmp(expected, model.events[event], sizeof(expected)) == 0);
            }
            CHECK(compare_snapshot(file, &model, &counts));
            ++counts.commands[command[0]];
            counts.events += model.count;
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB UI frame tree reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 original Frame hierarchy/lifecycle commands and %zu ordered methods "
           "match all child/lookup/parent/entered/counter words; %zu stale-parent and "
           "%zu shared-membership snapshots.\n",
           counts.events, counts.stale_parent_snapshots, counts.shared_memberships);
    printf("Operations add/remove/clear/pop/self/entry/exit/layout/path/parent/entered/"
           "counter/reset:");
    for (size_t index = 0; index < 13; ++index) {
        printf(" %zu", counts.commands[index]);
    }
    putchar('\n');
    return true;
}

int main(int argc, char **argv) {
    if (!edges()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
