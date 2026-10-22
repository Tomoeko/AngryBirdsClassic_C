#include "ab/aes.h"
#include "ab/lua_data_json.h"
#include "ab/lua_text.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    bool verify = argc == 4 && strcmp(argv[1], "--verify-encryption") == 0;
    if (argc != 3 && !verify) {
        fprintf(stderr, "Usage: ab_save_data [--verify-encryption] "
                        "PRIVATE_SAVE_KEY_FILE ENCRYPTED_SAVE\n");
        return 2;
    }
    size_t remaining = 32 * 1024 * 1024;
    AbBuffer key = {0}, encrypted = {0}, decrypted = {0}, reencrypted = {0};
    AbLuaTextData data = {0};
    AbError error = {0};
    bool okay = false;
    if (!ab_file_read(argv[verify ? 2 : 1], 32, &key, &error)) {
        goto cleanup;
    }
    if (key.size != 32) {
        ab_fail(&error, "The private save key must contain exactly 32 raw bytes");
        goto cleanup;
    }
    remaining -= key.size;
    if (!ab_file_read(argv[verify ? 3 : 2], remaining, &encrypted, &error)) {
        goto cleanup;
    }
    remaining -= encrypted.size;
    /* Decryption allocates the full cipher length, including removed padding. */
    if (encrypted.size > remaining) {
        ab_fail(&error, "Save decryption exceeds aggregate memory limit");
        goto cleanup;
    }
    remaining -= encrypted.size;
    if (!ab_asset_decrypt((AbSpan){encrypted.data, encrypted.size},
                          (AbSpan){key.data, key.size}, &decrypted, &error) ||
        !ab_lua_text_data((AbSpan){decrypted.data, decrypted.size}, remaining, &data,
                          &error)) {
        goto cleanup;
    }
    remaining -= data.data.allocated_bytes;
    if (verify) {
        if (decrypted.size > SIZE_MAX - 16 ||
            decrypted.size + 16 - decrypted.size % 16 > remaining) {
            ab_fail(&error, "Save encryption exceeds aggregate memory limit");
            goto cleanup;
        }
        if (!ab_asset_encrypt((AbSpan){decrypted.data, decrypted.size},
                              (AbSpan){key.data, key.size}, &reencrypted, &error)) {
            goto cleanup;
        }
        if (encrypted.size != reencrypted.size ||
            memcmp(encrypted.data, reencrypted.data, encrypted.size) != 0) {
            ab_fail(&error, "First-party encryption differs from captured ciphertext");
            goto cleanup;
        }
        printf("All %zu encrypted save bytes match.\n", encrypted.size);
    } else if (!ab_lua_data_json(&data.data, stdout, &error)) {
        goto cleanup;
    }
    okay = true;

cleanup:
    if (fflush(stdout) != 0 || ferror(stdout)) {
        okay = ab_fail(&error, "Cannot flush save data output");
    }
    if (!okay) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_lua_text_data_close(&data);
    ab_buffer_free(&reencrypted);
    ab_buffer_free(&decrypted);
    ab_buffer_free(&encrypted);
    ab_buffer_free(&key);
    return okay ? 0 : 1;
}
