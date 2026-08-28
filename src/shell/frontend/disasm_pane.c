#include "disasm_pane.h"

#include <stdio.h>
#include <string.h>

static const nk_flags k_pane_flags =
    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

void disasm_pane_init(
    disasm_pane_state *state, uint32_t source_id, disasm_6502_cpu_class cpu)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    debugger_disasm_view_init(&state->chrome);
    state->source_id = source_id;
    state->cpu_class = cpu;
    state->follow_pc = true;
    state->initialized = true;
    state->cache_count = 0;
}

disasm_pc_lock_cache *disasm_pane_cache(disasm_pane_state *state, uint32_t source_id)
{
    int i;

    if (state == NULL) {
        return NULL;
    }
    for (i = 0; i < state->cache_count; i++) {
        if (state->cache_ids[i] == source_id) {
            return &state->mem_cache[i];
        }
    }
    if (state->cache_count >= DISASM_PANE_CACHE_MAX) {
        return &state->mem_cache[0];
    }
    i = state->cache_count;
    state->cache_ids[i] = source_id;
    memset(&state->mem_cache[i], 0, sizeof(state->mem_cache[i]));
    state->cache_count++;
    return &state->mem_cache[i];
}

void disasm_pane_merge_bytes(
    disasm_pane_state *state,
    uint32_t source_id,
    uint16_t address,
    const uint8_t *bytes,
    uint16_t length)
{
    disasm_pc_lock_cache *cache = disasm_pane_cache(state, source_id);
    uint16_t i;

    if (cache == NULL || bytes == NULL) {
        return;
    }
    for (i = 0; i < length; i++) {
        uint16_t addr = (uint16_t)(address + i);
        cache->bytes[addr] = bytes[i];
        cache->valid[addr] = true;
    }
}

void disasm_pane_decode(disasm_pane_state *state, const symbol_resolver *symbols)
{
    const disasm_pc_lock_cache *cache;
    uint16_t address;
    uint8_t row;

    if (state == NULL) {
        return;
    }
    cache = disasm_pane_cache(state, state->source_id);
    address = state->top_address;
    for (row = 0; row < state->rows && row < DISASM_PC_LOCK_MAX_ROWS; row++) {
        uint8_t fetched[3];
        size_t available = cache != NULL ? disasm_pc_lock_fetch(cache, address, fetched) : 0u;

        state->lines[row].base = disasm_6502_decode_line(
            address,
            available > 0u ? fetched : NULL,
            available,
            symbols,
            state->cpu_class);
        state->lines[row].is_provisional = (available == 0u);
        address = (uint16_t)(address + state->lines[row].base.length);
    }
}

static void disasm_browse_up(void *ctx, debugger_disasm_view *view)
{
    disasm_pane_state *state = ctx;
    uint16_t cur;
    uint8_t row;

    (void)view;
    if (state == NULL || state->rows == 0u) {
        return;
    }
    cur = state->has_user_cursor ? state->cursor_address : state->top_address;
    for (row = 0; row < state->rows; row++) {
        if (state->lines[row].base.address == cur && row > 0u) {
            state->cursor_address = state->lines[row - 1u].base.address;
            state->has_user_cursor = true;
            state->follow_pc = false;
            state->pc_lock_active = false;
            if (row == 1u) {
                state->top_address = state->cursor_address;
                state->request_pending = false;
            }
            return;
        }
    }
    state->top_address = (uint16_t)(state->top_address - 1u);
    state->cursor_address = state->top_address;
    state->has_user_cursor = true;
    state->follow_pc = false;
    state->pc_lock_active = false;
    state->request_pending = false;
}

static void disasm_browse_down(void *ctx, debugger_disasm_view *view)
{
    disasm_pane_state *state = ctx;
    uint16_t cur;
    uint8_t row;

    (void)view;
    if (state == NULL || state->rows == 0u) {
        return;
    }
    cur = state->has_user_cursor ? state->cursor_address : state->top_address;
    for (row = 0; row < state->rows; row++) {
        if (state->lines[row].base.address == cur && (row + 1u) < state->rows) {
            state->cursor_address = state->lines[row + 1u].base.address;
            state->has_user_cursor = true;
            state->follow_pc = false;
            state->pc_lock_active = false;
            if (row + 2u >= state->rows) {
                state->top_address = state->lines[1].base.address;
                state->request_pending = false;
            }
            return;
        }
    }
    state->top_address = (uint16_t)(state->top_address + state->lines[0].base.length);
    state->request_pending = false;
}

static void disasm_browse_page_up(void *ctx, debugger_disasm_view *view)
{
    disasm_pane_state *state = ctx;
    uint8_t rows;

    (void)view;
    if (state == NULL) {
        return;
    }
    rows = state->rows > 1u ? (uint8_t)(state->rows - 1u) : 1u;
    state->top_address = (uint16_t)(state->top_address - rows);
    state->cursor_address = state->top_address;
    state->has_user_cursor = true;
    state->follow_pc = false;
    state->pc_lock_active = false;
    state->request_pending = false;
}

static void disasm_browse_page_down(void *ctx, debugger_disasm_view *view)
{
    disasm_pane_state *state = ctx;
    uint8_t last;

    (void)view;
    if (state == NULL || state->rows == 0u) {
        return;
    }
    last = (uint8_t)(state->rows - 1u);
    state->top_address = state->lines[last].base.address;
    state->cursor_address = state->top_address;
    state->has_user_cursor = true;
    state->follow_pc = false;
    state->pc_lock_active = false;
    state->request_pending = false;
}

static void disasm_on_cycle(void *ctx)
{
    /* filled from handle_key via table — placeholder unused */
    (void)ctx;
}

void disasm_pane_handle_key(
    disasm_pane_state *state,
    const SDL_KeyboardEvent *key,
    const memory_source *table,
    size_t count,
    const disasm_pane_ops *ops)
{
    debugger_disasm_ops chrome_ops;
    uint32_t next;

    if (state == NULL || ops == NULL) {
        return;
    }
    memset(&chrome_ops, 0, sizeof(chrome_ops));
    chrome_ops.ctx = state;
    chrome_ops.mode = DEBUGGER_DISASM_MODE_LIVE;
    chrome_ops.keys_enabled = ops->keys_enabled;
    chrome_ops.get_focus_pc = ops->focus_pc;
    chrome_ops.focus_valid = ops->focus_valid;
    chrome_ops.browse_up = disasm_browse_up;
    chrome_ops.browse_down = disasm_browse_down;
    chrome_ops.browse_page_up = disasm_browse_page_up;
    chrome_ops.browse_page_down = disasm_browse_page_down;
    chrome_ops.on_toggle_execute_bp = ops->toggle_execute_bp;
    chrome_ops.on_set_pc = ops->set_pc;
    chrome_ops.on_symbol_lookup = ops->open_symbol_lookup;
    chrome_ops.on_cycle_memory_mode = disasm_on_cycle;

    state->chrome.top_address = state->top_address;
    state->chrome.cursor_address = state->cursor_address;
    state->chrome.rows = state->rows;
    state->chrome.follow_focus = state->follow_pc;
    state->chrome.has_cursor = state->has_user_cursor;

    if (debugger_disasm_handle_key(&state->chrome, &chrome_ops, key)) {
        if (key != NULL && key->type == SDL_KEYDOWN &&
            (key->keysym.mod & KMOD_ALT) != 0 && key->keysym.sym == SDLK_m) {
            next = memory_source_cycle_next(table, count, state->source_id);
            state->source_id = next;
            state->request_pending = false;
        }
        state->top_address = state->chrome.top_address;
        state->cursor_address = state->chrome.cursor_address;
        state->follow_pc = state->chrome.follow_focus;
        state->has_user_cursor = state->chrome.has_cursor;
        if (!state->follow_pc) {
            state->pc_lock_active = false;
        }
    }
}

void disasm_pane_draw(
    struct nk_context *ctx,
    struct nk_rect bounds,
    disasm_pane_state *state,
    const memory_source *table,
    size_t count,
    bool running,
    uint16_t pc,
    bool has_cpu,
    const disasm_pane_ops *ops)
{
    uint8_t row;
    float row_h;
    const struct nk_user_font *font;
    const memory_source *src;
    char title[48];

    if (ctx == NULL || state == NULL) {
        return;
    }
    if (!state->initialized) {
        const memory_source *src0 = (table != NULL && count > 0u) ? &table[0] : NULL;
        disasm_pane_init(state, src0 != NULL ? src0->id : 0u, state->cpu_class);
        if (has_cpu) {
            state->top_address = pc;
        }
    }
    font = ctx->style.font;
    row_h = font != NULL ? font->height : 13.0f;
    {
        float content_h = bounds.h - 28.0f;
        uint8_t rows = (uint8_t)((content_h > row_h) ? (content_h / row_h) : 1);
        if (rows > 1u) {
            rows--;
        }
        if (rows == 0u) {
            rows = 1u;
        }
        if (rows > DISASM_PC_LOCK_MAX_ROWS) {
            rows = DISASM_PC_LOCK_MAX_ROWS;
        }
        state->rows = rows;
        state->chrome.rows = rows;
    }
    if (has_cpu) {
        bool pc_changed = !state->has_last_pc || state->last_pc != pc;
        if (running || state->follow_pc || pc_changed) {
            if (state->pc_lock_active || state->follow_pc || running) {
                uint16_t top = state->top_address;
                disasm_pc_lock_build(
                    disasm_pane_cache(state, state->source_id),
                    ops != NULL ? ops->symbols : NULL,
                    state->cpu_class,
                    pc,
                    state->rows,
                    state->lines,
                    &top);
                state->top_address = top;
                state->pc_lock_active = true;
            }
        }
        state->last_pc = pc;
        state->has_last_pc = true;
    }
    disasm_pane_decode(state, ops != NULL ? ops->symbols : NULL);
    if (ops != NULL && ops->request != NULL) {
        ops->request(ops->ctx, state->source_id, state->top_address, 256u);
    }

    src = memory_source_find_by_id(table, count, state->source_id);
    if (src != NULL && src->label != NULL) {
        snprintf(title, sizeof(title), "Disassembly [%s]", src->label);
    } else {
        snprintf(title, sizeof(title), "Disassembly");
    }

    if (nk_begin(ctx, title, bounds, k_pane_flags)) {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        for (row = 0; row < state->rows; row++) {
            char line[128];
            const disasm_6502_line *dec = &state->lines[row].base;
            char marker;
            bool is_focus = has_cpu && dec->address == pc;
            bool is_browse = state->has_user_cursor && dec->address == state->cursor_address;
            struct nk_rect rb;

            marker = debugger_disasm_row_marker(is_focus, is_browse && !is_focus);
            snprintf(
                line,
                sizeof(line),
                "%c %04X  %s",
                marker,
                dec->address,
                dec->text[0] != '\0' ? dec->text : "???");
            nk_layout_row_dynamic(ctx, row_h, 1);
            if (nk_widget(&rb, ctx) != NK_WIDGET_INVALID) {
                struct nk_color bg = is_focus ? nk_rgb(24, 62, 118) : nk_rgb(17, 22, 28);
                struct nk_color fg = is_browse && !is_focus ?
                    nk_rgb(255, 210, 80) : nk_rgb(232, 235, 238);
                nk_fill_rect(canvas, rb, 0.0f, bg);
                nk_draw_text(
                    canvas, rb, line, (int)strlen(line), font, bg, fg);
            }
        }
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, debugger_disasm_footer_hint(DEBUGGER_DISASM_MODE_LIVE), NK_TEXT_LEFT);
        if (ops != NULL && ops->draw_context_menu != NULL) {
            ops->draw_context_menu(ops->ctx, ctx);
        }
    }
    nk_end(ctx);
}
