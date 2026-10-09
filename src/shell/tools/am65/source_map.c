// 6502 assembler
// Stefan Wessels, 2025
// This is free and unencumbered software released into the public domain.

#include "asm_lib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *source_dup(const char *text, size_t length) {
    char *copy = (char *)malloc(length + 1u);
    if (copy == NULL) {
        return NULL;
    }
    if (length > 0u && text != NULL) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

static int line_next(
    const char *text,
    size_t size,
    size_t *pos,
    const char **start,
    size_t *length) {
    size_t i;

    if (text == NULL || pos == NULL || *pos >= size || text[*pos] == '\0') {
        return 0;
    }
    *start = text + *pos;
    i = *pos;
    while (i < size && text[i] != '\0' && text[i] != '\n' && text[i] != '\r') {
        i++;
    }
    *length = i - *pos;
    if (i < size && text[i] == '\r' && i + 1u < size && text[i + 1u] == '\n') {
        i += 2u;
    } else if (i < size && (text[i] == '\r' || text[i] == '\n')) {
        i++;
    }
    *pos = i;
    return 1;
}

int asm_source_line_slice(
    const char *text,
    size_t size,
    uint32_t line,
    const char **start,
    size_t *length) {
    size_t pos = 0;
    uint32_t number = 0;
    const char *row = NULL;
    size_t row_len = 0;

    if (start != NULL) {
        *start = NULL;
    }
    if (length != NULL) {
        *length = 0;
    }
    if (text == NULL || line == 0u) {
        return 0;
    }
    while (line_next(text, size, &pos, &row, &row_len)) {
        number++;
        if (number == line) {
            if (start != NULL) {
                *start = row;
            }
            if (length != NULL) {
                *length = row_len;
            }
            return 1;
        }
    }
    return 0;
}

size_t asm_source_line_count(const char *text, size_t size) {
    size_t pos = 0;
    size_t count = 0;
    const char *row;
    size_t row_len;

    while (line_next(text, size, &pos, &row, &row_len)) {
        count++;
    }
    return count;
}

size_t asm_source_file_line_count(const asm_source_image *image, uint32_t file_index) {
    if (image == NULL || file_index >= image->file_count) {
        return 0;
    }
    return asm_source_line_count(image->files[file_index].text, image->files[file_index].size);
}

void asm_source_image_free(asm_source_image *image) {
    size_t i;

    if (image == NULL) {
        return;
    }
    for (i = 0; i < image->file_count; i++) {
        free(image->files[i].display_name);
        free(image->files[i].text);
    }
    free(image->files);
    free(image->spans);
    free(image);
}

void asm_source_publish_release(asm_source_publish *publish) {
    size_t i;

    if (publish == NULL) {
        return;
    }
    asm_source_image_free(publish->session);
    for (i = 0; i < publish->file_map_count; i++) {
        asm_source_image_free(publish->file_maps[i]);
    }
    free(publish->file_maps);
    publish->session = NULL;
    publish->file_maps = NULL;
    publish->file_map_count = 0;
    publish->generation = 0;
}

static int clone_file(const asm_source_file *src, asm_source_file *dst) {
    dst->display_name = source_dup(src->display_name != NULL ? src->display_name : "", src->display_name != NULL ? strlen(src->display_name) : 0);
    dst->text = source_dup(src->text, src->size);
    dst->size = src->size;
    if (dst->display_name == NULL || dst->text == NULL) {
        free(dst->display_name);
        free(dst->text);
        dst->display_name = NULL;
        dst->text = NULL;
        return 0;
    }
    return 1;
}

int asm_source_image_clone(const asm_source_image *src, asm_source_image **out) {
    asm_source_image *image;
    size_t i;

    if (out == NULL) {
        return ASM_ERR;
    }
    *out = NULL;
    if (src == NULL) {
        return ASM_OK;
    }
    image = (asm_source_image *)calloc(1, sizeof(*image));
    if (image == NULL) {
        return ASM_ERR;
    }
    if (src->file_count > 0u) {
        image->files = (asm_source_file *)calloc(src->file_count, sizeof(*image->files));
        if (image->files == NULL) {
            asm_source_image_free(image);
            return ASM_ERR;
        }
        image->file_count = src->file_count;
        for (i = 0; i < src->file_count; i++) {
            if (!clone_file(&src->files[i], &image->files[i])) {
                asm_source_image_free(image);
                return ASM_ERR;
            }
        }
    }
    if (src->span_count > 0u) {
        image->spans = (asm_source_span *)malloc(src->span_count * sizeof(*image->spans));
        if (image->spans == NULL) {
            asm_source_image_free(image);
            return ASM_ERR;
        }
        memcpy(image->spans, src->spans, src->span_count * sizeof(*image->spans));
        image->span_count = src->span_count;
    }
    *out = image;
    return ASM_OK;
}

int asm_source_publish_clone(const asm_source_publish *src, asm_source_publish *dst) {
    asm_source_publish built;
    size_t i;

    if (dst == NULL) {
        return ASM_ERR;
    }
    memset(&built, 0, sizeof(built));
    if (src == NULL) {
        asm_source_publish_release(dst);
        return ASM_OK;
    }
    if (src->session != NULL && asm_source_image_clone(src->session, &built.session) != ASM_OK) {
        asm_source_publish_release(&built);
        return ASM_ERR;
    }
    if (src->file_map_count > 0u) {
        built.file_maps = (asm_source_image **)calloc(src->file_map_count, sizeof(*built.file_maps));
        if (built.file_maps == NULL) {
            asm_source_publish_release(&built);
            return ASM_ERR;
        }
        built.file_map_count = src->file_map_count;
        for (i = 0; i < src->file_map_count; i++) {
            if (asm_source_image_clone(src->file_maps[i], &built.file_maps[i]) != ASM_OK) {
                asm_source_publish_release(&built);
                return ASM_ERR;
            }
        }
    }
    built.generation = src->generation;
    asm_source_publish_release(dst);
    *dst = built;
    return ASM_OK;
}

static const asm_source_span *last_cover(const asm_source_image *image, uint32_t pc) {
    const asm_source_span *hit = NULL;
    size_t i;

    if (image == NULL) {
        return NULL;
    }
    for (i = 0; i < image->span_count; i++) {
        const asm_source_span *span = &image->spans[i];
        if (span->start <= pc && pc < span->end) {
            hit = span;
        }
    }
    return hit;
}

asm_source_hit asm_source_span_at_pc(
    const asm_source_image *session,
    const asm_source_image *const *file_maps,
    size_t file_map_count,
    uint16_t pc) {
    asm_source_hit hit;
    size_t i;
    uint32_t address = pc;

    hit.image = NULL;
    hit.span = NULL;
    hit.span = last_cover(session, address);
    if (hit.span != NULL) {
        hit.image = session;
        return hit;
    }
    if (file_maps == NULL) {
        return hit;
    }
    for (i = 0; i < file_map_count; i++) {
        const asm_source_span *span = last_cover(file_maps[i], address);
        if (span != NULL) {
            hit.image = file_maps[i];
            hit.span = span;
            return hit;
        }
    }
    return hit;
}

uint16_t asm_source_line_gutter(
    const asm_source_image *image,
    uint32_t file_index,
    uint32_t line,
    uint16_t prefer_pc,
    int *has_span) {
    const asm_source_span *prefer = NULL;
    const asm_source_span *lowest = NULL;
    size_t i;
    uint32_t pc = prefer_pc;

    if (has_span != NULL) {
        *has_span = 0;
    }
    if (image == NULL) {
        return 0;
    }
    for (i = 0; i < image->span_count; i++) {
        const asm_source_span *span = &image->spans[i];
        if (span->file_index != file_index || span->line != line) {
            continue;
        }
        if (lowest == NULL || span->start < lowest->start) {
            lowest = span;
        }
        if (span->start <= pc && pc < span->end) {
            prefer = span;
        }
    }
    if (prefer == NULL && lowest == NULL) {
        return 0;
    }
    if (has_span != NULL) {
        *has_span = 1;
    }
    return (uint16_t)(prefer != NULL ? prefer->start : lowest->start);
}

void asm_source_format_row(
    const asm_source_image *image,
    uint32_t file_index,
    uint32_t line,
    int has_pc,
    uint16_t pc,
    char *out,
    size_t out_size) {
    const char *row = NULL;
    size_t row_len = 0;
    int has_span = 0;
    uint16_t gutter;
    char address[8];
    char mark;
    char text[1024];
    size_t copy;

    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    if (image == NULL || file_index >= image->file_count) {
        return;
    }
    if (!asm_source_line_slice(image->files[file_index].text, image->files[file_index].size, line, &row, &row_len)) {
        return;
    }
    if (row_len == 0u) {
        return;
    }
    copy = row_len < sizeof(text) - 1u ? row_len : sizeof(text) - 1u;
    memcpy(text, row, copy);
    text[copy] = '\0';
    gutter = asm_source_line_gutter(image, file_index, line, pc, &has_span);
    if (has_span) {
        snprintf(address, sizeof(address), "%04X", gutter);
    } else {
        memcpy(address, "    ", 5);
    }
    mark = ' ';
    if (has_pc && has_span) {
        size_t i;
        for (i = 0; i < image->span_count; i++) {
            const asm_source_span *span = &image->spans[i];
            if (span->file_index == file_index && span->line == line &&
                span->start <= pc && pc < span->end) {
                mark = '>';
            }
        }
    }
    snprintf(out, out_size, "%s %c %s", address, mark, text);
}

const asm_source_image *asm_source_view_image(
    const asm_source_publish *publish,
    uint32_t which,
    uint32_t map_index) {
    if (publish == NULL) {
        return NULL;
    }
    if (which == ASM_SOURCE_VIEW_SESSION) {
        return publish->session;
    }
    if (which == ASM_SOURCE_VIEW_FILE && map_index < publish->file_map_count) {
        return publish->file_maps[map_index];
    }
    return NULL;
}

static int publish_has_file(const asm_source_publish *publish) {
    size_t i;

    if (publish == NULL) {
        return 0;
    }
    if (publish->session != NULL && publish->session->file_count > 0u) {
        return 1;
    }
    for (i = 0; i < publish->file_map_count; i++) {
        if (publish->file_maps[i] != NULL && publish->file_maps[i]->file_count > 0u) {
            return 1;
        }
    }
    return 0;
}

static void view_from_hit(
    const asm_source_publish *publish,
    const asm_source_hit *hit,
    asm_source_view_state *view) {
    size_t i;

    if (hit->image == publish->session) {
        view->which = ASM_SOURCE_VIEW_SESSION;
        view->map_index = 0;
    } else {
        view->which = ASM_SOURCE_VIEW_NONE;
        for (i = 0; i < publish->file_map_count; i++) {
            if (publish->file_maps[i] == hit->image) {
                view->which = ASM_SOURCE_VIEW_FILE;
                view->map_index = (uint32_t)i;
                break;
            }
        }
    }
    view->file_index = hit->span->file_index;
}

static void apply_initial_file(
    const asm_source_publish *publish,
    asm_source_view_state *view,
    int has_pc,
    uint16_t pc) {
    size_t i;

    view->which = ASM_SOURCE_VIEW_NONE;
    view->map_index = 0;
    view->file_index = 0;
    if (has_pc && publish->session != NULL) {
        const asm_source_span *span = last_cover(publish->session, pc);
        if (span != NULL) {
            view->which = ASM_SOURCE_VIEW_SESSION;
            view->file_index = span->file_index;
            return;
        }
    }
    if (publish->session != NULL && publish->session->file_count > 0u) {
        view->which = ASM_SOURCE_VIEW_SESSION;
        view->file_index = 0;
        return;
    }
    for (i = 0; i < publish->file_map_count; i++) {
        if (publish->file_maps[i] != NULL && publish->file_maps[i]->file_count > 0u) {
            view->which = ASM_SOURCE_VIEW_FILE;
            view->map_index = (uint32_t)i;
            view->file_index = 0;
            return;
        }
    }
}

const asm_source_image *asm_source_view_sync(
    const asm_source_publish *publish,
    asm_source_view_state *view,
    int running,
    int has_pc,
    uint16_t pc,
    uint32_t visible_rows) {
    int generation_mismatch;
    const asm_source_image *on_screen;

    if (view == NULL) {
        return NULL;
    }
    if (running) {
        view->follow_pc = 1;
    }
    if (publish == NULL || !publish_has_file(publish)) {
        if (publish != NULL && view->source_generation != publish->generation) {
            view->which = ASM_SOURCE_VIEW_NONE;
            view->map_index = 0;
            view->file_index = 0;
            if (!view->follow_pc) {
                view->top_line = 0;
                view->cursor_line = 0;
            }
            view->source_generation = publish->generation;
        } else if (publish == NULL) {
            view->which = ASM_SOURCE_VIEW_NONE;
        }
        return NULL;
    }

    generation_mismatch = view->source_generation != publish->generation;
    if (generation_mismatch) {
        apply_initial_file(publish, view, has_pc, pc);
        if (!view->follow_pc) {
            view->top_line = 0;
            view->cursor_line = 0;
        }
    }

    on_screen = asm_source_view_image(publish, view->which, view->map_index);
    if (view->follow_pc && has_pc) {
        asm_source_hit hit = asm_source_span_at_pc(
            publish->session,
            (const asm_source_image *const *)publish->file_maps,
            publish->file_map_count,
            pc);
        if (hit.span != NULL && hit.image != NULL) {
            uint32_t half = visible_rows / 2u;
            uint32_t index = hit.span->line > 0u ? hit.span->line - 1u : 0u;
            if (hit.image != on_screen || hit.span->file_index != view->file_index) {
                view_from_hit(publish, &hit, view);
            }
            view->cursor_line = hit.span->line;
            view->top_line = index > half ? index - half : 0u;
        }
    }

    if (generation_mismatch) {
        view->source_generation = publish->generation;
    }
    on_screen = asm_source_view_image(publish, view->which, view->map_index);
    if (on_screen == NULL || on_screen->file_count == 0u) {
        view->which = ASM_SOURCE_VIEW_NONE;
        return NULL;
    }
    return on_screen;
}

static void source_loc(const ASSEMBLER *as, ASM_FILE **out_file, size_t *out_line) {
    size_t i;
    const FILE_FRAME *fallback = NULL;

    for (i = as->file_stack.items; i > 0u; i--) {
        const FILE_FRAME *frame =
            AM65_ARRAY_GET((AM65_DYNARRAY *)&as->file_stack, FILE_FRAME, i - 1u);
        if (frame->is_macro) {
            if (fallback == NULL) {
                fallback = frame;
            }
            continue;
        }
        *out_file = frame->file;
        *out_line = frame->line_num;
        return;
    }
    *out_file = fallback != NULL ? fallback->file : as->current_file;
    *out_line = fallback != NULL ? fallback->line_num : as->current_line;
}

void assembler_source_spans_free(ASSEMBLER *as) {
    if (as == NULL) {
        return;
    }
    am65_array_free(&as->source_spans);
    AM65_ARRAY_INIT(&as->source_spans, asm_recorded_span);
}

void assembler_source_spans_reset(ASSEMBLER *as) {
    assembler_source_spans_free(as);
    as->source_map_record_failed = 0;
}

void assembler_source_note_emit(ASSEMBLER *as, uint32_t address) {
    asm_recorded_span *last;
    asm_recorded_span span;
    ASM_FILE *file = NULL;
    size_t line = 0;
    size_t target_index;

    if (as == NULL || as->source_map_record_failed) {
        return;
    }
    source_loc(as, &file, &line);
    target_index = (size_t)-1;
    {
        size_t i;
        for (i = 0; i < as->targets.items; i++) {
            TARGET *candidate = *AM65_ARRAY_GET(&as->targets, TARGET *, i);
            if (candidate == as->active_target) {
                target_index = i;
                break;
            }
        }
    }
    if (as->source_spans.items > 0u) {
        last = AM65_ARRAY_GET(&as->source_spans, asm_recorded_span, as->source_spans.items - 1u);
        if (last != NULL && last->file == file && last->line == (uint32_t)line &&
            last->target_index == (uint32_t)target_index && last->end == address) {
            last->end = address + 1u;
            return;
        }
    }
    memset(&span, 0, sizeof(span));
    span.start = address;
    span.end = address + 1u;
    span.file = file;
    span.line = (uint32_t)line;
    span.target_index = (uint32_t)target_index;
    if (AM65_ARRAY_ADD(&as->source_spans, span) != ASM_OK) {
        as->source_map_record_failed = 1;
        asm_warn(as, "source map recording failed; emitted bytes kept, session map unchanged");
    }
}

size_t assembler_source_span_count(const ASSEMBLER *as) {
    if (as == NULL) {
        return 0;
    }
    return as->source_spans.items;
}

const asm_recorded_span *assembler_source_span_at(const ASSEMBLER *as, size_t index) {
    if (as == NULL || index >= as->source_spans.items) {
        return NULL;
    }
    return AM65_ARRAY_GET((AM65_DYNARRAY *)&as->source_spans, asm_recorded_span, index);
}

int assembler_copy_source_image(
    const ASSEMBLER *as,
    assembler_source_keep_fn keep,
    void *user,
    asm_source_image **out) {
    asm_source_image *image;
    uint32_t *remap;
    size_t i;
    size_t kept = 0;

    if (out == NULL) {
        return ASM_ERR;
    }
    *out = NULL;
    if (as == NULL || as->source_map_record_failed) {
        return ASM_ERR;
    }
    image = (asm_source_image *)calloc(1, sizeof(*image));
    remap = (uint32_t *)malloc((as->files.items > 0u ? as->files.items : 1u) * sizeof(*remap));
    if (image == NULL || remap == NULL) {
        free(image);
        free(remap);
        return ASM_ERR;
    }
    for (i = 0; i < as->files.items; i++) {
        remap[i] = UINT32_MAX;
    }
    for (i = 0; i < as->source_spans.items; i++) {
        const asm_recorded_span *span = AM65_ARRAY_GET(&as->source_spans, asm_recorded_span, i);
        if (keep != NULL && !keep(user, span->target_index)) {
            continue;
        }
        kept++;
    }
    if (kept > 0u) {
        image->spans = (asm_source_span *)calloc(kept, sizeof(*image->spans));
        if (image->spans == NULL) {
            free(remap);
            asm_source_image_free(image);
            return ASM_ERR;
        }
    }
    for (i = 0; i < as->files.items; i++) {
        ASM_FILE *file = *AM65_ARRAY_GET((AM65_DYNARRAY *)&as->files, ASM_FILE *, i);
        size_t s;
        int used = 0;
        for (s = 0; s < as->source_spans.items; s++) {
            const asm_recorded_span *span = AM65_ARRAY_GET(&as->source_spans, asm_recorded_span, s);
            if (span->file != file) {
                continue;
            }
            if (keep != NULL && !keep(user, span->target_index)) {
                continue;
            }
            used = 1;
            break;
        }
        if (!used) {
            continue;
        }
        {
            asm_source_file *grown = (asm_source_file *)realloc(image->files, (image->file_count + 1u) * sizeof(*image->files));
            asm_source_file *dst;
            if (grown == NULL) {
                free(remap);
                asm_source_image_free(image);
                return ASM_ERR;
            }
            image->files = grown;
            dst = &image->files[image->file_count];
            memset(dst, 0, sizeof(*dst));
            dst->display_name = source_dup(file->display_name != NULL ? file->display_name : "", file->display_name != NULL ? strlen(file->display_name) : 0u);
            dst->text = source_dup(file->buf, file->size);
            dst->size = file->size;
            if (dst->display_name == NULL || dst->text == NULL) {
                free(dst->display_name);
                free(dst->text);
                free(remap);
                asm_source_image_free(image);
                return ASM_ERR;
            }
            remap[i] = (uint32_t)image->file_count;
            image->file_count++;
        }
    }
    {
        size_t n = 0;
        for (i = 0; i < as->source_spans.items; i++) {
            const asm_recorded_span *span = AM65_ARRAY_GET(&as->source_spans, asm_recorded_span, i);
            size_t file_slot;
            if (keep != NULL && !keep(user, span->target_index)) {
                continue;
            }
            file_slot = (size_t)-1;
            {
                size_t f;
                for (f = 0; f < as->files.items; f++) {
                    ASM_FILE *file = *AM65_ARRAY_GET((AM65_DYNARRAY *)&as->files, ASM_FILE *, f);
                    if (file == span->file) {
                        file_slot = f;
                        break;
                    }
                }
            }
            if (file_slot == (size_t)-1 || remap[file_slot] == UINT32_MAX) {
                continue;
            }
            image->spans[n].start = span->start;
            image->spans[n].end = span->end;
            image->spans[n].file_index = remap[file_slot];
            image->spans[n].line = span->line;
            image->spans[n].target_index = span->target_index;
            n++;
        }
        image->span_count = n;
    }
    free(remap);
    *out = image;
    return ASM_OK;
}
