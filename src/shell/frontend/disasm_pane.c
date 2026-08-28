#include "disasm_pane.h"

#include "debugger_layout.h"

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

static int disasm_pane_find_row(const disasm_pane_state *state, uint16_t address)
{
    uint8_t row;

    if (state == NULL) {
        return -1;
    }
    for (row = 0; row < state->rows && row < DISASM_PC_LOCK_MAX_ROWS; row++) {
        if (state->lines[row].base.address == address) {
            return (int)row;
        }
    }
    return -1;
}

static void disasm_pane_lock_pc(disasm_pane_state *state, const disasm_pane_ops *ops, uint16_t pc)
{
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
    state->pc_lock_address = pc;
    state->pc_lock_active = true;
    state->request_pending = false;
}

static void disasm_pane_draw_scrollbar(
    struct nk_context *ctx,
    disasm_pane_state *state,
    struct nk_rect bounds,
    bool running,
    const disasm_pane_ops *ops)
{
    struct nk_command_buffer *canvas;
    struct nk_rect track;
    struct nk_rect thumb;
    const struct nk_mouse *mouse;
    float visible_fraction;
    float thumb_h;
    float thumb_y;
    float usable_h;

    if (ctx == NULL || state == NULL || bounds.h <= 0.0f) {
        return;
    }

    canvas = nk_window_get_canvas(ctx);
    mouse = &ctx->input.mouse;
    track = nk_rect(bounds.x + 2.0f, bounds.y + 2.0f, bounds.w - 4.0f, bounds.h - 4.0f);

    visible_fraction = ((float)state->rows * 2.0f) / 65536.0f;
    thumb_h = track.h * visible_fraction;
    if (thumb_h < 16.0f) {
        thumb_h = 16.0f;
    }
    if (thumb_h > track.h) {
        thumb_h = track.h;
    }

    usable_h = track.h - thumb_h;
    thumb_y = track.y + usable_h * ((float)state->top_address / 65535.0f);
    thumb = nk_rect(track.x + 2.0f, thumb_y, track.w - 4.0f, thumb_h);

    if (nk_input_is_mouse_hovering_rect(&ctx->input, thumb) &&
        nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT) &&
        !state->scrollbar_dragging) {
        if (ops != NULL && ops->set_active_view != NULL) {
            ops->set_active_view(ops->ctx);
        }
        state->scrollbar_dragging = true;
        state->scrollbar_grab_offset = mouse->pos.y - thumb.y;
    }

    if (!nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT)) {
        state->scrollbar_dragging = false;
    }

    if (state->scrollbar_dragging) {
        float y = mouse->pos.y - state->scrollbar_grab_offset;
        float relative;

        if (y < track.y) {
            y = track.y;
        }
        if (y > track.y + usable_h) {
            y = track.y + usable_h;
        }

        relative = usable_h > 0.0f ? (y - track.y) / usable_h : 0.0f;
        if (!running) {
            state->top_address = (uint16_t)(relative * 65535.0f);
            state->follow_pc = false;
            state->pc_lock_active = false;
            state->request_pending = false;
        }
    } else if (nk_input_is_mouse_hovering_rect(&ctx->input, track) &&
        nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
        !nk_input_is_mouse_hovering_rect(&ctx->input, thumb)) {
        if (ops != NULL && ops->set_active_view != NULL) {
            ops->set_active_view(ops->ctx);
        }
        if (!running) {
            if (mouse->pos.y < thumb.y) {
                state->top_address = (uint16_t)(state->top_address - (uint16_t)(state->rows * 2u));
            } else {
                state->top_address = (uint16_t)(state->top_address + (uint16_t)(state->rows * 2u));
            }
            state->follow_pc = false;
            state->pc_lock_active = false;
            state->request_pending = false;
        }
    }

    nk_fill_rect(canvas, track, 0.0f, nk_rgb(35, 41, 47));
    nk_fill_rect(
        canvas,
        thumb,
        2.0f,
        nk_input_is_mouse_hovering_rect(&ctx->input, thumb) || state->scrollbar_dragging ?
            nk_rgb(160, 174, 186) :
            nk_rgb(103, 124, 139));
}

static char disasm_pane_line_char_at(const char *line, int index)
{
    size_t length;

    if (line == NULL || index < 0) {
        return ' ';
    }
    length = strlen(line);
    if ((size_t)index >= length) {
        return ' ';
    }
    return line[index];
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
    float scrollbar_w = 24.0f;
    float scrollbar_margin = 8.0f;
    const struct nk_user_font *font;
    bool any_dialog;
    bool dasm_active;
    bool pc_changed = false;

    if (ctx == NULL || state == NULL) {
        return;
    }
    if (!state->initialized) {
        const memory_source *src0 = (table != NULL && count > 0u) ? &table[0] : NULL;
        disasm_pane_init(state, src0 != NULL ? src0->id : 0u, state->cpu_class);
        state->follow_pc = true;
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
        pc_changed = !state->has_last_pc || state->last_pc != pc;
        if (running) {
            disasm_pane_lock_pc(state, ops, pc);
            state->follow_pc = true;
            state->has_user_cursor = false;
            state->chrome.address_entry = false;
        } else if (pc_changed) {
            disasm_pane_lock_pc(state, ops, pc);
        } else if (state->follow_pc) {
            int pc_row = disasm_pane_find_row(state, pc);
            if (pc_row < 0 || pc_row != (int)(state->rows / 2u)) {
                disasm_pane_lock_pc(state, ops, pc);
            }
        }
        state->last_pc = pc;
        state->has_last_pc = true;
    }

    any_dialog = ops != NULL && ops->any_dialog_open != NULL && ops->any_dialog_open(ops->ctx);
    if (!any_dialog &&
        nk_input_is_mouse_hovering_rect(&ctx->input, bounds) &&
        ctx->input.mouse.scroll_delta.y != 0.0f &&
        !running) {
        int32_t lines = ctx->input.mouse.scroll_delta.y > 0.0f ? -3 : 3;
        state->top_address = (uint16_t)(state->top_address + lines);
        state->request_pending = false;
        state->follow_pc = false;
        state->pc_lock_active = false;
    }

    if (state->pc_lock_active && has_cpu && (running || state->follow_pc || pc_changed)) {
        disasm_pane_lock_pc(state, ops, state->pc_lock_address != 0u ? state->pc_lock_address : pc);
    } else {
        disasm_pane_decode(state, ops != NULL ? ops->symbols : NULL);
    }
    if (ops != NULL && ops->request != NULL) {
        ops->request(ops->ctx, state->source_id, state->top_address, 256u);
    }

    if (nk_begin(ctx, "Disassembly", bounds, k_pane_flags)) {
        struct nk_style_window saved_window_style = ctx->style.window;
        float char_w = (ops != NULL && ops->char_width != NULL) ?
            ops->char_width(ops->ctx) : 8.0f;

        dasm_active = state->active;
        if (ops != NULL && ops->view_is_active != NULL) {
            dasm_active = ops->view_is_active(ops->ctx);
        }

        ctx->style.window.padding = nk_vec2(0.0f, 0.0f);
        ctx->style.window.spacing = nk_vec2(0.0f, 0.0f);
        ctx->style.window.group_padding = nk_vec2(0.0f, 0.0f);

        nk_layout_row_begin(ctx, NK_STATIC, row_h * (float)state->rows, 3);
        nk_layout_row_push(ctx, bounds.w - scrollbar_w - scrollbar_margin);
        if (nk_group_begin(ctx, "disassembly-rows", NK_WINDOW_NO_SCROLLBAR)) {
            for (row = 0; row < state->rows; row++) {
                const disasm_pc_lock_line *line = &state->lines[row];
                char bytes[16];
                char address_label[32] = "";
                char rendered[128];
                struct nk_rect row_bounds;
                int bp = (ops != NULL && ops->execute_bp != NULL) ?
                    ops->execute_bp(ops->ctx, line->base.address) : DISASM_PANE_BP_NONE;
                bool is_pc = has_cpu && line->base.address == pc;
                bool is_cursor = state->has_user_cursor &&
                    line->base.address == state->cursor_address && !is_pc;
                bool is_breakpoint = bp != DISASM_PANE_BP_NONE;
                bool is_enabled_breakpoint = bp == DISASM_PANE_BP_ENABLED;
                struct nk_style_selectable saved_selectable = ctx->style.selectable;
                nk_bool selected = is_cursor ? nk_true : nk_false;
                disasm_pane_target tgt = {false, false, 0, 0};
                char target[24] = "";

                if (line->is_provisional) {
                    snprintf(bytes, sizeof(bytes), "??      ");
                } else if (line->base.length == 2) {
                    snprintf(
                        bytes, sizeof(bytes), "%02X %02X   ",
                        line->base.bytes[0], line->base.bytes[1]);
                } else if (line->base.length >= 3) {
                    snprintf(
                        bytes, sizeof(bytes), "%02X %02X %02X",
                        line->base.bytes[0], line->base.bytes[1], line->base.bytes[2]);
                } else {
                    snprintf(bytes, sizeof(bytes), "%02X      ", line->base.bytes[0]);
                }

                if (ops != NULL && ops->symbols != NULL &&
                    ops->symbols->address_to_label != NULL) {
                    (void)ops->symbols->address_to_label(
                        ops->symbols->userdata,
                        line->base.address,
                        address_label,
                        sizeof(address_label));
                }
                address_label[15] = '\0';

                if (ops != NULL && ops->annotate_target != NULL) {
                    ops->annotate_target(ops->ctx, &line->base, &tgt);
                }
                if (tgt.show) {
                    if (tgt.has_value) {
                        snprintf(
                            target, sizeof(target), " [$%04X:%02X]",
                            tgt.address, tgt.value);
                    } else {
                        snprintf(target, sizeof(target), " [$%04X]", tgt.address);
                    }
                }
                snprintf(
                    rendered,
                    sizeof(rendered),
                    "%c%c %04X %-15s %-8s %-20s%s",
                    is_pc ? '>' : ' ',
                    is_breakpoint ? (is_enabled_breakpoint ? 'X' : 'x') : ' ',
                    line->base.address,
                    address_label,
                    bytes,
                    line->base.text[0] != '\0' ? line->base.text : "???",
                    target);

                if (line->is_provisional) {
                    ctx->style.selectable.normal = nk_style_item_color(nk_rgb(20, 24, 28));
                    ctx->style.selectable.hover = nk_style_item_color(nk_rgb(25, 29, 33));
                    ctx->style.selectable.normal_active = nk_style_item_color(nk_rgb(30, 60, 74));
                    ctx->style.selectable.text_normal = nk_rgb(72, 80, 88);
                } else if (line->base.forced_byte) {
                    ctx->style.selectable.normal = nk_style_item_color(nk_rgb(30, 34, 38));
                    ctx->style.selectable.hover = nk_style_item_color(nk_rgb(39, 45, 51));
                    ctx->style.selectable.normal_active = nk_style_item_color(nk_rgb(49, 78, 94));
                    ctx->style.selectable.text_normal = nk_rgb(125, 136, 145);
                }
                if (is_pc) {
                    ctx->style.selectable.normal = nk_style_item_color(nk_rgb(83, 73, 24));
                    ctx->style.selectable.text_normal = nk_rgb(255, 244, 120);
                }
                if (is_breakpoint && !is_pc) {
                    ctx->style.selectable.text_normal = is_enabled_breakpoint ?
                        nk_rgb(255, 151, 122) :
                        nk_rgb(169, 126, 202);
                }
                if (is_cursor) {
                    ctx->style.selectable.normal_active = nk_style_item_color(nk_rgb(21, 91, 116));
                    ctx->style.selectable.text_normal_active = nk_rgb(226, 246, 255);
                    ctx->style.selectable.text_hover_active = nk_rgb(226, 246, 255);
                    ctx->style.selectable.text_pressed_active = nk_rgb(226, 246, 255);
                }

                nk_layout_row_dynamic(ctx, row_h, 1);
                row_bounds = nk_widget_bounds(ctx);
                if (nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) &&
                    nk_input_is_mouse_hovering_rect(&ctx->input, row_bounds)) {
                    if (ops != NULL && ops->set_active_view != NULL) {
                        ops->set_active_view(ops->ctx);
                    }
                    if (!running) {
                        int text_col;
                        float rel_x = ctx->input.mouse.pos.x - row_bounds.x;
                        if (rel_x < 0.0f) {
                            rel_x = 0.0f;
                        }
                        text_col = (int)(rel_x / char_w);
                        state->cursor_address = line->base.address;
                        state->has_user_cursor = true;
                        state->cursor_row = row;
                        state->cursor_length = line->base.length == 0u ? 1u : line->base.length;
                        state->follow_pc = false;
                        state->pc_lock_active = false;
                        if (text_col >= 3 && text_col <= 6) {
                            state->chrome.address_entry = true;
                            state->chrome.active_address_digit = (uint8_t)(text_col - 3);
                        } else {
                            state->chrome.address_entry = false;
                        }
                    }
                }
                if (nk_selectable_label(ctx, rendered, NK_TEXT_LEFT, &selected)) {
                    if (!running) {
                        if (ops != NULL && ops->set_active_view != NULL) {
                            ops->set_active_view(ops->ctx);
                        }
                        state->cursor_address = line->base.address;
                        state->has_user_cursor = true;
                        state->cursor_row = row;
                        state->cursor_length = line->base.length == 0u ? 1u : line->base.length;
                        state->chrome.address_entry = false;
                        state->follow_pc = false;
                        state->pc_lock_active = false;
                    }
                }
                if (dasm_active && state->chrome.address_entry &&
                    line->base.address == state->cursor_address) {
                    struct nk_rect cursor_rect;
                    char text[2];
                    int text_col = 3 + (int)state->chrome.active_address_digit;
                    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

                    cursor_rect = nk_rect(
                        row_bounds.x + char_w * (float)text_col,
                        row_bounds.y + 1.0f,
                        char_w,
                        row_bounds.h - 2.0f);
                    text[0] = disasm_pane_line_char_at(rendered, text_col);
                    text[1] = '\0';
                    nk_fill_rect(canvas, cursor_rect, 0.0f, nk_rgb(255, 244, 120));
                    nk_draw_text(
                        canvas,
                        cursor_rect,
                        text,
                        1,
                        font,
                        nk_rgb(255, 244, 120),
                        nk_rgb(20, 24, 28));
                }
                ctx->style.selectable = saved_selectable;
            }
            nk_group_end(ctx);
        }
        nk_layout_row_push(ctx, scrollbar_w);
        if (nk_group_begin(ctx, "disassembly-scrollbar", NK_WINDOW_NO_SCROLLBAR)) {
            disasm_pane_draw_scrollbar(
                ctx, state, nk_window_get_content_region(ctx), running, ops);
            nk_group_end(ctx);
        }
        nk_layout_row_push(ctx, scrollbar_margin);
        nk_spacing(ctx, 1);
        nk_layout_row_end(ctx);

        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT, bounds) &&
            ops != NULL && ops->open_context_menu != NULL) {
            ops->open_context_menu(ops->ctx);
        }
        if (ops != NULL && ops->draw_context_menu != NULL) {
            ops->draw_context_menu(ops->ctx, ctx, state);
        }
        if (!any_dialog && dasm_active) {
            debugger_draw_active_view_border(ctx);
        }
        ctx->style.window = saved_window_style;
    }
    nk_end(ctx);
}
