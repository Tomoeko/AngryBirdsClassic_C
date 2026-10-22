#ifndef AB_LUA_TEXT_WRITE_H
#define AB_LUA_TEXT_WRITE_H

#include "ab/lua_data.h"

typedef struct {
    uint8_t *data;
    size_t size, capacity;
    bool writing, failed;
} AbLuaTextWriter;

typedef struct {
    void *context;
    /* Cursor begins at zero for each table. Emit actual source next() order;
     * advance on every present entry. This callback must not mutate the graph. */
    bool (*next)(void *context, const AbDataTable *table, size_t *cursor,
                 AbDataEntry *output, bool *present, AbError *error);
} AbLuaTextWriteOrder;

/* LuaTable.write/value/string-literal data serialization, with root global
 * assignments and nested constructors. Caller storage is fixed and output
 * appends at size. Preserve source six-digit %g numbers, escaped CString prefix,
 * compact ordered numeric prefixes, keyword/bracket keys, this/_G exclusions,
 * indentation and empty-table newlines. A missing order callback uses the
 * table's explicitly prepared entry order, not an inferred native hash order.
 *
 * Finite numbers, plain immutable tables, C numeric locale/nearest rounding and
 * at most32 nested owners are supported. Cycles and runtime objects reject or
 * skip at their source type gate. As in the target, an empty string key passes
 * the name check; this legacy case can emit text that is not valid Lua input.
 * Caller must retain all spans through the call. No VM, allocation, file I/O,
 * key acquisition or encryption. Failure retains output prefix and stops the
 * writer; invalid entry/reentry preserves the running owner. */
bool ab_lua_text_write(const AbDataTable *table, AbLuaTextWriteOrder order,
                       AbLuaTextWriter *writer, AbError *error);

#endif
