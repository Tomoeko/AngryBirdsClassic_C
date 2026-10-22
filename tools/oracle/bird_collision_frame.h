#ifndef AB_ORACLE_BIRD_COLLISION_FRAME_H
#define AB_ORACLE_BIRD_COLLISION_FRAME_H

/* Read-only ARM32 frame check, recovered from original lua_getstack at ELF
 * 0x83b964 and lua_getinfo at 0x83bbf8. lua_State.ci/base_ci are +0x14/+0x28;
 * CallInfo is 24 bytes, with func at +4 and savedpc at +12. A Lua closure's
 * Proto is +16, with code at +12 and sizecode at +44. Original getinfo computes
 * the zero-based current instruction as (savedpc - code) / 4 - 1.
 *
 * Use only inside a C boundary called directly by the unchanged original bird
 * callback. Validate its closure/prototype/code fingerprint before deriving PC.
 * No original state, instruction, frame or register is changed. */
static uint32_t bird_collision_caller_pc(void *lua) {
    const uint32_t *state = lua;
    uintptr_t ci_address = state[5], base_address = state[10];
    if (!ci_address || !base_address || ci_address < base_address + 24u ||
        (ci_address - base_address) % 24u != 0) {
        fail("Unexpected original bird callback call-frame bounds\n");
    }
    const uint32_t *caller = (const uint32_t *)(ci_address - 24u);
    const uint32_t *function = (const uint32_t *)(uintptr_t)caller[1];
    if (!function || function[1] != 6 || !function[0]) {
        fail("Missing original bird callback caller function\n");
    }
    const uint32_t *closure = (const uint32_t *)(uintptr_t)function[0];
    if (((const uint8_t *)closure)[6] != 0 || ((const uint8_t *)closure)[7] != 1 ||
        !closure[4]) {
        fail("Unexpected original bird callback closure ABI\n");
    }
    const uint32_t *prototype = (const uint32_t *)(uintptr_t)closure[4];
    if (prototype[11] != 974 || !prototype[3]) {
        fail("Unexpected original bird callback prototype ABI\n");
    }
    const uint32_t *code = (const uint32_t *)(uintptr_t)prototype[3];
    uintptr_t saved = caller[3], begin = prototype[3];
    if (code[0] != UINT32_C(0x00000205) || code[973] != UINT32_C(0x0080001e) ||
        saved <= begin || saved > begin + 974u * 4u || (saved - begin) % 4u != 0) {
        fail("Unexpected original bird callback saved instruction\n");
    }
    return (uint32_t)((saved - begin) / 4u - 1u);
}

#endif
