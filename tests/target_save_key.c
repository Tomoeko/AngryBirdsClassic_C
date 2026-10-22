#include "ab/target_resources.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2 || argc > 3)
        return 2;
    AbBuffer bytes = {0}, expected = {0};
    AbZip archive = {0};
    AbError error = {0};
    uint8_t resource[32], save[32], sentinel[32];
    memset(sentinel, 0xa5, sizeof(sentinel));
    memcpy(save, sentinel, sizeof(save));
    bool okay = false;
    if (ab_target_save_key(NULL, 16 * 1024 * 1024, save, NULL) ||
        memcmp(save, sentinel, sizeof(save)) ||
        !ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) ||
        !ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, resource, &error) ||
        !ab_target_save_key(&archive, 16 * 1024 * 1024, save, &error) ||
        !memcmp(resource, save, sizeof(save)))
        goto release;
    if (argc == 3 && (!ab_file_read(argv[2], 32, &expected, &error) ||
                      expected.size != 32 || memcmp(save, expected.data, sizeof(save))))
        goto release;
    puts(argc == 3 ? "All 32 save-key bytes match the original native result."
                   : "Pinned resource/save operands decode separately; invalid input "
                     "preserves output.");
    okay = true;
release:
    if (!okay)
        fprintf(stderr, "%s\n",
                error.message[0] ? error.message : "Target key comparison failed");
    memset(save, 0, sizeof(save));
    memset(resource, 0, sizeof(resource));
    ab_zip_close(&archive);
    ab_buffer_free(&expected);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
