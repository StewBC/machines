// 6502 assembler
// Stefan Wessels, 2025
// This is free and unencumbered software released into the public domain.

#pragma once

#include <stddef.h>
#include <stdint.h>

struct ASM_FILE;

/* Pass-2 record. `file` is live only until assembler_shutdown. */
typedef struct asm_recorded_span {
    uint32_t start;
    uint32_t end;
    struct ASM_FILE *file;
    uint32_t line;
    uint32_t target_index;
} asm_recorded_span;

typedef struct asm_source_file {
    char *display_name;
    char *text;
    size_t size;
} asm_source_file;

typedef struct asm_source_span {
    uint32_t start;
    uint32_t end;
    uint32_t file_index;
    uint32_t line;
    uint32_t target_index;
} asm_source_span;

typedef struct asm_source_image {
    asm_source_file *files;
    size_t file_count;
    asm_source_span *spans;
    size_t span_count;
} asm_source_image;

typedef struct asm_source_hit {
    const asm_source_image *image;
    const asm_source_span *span;
} asm_source_hit;

typedef struct asm_source_publish {
    asm_source_image *session;
    asm_source_image **file_maps;
    size_t file_map_count;
    uint32_t generation;
} asm_source_publish;

enum {
    ASM_SOURCE_VIEW_NONE = 0,
    ASM_SOURCE_VIEW_SESSION = 1,
    ASM_SOURCE_VIEW_FILE = 2
};

/* Fields the disassembly slot keeps beside the byte-listing cursor.
   source_top_line is the 0-based index of the first visible source line. */
typedef struct asm_source_view_state {
    int follow_pc;
    uint32_t which;
    uint32_t map_index;
    uint32_t file_index;
    uint32_t cursor_line;
    uint32_t top_line;
    uint32_t source_generation;
} asm_source_view_state;

void asm_source_image_free(asm_source_image *image);
void asm_source_publish_release(asm_source_publish *publish);

/* Decision 13. *out is malloc'd and ends in .srcmap. 0 on success. */
int asm_source_map_path(const char *symbol_path, char **out);
/* 0 on success. A NULL image or a failed open leaves no partial file claim. */
int asm_source_map_write(const asm_source_image *image, const char *path);
/* 0 and a new image on success. Missing or rejected: *out NULL and non-zero. */
int asm_source_map_read(const char *path, asm_source_image **out);
int asm_source_image_clone(const asm_source_image *src, asm_source_image **out);
int asm_source_publish_clone(const asm_source_publish *src, asm_source_publish *dst);

asm_source_hit asm_source_span_at_pc(
    const asm_source_image *session,
    const asm_source_image *const *file_maps,
    size_t file_map_count,
    uint16_t pc);

uint16_t asm_source_line_gutter(
    const asm_source_image *image,
    uint32_t file_index,
    uint32_t line,
    uint16_t prefer_pc,
    int *has_span);

/* 1-based line slice. Breaks match file_read_line. *start points into text. */
int asm_source_line_slice(
    const char *text,
    size_t size,
    uint32_t line,
    const char **start,
    size_t *length);
size_t asm_source_line_count(const char *text, size_t size);
size_t asm_source_file_line_count(const asm_source_image *image, uint32_t file_index);

/* "%s %c %s" row, or an empty string for an empty source line. */
void asm_source_format_row(
    const asm_source_image *image,
    uint32_t file_index,
    uint32_t line,
    int has_pc,
    uint16_t pc,
    char *out,
    size_t out_size);

const asm_source_image *asm_source_view_image(
    const asm_source_publish *publish,
    uint32_t which,
    uint32_t map_index);

/* Running forces follow. A generation mismatch applies the initial-file rule,
   then follow recenters on the same frame. Stores source_generation after that. */
const asm_source_image *asm_source_view_sync(
    const asm_source_publish *publish,
    asm_source_view_state *view,
    int running,
    int has_pc,
    uint16_t pc,
    uint32_t visible_rows);
