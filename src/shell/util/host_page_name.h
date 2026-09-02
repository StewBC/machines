#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct host_page_name_state {
    char last_stem[16]; /* "YYYYMMDD-HHMMSS" or empty */
    uint8_t seq;        /* 0..99 last used XX for last_stem */
} host_page_name_state;

/* Fills stem[16] with YYYYMMDD-HHMMSS. false on clock failure.
 * Public for tests; production flush path goes through build_path. */
bool host_page_name_stem_now(char stem[16]);

/*
 * Obtains "now" via stem_now internally.
 * Builds path as snprintf("%s/%s%02u.%s", dir, stem, xx, ext)
 * — XX is zero-padded 00..99 concatenated to the stem (c64m bit-identical).
 * ext is WITHOUT a dot, e.g. "bmp" → file ends in ".bmp".
 * Does NOT mutate st; on success fills out_stem/out_xx for a later commit.
 * Returns false on clock failure, seq exhaustion (would exceed 99), or
 * snprintf fail / bad args.
 */
bool host_page_name_build_path(
    const host_page_name_state *st,
    const char *dir,
    const char *ext,
    char *path,
    size_t path_sz,
    char out_stem[16],
    uint8_t *out_xx);

/* Call ONLY after host_page_writer_write succeeds. */
void host_page_name_commit(
    host_page_name_state *st,
    const char stem[16],
    uint8_t xx);

#ifdef __cplusplus
}
#endif
