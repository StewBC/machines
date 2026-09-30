#include "asm.h"
#include "errorlog.h"
#include "../test_file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t memory[65536];
    size_t writes;
} test_memory;

typedef struct {
    test_memory def;
    test_memory *extra;
} assert_host;

static void output_byte(void *user, uint16_t addr, uint8_t val)
{
    test_memory *mem = (test_memory *)user;
    mem->memory[addr] = val;
    mem->writes++;
}

static void *open_target(
    void *user,
    const char *name, int name_len,
    const char *path, int path_len,
    const char *dest, int dest_len,
    assembler_output_format format)
{
    assert_host *host = (assert_host *)user;
    (void)name;
    (void)name_len;
    (void)path;
    (void)path_len;
    (void)dest;
    (void)dest_len;
    (void)format;
    host->extra = calloc(1, sizeof(*host->extra));
    return host->extra;
}

static void release_target(void *user, void *target)
{
    /* The test owns host->extra and reads it after assembler_shutdown. */
    (void)user;
    (void)target;
}

static int write_source(char *path, size_t path_size, const char *source)
{
    return c64m_test_write_temp_file(path, path_size, "c64m_asm_assert", source);
}

static void dump_log(const ERRORLOG *log)
{
    size_t i;
    for (i = 0; i < log->log_array.items; i++) {
        ERROR_ENTRY *entry = AM65_ARRAY_GET(&log->log_array, ERROR_ENTRY, i);
        fprintf(stderr, "  %s\n", entry && entry->err_str ? entry->err_str : "");
    }
}

static int log_has(const ERRORLOG *log, const char *needle)
{
    size_t i;
    for (i = 0; i < log->log_array.items; i++) {
        ERROR_ENTRY *entry = AM65_ARRAY_GET(&log->log_array, ERROR_ENTRY, i);
        if (entry && entry->err_str && strstr(entry->err_str, needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int log_has_severity(const ERRORLOG *log, const char *needle, int severity)
{
    size_t i;
    for (i = 0; i < log->log_array.items; i++) {
        ERROR_ENTRY *entry = AM65_ARRAY_GET(&log->log_array, ERROR_ENTRY, i);
        if (entry && entry->err_str && entry->severity == severity &&
            strstr(entry->err_str, needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int assemble_file(const char *path, test_memory *mem, ERRORLOG *log, int auto_adjust)
{
    CB_ASM_CTX cb;
    ASSEMBLER as;
    int result;

    memset(&cb, 0, sizeof(cb));
    cb.user = mem;
    cb.default_target = mem;
    cb.output_byte = output_byte;

    if (assembler_init(&as, log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed\n");
        return ASM_ERR;
    }
    if (auto_adjust) {
        assembler_set_auto_adjust_segments(&as, 1);
    }
    result = assembler_assemble(&as, path, 0x0801);
    assembler_shutdown(&as);
    return result;
}

static int assemble_source(const char *source, test_memory *mem, ERRORLOG *log, int auto_adjust)
{
    char path[128];
    int result;

    if (write_source(path, sizeof(path), source) != 0) {
        return ASM_ERR;
    }
    result = assemble_file(path, mem, log, auto_adjust);
    c64m_test_remove_file(path);
    return result;
}

static int assemble_host(const char *source, assert_host *host, ERRORLOG *log)
{
    char path[128];
    CB_ASM_CTX cb;
    ASSEMBLER as;
    int result;

    if (write_source(path, sizeof(path), source) != 0) {
        return ASM_ERR;
    }
    memset(&cb, 0, sizeof(cb));
    cb.user = host;
    cb.default_target = &host->def;
    cb.output_byte = output_byte;
    cb.target_open = open_target;
    cb.target_release = release_target;
    if (assembler_init(&as, log, &cb) != ASM_OK) {
        c64m_test_remove_file(path);
        return ASM_ERR;
    }
    result = assembler_assemble(&as, path, 0x0801);
    assembler_shutdown(&as);
    c64m_test_remove_file(path);
    return result;
}

static int test_constant_assertions(void)
{
    test_memory mem;
    ERRORLOG log;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".assert 1\n"
            ".assert $10000\n"
            ".assert 2 .gt 1, \"kept\"\n"
            ".byte $22\n",
            &mem, &log, 0) != ASM_OK) {
        fprintf(stderr, "passing constant assertions failed\n");
        dump_log(&log);
        failures++;
    }
    if (mem.memory[0x0801] != 0x22 || mem.writes != 1) {
        fprintf(stderr, "passing constant assertions did not emit\n");
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".byte $11\n.assert 0, \"nope\"\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, "assertion failed: nope") ||
        mem.writes != 0 || mem.memory[0x0801] != 0) {
        fprintf(stderr, "failing custom assertion still emitted or lacked its message\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".assert 0\n.byte $11\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, "assertion failed") ||
        log_has(&log, "assertion failed:") ||
        mem.writes != 0) {
        fprintf(stderr, "default assertion message mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".assert 0, \"too\\tbig\"\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, "assertion failed: too\tbig")) {
        fprintf(stderr, "assertion message escape was not decoded\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    return failures;
}

static int test_segment_queries(void)
{
    test_memory mem;
    ERRORLOG log;
    int failures = 0;
    const char *source =
        ".byte $99\n"
        ".segdef \"CODE\", $2000\n"
        ".segment \"CODE\"\n"
        "    .byte $01, $02, $03\n"
        ".assert startof(\"CODE\") .eq $2000\n"
        ".assert endof(\"CODE\") .eq $2003\n"
        ".assert sizeof(\"CODE\") .eq 3\n"
        ".assert SizeOf(\"code\") .eq 3\n"
        ".assert startof(\"\") .eq $0801\n"
        ".assert sizeof(\"\") .eq 1\n"
        ".assert endof(\"\") .eq $0802\n";

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(source, &mem, &log, 0) != ASM_OK) {
        fprintf(stderr, "segment queries failed\n");
        dump_log(&log);
        failures++;
    }
    if (mem.memory[0x0801] != 0x99 || mem.memory[0x2000] != 0x01 ||
        mem.memory[0x2001] != 0x02 || mem.memory[0x2002] != 0x03) {
        fprintf(stderr, "segment query output mismatch\n");
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".segdef \"CODE\", $2000\n"
            ".segment \"CODE\"\n"
            "    .byte $01, $02, $03\n"
            ".assert sizeof(\"CODE\") .eq 4\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "assertion failed")) {
        fprintf(stderr, "wrong sizeof was accepted or emitted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".segdef \"PAD\", $3000\n"
            ".assert sizeof(\"PAD\") .eq 0\n"
            ".assert startof(\"PAD\") .eq $3000\n"
            ".byte $44\n",
            &mem, &log, 0) != ASM_OK ||
        mem.memory[0x0801] != 0x44) {
        fprintf(stderr, "zero-length segment query failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    return failures;
}

static int test_end_of_address_space(void)
{
    test_memory mem;
    ERRORLOG log;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".segdef \"ROM\", end=$FFFF\n"
            ".segment \"ROM\"\n"
            "    .byte $EE\n"
            ".assert endof(\"ROM\") .eq $10000\n"
            ".assert sizeof(\"ROM\") .eq 1\n"
            ".assert startof(\"ROM\") .eq $FFFF\n",
            &mem, &log, 0) != ASM_OK ||
        mem.memory[0xFFFF] != 0xEE) {
        fprintf(stderr, "exclusive $10000 end query failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    return failures;
}

static int test_derived_layout(void)
{
    test_memory mem;
    ERRORLOG log;
    int failures = 0;
    const char *stable =
        ".segdef \"A\", $1000\n"
        ".segdef \"B\", after=\"A\", noemit\n"
        ".segdef \"C\", after=\"B\"\n"
        ".segdef \"TOP\", end=$10ff\n"
        ".segdef \"D\", after=\"TOP\"\n"
        ".segment \"A\"\n"
        "    .res $10, $a1\n"
        ".segment \"B\"\n"
        "    .res $10\n"
        ".segment \"C\"\n"
        "    .byte $c1\n"
        ".segment \"TOP\"\n"
        "    .res $10, $d1\n"
        ".segment \"D\"\n"
        "    .byte $d2\n"
        ".assert startof(\"A\") .eq $1000\n"
        ".assert endof(\"A\") .eq startof(\"B\")\n"
        ".assert sizeof(\"B\") .eq $10\n"
        ".assert startof(\"C\") .eq $1020\n"
        ".assert endof(\"TOP\") .eq $1100\n"
        ".assert startof(\"D\") .eq $1100\n";
    const char *moved =
        ".segdef \"LOW\", $1000\n"
        ".segdef \"A\", $1040\n"
        ".segdef \"B\", after=\"A\", noemit\n"
        ".segdef \"C\", after=\"B\"\n"
        ".segment \"LOW\"\n"
        "    .res $80, $11\n"
        ".segment \"A\"\n"
        "    .res $10, $a1\n"
        ".segment \"B\"\n"
        "    .res $10\n"
        ".segment \"C\"\n"
        "    .byte $c1\n"
        ".assert startof(\"A\") .eq $1080\n"
        ".assert endof(\"A\") .eq $1090\n"
        ".assert sizeof(\"B\") .eq $10\n"
        ".assert endof(\"B\") .eq startof(\"C\")\n"
        ".assert startof(\"C\") .eq $10A0\n";

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(stable, &mem, &log, 0) != ASM_OK ||
        mem.memory[0x1000] != 0xa1 || mem.memory[0x1020] != 0xc1 ||
        mem.memory[0x1100] != 0xd2) {
        fprintf(stderr, "stable after=/end= assertions failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(moved, &mem, &log, 1) != ASM_OK ||
        mem.memory[0x1080] != 0xa1 || mem.memory[0x10a0] != 0xc1) {
        fprintf(stderr, "auto-adjust after= assertions failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    return failures;
}

static int test_reclaim_queries(void)
{
    test_memory mem;
    ERRORLOG log;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".segdef \"TITLE\", $2000, reclaimable\n"
            ".segdef \"BSS\", $2000, noemit, overlap_reclaimable\n"
            ".segdef \"REUSE\", reclaim=\"TITLE\"\n"
            ".segment \"TITLE\"\n"
            "    .res $100, $11\n"
            ".segment \"BSS\"\n"
            "    .res $80\n"
            ".segment \"REUSE\"\n"
            "    .res $10\n"
            ".assert sizeof(\"TITLE\") .eq $100\n"
            ".assert sizeof(\"BSS\") .eq $80\n"
            ".assert startof(\"BSS\") .eq startof(\"TITLE\")\n"
            ".assert startof(\"REUSE\") .eq $2000\n"
            ".assert sizeof(\"REUSE\") .eq $10\n",
            &mem, &log, 0) != ASM_OK ||
        mem.memory[0x2000] != 0x11 || mem.writes != 0x100) {
        fprintf(stderr, "reclaimable layout queries failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    return failures;
}

static int test_unknown_and_other_target(void)
{
    test_memory mem;
    ERRORLOG log;
    assert_host host;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".byte $22\n"
            ".assert startof(\"NOPE\") .eq 0\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "Unknown segment \"NOPE\"")) {
        fprintf(stderr, "unknown segment was not rejected\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&host, 0, sizeof(host));
    errlog_init(&log);
    if (assemble_host(
            ".scope Other file=\"x.bin\"\n"
            ".segdef \"CODE\", $2000\n"
            ".segment \"CODE\"\n"
            "    .byte $11\n"
            ".endscope\n"
            ".byte $22\n"
            ".assert startof(\"CODE\") .eq $2000\n",
            &host, &log) == ASM_OK ||
        host.def.writes != 0 ||
        (host.extra && host.extra->writes != 0) ||
        !log_has(&log, "belongs to another target")) {
        fprintf(stderr, "wrong-target query was not rejected\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    free(host.extra);

    memset(&host, 0, sizeof(host));
    errlog_init(&log);
    if (assemble_host(
            ".scope Other file=\"x.bin\"\n"
            ".segdef \"CODE\", $2000\n"
            ".segment \"CODE\"\n"
            "    .byte $11\n"
            ".assert startof(\"CODE\") .eq $2000\n"
            ".endscope\n",
            &host, &log) != ASM_OK ||
        !host.extra || host.extra->writes != 1 ||
        host.extra->memory[0x2000] != 0x11 ||
        host.def.writes != 0) {
        fprintf(stderr, "same-target query inside a redirect failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    free(host.extra);
    return failures;
}

static int test_scope_include_and_macro(void)
{
    test_memory mem;
    ERRORLOG log;
    char inc_path[128];
    char main_path[128];
    char source[256];
    const char *base;
    const char *slash;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".scope Box\n"
            "hidden = 7\n"
            ".assert hidden .eq 7\n"
            ".endscope\n"
            ".assert here .eq $2000\n"
            ".segdef \"CODE\", $2000\n"
            ".segment \"CODE\"\n"
            "here:\n"
            "    .byte $01\n",
            &mem, &log, 0) != ASM_OK ||
        mem.memory[0x2000] != 0x01) {
        fprintf(stderr, "scoped and forward symbol assertions failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".if 0\n"
            ".assert 0, \"skipped\"\n"
            ".endif\n"
            ".byte $42\n",
            &mem, &log, 0) != ASM_OK ||
        log_has(&log, "skipped") ||
        mem.memory[0x0801] != 0x42) {
        fprintf(stderr, "skipped .assert was evaluated\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".macro chk val\n"
            ".assert val .eq 3\n"
            ".endmacro\n"
            "chk 3\n"
            ".byte $01\n",
            &mem, &log, 0) != ASM_OK ||
        mem.memory[0x0801] != 0x01) {
        fprintf(stderr, "macro-substituted assertion failed\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".macro boom\n"
            ".assert 0, \"from macro\"\n"
            ".endmacro\n"
            ".byte $01\n"
            "boom\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "from macro") ||
        !log_has(&log, "L:00002")) {
        fprintf(stderr, "macro-body assertion diagnostic mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    if (write_source(inc_path, sizeof(inc_path), ".assert 0, \"from include\"\n") != 0) {
        return failures + 1;
    }
    base = inc_path;
    slash = strrchr(inc_path, '/');
    if (strrchr(inc_path, '\\') &&
        (!slash || strrchr(inc_path, '\\') > slash)) {
        slash = strrchr(inc_path, '\\');
    }
    if (slash) {
        base = slash + 1;
    }
    snprintf(source, sizeof(source), ".byte $01\n.include \"%s\"\n", base);
    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (write_source(main_path, sizeof(main_path), source) != 0) {
        c64m_test_remove_file(inc_path);
        errlog_shutdown(&log);
        return failures + 1;
    }
    if (assemble_file(main_path, &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "from include") ||
        !log_has(&log, base)) {
        fprintf(stderr, "included assertion diagnostic mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    c64m_test_remove_file(main_path);
    c64m_test_remove_file(inc_path);
    return failures;
}

static int test_rejected_expressions(void)
{
    test_memory mem;
    ERRORLOG log;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            "n = 1\n"
            ".assert n++\n"
            ".byte n\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "cannot modify a symbol")) {
        fprintf(stderr, "assertion side effect was accepted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".assert * .eq 0\n.byte 1\n", &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "location counter is not valid")) {
        fprintf(stderr, "location counter was accepted in .assert\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".assert :+\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, "Anonymous labels are not valid")) {
        fprintf(stderr, "anonymous label was accepted in .assert\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".segdef \"CODE\", $2000\n"
            ".segment \"CODE\"\n"
            "    .byte startof(\"CODE\")\n",
            &mem, &log, 0) == ASM_OK ||
        !log_has(&log, "startof is only valid in .assert")) {
        fprintf(stderr, "layout query outside .assert was accepted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source("startof = 4\n.byte startof\n", &mem, &log, 0) != ASM_OK ||
        mem.memory[0x0801] != 4) {
        fprintf(stderr, "symbol named startof was stolen by the query\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    return failures;
}

static int test_explicit_error(void)
{
    test_memory mem;
    ERRORLOG log;
    char inc_path[128];
    char main_path[128];
    char source[256];
    const char *base;
    const char *slash;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".byte $11\n"
            ".error \"nope\"\n"
            ".error \"too\\tbig\"\n"
            ".error \"100% done\"\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 || mem.memory[0x0801] != 0 ||
        !log_has(&log, "nope") ||
        !log_has(&log, "too\tbig") ||
        !log_has(&log, "100% done") ||
        log_has(&log, "assertion failed")) {
        fprintf(stderr, "explicit .error diagnostic mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".byte $22\n.error\n", &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, ".error requires a string")) {
        fprintf(stderr, "missing .error message was accepted or emitted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".error \"\"\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, ".error requires a string")) {
        fprintf(stderr, "empty .error message was accepted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".error \"nope\" extra\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, "Unexpected token after .error message")) {
        fprintf(stderr, "trailing .error token was accepted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".if 0\n"
            ".error \"skipped\"\n"
            ".endif\n"
            ".byte $42\n",
            &mem, &log, 0) != ASM_OK ||
        log_has(&log, "skipped") ||
        mem.memory[0x0801] != 0x42 ||
        mem.writes != 1) {
        fprintf(stderr, "skipped .error was reported\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".if 1\n"
            ".error \"taken\"\n"
            ".endif\n"
            ".byte $01\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "taken")) {
        fprintf(stderr, "taken .error did not fail cleanly\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".macro boom\n"
            ".error \"from macro\"\n"
            ".endmacro\n"
            ".byte $01\n"
            "boom\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "from macro") ||
        !log_has(&log, "L:00002")) {
        fprintf(stderr, "macro-body .error diagnostic mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    if (write_source(inc_path, sizeof(inc_path), ".error \"from include\"\n") != 0) {
        return failures + 1;
    }
    base = inc_path;
    slash = strrchr(inc_path, '/');
    if (strrchr(inc_path, '\\') &&
        (!slash || strrchr(inc_path, '\\') > slash)) {
        slash = strrchr(inc_path, '\\');
    }
    if (slash) {
        base = slash + 1;
    }
    snprintf(source, sizeof(source), ".byte $01\n.include \"%s\"\n", base);
    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (write_source(main_path, sizeof(main_path), source) != 0) {
        c64m_test_remove_file(inc_path);
        errlog_shutdown(&log);
        return failures + 1;
    }
    if (assemble_file(main_path, &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has(&log, "from include") ||
        !log_has(&log, base)) {
        fprintf(stderr, "included .error diagnostic mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    c64m_test_remove_file(main_path);
    c64m_test_remove_file(inc_path);
    return failures;
}

static int test_explicit_warning(void)
{
    test_memory mem;
    ERRORLOG log;
    char inc_path[128];
    char main_path[128];
    char source[256];
    const char *base;
    const char *slash;
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".byte $11\n"
            ".warning \"careful\"\n"
            ".warning \"too\\tbig\"\n"
            ".warning \"100% done\"\n"
            ".byte $22\n",
            &mem, &log, 0) != ASM_OK ||
        mem.writes != 2 ||
        mem.memory[0x0801] != 0x11 ||
        mem.memory[0x0802] != 0x22 ||
        !log_has_severity(&log, "careful", ASM_SEVERITY_WARNING) ||
        !log_has_severity(&log, "too\tbig", ASM_SEVERITY_WARNING) ||
        !log_has_severity(&log, "100% done", ASM_SEVERITY_WARNING) ||
        errlog_error_count(&log) != 0) {
        fprintf(stderr, "warning did not assemble cleanly\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".byte $22\n.warning\n", &mem, &log, 0) == ASM_OK ||
        mem.writes != 1 || mem.memory[0x0801] != 0x22 ||
        !log_has_severity(&log, ".warning requires a string", ASM_SEVERITY_ERROR)) {
        fprintf(stderr, "missing .warning message was accepted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".warning \"\"\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, ".warning requires a string")) {
        fprintf(stderr, "empty .warning message was accepted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(".warning \"careful\" extra\n", &mem, &log, 0) == ASM_OK ||
        !log_has(&log, "Unexpected token after .warning message")) {
        fprintf(stderr, "trailing .warning token was accepted\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".if 0\n"
            ".warning \"skipped\"\n"
            ".endif\n"
            ".byte $42\n",
            &mem, &log, 0) != ASM_OK ||
        log_has(&log, "skipped") ||
        mem.memory[0x0801] != 0x42 ||
        mem.writes != 1) {
        fprintf(stderr, "skipped .warning was reported\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".warning \"heads up\"\n"
            ".assert 0, \"still checked\"\n"
            ".byte $44\n",
            &mem, &log, 0) == ASM_OK ||
        mem.writes != 0 ||
        !log_has_severity(&log, "heads up", ASM_SEVERITY_WARNING) ||
        !log_has_severity(&log, "still checked", ASM_SEVERITY_ERROR)) {
        fprintf(stderr, "warning suppressed a later assertion\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_source(
            ".macro boom\n"
            ".warning \"from macro\"\n"
            ".endmacro\n"
            ".byte $01\n"
            "boom\n",
            &mem, &log, 0) != ASM_OK ||
        mem.writes != 1 ||
        !log_has_severity(&log, "from macro", ASM_SEVERITY_WARNING) ||
        !log_has(&log, "L:00002")) {
        fprintf(stderr, "macro-body .warning diagnostic mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);

    if (write_source(inc_path, sizeof(inc_path), ".warning \"from include\"\n") != 0) {
        return failures + 1;
    }
    base = inc_path;
    slash = strrchr(inc_path, '/');
    if (strrchr(inc_path, '\\') &&
        (!slash || strrchr(inc_path, '\\') > slash)) {
        slash = strrchr(inc_path, '\\');
    }
    if (slash) {
        base = slash + 1;
    }
    snprintf(source, sizeof(source), ".byte $01\n.include \"%s\"\n", base);
    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (write_source(main_path, sizeof(main_path), source) != 0) {
        c64m_test_remove_file(inc_path);
        errlog_shutdown(&log);
        return failures + 1;
    }
    if (assemble_file(main_path, &mem, &log, 0) != ASM_OK ||
        mem.writes != 1 ||
        !log_has_severity(&log, "from include", ASM_SEVERITY_WARNING) ||
        !log_has(&log, base)) {
        fprintf(stderr, "included .warning diagnostic mismatch\n");
        dump_log(&log);
        failures++;
    }
    errlog_shutdown(&log);
    c64m_test_remove_file(main_path);
    c64m_test_remove_file(inc_path);
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += test_constant_assertions();
    failures += test_segment_queries();
    failures += test_end_of_address_space();
    failures += test_derived_layout();
    failures += test_reclaim_queries();
    failures += test_unknown_and_other_target();
    failures += test_scope_include_and_macro();
    failures += test_rejected_expressions();
    failures += test_explicit_error();
    failures += test_explicit_warning();
    if (failures) {
        fprintf(stderr, "%d assertion test failure(s)\n", failures);
        return 1;
    }
    return 0;
}
