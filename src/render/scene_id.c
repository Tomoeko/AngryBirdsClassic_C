#include "ab/scene_id.h"

#include <string.h>
#include <math.h>

bool ab_scene_id_next(uint32_t *counter, AbSceneId *output, AbError *error) {
    if (!counter || !output)
        return ab_fail(error, "Missing scene identifier storage");
    uint32_t next = *counter + 1;
    AbSceneId result;
    memcpy(&result.value, &next, sizeof(next));
    result.script_number = (float)result.value;
    *counter = next;
    *output = result;
    return true;
}

int32_t ab_scene_id_integer(float number) {
    if (isnan(number))
        return 0;
    if (number >= 2147483648.0f)
        return INT32_MAX;
    if (number <= -2147483648.0f)
        return INT32_MIN;
    return (int32_t)number;
}
