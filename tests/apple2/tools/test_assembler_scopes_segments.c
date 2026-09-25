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

static void output_byte(void *user, uint16_t addr, uint8_t val)
{
    test_memory *mem = (test_memory *)user;
    mem->memory[addr] = val;
    mem->writes++;
}

static int write_source(char *path, size_t path_size, const char *source)
{
    return a2m_test_write_temp_file(path, path_size, "a2m_assembler_scopes", source);
}

static int assemble_file(const char *path, test_memory *mem, ERRORLOG *log)
{
    CB_ASM_CTX cb;
    ASSEMBLER as;
    int result;

    memset(&cb, 0, sizeof(cb));
    cb.user = mem;
    cb.output_byte = output_byte;

    if (assembler_init(&as, log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed\n");
        return ASM_ERR;
    }

    result = assembler_assemble(&as, path, 0x0801);
    assembler_shutdown(&as);
    return result;
}

static int assemble_file_auto_adjust(const char *path, test_memory *mem, ERRORLOG *log)
{
    CB_ASM_CTX cb;
    ASSEMBLER as;
    int result;

    memset(&cb, 0, sizeof(cb));
    cb.user = mem;
    cb.output_byte = output_byte;

    if (assembler_init(&as, log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed\n");
        return ASM_ERR;
    }

    assembler_set_auto_adjust_segments(&as, 1);
    result = assembler_assemble(&as, path, 0x0801);
    assembler_shutdown(&as);
    return result;
}

static int test_scopes_and_procs(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    const uint8_t expected[] = {
        0x11,
        0x01, 0x08,
        0x22,
        0x04, 0x08,
        0x00, 0x01
    };
    const char *source =
        ".scope Alpha\n"
        "start:\n"
        "    .byte $11\n"
        ".endscope\n"
        ".word Alpha::start\n"
        ".proc Worker\n"
        "entry:\n"
        "    .byte $22\n"
        ".endproc\n"
        ".word Worker::entry\n"
        ".for i=0, i .lt 2, i++\n"
        "    .scope\n"
        "start:\n"
        "        .byte i\n"
        "    .endscope\n"
        ".endfor\n";
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    errlog_init(&log);
    if (assemble_file(path, &mem, &log) != ASM_OK) {
        fprintf(stderr, "scope/proc assembly failed with %zu errors\n", log.log_array.items);
        failures++;
    }
    if (memcmp(&mem.memory[0x0801], expected, sizeof(expected)) != 0) {
        fprintf(stderr, "scope/proc output mismatch\n");
        failures++;
    }
    errlog_shutdown(&log);
    a2m_test_remove_file(path);

    return failures;
}

static int test_quoted_scope_name(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    const uint8_t expected[] = {
        0x33,
        0x01, 0x08
    };
    /* A quoted scope name is equivalent to the bare identifier and stays
       referenceable through :: qualified lookup. */
    const char *source =
        ".scope \"Alpha\"\n"
        "start:\n"
        "    .byte $33\n"
        ".endscope\n"
        ".word Alpha::start\n";
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    errlog_init(&log);
    if (assemble_file(path, &mem, &log) != ASM_OK) {
        fprintf(stderr, "quoted scope name assembly failed with %zu errors\n", log.log_array.items);
        failures++;
    }
    if (memcmp(&mem.memory[0x0801], expected, sizeof(expected)) != 0) {
        fprintf(stderr, "quoted scope name output mismatch\n");
        failures++;
    }
    errlog_shutdown(&log);
    a2m_test_remove_file(path);

    return failures;
}

static int test_segments(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    const char *source =
        ".byte $01\n"
        ".segdef \"ZP\", $0002, noemit\n"
        ".segdef \"CODE2\", $0900\n"
        ".segment \"ZP\"\n"
        "    .byte $aa, $bb\n"
        ".segment \"CODE2\"\n"
        "    .byte $cc\n"
        ".segment \"\"\n"
        "    .byte $02\n";
    int failures = 0;

    memset(&mem, 0, sizeof(mem));
    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    errlog_init(&log);
    if (assemble_file(path, &mem, &log) != ASM_OK) {
        fprintf(stderr, "segment assembly failed with %zu errors\n", log.log_array.items);
        failures++;
    }
    if (mem.memory[0x0801] != 0x01 || mem.memory[0x0802] != 0x02 ||
        mem.memory[0x0900] != 0xcc || mem.memory[0x0002] != 0x00 ||
        mem.memory[0x0003] != 0x00) {
        fprintf(stderr, "segment output mismatch\n");
        failures++;
    }
    errlog_shutdown(&log);
    a2m_test_remove_file(path);

    return failures;
}

static int test_scope_segment_errors(void)
{
    struct {
        const char *name;
        const char *source;
    } cases[] = {
        {"top-level endscope", ".endscope\n"},
        {"top-level endproc", ".endproc\n"},
        {"missing segment", ".segment \"NOPE\"\n"},
        {"quoted non-identifier scope name", ".scope \"bad name\"\n.endscope\n"},
    };
    int failures = 0;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char path[128];
        test_memory mem;
        ERRORLOG log;

        memset(&mem, 0, sizeof(mem));
        if (write_source(path, sizeof(path), cases[i].source) != 0) {
            failures++;
            continue;
        }

        errlog_init(&log);
        if (assemble_file(path, &mem, &log) != ASM_ERR || log.log_array.items == 0) {
            fprintf(stderr, "%s was not rejected\n", cases[i].name);
            failures++;
        }
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
    }

    return failures;
}

typedef struct {
    size_t count;
    size_t target_index[8];
    char name[8][32];
    uint16_t address[8];
} adjustment_capture;

static void capture_adjustment(
    size_t target_index,
    const char *segment_name,
    uint16_t address,
    void *user)
{
    adjustment_capture *capture = (adjustment_capture *)user;
    if (capture->count >= 8) {
        return;
    }
    capture->target_index[capture->count] = target_index;
    snprintf(
        capture->name[capture->count],
        sizeof(capture->name[capture->count]),
        "%s",
        segment_name);
    capture->address[capture->count] = address;
    capture->count++;
}

static int test_segment_auto_adjust_converges(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    ASSEMBLER as;
    CB_ASM_CTX cb;
    adjustment_capture capture;
    int failures = 0;
    const char *source =
        ".segdef \"A\", $1000\n"
        ".segdef \"B\", $1080\n"
        ".segdef \"C\", $1200\n"
        ".segment \"A\"\n"
        "    .res $100\n"
        "    .byte $a1\n"
        ".segment \"B\"\n"
        "    .align $100\n"
        "    .byte $b1\n"
        ".segment \"C\"\n"
        "    .byte $c1\n";

    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    /* Existing/default behavior remains a hard overlap failure. */
    memset(&mem, 0, sizeof(mem));
    errlog_init(&log);
    if (assemble_file(path, &mem, &log) != ASM_ERR ||
        log.log_array.items == 0) {
        fprintf(stderr, "overlap unexpectedly succeeded without auto-adjust\n");
        failures++;
    }
    errlog_shutdown(&log);

    /* Moving B across the $1100 alignment boundary grows its padding, so the
       first suggestion makes B overlap C. A second retry must move C again. */
    memset(&mem, 0, sizeof(mem));
    memset(&capture, 0, sizeof(capture));
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.output_byte = output_byte;
    errlog_init(&log);
    if (assembler_init(&as, &log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed for auto-adjust test\n");
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
        return failures + 1;
    }
    assembler_set_auto_adjust_segments(&as, 1);
    if (assembler_assemble(&as, path, 0x0801) != ASM_OK) {
        fprintf(stderr, "auto-adjust assembly failed with %zu errors\n",
                log.log_array.items);
        failures++;
    }
    assembler_walk_segment_adjustments(&as, capture_adjustment, &capture);
    if (capture.count != 3 ||
        strcmp(capture.name[0], "A") != 0 || capture.address[0] != 0x1000 ||
        strcmp(capture.name[1], "B") != 0 || capture.address[1] != 0x1101 ||
        strcmp(capture.name[2], "C") != 0 || capture.address[2] != 0x1201) {
        fprintf(stderr, "auto-adjust final suggestion map mismatch\n");
        failures++;
    }
    if (mem.memory[0x1100] != 0xa1 ||
        mem.memory[0x1200] != 0xb1 ||
        mem.memory[0x1201] != 0xc1) {
        fprintf(stderr, "auto-adjust output landed at wrong addresses\n");
        failures++;
    }
    if (mem.writes != 514) {
        fprintf(stderr, "auto-adjust emitted provisional passes (%zu writes)\n",
                mem.writes);
        failures++;
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

static int errorlog_contains(const ERRORLOG *log, const char *needle)
{
    for (size_t i = 0; i < log->log_array.items; i++) {
        const ERROR_ENTRY *e = AM65_ARRAY_GET(&log->log_array, ERROR_ENTRY, i);
        if (e->err_str && strstr(e->err_str, needle) != NULL) {
            return 1;
        }
    }
    return 0;
}

/* A locked segment is an anchor auto-adjust must not move. When lower segments
   overrun it, the reorder is abandoned and assembly fails naming the anchor -
   rather than reshuffling the layout around the pinned address. */
static int test_segment_locked_blocks_reorder(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    ASSEMBLER as;
    CB_ASM_CTX cb;
    int failures = 0;
    const char *source =
        ".segdef \"CODE\", $1000\n"
        ".segdef \"HIRES\", $1080, locked\n"
        ".segment \"CODE\"\n"
        "    .res $100\n"
        "    .byte $c0\n"
        ".segment \"HIRES\"\n"
        "    .byte $11\n";

    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    memset(&mem, 0, sizeof(mem));
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.output_byte = output_byte;
    errlog_init(&log);
    if (assembler_init(&as, &log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed for locked-block test\n");
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
        return 1;
    }
    assembler_set_auto_adjust_segments(&as, 1);
    if (assembler_assemble(&as, path, 0x0801) != ASM_ERR) {
        fprintf(stderr, "auto-adjust moved a locked segment instead of failing\n");
        failures++;
    }
    if (!errorlog_contains(&log, "Locked segment")) {
        fprintf(stderr, "locked overrun did not report the anchor\n");
        failures++;
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

/* An overlap among non-locked segments still auto-adjusts around a locked
   anchor: the anchor keeps its declared address (no adjustment entry) while the
   lower segments are compacted. */
static int test_segment_locked_allows_reorder(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    ASSEMBLER as;
    CB_ASM_CTX cb;
    adjustment_capture capture;
    int failures = 0;
    const char *source =
        ".segdef \"A\", $1000\n"
        ".segdef \"B\", $1080\n"
        ".segdef \"HIRES\", $2000, emit, locked\n"
        ".segment \"A\"\n"
        "    .res $100\n"
        "    .byte $a1\n"
        ".segment \"B\"\n"
        "    .byte $b1\n"
        ".segment \"HIRES\"\n"
        "    .byte $11\n";

    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    memset(&mem, 0, sizeof(mem));
    memset(&capture, 0, sizeof(capture));
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.output_byte = output_byte;
    errlog_init(&log);
    if (assembler_init(&as, &log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed for locked-allow test\n");
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
        return 1;
    }
    assembler_set_auto_adjust_segments(&as, 1);
    if (assembler_assemble(&as, path, 0x0801) != ASM_OK) {
        fprintf(stderr, "auto-adjust around a locked anchor failed with %zu errors\n",
                log.log_array.items);
        failures++;
    }
    assembler_walk_segment_adjustments(&as, capture_adjustment, &capture);
    /* A and B are compacted; the locked HIRES anchor is never suggested. */
    if (capture.count != 2 ||
        strcmp(capture.name[0], "A") != 0 || capture.address[0] != 0x1000 ||
        strcmp(capture.name[1], "B") != 0 || capture.address[1] != 0x1101) {
        fprintf(stderr, "locked-anchor adjustment map mismatch (count %zu)\n",
                capture.count);
        failures++;
    }
    if (mem.memory[0x1100] != 0xa1 ||
        mem.memory[0x1101] != 0xb1 ||
        mem.memory[0x2000] != 0x11) {
        fprintf(stderr, "locked-anchor output landed at wrong addresses\n");
        failures++;
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

/* A reclaim segment piggybacks on an emitted host: it takes the host's start
   address, is implicitly noemit (writes nothing), and its labels resolve into
   the host's memory so runtime buffers can reuse that region. */
static int test_segment_reclaim(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    int failures = 0;
    const char *source =
        ".segdef \"TITLE\", $5000\n"
        ".segdef \"REUSE\", reclaim=\"TITLE\"\n"
        ".segment \"TITLE\"\n"
        "    .byte $11, $22, $33, $44\n"
        ".segment \"REUSE\"\n"
        "buf:\n"
        "    .res 2\n"
        ".segment \"\"\n"
        "    .word buf\n";

    memset(&mem, 0, sizeof(mem));
    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    errlog_init(&log);
    if (assemble_file(path, &mem, &log) != ASM_OK) {
        fprintf(stderr, "reclaim assembly failed with %zu errors\n", log.log_array.items);
        failures++;
    }
    /* buf resolves to TITLE's start ($5000). */
    if (mem.memory[0x0801] != 0x00 || mem.memory[0x0802] != 0x50) {
        fprintf(stderr, "reclaim label did not resolve to host start\n");
        failures++;
    }
    /* TITLE bytes intact -- the noemit reclaim segment wrote nothing over them. */
    if (mem.memory[0x5000] != 0x11 || mem.memory[0x5003] != 0x44) {
        fprintf(stderr, "reclaim host bytes clobbered or missing\n");
        failures++;
    }
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

/* A reclaim segment may not be larger than the host it piggybacks on. */
static int test_segment_reclaim_overflow(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    int failures = 0;
    const char *source =
        ".segdef \"TITLE\", $5000\n"
        ".segdef \"REUSE\", reclaim=\"TITLE\"\n"
        ".segment \"TITLE\"\n"
        "    .byte $11, $22\n"
        ".segment \"REUSE\"\n"
        "    .res 4\n";

    memset(&mem, 0, sizeof(mem));
    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    errlog_init(&log);
    if (assemble_file(path, &mem, &log) != ASM_ERR) {
        fprintf(stderr, "reclaim overflow was not rejected\n");
        failures++;
    }
    if (!errorlog_contains(&log, "overflows host")) {
        fprintf(stderr, "reclaim overflow did not name the host overflow\n");
        failures++;
    }
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

/* An auto-adjust move of the host drags its reclaim segment along, because the
   reclaim .segdef simply re-reads the host's (now adjusted) start on the retry
   re-parse. A grows past B, B is packed after A, and B's reclaim buffer follows
   B to its new address. */
static int test_segment_reclaim_follows_host(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    ASSEMBLER as;
    CB_ASM_CTX cb;
    int failures = 0;
    const char *source =
        ".segdef \"A\", $1000\n"
        ".segdef \"B\", $1040\n"
        ".segdef \"BR\", reclaim=\"B\"\n"
        ".segdef \"CHECK\", $4000, locked\n"
        ".segment \"A\"\n"
        "    .res $80\n"
        "    .byte $a1\n"
        ".segment \"B\"\n"
        "    .byte $b1\n"
        ".segment \"BR\"\n"
        "bufb:\n"
        "    .res 1\n"
        ".segment \"CHECK\"\n"
        "    .word bufb\n";

    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    memset(&mem, 0, sizeof(mem));
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.output_byte = output_byte;
    errlog_init(&log);
    if (assembler_init(&as, &log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed for reclaim-follows test\n");
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
        return 1;
    }
    assembler_set_auto_adjust_segments(&as, 1);
    if (assembler_assemble(&as, path, 0x0801) != ASM_OK) {
        fprintf(stderr, "reclaim-follows assembly failed with %zu errors\n",
                log.log_array.items);
        failures++;
    }
    /* B packed after A ($1000 + $81) = $1081; bufb rides along. The word is held
       in a locked high segment so it does not anchor the packer itself. */
    if (mem.memory[0x4000] != 0x81 || mem.memory[0x4001] != 0x10) {
        fprintf(stderr, "reclaim buffer did not follow host to $1081\n");
        failures++;
    }
    if (mem.memory[0x1081] != 0xb1) {
        fprintf(stderr, "host byte landed at wrong address after adjust\n");
        failures++;
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

/* An independently placed noemit segment may overlap any number of emitted
   reclaimable segments when both sides opt in. Its placement is not inherited
   from either emitted segment. */
static int test_segment_overlap_reclaimable(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    int failures = 0;
    const char *source =
        ".segdef \"TITLE_A\", $bff0, reclaimable\n"
        ".segdef \"TITLE_B\", $c020, reclaimable\n"
        ".segdef \"BSS\", end=$c02f, noemit, overlap_reclaimable\n"
        ".segdef \"CHECK\", $d000, locked\n"
        ".segment \"TITLE_A\"\n"
        "    .res $20, $a1\n"
        ".segment \"TITLE_B\"\n"
        "    .res $10, $b1\n"
        ".segment \"BSS\"\n"
        "bss_start:\n"
        "    .res $30\n"
        ".segment \"CHECK\"\n"
        "    .word bss_start\n";

    memset(&mem, 0, sizeof(mem));
    if (write_source(path, sizeof(path), source) != 0) {
        return 1;
    }

    errlog_init(&log);
    if (assemble_file_auto_adjust(path, &mem, &log) != ASM_OK) {
        fprintf(stderr, "overlap-reclaimable assembly failed with %zu errors\n",
                log.log_array.items);
        failures++;
    }
    if (mem.memory[0xd000] != 0x00 || mem.memory[0xd001] != 0xc0) {
        fprintf(stderr, "overlap-reclaimable BSS did not keep its own placement\n");
        failures++;
    }
    if (mem.memory[0xbff0] != 0xa1 || mem.memory[0xc00f] != 0xa1 ||
        mem.memory[0xc020] != 0xb1 || mem.memory[0xc02f] != 0xb1) {
        fprintf(stderr, "overlap-reclaimable noemit segment clobbered emitted data\n");
        failures++;
    }
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

/* reclaim= must name a defined, emitted host; a plain noemit segment may not
   overlap anything (that is what reclaim is for). */
static int test_segment_reclaim_and_noemit_errors(void)
{
    struct {
        const char *name;
        const char *source;
        const char *needle;
    } cases[] = {
        {"reclaim undefined host",
         ".segdef \"R\", reclaim=\"NOPE\"\n",
         "is not defined"},
        {"reclaim noemit host",
         ".segdef \"H\", $1000, noemit\n"
         ".segdef \"R\", reclaim=\"H\"\n",
         "must be an emitted segment"},
        {"noemit overlaps emit",
         ".segdef \"CODE\", $1000\n"
         ".segdef \"VARS\", $1002, noemit\n"
         ".segment \"CODE\"\n"
         "    .byte $01,$02,$03,$04\n"
         ".segment \"VARS\"\n"
         "    .res 2\n",
         "noemit segment"},
        {"noemit overlaps noemit",
         ".segdef \"V1\", $1000, noemit\n"
         ".segdef \"V2\", $1001, noemit\n"
         ".segment \"V1\"\n"
         "    .res 4\n"
         ".segment \"V2\"\n"
         "    .res 4\n",
         "noemit segment"},
        {"plain noemit overlaps reclaimable",
         ".segdef \"CODE\", $1000, reclaimable\n"
         ".segdef \"VARS\", $1001, noemit\n"
         ".segment \"CODE\"\n"
         "    .res 4\n"
         ".segment \"VARS\"\n"
         "    .res 2\n",
         "noemit segment"},
        {"opted-in noemit overlaps ordinary emit",
         ".segdef \"CODE\", $1000\n"
         ".segdef \"VARS\", $1001, noemit, overlap_reclaimable\n"
         ".segment \"CODE\"\n"
         "    .res 4\n"
         ".segment \"VARS\"\n"
         "    .res 2\n",
         "noemit segment"},
        {"reclaimable noemit",
         ".segdef \"BAD\", $1000, noemit, reclaimable\n",
         "requires an emitted segment"},
        {"overlap-reclaimable emit",
         ".segdef \"BAD\", $1000, overlap_reclaimable\n",
         "requires a noemit segment"},
    };
    int failures = 0;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char path[128];
        test_memory mem;
        ERRORLOG log;

        memset(&mem, 0, sizeof(mem));
        if (write_source(path, sizeof(path), cases[i].source) != 0) {
            failures++;
            continue;
        }

        errlog_init(&log);
        if (assemble_file(path, &mem, &log) != ASM_ERR) {
            fprintf(stderr, "%s was not rejected\n", cases[i].name);
            failures++;
        } else if (!errorlog_contains(&log, cases[i].needle)) {
            fprintf(stderr, "%s did not report \"%s\"\n", cases[i].name, cases[i].needle);
            failures++;
        }
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
    }

    return failures;
}

static int test_segment_end_at_64k_boundary(void)
{
    const struct {
        const char *name;
        const char *source;
        int expected_result;
        const char *error_text;
    } cases[] = {
        {"last byte",
         ".org $ffff\n"
         ".byte $a5\n",
         ASM_OK, NULL},
        {"full 64K noemit segment",
         ".segdef \"ALL\", $0000, noemit\n"
         ".segment \"ALL\"\n"
         ".res $10000\n",
         ASM_OK, NULL},
        {"byte beyond address space",
         ".org $ffff\n"
         ".byte 1, 2\n",
         ASM_ERR, "Output exceeds the 64K address space"},
        {"label beyond address space",
         ".org $ffff\n"
         ".byte 1\n"
         "past:\n",
         ASM_ERR, "Label is outside the 64K address space"},
    };
    int failures = 0;

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char path[128];
        test_memory mem;
        ERRORLOG log;

        memset(&mem, 0, sizeof(mem));
        if(write_source(path, sizeof(path), cases[i].source) != 0) {
            failures++;
            continue;
        }
        errlog_init(&log);
        int result = assemble_file(path, &mem, &log);
        if(result != cases[i].expected_result) {
            fprintf(stderr, "%s returned %d instead of %d\n",
                    cases[i].name, result, cases[i].expected_result);
            failures++;
        } else if(cases[i].error_text &&
                  !errorlog_contains(&log, cases[i].error_text)) {
            fprintf(stderr, "%s did not report \"%s\"\n",
                    cases[i].name, cases[i].error_text);
            failures++;
        }
        if(i == 0 && (mem.writes != 1 || mem.memory[0xffff] != 0xa5)) {
            fprintf(stderr, "last byte was not emitted at $FFFF\n");
            failures++;
        }
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
    }
    return failures;
}

static int test_end_anchored_segments(void)
{
    char path[128];
    test_memory mem;
    ERRORLOG log;
    int failures = 0;
    const char *source =
        ".segdef \"BSS\", end=$cfff, noemit\n"
        ".segdef \"ALIGNED\", end=$1300, noemit\n"
        ".segdef \"TOP\", end=$ffff, emit\n"
        ".segdef \"CODE\", $1000\n"
        ".segment \"BSS\"\n"
        "first: .res 1\n"
        "    * += 1\n"
        "middle: .res 1\n"
        ".segment \"ALIGNED\"\n"
        "    .res 1\n"
        "    .align $100\n"
        "aligned: .res 1\n"
        ".segment \"BSS\"\n"
        "last: .res 1\n"
        ".segment \"TOP\"\n"
        "    .byte $a5\n"
        ".segment \"CODE\"\n"
        "    .word first, middle, last, aligned\n"
        "    lda first\n";

    memset(&mem, 0, sizeof(mem));
    if(write_source(path, sizeof(path), source) != 0) {
        return 1;
    }
    errlog_init(&log);
    if(assemble_file(path, &mem, &log) != ASM_OK) {
        fprintf(stderr, "end-anchored assembly failed with %zu errors\n",
                log.log_array.items);
        failures++;
    }
    const uint8_t expected[] = {
        0xfc, 0xcf, 0xfe, 0xcf, 0xff, 0xcf, 0x00, 0x13,
        0xad, 0xfc, 0xcf
    };
    if(memcmp(&mem.memory[0x1000], expected, sizeof(expected)) != 0 ||
       mem.memory[0xffff] != 0xa5 || mem.writes != sizeof(expected) + 1) {
        fprintf(stderr, "end-anchored placement or output mismatch\n");
        failures++;
    }
    errlog_shutdown(&log);
    a2m_test_remove_file(path);
    return failures;
}

static int test_end_anchored_segment_errors(void)
{
    const struct {
        const char *name;
        const char *source;
        const char *needle;
    } cases[] = {
        {"too large",
         ".segdef \"E\", end=$0001, noemit\n"
         ".segment \"E\"\n"
         ".res 3\n",
         "only $0002 bytes fit"},
        {"empty",
         ".segdef \"E\", end=$cfff, noemit\n",
         "is empty"},
        {"org",
         ".segdef \"E\", end=$cfff, noemit\n"
         ".segment \"E\"\n"
         ".org $c000\n"
         ".res 1\n",
         ".org is not allowed"},
        {"absolute location assignment",
         ".segdef \"E\", end=$cfff, noemit\n"
         ".segment \"E\"\n"
         "* = $c000\n"
         ".res 1\n",
         "Absolute * = assignment is not allowed"},
        {"non-convergent alignment",
         ".segdef \"E\", end=$12ff, noemit\n"
         ".segment \"E\"\n"
         ".res 1\n"
         ".align $100\n"
         ".res 1\n",
         "did not converge"},
        {"unresolved end",
         ".segdef \"E\", end=later, noemit\n"
         "later = $cfff\n"
         ".segment \"E\"\n"
         ".res 1\n",
         "must resolve during pass 1"},
    };
    int failures = 0;

    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char path[128];
        test_memory mem;
        ERRORLOG log;
        memset(&mem, 0, sizeof(mem));
        if(write_source(path, sizeof(path), cases[i].source) != 0) {
            failures++;
            continue;
        }
        errlog_init(&log);
        if(assemble_file(path, &mem, &log) != ASM_ERR) {
            fprintf(stderr, "%s was not rejected\n", cases[i].name);
            failures++;
        } else if(!errorlog_contains(&log, cases[i].needle)) {
            fprintf(stderr, "%s did not report \"%s\"\n",
                    cases[i].name, cases[i].needle);
            failures++;
        }
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
    }
    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_scopes_and_procs();
    failures += test_quoted_scope_name();
    failures += test_segments();
    failures += test_scope_segment_errors();
    failures += test_segment_auto_adjust_converges();
    failures += test_segment_locked_blocks_reorder();
    failures += test_segment_locked_allows_reorder();
    failures += test_segment_reclaim();
    failures += test_segment_reclaim_overflow();
    failures += test_segment_reclaim_follows_host();
    failures += test_segment_overlap_reclaimable();
    failures += test_segment_reclaim_and_noemit_errors();
    failures += test_segment_end_at_64k_boundary();
    failures += test_end_anchored_segments();
    failures += test_end_anchored_segment_errors();

    return failures == 0 ? 0 : 1;
}
