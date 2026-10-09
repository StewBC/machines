#include "source_map.h"
#include "symbol_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

static int failures = 0;

static void expect(int cond, const char *label)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", label);
        failures++;
    }
}

static char *dup_text(const char *text, size_t *size)
{
    size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1u);

    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1u);
    if (size != NULL) {
        *size = length;
    }
    return copy;
}

static asm_source_image *image_new(void)
{
    return (asm_source_image *)calloc(1, sizeof(asm_source_image));
}

static int add_file(asm_source_image *image, const char *name, const char *text)
{
    asm_source_file *files;
    char *display;
    char *body;
    size_t size = 0;

    files = (asm_source_file *)realloc(
        image->files, (image->file_count + 1u) * sizeof(*files));
    display = dup_text(name, NULL);
    body = dup_text(text, &size);
    if (files == NULL || display == NULL || body == NULL) {
        if (files != NULL) {
            image->files = files;
        }
        free(display);
        free(body);
        return 0;
    }
    image->files = files;
    image->files[image->file_count].display_name = display;
    image->files[image->file_count].text = body;
    image->files[image->file_count].size = size;
    image->file_count++;
    return 1;
}

static int add_span(
    asm_source_image *image,
    uint32_t start,
    uint32_t end,
    uint32_t file_index,
    uint32_t line)
{
    asm_source_span *spans;
    asm_source_span *span;

    spans = (asm_source_span *)realloc(
        image->spans, (image->span_count + 1u) * sizeof(*spans));
    if (spans == NULL) {
        return 0;
    }
    image->spans = spans;
    span = &image->spans[image->span_count];
    span->start = start;
    span->end = end;
    span->file_index = file_index;
    span->line = line;
    span->target_index = 0;
    image->span_count++;
    return 1;
}

static void test_lookup(void)
{
    asm_source_image *session;
    asm_source_image *first;
    asm_source_image *second;
    const asm_source_image *maps[2];
    asm_source_hit hit;
    int has_span = 0;
    char row[128];

    session = image_new();
    first = image_new();
    second = image_new();
    expect(session != NULL && first != NULL && second != NULL, "images");
    if (session == NULL || first == NULL || second == NULL) {
        asm_source_image_free(session);
        asm_source_image_free(first);
        asm_source_image_free(second);
        return;
    }
    expect(add_file(session, "session.s", "ldx #0\nlda name, x\njsr print\n"), "session file");
    expect(add_span(session, 0xC000u, 0xC002u, 0u, 1u), "ldx span");
    expect(add_span(session, 0xC002u, 0xC005u, 0u, 2u), "lda span");
    expect(add_span(session, 0xC007u, 0xC008u, 0u, 2u), "earlier jsr overlap");
    expect(add_span(session, 0xC007u, 0xC00Au, 0u, 3u), "jsr span");
    /* Last matching span wins, so the jsr line covers $C007 and $C008. */
    expect(add_file(first, "file0.s", "hidden\nonlyfile\n"), "file0");
    expect(add_span(first, 0xC000u, 0xC001u, 0u, 1u), "hidden file span");
    expect(add_span(first, 0x2000u, 0x2001u, 0u, 2u), "file0 fallthrough");
    expect(add_file(second, "file1.s", "later\n"), "file1");
    expect(add_span(second, 0x2000u, 0x2002u, 0u, 1u), "file1 loses at $2000");
    expect(add_span(second, 0x2100u, 0x2101u, 0u, 1u), "file1 unique");

    maps[0] = first;
    maps[1] = second;

    hit = asm_source_span_at_pc(session, maps, 2, 0xC007u);
    expect(hit.image == session && hit.span != NULL && hit.span->line == 3u, "PC C007 hits jsr");
    hit = asm_source_span_at_pc(session, maps, 2, 0xC008u);
    expect(hit.image == session && hit.span != NULL && hit.span->line == 3u, "PC C008 hits jsr");
    hit = asm_source_span_at_pc(session, maps, 2, 0xC000u);
    expect(hit.image == session && hit.span != NULL && hit.span->line == 1u, "session hides file map");
    hit = asm_source_span_at_pc(session, maps, 2, 0xC015u);
    expect(hit.image == NULL && hit.span == NULL, "PC C015 misses");
    hit = asm_source_span_at_pc(session, maps, 2, 0x2000u);
    expect(hit.image == first && hit.span != NULL && hit.span->line == 2u, "first file map wins");
    hit = asm_source_span_at_pc(session, maps, 2, 0x2100u);
    expect(hit.image == second && hit.span != NULL, "later file map fallthrough");

    expect(
        asm_source_line_gutter(session, 0u, 3u, 0xC008u, &has_span) == 0xC007u && has_span,
        "gutter is span start");
    asm_source_format_row(session, 0u, 3u, 1, 0xC008u, row, sizeof(row));
    expect(row[5] == '>', "PC mark on jsr line");
    asm_source_format_row(session, 0u, 1u, 1, 0xC008u, row, sizeof(row));
    expect(row[5] == ' ', "other line is unmarked");

    asm_source_image_free(session);
    asm_source_image_free(first);
    asm_source_image_free(second);
}

static void test_view_sync(void)
{
    asm_source_image *session;
    asm_source_publish publish;
    asm_source_view_state view;
    const asm_source_image *image;

    session = image_new();
    expect(session != NULL && add_file(session, "session.s", "ldx #0\nlda name, x\njsr print\n"), "view file");
    expect(add_span(session, 0xC007u, 0xC00Au, 0u, 3u), "view span");
    if (session == NULL || session->file_count == 0u) {
        asm_source_image_free(session);
        return;
    }

    memset(&publish, 0, sizeof(publish));
    memset(&view, 0, sizeof(view));
    publish.session = session;
    publish.generation = 3u;
    view.follow_pc = 1;
    image = asm_source_view_sync(&publish, &view, 0, 1, 0xC008u, 2u);
    expect(image == session, "follow shows the session file");
    expect(view.which == ASM_SOURCE_VIEW_SESSION, "initial file is the session");
    expect(view.cursor_line == 3u, "follow sets the cursor line");
    expect(view.top_line == 1u, "follow centers the PC line");
    expect(view.source_generation == 3u, "generation stored after the rule");

    view.follow_pc = 0;
    view.top_line = 4u;
    view.cursor_line = 2u;
    image = asm_source_view_sync(&publish, &view, 0, 1, 0xC008u, 2u);
    expect(image == session && view.top_line == 4u && view.cursor_line == 2u, "detached scroll stays");

    publish.generation = 4u;
    view.follow_pc = 0;
    view.top_line = 4u;
    view.cursor_line = 2u;
    image = asm_source_view_sync(&publish, &view, 0, 1, 0xC008u, 2u);
    expect(image == session, "new generation still has the session file");
    expect(view.top_line == 0u && view.cursor_line == 0u, "detached generation resets the cursor");
    expect(view.source_generation == 4u, "generation advances once");

    session->file_count = 0u;
    publish.generation = 5u;
    view.follow_pc = 0;
    view.top_line = 3u;
    view.cursor_line = 2u;
    image = asm_source_view_sync(&publish, &view, 0, 1, 0xC008u, 2u);
    expect(image == NULL && view.which == ASM_SOURCE_VIEW_NONE, "empty image is no source");
    expect(view.top_line == 0u && view.cursor_line == 0u, "empty image clears a detached cursor");
    session->file_count = 1u;

    view.follow_pc = 1;
    view.top_line = 0u;
    view.cursor_line = 1u;
    publish.generation = 6u;
    image = asm_source_view_sync(&publish, &view, 1, 1, 0xC008u, 2u);
    expect(view.follow_pc == 1, "running forces follow");
    expect(view.cursor_line == 3u && view.top_line == 1u, "running recenters");

    asm_source_image_free(session);
}

static char *temp_path(const char *leaf)
{
    char *path = (char *)malloc(160u);
    long pid;

    if (path == NULL) {
        return NULL;
    }
#if defined(_WIN32)
    pid = (long)_getpid();
#else
    pid = (long)getpid();
#endif
    snprintf(path, 160u, "/tmp/srcmap_%ld_%s", pid, leaf);
    return path;
}

static int write_bytes(const char *path, const char *bytes, size_t length)
{
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        return 0;
    }
    if (length > 0u && fwrite(bytes, 1u, length, fp) != length) {
        fclose(fp);
        return 0;
    }
    return fclose(fp) == 0;
}

static void expect_suffix(const char *symbol, const char *want)
{
    char *out = NULL;

    expect(
        asm_source_map_path(symbol, &out) == 0 && out != NULL && strcmp(out, want) == 0,
        want);
    free(out);
}

static void expect_reject(const char *leaf, const char *bytes)
{
    char *path = temp_path(leaf);
    asm_source_image *image = (asm_source_image *)0x1;

    expect(path != NULL && write_bytes(path, bytes, strlen(bytes)), leaf);
    expect(asm_source_map_read(path, &image) != 0 && image == NULL, leaf);
    if (path != NULL) {
        remove(path);
    }
    free(path);
}

static void test_source_map_file(void)
{
    asm_source_image *image;
    asm_source_image *loaded = NULL;
    char *path;
    symbol_table *table;
    size_t before;
    const char *worked =
        "* = $C000\n"
        "\n"
        "print = $ffd\n"
        "\n"
        "ldx #0\n"
        ": lda name, x\n"
        "beq done\n"
        "jsr print\n"
        "inx\n"
        "bne :-\n"
        "done: rts\n"
        "\n"
        "name: .string \"stefan\", $00\n";
    const char *bare = "lda #1";

    expect_suffix("prog.bin.sym", "prog.bin.srcmap");
    expect_suffix("listing.sym", "listing.srcmap");
    expect_suffix("FOO.SYM", "FOO.srcmap");
    expect_suffix("listing.txt", "listing.txt.srcmap");

    expect_reject("bad_version", "AM65SRCMAP 10\n");
    expect_reject(
        "bad_index",
        "AM65SRCMAP 1\n"
        "FILE 0 1 1\n"
        "ab"
        "SPAN 1 1 0000 0001\n");
    expect_reject(
        "nondense",
        "AM65SRCMAP 1\n"
        "FILE 1 1 1\n"
        "ab");
    expect_reject(
        "end_before_start",
        "AM65SRCMAP 1\n"
        "FILE 0 1 1\n"
        "ab"
        "SPAN 0 1 0010 000F\n");
    expect_reject(
        "end_past_64k",
        "AM65SRCMAP 1\n"
        "FILE 0 1 1\n"
        "ab"
        "SPAN 0 1 0000 10001\n");
    expect_reject(
        "trailing_junk",
        "AM65SRCMAP 1\n"
        "FILE 0 1 1\n"
        "ab"
        "SPAN 0 1 0000 0001\n"
        "NOPE\n");

    image = image_new();
    expect(image != NULL, "round-trip image");
    if (image == NULL) {
        return;
    }
    expect(add_file(image, "worked.asm", worked), "worked file");
    expect(add_file(image, "nonewline.asm", bare), "bare file");
    image->files[1].size = 6u;
    expect(add_span(image, 0xC000u, 0xC002u, 0u, 5u), "ldx");
    expect(add_span(image, 0xC007u, 0xC00Au, 0u, 8u), "jsr");
    expect(add_span(image, 0xC00Eu, 0xC015u, 0u, 13u), "string");
    expect(add_span(image, 0xFFFFu, 0x10000u, 1u, 1u), "last byte");
    path = temp_path("round.srcmap");
    expect(path != NULL && asm_source_map_write(image, path) == 0, "write map");
    expect(asm_source_map_read(path, &loaded) == 0 && loaded != NULL, "read map");
    if (loaded != NULL) {
        expect(loaded->file_count == 2u && loaded->span_count == 4u, "round-trip counts");
        expect(
            loaded->files[0].size == strlen(worked) &&
                memcmp(loaded->files[0].text, worked, strlen(worked)) == 0 &&
                strcmp(loaded->files[0].display_name, "worked.asm") == 0,
            "worked text");
        expect(
            loaded->files[1].size == 6u &&
                memcmp(loaded->files[1].text, bare, 6u) == 0 &&
                strcmp(loaded->files[1].display_name, "nonewline.asm") == 0,
            "text without a newline");
        expect(
            loaded->spans[1].start == 0xC007u && loaded->spans[1].end == 0xC00Au &&
                loaded->spans[1].line == 8u && loaded->spans[1].file_index == 0u,
            "jsr span");
        expect(
            loaded->spans[3].start == 0xFFFFu && loaded->spans[3].end == 0x10000u &&
                loaded->spans[3].file_index == 1u,
            "span through $FFFF");
    }
    {
        FILE *fp = path != NULL ? fopen(path, "rb") : NULL;
        char raw[4096];
        size_t n = 0u;

        if (fp != NULL) {
            n = fread(raw, 1u, sizeof(raw) - 1u, fp);
            fclose(fp);
        }
        raw[n] = '\0';
        expect(strstr(raw, "nonewline.asmlda #1SPAN") != NULL, "adjacent blobs");
    }
    asm_source_image_free(loaded);
    asm_source_image_free(image);
    if (path != NULL) {
        remove(path);
    }

    table = symbol_table_create();
    image = image_new();
    loaded = NULL;
    expect(table != NULL && image != NULL, "symbol table");
    if (table != NULL && image != NULL && path != NULL &&
        add_file(image, "alone.asm", "nop") &&
        add_span(image, 0x2000u, 0x2001u, 0u, 1u) &&
        asm_source_map_write(image, path) == 0 &&
        symbol_table_add(table, 0x1000u, "KEEP", SYMBOL_SOURCE_FILE, "keep.sym", false) ==
            SYMBOL_OK) {
        before = symbol_table_count(table);
        expect(asm_source_map_read(path, &loaded) == 0 && loaded != NULL, "load map alone");
        expect(symbol_table_count(table) == before, "map load is not a symbol import");
    } else {
        expect(0, "map load is not a symbol import");
    }
    asm_source_image_free(image);
    asm_source_image_free(loaded);
    symbol_table_destroy(table);
    loaded = (asm_source_image *)0x1;
    expect(
        asm_source_map_read("/tmp/srcmap_missing_no_such_file", &loaded) != 0 && loaded == NULL,
        "missing map");
    free(path);
}

int main(void)
{
    test_lookup();
    test_view_sync();
    test_source_map_file();
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
