#pragma once

#include "apple2.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* LE fourcc 'A2ST' */
#define A2_SNAPSHOT_MAGIC 0x41325354u
#define A2_SNAPSHOT_VERSION 3u
#define A2_SNAPSHOT_VERSION_MIN 1u

typedef enum a2_snapshot_content_mode {
    A2_SNAPSHOT_CONTENT_REFERENCED = 1
} a2_snapshot_content_mode;

enum {
    A2_SNAPSHOT_FLAG_EXTERNAL_MEDIA_REFERENCES = 0x00000001u
};

/*
 * Chunked little-endian machine snapshot (c64m-shaped).
 * Does not serialize host pointers, page maps, framebuffer, paste, or
 * write_history. After load, banking maps are rebuilt from soft switches.
 *
 * Media (Disk II / SmartPort) is path-referenced. Missing media on load is a
 * hard failure. Dirty Disk II images are flushed to their files before save.
 *
 * RAM_: //e always full 128K + 32K LC. ][+ (v3) stores only main 64K + main
 * LC 16K — there is no aux. Load still accepts sparse or full layouts;
 * omitted aux restores the cold-reset baseline. v1/v2 are always full.
 */
size_t apple2_snapshot_size(const apple2_t *m);
size_t apple2_snapshot_save(const apple2_t *m, uint8_t *out, size_t out_cap);
bool apple2_snapshot_load(apple2_t *m, const uint8_t *in, size_t in_len);

/* Flush dirty file-backed Disk II images (call before save). Returns false if
   a dirty image could not be flushed. Mutates media only (not CPU/RAM). */
bool apple2_snapshot_flush_media(apple2_t *m);
