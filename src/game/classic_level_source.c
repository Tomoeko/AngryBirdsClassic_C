#include "ab/classic_level_source.h"
#include "ab/hash.h"

#include <stdlib.h>
#include <string.h>

struct AbClassicLevelSource {
    const AbZip *archive;
    AbSpan input;
    AbTargetData catalog;
    uint8_t key[32];
};

bool ab_classic_level_source_open(const AbZip *archive, size_t allocation_limit,
                                  AbClassicLevelSource **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing shared level-source output");
    *output = NULL;
    if (!archive || !archive->input.data ||
        allocation_limit < sizeof(AbClassicLevelSource))
        return ab_fail(error, "Invalid shared level-source archive/storage");
    uint8_t digest[32];
    char hash[65];
    if (!ab_sha256(archive->input, digest, error))
        return false;
    ab_digest_hex(digest, hash);
    if (strcmp(hash,
               "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad"))
        return ab_fail(error, "Shared level source requires the pinned APK");
    AbClassicLevelSource *source = calloc(1, sizeof(*source));
    if (!source)
        return ab_fail(error, "Cannot allocate shared level source");
    source->archive = archive;
    source->input = archive->input;
    size_t remaining = allocation_limit - sizeof(*source);
    if (!ab_target_resource_key(archive, remaining, source->key, error) ||
        !ab_target_data_open(archive, "assets/data/scripts/blocks.lua",
                             (AbSpan){source->key, sizeof(source->key)}, remaining,
                             NULL, &source->catalog, error)) {
        ab_classic_level_source_close(source);
        return false;
    }
    *output = source;
    return true;
}

void ab_classic_level_source_close(AbClassicLevelSource *source) {
    if (!source)
        return;
    ab_target_data_close(&source->catalog);
    memset(source->key, 0, sizeof(source->key));
    free(source);
}

size_t ab_classic_level_source_bytes(const AbClassicLevelSource *source) {
    return source ? sizeof(*source) + source->catalog.bytes.size +
                        source->catalog.chunk.allocated_bytes +
                        source->catalog.data.allocated_bytes
                  : 0;
}

bool ab_classic_level_source_view(const AbClassicLevelSource *source,
                                  const AbZip *archive, AbClassicLevelSourceView *view,
                                  AbError *error) {
    if (!source || !archive || !view || source->archive != archive ||
        source->input.data != archive->input.data ||
        source->input.size != archive->input.size)
        return ab_fail(error, "Shared level source does not own this archive identity");
    *view = (AbClassicLevelSourceView){.catalog = &source->catalog.data,
                                       .key = {source->key, sizeof(source->key)}};
    return true;
}
