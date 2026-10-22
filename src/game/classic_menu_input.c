#include "ab/classic_menu_input.h"

#include <string.h>

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool down(const AbDataTable *table) {
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING,
                                         .string = {(const uint8_t *)"LBUTTON", 7}});
    return value && truthy(*value);
}

bool ab_classic_menu_input_reset(AbClassicMenuInput *input, AbError *error) {
    if (!input || !input->initialized || input->calling || input->failed)
        return ab_fail(error, "Invalid or reentrant menu input reset");
    for (size_t index = 0; index < 3; ++index) {
        input->button[index].value = (AbDataValue){0};
        input->tables[index].count = 0;
    }
    input->wheel = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    input->count = 0;
    return true;
}

bool ab_classic_menu_input_initialize(AbClassicMenuInputConfig config,
                                      AbClassicMenuInput *output, AbError *error) {
    if (!output || !config.raw || !config.raw->pressed || !config.raw->held ||
        !config.raw->released || config.storage.capacity > UINT32_C(0x1000000) ||
        (config.storage.capacity &&
         (!config.storage.filtered || !config.storage.copied || !config.storage.keys)))
        return ab_fail(error, "Invalid concrete menu input storage/bindings");
    *output = (AbClassicMenuInput){.config = config, .initialized = true};
    for (size_t index = 0; index < 3; ++index) {
        output->button[index].key = (AbDataValue){
            .kind = AB_DATA_STRING, .string = {(const uint8_t *)"LBUTTON", 7}};
        output->tables[index] = (AbDataTable){output->button + index, 0, 1, NULL};
    }
    return ab_classic_menu_input_reset(output, error);
}

AbClassicMenuInputView ab_classic_menu_input_view(const AbClassicMenuInput *input) {
    return input && input->initialized
               ? (AbClassicMenuInputView){input->tables,
                                          input->tables + 1,
                                          input->tables + 2,
                                          input->wheel,
                                          input->config.storage.filtered,
                                          input->count}
               : (AbClassicMenuInputView){0};
}

static bool copy_touches(AbClassicMenuInput *input, const AbClassicTouch *source,
                         size_t count, AbClassicTouch *output, AbError *error) {
    AbClassicMenuInputStorage *storage = &input->config.storage;
    if (count > storage->capacity || (count && !source))
        return ab_fail(error, "Menu touch copy exceeds bound or lacks source");
    for (size_t index = 0; index < count; ++index) {
        if ((unsigned)source[index].consumed.kind >= (unsigned)AB_DATA_TABLE)
            return ab_fail(error, "Unsupported nested menu touch consumed value");
        storage->keys[index] = source[index].id;
    }
    AbIntegerOrderLayout layout;
    if (!ab_integer_order_growing(storage->keys, count, &storage->order, &layout,
                                  error))
        return false;
    for (size_t index = 0; index < count; ++index)
        output[index] = source[storage->order.indices[index]];
    return true;
}

static bool clicks(AbClassicMenuInput *input, AbUiMenuManager *manager,
                   AbError *error) {
    AbUiMenuManagerView menu;
    if (!ab_ui_menu_manager_view(manager, &menu))
        return ab_fail(error, "Missing actual menu manager for pointer filtering");
    if (!ab_ui_pointer_value_truthy(&menu.allow_input))
        return true;
    const AbClassicMenuRawInput *raw = input->config.raw;
    const char *const events[] = {"LPRESS", "LHOLD", "LRELEASE", "WHEEL"};
    bool all_accepted = true;
    for (size_t phase = 0; phase < 4; ++phase) {
        const AbDataTable *table = phase == 0   ? raw->pressed
                                   : phase == 1 ? raw->held
                                                : raw->released;
        if (phase < 3 && !table)
            return ab_fail(error, "Missing reached raw menu key table");
        if (!(phase == 3 ? truthy(raw->wheel) : down(table)))
            continue;
        bool accepted = false;
        AbSpan event = {(const uint8_t *)events[phase], strlen(events[phase])};
        if (!ab_ui_menu_pointer(manager, event, raw->cursor, input->config.pointer,
                                input->config.context, &accepted, error))
            return false;
        AbDataValue value = {.kind = AB_DATA_BOOLEAN, .bits = accepted};
        if (phase == 3)
            input->wheel = value;
        else {
            input->button[phase].value = value;
            input->tables[phase].count = 1;
        }
        all_accepted = all_accepted && accepted;
    }
    if (all_accepted && !copy_touches(input, raw->touches, raw->touch_count,
                                      input->config.storage.filtered, error))
        return false;
    input->count = all_accepted ? raw->touch_count : 0;
    if (truthy(raw->extra_mouse_events) &&
        (!input->config.extra_mouse ||
         !input->config.extra_mouse(input->config.context, manager, error)))
        return input->config.extra_mouse
                   ? false
                   : ab_fail(error, "Missing reached extra mouse button service");
    if (truthy(raw->hover_scaling) &&
        (!input->config.hover ||
         !input->config.hover(input->config.context, manager, error)))
        return input->config.hover
                   ? false
                   : ab_fail(error, "Missing reached menu hover service");
    return true;
}

bool ab_classic_menu_input_clicks(AbClassicMenuInput *input, AbUiMenuManager *manager,
                                  AbError *error) {
    if (!input || !input->initialized || input->calling || input->failed)
        return ab_fail(error, "Invalid or reentrant menu pointer filtering");
    input->calling = true;
    bool okay = clicks(input, manager, error);
    input->calling = false;
    input->failed = !okay;
    return okay;
}

bool ab_classic_menu_input_copy_touches(AbClassicMenuInput *input,
                                        const AbClassicTouch **output, size_t *count,
                                        AbError *error) {
    if (!input || !input->initialized || input->calling || input->failed || !output ||
        !count)
        return ab_fail(error, "Invalid or reentrant filtered touch copy");
    input->calling = true;
    bool okay = copy_touches(input, input->config.storage.filtered, input->count,
                             input->config.storage.copied, error);
    if (okay) {
        *output = input->config.storage.copied;
        *count = input->count;
    }
    input->calling = false;
    input->failed = !okay;
    return okay;
}
