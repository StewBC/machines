#include "asm.h"
#include "errorlog.h"
#include "source_map.h"
#include "../test_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t memory[65536];
    size_t writes;
} test_memory;

static void output_byte(void *user, uint16_t addr, uint8_t val)
{
    test_memory *mem = (test_memory *)user;
    mem->memory[addr] = val;
    mem->writes++;
}

static void dump_log(const ERRORLOG *log)
{
    size_t i;

    for (i = 0; i < log->log_array.items; i++) {
        ERROR_ENTRY *entry = AM65_ARRAY_GET(&log->log_array, ERROR_ENTRY, i);
        fprintf(stderr, "  %s\n", entry && entry->err_str ? entry->err_str : "");
    }
}

static int assemble_source(const char *source, const char *prefix, ASSEMBLER *as, ERRORLOG *log, test_memory *mem, char *path, size_t path_size)
{
    CB_ASM_CTX cb;

    memset(mem, 0, sizeof(*mem));
    if (c64m_test_write_temp_file(path, path_size, prefix, source) != 0) {
        return -1;
    }
    errlog_init(log);
    memset(&cb, 0, sizeof(cb));
    cb.user = mem;
    cb.default_target = mem;
    cb.output_byte = output_byte;
    if (assembler_init(as, log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed\n");
        errlog_shutdown(log);
        c64m_test_remove_file(path);
        return -1;
    }
    if (assembler_assemble(as, path, 0) != ASM_OK) {
        fprintf(stderr, "assemble failed for %s\n", prefix);
        dump_log(log);
        assembler_shutdown(as);
        errlog_shutdown(log);
        c64m_test_remove_file(path);
        return -1;
    }
    return 0;
}

static int expect_span(const ASSEMBLER *as, size_t index, uint32_t start, uint32_t end, uint32_t line)
{
    const asm_recorded_span *span = assembler_source_span_at(as, index);

    if (span == NULL || span->start != start || span->end != end || span->line != line) {
        fprintf(stderr, "span %zu: expected %04X-%04X line %u, got %s\n",
            index,
            start,
            end,
            line,
            span == NULL ? "missing" : "mismatch");
        if (span != NULL) {
            fprintf(stderr, "  actual %04X-%04X line %u\n", span->start, span->end, span->line);
        }
        return 1;
    }
    return 0;
}

static int test_worked_spans(void)
{
    ASSEMBLER as;
    ERRORLOG log;
    test_memory mem;
    char path[512];
    asm_source_image *image = NULL;
    int has_span = 0;
    uint16_t gutter;
    int failures = 0;
    const char *source =
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

    if (assemble_source(source, "c64m_srcmap_worked", &as, &log, &mem, path, sizeof(path)) != 0) {
        return 1;
    }
    if (assembler_source_span_count(&as) != 8u) {
        fprintf(stderr, "worked spans: expected 8, got %zu\n", assembler_source_span_count(&as));
        failures++;
    }
    failures += expect_span(&as, 0, 0xC000, 0xC002, 5);
    failures += expect_span(&as, 1, 0xC002, 0xC005, 6);
    failures += expect_span(&as, 2, 0xC005, 0xC007, 7);
    failures += expect_span(&as, 3, 0xC007, 0xC00A, 8);
    failures += expect_span(&as, 4, 0xC00A, 0xC00B, 9);
    failures += expect_span(&as, 5, 0xC00B, 0xC00D, 10);
    failures += expect_span(&as, 6, 0xC00D, 0xC00E, 11);
    failures += expect_span(&as, 7, 0xC00E, 0xC015, 13);
    if (assembler_copy_source_image(&as, NULL, NULL, &image) != ASM_OK || image == NULL) {
        fprintf(stderr, "worked copy failed\n");
        failures++;
    } else {
        asm_source_hit at_c007 = asm_source_span_at_pc(image, NULL, 0, 0xC007);
        asm_source_hit at_c008 = asm_source_span_at_pc(image, NULL, 0, 0xC008);
        asm_source_hit at_c015 = asm_source_span_at_pc(image, NULL, 0, 0xC015);
        gutter = asm_source_line_gutter(image, 0, 8, 0xC008, &has_span);
        if (at_c007.image != image || at_c007.span == NULL || at_c007.span->line != 8u) {
            fprintf(stderr, "PC C007 did not hit jsr\n");
            failures++;
        }
        if (at_c008.span == NULL || at_c008.span != at_c007.span) {
            fprintf(stderr, "PC C008 did not hit the jsr span\n");
            failures++;
        }
        if (!has_span || gutter != 0xC007) {
            fprintf(stderr, "gutter at C008 is %04X has %d\n", gutter, has_span);
            failures++;
        }
        if (at_c015.image != NULL || at_c015.span != NULL) {
            fprintf(stderr, "PC C015 should miss\n");
            failures++;
        }
        {
            char map_path[560];
            asm_source_image *loaded = NULL;

            snprintf(map_path, sizeof(map_path), "%s.srcmap", path);
            if (asm_source_map_write(image, map_path) != 0 ||
                asm_source_map_read(map_path, &loaded) != 0 ||
                loaded == NULL ||
                loaded->file_count != image->file_count ||
                loaded->span_count != 8u ||
                loaded->files[0].size != image->files[0].size ||
                memcmp(loaded->files[0].text, image->files[0].text, image->files[0].size) != 0 ||
                strcmp(loaded->files[0].display_name, image->files[0].display_name) != 0 ||
                loaded->spans[3].start != 0xC007u ||
                loaded->spans[3].end != 0xC00Au ||
                loaded->spans[3].line != 8u) {
                fprintf(stderr, "worked source map round trip failed\n");
                failures++;
            }
            asm_source_image_free(loaded);
            remove(map_path);
        }
        asm_source_image_free(image);
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(path);
    return failures;
}

static int test_include_line(void)
{
    ASSEMBLER as;
    ERRORLOG log;
    test_memory mem;
    CB_ASM_CTX cb;
    char inc_path[512];
    char root_path[512];
    char source[640];
    asm_source_image *image = NULL;
    const asm_recorded_span *span;
    int failures = 0;

    if (c64m_test_write_temp_file(inc_path, sizeof(inc_path), "c64m_srcmap_inc", "    nop\n") != 0) {
        return 1;
    }
    snprintf(source, sizeof(source), ".include \"%s\"\n", inc_path);
    if (c64m_test_write_temp_file(root_path, sizeof(root_path), "c64m_srcmap_root", source) != 0) {
        c64m_test_remove_file(inc_path);
        return 1;
    }
    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.default_target = &mem;
    cb.output_byte = output_byte;
    if (assembler_init(&as, &log, &cb) != ASM_OK || assembler_assemble(&as, root_path, 0) != ASM_OK) {
        fprintf(stderr, "include assemble failed\n");
        dump_log(&log);
        failures++;
    } else if (assembler_source_span_count(&as) != 1u) {
        fprintf(stderr, "include span count %zu\n", assembler_source_span_count(&as));
        failures++;
    } else {
        span = assembler_source_span_at(&as, 0);
        if (span == NULL || span->line != 1u || span->file == NULL ||
            span->file->display_name == NULL || strstr(span->file->display_name, "c64m_srcmap_inc") == NULL) {
            fprintf(stderr, "include span is not the included file line 1 (%s)\n",
                span && span->file && span->file->display_name ? span->file->display_name : "");
            failures++;
        }
        if (assembler_copy_source_image(&as, NULL, NULL, &image) == ASM_OK && image != NULL && image->file_count == 1u) {
            const char *row = NULL;
            size_t len = 0;
            if (!asm_source_line_slice(image->files[0].text, image->files[0].size, 1, &row, &len) ||
                len != 7u || memcmp(row, "    nop", 7) != 0) {
                fprintf(stderr, "include stored row mismatch\n");
                failures++;
            }
        } else {
            fprintf(stderr, "include copy failed\n");
            failures++;
        }
        asm_source_image_free(image);
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(inc_path);
    c64m_test_remove_file(root_path);
    return failures;
}

static int test_macro_call_site(void)
{
    ASSEMBLER as;
    ERRORLOG log;
    test_memory mem;
    char path[512];
    asm_source_image *image = NULL;
    const char *row = NULL;
    size_t len = 0;
    int failures = 0;
    const char *source =
        ".macro two\n"
        "    nop\n"
        "    nop\n"
        ".endmacro\n"
        ".macro outer\n"
        "    two\n"
        ".endmacro\n"
        "outer\n";

    if (assemble_source(source, "c64m_srcmap_macro", &as, &log, &mem, path, sizeof(path)) != 0) {
        return 1;
    }
    if (assembler_source_span_count(&as) != 1u) {
        fprintf(stderr, "macro spans: expected 1 coalesced, got %zu\n", assembler_source_span_count(&as));
        failures++;
    } else {
        const asm_recorded_span *span = assembler_source_span_at(&as, 0);
        if (span == NULL || span->line != 8u || span->end != span->start + 2u) {
            fprintf(stderr, "macro span is not the outer call\n");
            failures++;
        }
    }
    if (assembler_copy_source_image(&as, NULL, NULL, &image) == ASM_OK && image != NULL) {
        if (!asm_source_line_slice(image->files[0].text, image->files[0].size, 8, &row, &len) ||
            len != 5u || memcmp(row, "outer", 5) != 0) {
            fprintf(stderr, "macro stored call row mismatch\n");
            failures++;
        }
        asm_source_image_free(image);
    } else {
        fprintf(stderr, "macro copy failed\n");
        failures++;
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(path);
    return failures;
}

static int test_crlf_rows(void)
{
    ASSEMBLER as;
    ERRORLOG log;
    test_memory mem;
    char path[512];
    asm_source_image *image = NULL;
    const char *row = NULL;
    size_t len = 0;
    int failures = 0;
    const char *crlf =
        ".macro one\r\n"
        "    nop\r\n"
        ".endmacro\r\n"
        "one\r\n";
    const char *cr_only =
        "nop\r"
        "lda #1\r";

    if (assemble_source(crlf, "c64m_srcmap_crlf", &as, &log, &mem, path, sizeof(path)) != 0) {
        return 1;
    }
    if (assembler_copy_source_image(&as, NULL, NULL, &image) != ASM_OK || image == NULL ||
        image->span_count != 1u || image->spans[0].line != 4u ||
        !asm_source_line_slice(image->files[0].text, image->files[0].size, 4, &row, &len) ||
        len != 3u || memcmp(row, "one", 3) != 0) {
        fprintf(stderr, "crlf call row mismatch\n");
        failures++;
    }
    asm_source_image_free(image);
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(path);

    image = NULL;
    if (assemble_source(cr_only, "c64m_srcmap_cr", &as, &log, &mem, path, sizeof(path)) != 0) {
        return failures + 1;
    }
    if (assembler_source_span_count(&as) != 2u ||
        assembler_copy_source_image(&as, NULL, NULL, &image) != ASM_OK || image == NULL) {
        fprintf(stderr, "cr-only span count %zu\n", assembler_source_span_count(&as));
        failures++;
    } else {
        if (!asm_source_line_slice(image->files[0].text, image->files[0].size, 1, &row, &len) ||
            len != 3u || memcmp(row, "nop", 3) != 0 ||
            !asm_source_line_slice(image->files[0].text, image->files[0].size, 2, &row, &len) ||
            len != 6u || memcmp(row, "lda #1", 6) != 0 ||
            asm_source_line_count(image->files[0].text, image->files[0].size) != 2u) {
            fprintf(stderr, "cr-only rows drifted\n");
            failures++;
        }
        asm_source_image_free(image);
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(path);
    return failures;
}

static void collect_name(const char *name, uint16_t address, void *user)
{
    (void)address;
    if (strcmp(name, "bss_mark") == 0) {
        *(int *)user = 1;
    }
}

static int test_noemit(void)
{
    ASSEMBLER as;
    ERRORLOG log;
    test_memory mem;
    char path[512];
    int saw = 0;
    int failures = 0;
    const char *source =
        ".segdef \"BSS\", $2000, noemit\n"
        ".segment \"BSS\"\n"
        "bss_mark:\n"
        "    .byte $11\n"
        ".segment \"\"\n"
        "    nop\n";

    if (assemble_source(source, "c64m_srcmap_noemit", &as, &log, &mem, path, sizeof(path)) != 0) {
        return 1;
    }
    if (assembler_source_span_count(&as) != 1u) {
        fprintf(stderr, "noemit produced %zu spans\n", assembler_source_span_count(&as));
        failures++;
    } else {
        const asm_recorded_span *span = assembler_source_span_at(&as, 0);
        if (span == NULL || span->start != 0u) {
            fprintf(stderr, "noemit nop span missing\n");
            failures++;
        }
    }
    assembler_walk_symbols(&as, collect_name, &saw);
    if (!saw) {
        fprintf(stderr, "noemit label missing from the walk\n");
        failures++;
    }
    if (mem.memory[0x2000] != 0) {
        fprintf(stderr, "noemit byte was written\n");
        failures++;
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(path);
    return failures;
}

static int test_replace_and_last_match(void)
{
    ASSEMBLER as;
    ERRORLOG log;
    test_memory mem;
    CB_ASM_CTX cb;
    char longer_path[512];
    char shorter_path[512];
    asm_source_image *first = NULL;
    asm_source_image *second = NULL;
    asm_source_hit hit;
    int failures = 0;
    size_t i;

    if (c64m_test_write_temp_file(longer_path, sizeof(longer_path), "c64m_srcmap_long", "nop\nnop\nnop\nnop\n") != 0) {
        return 1;
    }
    if (c64m_test_write_temp_file(shorter_path, sizeof(shorter_path), "c64m_srcmap_short", "* = $C000\nnop\n.org $C000\nlda #1\n") != 0) {
        c64m_test_remove_file(longer_path);
        return 1;
    }
    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.default_target = &mem;
    cb.output_byte = output_byte;
    if (assembler_init(&as, &log, &cb) != ASM_OK ||
        assembler_assemble(&as, longer_path, 0) != ASM_OK ||
        assembler_copy_source_image(&as, NULL, NULL, &first) != ASM_OK) {
        fprintf(stderr, "longer assemble failed\n");
        dump_log(&log);
        failures++;
    }
    if (failures == 0 &&
        (assembler_assemble(&as, shorter_path, 0) != ASM_OK ||
         assembler_copy_source_image(&as, NULL, NULL, &second) != ASM_OK)) {
        fprintf(stderr, "shorter assemble failed\n");
        dump_log(&log);
        failures++;
    }
    if (failures == 0) {
        if (assembler_source_span_count(&as) != second->span_count) {
            fprintf(stderr, "pass 2 did not replace the span list\n");
            failures++;
        }
        for (i = 0; i < second->span_count; i++) {
            if (second->spans[i].start < 0xC000u) {
                fprintf(stderr, "shorter image kept a longer-only address\n");
                failures++;
            }
        }
        hit = asm_source_span_at_pc(second, NULL, 0, 0xC000);
        if (hit.span == NULL || hit.span->start != 0xC000u || hit.span->end != 0xC002u ||
            hit.span != &second->spans[second->span_count - 1u]) {
            fprintf(stderr, "last match did not keep the later lda span\n");
            failures++;
        }
        if (first == NULL || first->span_count < 4u) {
            fprintf(stderr, "first image was not retained by the caller\n");
            failures++;
        }
    }
    asm_source_image_free(first);
    asm_source_image_free(second);
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(longer_path);
    c64m_test_remove_file(shorter_path);
    return failures;
}

static int drop_nonzero_target(void *user, uint32_t target_index)
{
    (void)user;
    return target_index == 0u;
}

static test_memory side_mem;

static void *keep_target_open(void *user, const char *name, int name_len,
    const char *path, int path_len, const char *dest, int dest_len,
    assembler_output_format format)
{
    (void)user;
    (void)name;
    (void)name_len;
    (void)path;
    (void)path_len;
    (void)dest;
    (void)dest_len;
    (void)format;
    memset(&side_mem, 0, sizeof(side_mem));
    return &side_mem;
}

static int test_keep_drops_other_targets(void)
{
    ASSEMBLER as;
    ERRORLOG log;
    test_memory mem;
    CB_ASM_CTX cb;
    char path[512];
    asm_source_image *image = NULL;
    int failures = 0;
    const char *source =
        "nop\n"
        ".scope side file=\"side.bin\"\n"
        "    lda #1\n"
        ".endscope\n";

    if (c64m_test_write_temp_file(path, sizeof(path), "c64m_srcmap_keep", source) != 0) {
        return 1;
    }
    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.default_target = &mem;
    cb.output_byte = output_byte;
    cb.target_open = keep_target_open;
    if (assembler_init(&as, &log, &cb) != ASM_OK) {
        fprintf(stderr, "keep init failed\n");
        errlog_shutdown(&log);
        c64m_test_remove_file(path);
        return 1;
    }
    if (assembler_assemble(&as, path, 0) != ASM_OK) {
        fprintf(stderr, "keep assemble failed\n");
        dump_log(&log);
        assembler_shutdown(&as);
        errlog_shutdown(&log);
        c64m_test_remove_file(path);
        return 1;
    }
    if (assembler_source_span_count(&as) < 2u) {
        fprintf(stderr, "expected spans on both targets\n");
        failures++;
    }
    if (assembler_copy_source_image(&as, drop_nonzero_target, NULL, &image) != ASM_OK || image == NULL) {
        fprintf(stderr, "filtered copy failed\n");
        failures++;
    } else if (image->span_count != 1u || image->spans[0].target_index != 0u) {
        fprintf(stderr, "filtered image kept %zu spans\n", image->span_count);
        failures++;
    }
    asm_source_image_free(image);
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    c64m_test_remove_file(path);
    remove("side.bin");
    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_worked_spans();
    failures += test_include_line();
    failures += test_macro_call_site();
    failures += test_crlf_rows();
    failures += test_noemit();
    failures += test_replace_and_last_match();
    failures += test_keep_drops_other_targets();
    return failures == 0 ? 0 : 1;
}
