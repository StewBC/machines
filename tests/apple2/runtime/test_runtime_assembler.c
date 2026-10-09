#include "runtime.h"
#include "runtime_client.h"
#include "runtime_assembler.h"
#include "runtime_event.h"
#include "apple2.h"
#include "am65/source_map.h"
#include "../test_file.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static int poll_event(
    runtime_client *client,
    runtime_event *event,
    runtime_event_type type,
    double timeout_s)
{
    clock_t start = clock();
    while ((double)(clock() - start) / (double)CLOCKS_PER_SEC < timeout_s) {
        while (runtime_client_poll_event(client, event)) {
            if (event->type == RUNTIME_EVENT_ERROR) {
                fprintf(stderr, "runtime error: %s\n", event->data.error.message);
                exit(1);
            }
            if (event->type == type) {
                return 1;
            }
        }
    }
    return 0;
}

static int file_text_has(const char *path, const char *needle)
{
    FILE *fp = fopen(path, "rb");
    char buf[4096];
    size_t n;

    if (fp == NULL) {
        return 0;
    }
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';
    return strstr(buf, needle) != NULL;
}

static void expect_session_span(const asm_source_image *image, uint16_t pc, int present)
{
    asm_source_hit hit = asm_source_span_at_pc(image, NULL, 0, pc);

    if (present) {
        if (hit.span == NULL || hit.image != image) {
            fprintf(stderr, "FAIL: session span at $%04X missing\n", pc);
            exit(1);
        }
        return;
    }
    if (hit.span != NULL) {
        fprintf(stderr, "FAIL: session span at $%04X was published\n", pc);
        exit(1);
    }
}

static void test_session_image_direct(void)
{
    static apple2_t machine;
    char path[128];
    char error[256];
    asm_source_image *memory_image = NULL;
    asm_source_image *file_image = NULL;
    asm_source_image *failed = (asm_source_image *)0x1;
    const char *slash;

    if (!apple2_init(&machine)) {
        fail("apple2_init");
    }
    if (a2m_test_write_temp_file(path, sizeof(path), "a2m_rt_src_mem", "nop\n") != 0) {
        fail("memory source");
    }
    if (!runtime_assemble_file_ex_options(
            &machine, NULL, path, 0x3000, "current", NULL,
            NULL, NULL, NULL, NULL, 0, error, sizeof(error), &memory_image) ||
        memory_image == NULL) {
        fprintf(stderr, "FAIL: memory session image: %s\n", error);
        exit(1);
    }
    expect_session_span(memory_image, 0x3000, 1);

    if (a2m_test_write_temp_file(
            path,
            sizeof(path),
            "a2m_rt_src_file",
            ".scope side file=\"side.bin\"\n"
            "    lda #1\n"
            ".endscope\n") != 0) {
        fail("file-only source");
    }
    if (!runtime_assemble_file_ex_options(
            &machine, NULL, path, 0x3000, "current", NULL,
            NULL, NULL, NULL, NULL, 0, error, sizeof(error), &file_image) ||
        file_image == NULL || file_image->file_count != 0u) {
        fprintf(stderr, "FAIL: file-only session image: %s\n", error);
        exit(1);
    }
    if (!runtime_assemble_file_ex_options(
            &machine, NULL, path, 0x3000, "current", NULL,
            NULL, NULL, NULL, NULL, 0, error, sizeof(error), NULL) ||
        file_image->file_count != 0u) {
        fail("NULL out-pointer changed the caller image");
    }
    if (a2m_test_write_temp_file(path, sizeof(path), "a2m_rt_src_bad", "lda missing\n") != 0) {
        fail("failed source");
    }
    if (runtime_assemble_file_ex_options(
            &machine, NULL, path, 0x3000, "current", NULL,
            NULL, NULL, NULL, NULL, 0, error, sizeof(error), &failed) ||
        failed != NULL) {
        fail("failed assemble published a session image");
    }

    slash = strrchr(path, '/');
    if (slash == NULL) {
        a2m_test_remove_file("side.bin");
        a2m_test_remove_file("side.bin.sym");
    } else {
        char side[180];
        char sym[200];

        snprintf(side, sizeof(side), "%.*sside.bin", (int)(slash - path + 1), path);
        snprintf(sym, sizeof(sym), "%s.sym", side);
        a2m_test_remove_file(side);
        a2m_test_remove_file(sym);
    }
    asm_source_image_free(memory_image);
    asm_source_image_free(file_image);
    asm_source_image_free(failed);
    a2m_test_remove_file(path);
    apple2_shutdown(&machine);
}

static int snapshot_find(
    const runtime_symbol_snapshot *snap,
    const char *name,
    uint16_t *address,
    symbol_source_kind *kind)
{
    size_t i;

    for (i = 0; i < snap->count; i++) {
        const runtime_symbol_snapshot_entry *entry = &snap->entries[i];
        const runtime_symbol_source_snapshot_entry *source;

        if (strcmp(entry->name, name) != 0) {
            continue;
        }
        source = runtime_symbol_snapshot_find_source(snap, entry->source_id);
        if (address != NULL) {
            *address = entry->address;
        }
        if (kind != NULL && source != NULL) {
            *kind = (symbol_source_kind)source->source_kind;
        }
        return 1;
    }
    return 0;
}

int main(void)
{
    runtime_config config;
    runtime *rt;
    runtime_client *client;
    runtime_event event;
    char path[128];
    char targets_path[128];
    char invalid_path[128];
    char symbol_path[160];
    char follow_path[160];
    static runtime_symbol_snapshot symbol_snap;
    static asm_source_publish source_publish;
    uint32_t source_generation = 0u;
    const char *source =
        "* = $3000\n"
        "print = $0ffd\n"
        "entry:\n"
        "    lda #$42\n"
        "    sta $0300\n"
        "    stz $0301\n"
        "    rts\n";

    test_session_image_direct();

    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fail("SDL_Init");
    }

    if (a2m_test_write_temp_file(path, sizeof(path), "a2m_rt_asm", source) != 0) {
        fail("temp file");
    }

    if (a2m_test_write_temp_file(
            symbol_path, sizeof(symbol_path), "a2m_rt_sym", "2222  FROMFILE\n") != 0) {
        fail("symbol file");
    }

    runtime_config_init(&config);
    config.start_running = false;
    config.symbol_files = symbol_path;
    rt = runtime_create(&config);
    if (rt == NULL || !runtime_start(rt)) {
        fail("runtime start");
    }
    client = runtime_get_client(rt);

    if (!poll_event(client, &event, RUNTIME_EVENT_STARTED, 2.0)) {
        fail("STARTED");
    }
    (void)poll_event(client, &event, RUNTIME_EVENT_PAUSED, 2.0);

    if (!runtime_client_assemble_file(client, path, 0x3000)) {
        fail("assemble request");
    }
    if (!poll_event(client, &event, RUNTIME_EVENT_ASSEMBLE_COMPLETE, 5.0)) {
        fail("ASSEMBLE_COMPLETE");
    }
    if (event.data.assemble.address != 0x3000u ||
        strcmp(event.data.assemble.path, path) != 0) {
        fail("assemble meta");
    }

    if (!runtime_client_request_memory(client, 0x3000, 8, RUNTIME_MEMORY_MODE_MAP)) {
        fail("request memory");
    }
    if (!poll_event(client, &event, RUNTIME_EVENT_MEMORY_RESPONSE, 2.0)) {
        fail("MEMORY_RESPONSE");
    }
    if (event.data.memory.bytes[0] != 0xA9 || event.data.memory.bytes[1] != 0x42) {
        fprintf(
            stderr,
            "bytes: %02X %02X\n",
            event.data.memory.bytes[0],
            event.data.memory.bytes[1]);
        fail("lda not present");
    }

    {
        uint16_t address = 0;
        symbol_source_kind kind = SYMBOL_SOURCE_USER;

        if (!runtime_client_poll_symbols(client, &symbol_snap)) {
            fail("symbol snapshot after assemble");
        }
        if (!snapshot_find(&symbol_snap, "entry", &address, &kind) ||
            address != 0x3000 || kind != SYMBOL_SOURCE_ASSEMBLER) {
            fail("entry was not imported");
        }
        if (!snapshot_find(&symbol_snap, "print", &address, &kind) ||
            address != 0x0ffd || kind != SYMBOL_SOURCE_ASSEMBLER) {
            fail("equate was not imported");
        }
        if (!snapshot_find(&symbol_snap, "FROMFILE", &address, &kind) ||
            address != 0x2222 || kind != SYMBOL_SOURCE_FILE) {
            fail("loaded file symbol missing after assemble");
        }
    }

    {
        asm_source_hit hit;

        if (!runtime_client_copy_source_if_changed(
                client, &source_publish, &source_generation)) {
            fail("session image after assemble");
        }
        hit = asm_source_span_at_pc(source_publish.session, NULL, 0, 0x3000);
        if (hit.span == NULL || hit.image != source_publish.session) {
            fail("session span at $3000");
        }
        (void)runtime_client_poll_symbols(client, &symbol_snap);
        if (runtime_client_copy_source_if_changed(
                client, &source_publish, &source_generation)) {
            fail("symbol poll replaced the session image");
        }
        hit = asm_source_span_at_pc(source_publish.session, NULL, 0, 0x3000);
        if (hit.span == NULL) {
            fail("symbol poll cleared the session image");
        }
    }

    {
        char file_only_path[160];
        char dual_path[160];
        const char *slash;
        const char *targets_source =
            ".if AM65 .eq 0\n"
            "    .if APPLE2 .eq 1\n"
            "        .scope file_only file=\"file_only.bin\"\n"
            "            .org $3100\n"
            "            file_mark: .byte $a2\n"
            "            file_equ = $42\n"
            "        .endscope\n"
            "        .scope dual file=\"dual.bin\" dest=\"aux\"\n"
            "            .org $3400\n"
            "            .byte $ad\n"
            "        .endscope\n"
            "    .endif\n"
            ".endif\n"
            ".scope auxiliary dest=\"aux\"\n"
            "    .org $3200\n"
            "    aux_mark: .byte $a8\n"
            ".endscope\n"
            ".scope language_card dest=\"lc2\"\n"
            "    .org $d000\n"
            "    .byte $c2\n"
            ".endscope\n"
            ".scope combined dest=\"aux,lc2\"\n"
            "    .org $3300\n"
            "    .byte $ac\n"
            ".endscope\n";

        if (a2m_test_write_temp_file(
                targets_path,
                sizeof(targets_path),
                "a2m_rt_asm_targets",
                targets_source) != 0) {
            fail("target temp file");
        }
        slash = strrchr(targets_path, '/');
#if defined(_WIN32)
        {
            const char *backslash = strrchr(targets_path, '\\');
            if (backslash != NULL && (slash == NULL || backslash > slash)) {
                slash = backslash;
            }
        }
#endif
        if (slash == NULL) {
            snprintf(file_only_path, sizeof(file_only_path), "file_only.bin");
            snprintf(dual_path, sizeof(dual_path), "dual.bin");
        } else {
            snprintf(
                file_only_path,
                sizeof(file_only_path),
                "%.*sfile_only.bin",
                (int)(slash - targets_path + 1),
                targets_path);
            snprintf(
                dual_path,
                sizeof(dual_path),
                "%.*sdual.bin",
                (int)(slash - targets_path + 1),
                targets_path);
        }
        if (!runtime_client_assemble_file(client, targets_path, 0x3000)) {
            fail("target assemble request");
        }
        if (!poll_event(client, &event, RUNTIME_EVENT_ASSEMBLE_COMPLETE, 5.0)) {
            fail("target ASSEMBLE_COMPLETE");
        }

        /* file= alone writes a host file and must not poke memory. */
        if (!runtime_client_request_memory(client, 0x3100, 1, RUNTIME_MEMORY_MODE_MAP) ||
            !poll_event(client, &event, RUNTIME_EVENT_MEMORY_RESPONSE, 2.0) ||
            event.data.memory.bytes[0] == 0xA2) {
            fail("file-only scope poked memory");
        }
        {
            FILE *fp = fopen(file_only_path, "rb");
            unsigned char byte = 0;

            if (fp == NULL || fread(&byte, 1, 1, fp) != 1 || byte != 0xA2) {
                if (fp != NULL) {
                    fclose(fp);
                }
                fail("file-only scope did not write file_only.bin");
            }
            fclose(fp);
        }
        {
            char sym_path[180];
            char source_sym[180];

            snprintf(sym_path, sizeof(sym_path), "%s.sym", file_only_path);
            snprintf(source_sym, sizeof(source_sym), "%s.sym", targets_path);
            if (!file_text_has(sym_path, "3100  file_only::file_mark") ||
                !file_text_has(sym_path, "0042  file_only::file_equ")) {
                fail("file_only.bin.sym missing file-only symbols");
            }
            if (file_text_has(sym_path, "aux_mark")) {
                fail("dest-only label was written into file_only.bin.sym");
            }
            {
                FILE *source_fp = fopen(source_sym, "rb");
                if (source_fp != NULL) {
                    fclose(source_fp);
                    fail("memory assemble grew a sidecar beside the source");
                }
            }
            a2m_test_remove_file(sym_path);
        }

        /* file= + dest= writes memory and a host file. */
        if (!runtime_client_request_memory(client, 0x3400, 1, RUNTIME_MEMORY_MODE_AUX) ||
            !poll_event(client, &event, RUNTIME_EVENT_MEMORY_RESPONSE, 2.0) ||
            event.data.memory.bytes[0] != 0xAD) {
            fail("dual file+dest memory write");
        }
        {
            FILE *fp = fopen(dual_path, "rb");
            unsigned char byte = 0;

            if (fp == NULL || fread(&byte, 1, 1, fp) != 1 || byte != 0xAD) {
                if (fp != NULL) {
                    fclose(fp);
                }
                fail("dual file+dest did not write dual.bin");
            }
            fclose(fp);
        }
        {
            char dual_sym[180];
            snprintf(dual_sym, sizeof(dual_sym), "%s.sym", dual_path);
            if (file_text_has(dual_sym, "aux_mark") ||
                file_text_has(dual_sym, "file_only::file_mark")) {
                fail("dual.bin.sym contains another target's label");
            }
            a2m_test_remove_file(dual_sym);
        }

        if (!runtime_client_request_memory(client, 0x3200, 1, RUNTIME_MEMORY_MODE_AUX) ||
            !poll_event(client, &event, RUNTIME_EVENT_MEMORY_RESPONSE, 2.0) ||
            event.data.memory.bytes[0] != 0xA8) {
            fail("aux destination");
        }
        if (!runtime_client_request_memory(client, 0xD000, 1, RUNTIME_MEMORY_MODE_LC2) ||
            !poll_event(client, &event, RUNTIME_EVENT_MEMORY_RESPONSE, 2.0) ||
            event.data.memory.bytes[0] != 0xC2) {
            fail("lc2 destination");
        }
        if (!runtime_client_request_memory(client, 0x3300, 1, RUNTIME_MEMORY_MODE_AUX) ||
            !poll_event(client, &event, RUNTIME_EVENT_MEMORY_RESPONSE, 2.0) ||
            event.data.memory.bytes[0] != 0xAC) {
            fail("combined aux,lc2 destination");
        }
        {
            asm_source_hit hit;

            if (!runtime_client_copy_source_if_changed(
                    client, &source_publish, &source_generation)) {
                fail("session image after target assemble");
            }
            hit = asm_source_span_at_pc(
                source_publish.session,
                (const asm_source_image *const *)source_publish.file_maps,
                source_publish.file_map_count,
                0x3200);
            if (hit.span == NULL || hit.image != source_publish.session) {
                fail("dest span missing from session image");
            }
            hit = asm_source_span_at_pc(
                source_publish.session,
                (const asm_source_image *const *)source_publish.file_maps,
                source_publish.file_map_count,
                0x3100);
            if (hit.span != NULL) {
                fail("file-only span was published in the session image");
            }
            hit = asm_source_span_at_pc(source_publish.session, NULL, 0, 0x3000);
            if (hit.span != NULL) {
                fail("session image was not replaced");
            }
        }

        a2m_test_remove_file(file_only_path);
        a2m_test_remove_file(dual_path);
    }

    {
        const char *invalid_source =
            ".scope obsolete dest=\"6502\"\n"
            "    .byte $ff\n"
            ".endscope\n";

        if (a2m_test_write_temp_file(
                invalid_path,
                sizeof(invalid_path),
                "a2m_rt_asm_invalid_dest",
                invalid_source) != 0) {
            fail("invalid target temp file");
        }
        if (!runtime_client_assemble_file(client, invalid_path, 0x3000)) {
            fail("invalid target assemble request");
        }
        if (!poll_event(client, &event, RUNTIME_EVENT_ASSEMBLE_ERROR, 5.0)) {
            fail("6502 destination should be rejected");
        }
    }

    {
        uint16_t address = 0;
        symbol_source_kind kind = SYMBOL_SOURCE_USER;
        const char *follow_source =
            "* = $3000\n"
            "other:\n"
            "    lda #$01\n";

        if (a2m_test_write_temp_file(
                follow_path, sizeof(follow_path), "a2m_rt_asm_follow", follow_source) != 0) {
            fail("follow temp file");
        }
        if (!runtime_client_assemble_file(client, follow_path, 0x3000)) {
            fail("follow assemble request");
        }
        if (!poll_event(client, &event, RUNTIME_EVENT_ASSEMBLE_COMPLETE, 5.0)) {
            fail("follow ASSEMBLE_COMPLETE");
        }
        if (!runtime_client_poll_symbols(client, &symbol_snap)) {
            fail("symbol snapshot after following assemble");
        }
        if (!snapshot_find(&symbol_snap, "other", &address, &kind) ||
            address != 0x3000 || kind != SYMBOL_SOURCE_ASSEMBLER) {
            fail("following assemble did not import other");
        }
        if (snapshot_find(&symbol_snap, "entry", NULL, NULL) ||
            snapshot_find(&symbol_snap, "print", NULL, NULL) ||
            snapshot_find(&symbol_snap, "file_only::file_mark", NULL, NULL)) {
            fail("following assemble left assembler symbols in place");
        }
        if (!snapshot_find(&symbol_snap, "FROMFILE", &address, &kind) ||
            address != 0x2222 || kind != SYMBOL_SOURCE_FILE) {
            fail("following assemble removed the loaded file symbol");
        }
        a2m_test_remove_file(follow_path);
        a2m_test_remove_file(symbol_path);
    }

    asm_source_publish_release(&source_publish);
    memset(&source_publish, 0, sizeof(source_publish));
    source_generation = 0u;
    (void)runtime_client_quit(client);
    (void)poll_event(client, &event, RUNTIME_EVENT_STOPPED, 2.0);
    runtime_stop(rt);
    runtime_destroy(rt);

    /* The ][+ selects the portable 6502 profile, so the same undirected STZ
       source must be rejected unless it explicitly opts into .65c02. */
    runtime_config_init(&config);
    config.start_running = false;
    config.apple_model = 1;
    rt = runtime_create(&config);
    if (rt == NULL || !runtime_start(rt)) {
        fail("runtime ][+ start");
    }
    client = runtime_get_client(rt);
    if (!poll_event(client, &event, RUNTIME_EVENT_STARTED, 2.0)) {
        fail("][+ STARTED");
    }
    (void)poll_event(client, &event, RUNTIME_EVENT_PAUSED, 2.0);
    if (!runtime_client_copy_source_if_changed(
            client, &source_publish, &source_generation) ||
        source_publish.session != NULL) {
        fail("][+ source publish before assemble");
    }
    if (!runtime_client_assemble_file(client, path, 0x3000)) {
        fail("][+ assemble request");
    }
    if (!poll_event(client, &event, RUNTIME_EVENT_ASSEMBLE_ERROR, 5.0)) {
        fail("][+ should reject 65C02 opcode");
    }
    if (runtime_client_copy_source_if_changed(
            client, &source_publish, &source_generation) ||
        source_publish.session != NULL) {
        fail("failed assemble replaced the session image");
    }
    asm_source_publish_release(&source_publish);
    (void)runtime_client_quit(client);
    (void)poll_event(client, &event, RUNTIME_EVENT_STOPPED, 2.0);
    runtime_stop(rt);
    runtime_destroy(rt);

    a2m_test_remove_file(path);
    a2m_test_remove_file(targets_path);
    a2m_test_remove_file(invalid_path);
    SDL_Quit();
    printf("ok\n");
    return 0;
}
