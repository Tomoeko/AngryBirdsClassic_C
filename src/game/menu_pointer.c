#include "ab/menu_pointer.h"

#include <math.h>

bool ab_menu_pointer_filter(const AbMenuPointerRoute *routes, size_t count,
                            const AbMenuPointerInput *input,
                            AbMenuPointerResult *output, AbError *error) {
    if ((count && !routes) || !input || !output || (input->events & ~15u) ||
        !isfinite(input->cursor.x) || !isfinite(input->cursor.y)) {
        return ab_fail(error, "Invalid menu pointer routes or input");
    }
    AbMenuPointerResult result = {.present = 1u << AB_MENU_POINTER_WHEEL};
    if (!input->allow_input) {
        *output = result;
        return true;
    }
    result.present |= input->events & 7u;
    result.copy_touches = true;
    for (unsigned event = 0; event < 4; ++event) {
        uint32_t bit = 1u << event;
        if (!(input->events & bit)) {
            continue;
        }
        bool consumed = false;
        for (size_t index = 0; index < count && !consumed; ++index) {
            if (routes[index].callback &&
                !routes[index].callback(routes[index].context,
                                        (AbMenuPointerEvent)event, input->cursor,
                                        &consumed, error)) {
                return false;
            }
        }
        if (!consumed) {
            result.accepted |= bit;
        }
        result.copy_touches = result.copy_touches && !consumed;
    }
    *output = result;
    return true;
}
