#pragma once

#include "debugger_disasm.h"
#include "disasm_6502.h"
#include "disasm_pc_lock.h"
#include "memory_source.h"
#include "nuklear_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DISASM_PANE_CACHE_MAX = 8
};

typedef struct disasm_pane_state {
    debugger_disasm_view chrome;
    uint16_t top_address;
    uint16_t cursor_address;
    uint16_t pc_lock_address;
    uint32_t source_id;
    disasm_6502_cpu_class cpu_class;
    uint8_t rows;
    uint8_t cursor_row;
    uint8_t cursor_length;
    uint16_t last_pc;
    bool initialized;
    bool request_pending;
    bool active;
    bool follow_pc;
    bool pc_lock_active;
    bool has_last_pc;
    bool has_user_cursor;
    bool scrollbar_dragging;
    float scrollbar_grab_offset;
    disasm_pc_lock_line lines[DISASM_PC_LOCK_MAX_ROWS];
    disasm_pc_lock_cache mem_cache[DISASM_PANE_CACHE_MAX];
    uint32_t cache_ids[DISASM_PANE_CACHE_MAX];
    int cache_count;
} disasm_pane_state;

typedef struct disasm_pane_ops {
    void *ctx;
    const symbol_resolver *symbols;
    void (*request)(void *ctx, uint32_t source_id, uint16_t address, uint16_t length);
    void (*toggle_execute_bp)(void *ctx);
    void (*set_pc)(void *ctx, uint16_t address);
    void (*open_symbol_lookup)(void *ctx);
    bool (*any_dialog_open)(void *ctx);
    bool (*view_is_active)(void *ctx);
    void (*draw_context_menu)(void *ctx, struct nk_context *nk);
    uint16_t (*focus_pc)(void *ctx);
    bool (*focus_valid)(void *ctx);
    bool (*keys_enabled)(void *ctx);
} disasm_pane_ops;

void disasm_pane_init(disasm_pane_state *state, uint32_t source_id, disasm_6502_cpu_class cpu);

disasm_pc_lock_cache *disasm_pane_cache(disasm_pane_state *state, uint32_t source_id);

void disasm_pane_merge_bytes(
    disasm_pane_state *state,
    uint32_t source_id,
    uint16_t address,
    const uint8_t *bytes,
    uint16_t length);

void disasm_pane_decode(disasm_pane_state *state, const symbol_resolver *symbols);

void disasm_pane_handle_key(
    disasm_pane_state *state,
    const SDL_KeyboardEvent *key,
    const memory_source *table,
    size_t count,
    const disasm_pane_ops *ops);

void disasm_pane_draw(
    struct nk_context *ctx,
    struct nk_rect bounds,
    disasm_pane_state *state,
    const memory_source *table,
    size_t count,
    bool running,
    uint16_t pc,
    bool has_cpu,
    const disasm_pane_ops *ops);

#ifdef __cplusplus
}
#endif
