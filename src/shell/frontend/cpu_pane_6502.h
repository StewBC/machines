#pragma once

#include "nuklear_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cpu_pane_6502_field {
    CPU_PANE_6502_FIELD_NONE = 0,
    CPU_PANE_6502_FIELD_PC,
    CPU_PANE_6502_FIELD_SP,
    CPU_PANE_6502_FIELD_A,
    CPU_PANE_6502_FIELD_X,
    CPU_PANE_6502_FIELD_Y,
    CPU_PANE_6502_FIELD_STATUS_N,
    CPU_PANE_6502_FIELD_STATUS_V,
    CPU_PANE_6502_FIELD_STATUS_UNUSED,
    CPU_PANE_6502_FIELD_STATUS_B,
    CPU_PANE_6502_FIELD_STATUS_D,
    CPU_PANE_6502_FIELD_STATUS_I,
    CPU_PANE_6502_FIELD_STATUS_Z,
    CPU_PANE_6502_FIELD_STATUS_C
} cpu_pane_6502_field;

typedef struct cpu_pane_6502_regs {
    uint16_t pc;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t sp;
    uint8_t p;
} cpu_pane_6502_regs;

typedef struct cpu_pane_6502_state {
    cpu_pane_6502_field active_field;
    char pc[5];
    char sp[3];
    char a[3];
    char x[3];
    char y[3];
    char flags[8][2];
    bool cancel_edit;
} cpu_pane_6502_state;

typedef struct cpu_pane_6502_ops {
    void *ctx;
    void (*set_pc)(void *ctx, uint16_t value);
    void (*set_sp)(void *ctx, uint8_t value);
    void (*set_a)(void *ctx, uint8_t value);
    void (*set_x)(void *ctx, uint8_t value);
    void (*set_y)(void *ctx, uint8_t value);
    void (*set_status)(void *ctx, uint8_t value);
} cpu_pane_6502_ops;

void cpu_pane_6502_init(cpu_pane_6502_state *state);

void cpu_pane_6502_format(
    cpu_pane_6502_state *state,
    const cpu_pane_6502_regs *regs,
    cpu_pane_6502_field except);

bool cpu_pane_6502_parse_hex(const char *text, size_t max_digits, uint16_t *out);
bool cpu_pane_6502_parse_flag(const char *text, uint8_t *out);

void cpu_pane_6502_commit(
    cpu_pane_6502_state *state,
    cpu_pane_6502_field field,
    const cpu_pane_6502_regs *regs,
    const cpu_pane_6502_ops *ops);

void cpu_pane_6502_draw(
    struct nk_context *ctx,
    struct nk_rect bounds,
    cpu_pane_6502_state *state,
    const cpu_pane_6502_regs *regs,
    bool has_cpu,
    bool editable,
    const char *empty_status,
    const cpu_pane_6502_ops *ops);

#ifdef __cplusplus
}
#endif
