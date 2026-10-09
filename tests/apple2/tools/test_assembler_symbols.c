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
    char name[64];
    uint16_t address;
} seen_symbol;

typedef struct {
    seen_symbol items[16];
    int count;
} seen_list;

static void output_byte(void *user, uint16_t addr, uint8_t val)
{
    test_memory *mem = (test_memory *)user;
    mem->memory[addr] = val;
    mem->writes++;
}

static void collect_symbol(const char *name, uint16_t address, void *user)
{
    seen_list *seen = (seen_list *)user;

    if (seen->count >= (int)(sizeof(seen->items) / sizeof(seen->items[0]))) {
        seen->count++;
        return;
    }
    snprintf(seen->items[seen->count].name, sizeof(seen->items[seen->count].name), "%s", name);
    seen->items[seen->count].address = address;
    seen->count++;
}

static const seen_symbol *find_symbol(const seen_list *seen, const char *name)
{
    int i;

    for (i = 0; i < seen->count && i < (int)(sizeof(seen->items) / sizeof(seen->items[0])); i++) {
        if (strcmp(seen->items[i].name, name) == 0) {
            return &seen->items[i];
        }
    }
    return NULL;
}

static void dump_log(const ERRORLOG *log)
{
    size_t i;

    for (i = 0; i < log->log_array.items; i++) {
        ERROR_ENTRY *entry = AM65_ARRAY_GET(&log->log_array, ERROR_ENTRY, i);
        fprintf(stderr, "  %s\n", entry && entry->err_str ? entry->err_str : "");
    }
}

/* The worked listing: three names, no anonymous label, and the emitted bytes. */
static int test_worked_symbol_walk(void)
{
    char path[128];
    test_memory mem;
    seen_list seen;
    ERRORLOG log;
    CB_ASM_CTX cb;
    ASSEMBLER as;
    const seen_symbol *sym;
    int failures = 0;
    const uint8_t expected[] = {
        0xA2, 0x00,
        0xBD, 0x0E, 0xC0,
        0xF0, 0x06,
        0x20, 0xFD, 0x0F,
        0xE8,
        0xD0, 0xF5,
        0x60,
        0x73, 0x74, 0x65, 0x66, 0x61, 0x6E, 0x00
    };
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

    memset(&mem, 0, sizeof(mem));
    memset(&seen, 0, sizeof(seen));
    if (a2m_test_write_temp_file(path, sizeof(path), "a2m_asm_symbols", source) != 0) {
        return 1;
    }

    errlog_init(&log);
    memset(&cb, 0, sizeof(cb));
    cb.user = &mem;
    cb.default_target = &mem;
    cb.output_byte = output_byte;
    if (assembler_init(&as, &log, &cb) != ASM_OK) {
        fprintf(stderr, "assembler_init failed\n");
        errlog_shutdown(&log);
        a2m_test_remove_file(path);
        return 1;
    }
    if (assembler_assemble(&as, path, 0) != ASM_OK) {
        fprintf(stderr, "worked example failed to assemble\n");
        dump_log(&log);
        failures++;
    } else {
        assembler_walk_symbols(&as, collect_symbol, &seen);
    }
    assembler_shutdown(&as);
    errlog_shutdown(&log);
    a2m_test_remove_file(path);

    if (failures != 0) {
        return failures;
    }
    if (seen.count != 3) {
        int i;
        fprintf(stderr, "expected 3 symbols, got %d\n", seen.count);
        for (i = 0; i < seen.count && i < (int)(sizeof(seen.items) / sizeof(seen.items[0])); i++) {
            fprintf(stderr, "  %04X  %s\n", seen.items[i].address, seen.items[i].name);
        }
        return 1;
    }
    sym = find_symbol(&seen, "print");
    if (sym == NULL || sym->address != 0x0FFD) {
        fprintf(stderr, "print missing or not at $0FFD\n");
        failures++;
    }
    sym = find_symbol(&seen, "done");
    if (sym == NULL || sym->address != 0xC00D) {
        fprintf(stderr, "done missing or not at $C00D\n");
        failures++;
    }
    sym = find_symbol(&seen, "name");
    if (sym == NULL || sym->address != 0xC00E) {
        fprintf(stderr, "name missing or not at $C00E\n");
        failures++;
    }
    if (memcmp(&mem.memory[0xC000], expected, sizeof(expected)) != 0 || mem.writes != sizeof(expected)) {
        size_t i;
        fprintf(stderr, "worked example bytes mismatch (%zu writes)\n", mem.writes);
        for (i = 0; i < sizeof(expected); i++) {
            fprintf(stderr, "  %04lX: %02X want %02X\n",
                    (unsigned long)(0xC000u + i),
                    mem.memory[0xC000 + i],
                    expected[i]);
        }
        failures++;
    }
    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_worked_symbol_walk();
    return failures == 0 ? 0 : 1;
}
