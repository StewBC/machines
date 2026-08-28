#include "cpu_pane_6502.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static uint16_t g_pc;
static uint8_t g_status;

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", \
                    __FILE__, __LINE__, #expr); \
            failures++; \
        } \
    } while (0)

static void set_pc(void *ctx, uint16_t value)
{
    (void)ctx;
    g_pc = value;
}

static void set_status(void *ctx, uint8_t value)
{
    (void)ctx;
    g_status = value;
}

int main(void)
{
    cpu_pane_6502_state state;
    cpu_pane_6502_regs regs;
    cpu_pane_6502_ops ops;
    uint16_t value;
    uint8_t flag;

    cpu_pane_6502_init(&state);
    memset(&regs, 0, sizeof(regs));
    regs.pc = 0x1234;
    regs.a = 0xAB;
    regs.p = 0x80u; /* N */
    cpu_pane_6502_format(&state, &regs, CPU_PANE_6502_FIELD_NONE);
    CHECK(strcmp(state.pc, "1234") == 0);
    CHECK(strcmp(state.a, "AB") == 0);
    CHECK(state.flags[0][0] == '1');

    CHECK(cpu_pane_6502_parse_hex("00FF", 4, &value));
    CHECK(value == 0x00FFu);
    CHECK(!cpu_pane_6502_parse_hex("GG", 2, &value));
    CHECK(cpu_pane_6502_parse_flag("1", &flag) && flag == 1u);

    memset(&ops, 0, sizeof(ops));
    ops.set_pc = set_pc;
    ops.set_status = set_status;
    memcpy(state.pc, "C000", 5);
    cpu_pane_6502_commit(&state, CPU_PANE_6502_FIELD_PC, &regs, &ops);
    CHECK(g_pc == 0xC000u);

    memcpy(state.flags[0], "0", 2);
    cpu_pane_6502_commit(&state, CPU_PANE_6502_FIELD_STATUS_N, &regs, &ops);
    CHECK(g_status == 0u);

    if (failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
