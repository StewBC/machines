#include "memview_pane.h"

#include "debugger_layout.h"

#include <stdio.h>
#include <string.h>

enum {
    MEMVIEW_PANE_SNAPSHOT_MAX = 1024
};

static const nk_flags k_pane_flags =
    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_NO_SCROLLBAR;

static const struct {
    struct nk_color text;
    struct nk_color bg;
} k_view_colors[MEMVIEW_PANE_VIEW_MAX] = {
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0x00, 0x00, 0x00, 0xFF } },
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0x2F, 0x4F, 0x4F, 0xFF } },
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0x00, 0x1F, 0x3F, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0xA9, 0xCC, 0xE3, 0xFF } },
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0x2E, 0x8B, 0x57, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0xFF, 0xFF, 0xE0, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0xFF, 0xD1, 0xDC, 0xFF } },
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0xFF, 0x8C, 0x00, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0xE0, 0xFF, 0xFF, 0xFF } },
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0x5D, 0x3F, 0xD3, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0x90, 0xEE, 0x90, 0xFF } },
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0x80, 0x00, 0x00, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0xAD, 0xD8, 0xE6, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0xF0, 0x80, 0x80, 0xFF } },
    { { 0xFF, 0xFF, 0xFF, 0xFF }, { 0x8B, 0x00, 0x00, 0xFF } },
    { { 0x00, 0x00, 0x00, 0xFF }, { 0xFF, 0xFF, 0xFF, 0xFF } }
};

char memview_pane_ascii(uint8_t value, bool highbit_ascii)
{
    uint8_t shown = highbit_ascii ? (uint8_t)(value & 0x7fu) : value;

    if (shown >= 32u && shown <= 126u) {
        return (char)shown;
    }
    return '.';
}

void memview_pane_apply_source(
    memview_pane_view *view,
    const memory_source *table,
    size_t count,
    uint32_t source_id)
{
    const memory_source *src;

    if (view == NULL) {
        return;
    }
    view->source_id = source_id;
    view->request_pending = false;
    src = memory_source_find_by_id(table, count, source_id);
    if (!view->highbit_user_set) {
        view->highbit_ascii = src != NULL && (src->flags & MEMSRC_HIGHBIT_ASCII) != 0u;
    }
}

static bool memview_source_writable(const memory_source *src)
{
    return src != NULL && (src->flags & MEMSRC_WRITABLE) != 0u;
}

static struct nk_color memview_source_border_color(const memory_source *src, size_t index)
{
    static const struct nk_color k_plane[] = {
        { 60, 120, 200, 255 },
        { 80, 160, 120, 255 },
        { 160, 100, 180, 255 },
        { 180, 80, 140, 255 },
        { 200, 130, 40, 255 },
        { 188, 198, 190, 255 }
    };

    if (src != NULL && (src->flags & MEMSRC_FOREIGN_BUS) != 0u) {
        return nk_rgb(120, 126, 132);
    }
    if (src != NULL && (src->flags & MEMSRC_WRITABLE) == 0u) {
        return nk_rgb(200, 130, 40);
    }
    return k_plane[index % (sizeof(k_plane) / sizeof(k_plane[0]))];
}

static uint16_t memview_visible_count(const memview_pane_view *view)
{
    if (view == NULL || view->columns == 0u) {
        return 0u;
    }
    return (uint16_t)(view->rows * view->columns);
}

static bool memview_cursor_visible(const memview_pane_view *view)
{
    uint16_t offset;

    if (view == NULL) {
        return false;
    }
    offset = (uint16_t)(view->cursor_address - view->view_address);
    return offset < memview_visible_count(view);
}

static void memview_recenter_cursor(memview_pane_view *view)
{
    uint16_t visible = memview_visible_count(view);
    uint16_t offset;
    uint16_t col_in_view;

    if (visible == 0u || view->columns == 0u || memview_cursor_visible(view)) {
        return;
    }
    offset = (uint16_t)(view->cursor_address - view->view_address);
    col_in_view = (uint16_t)(offset % view->columns);
    if (offset >= 0x8000u) {
        view->view_address = (uint16_t)(view->cursor_address - col_in_view);
    } else {
        view->view_address = (uint16_t)(
            view->cursor_address - col_in_view -
            (uint16_t)((view->rows - 1u) * view->columns));
    }
}

static int memview_total_rows(const memview_pane_state *state)
{
    int i;
    int total = 0;

    for (i = 0; i < state->view_count; i++) {
        total += state->views[i].rows;
    }
    return total;
}

static memview_pane_view *memview_active(memview_pane_state *state)
{
    if (state == NULL || state->view_count <= 0) {
        return NULL;
    }
    if (state->active_index < 0 || state->active_index >= state->view_count) {
        state->active_index = 0;
    }
    return &state->views[state->active_index];
}

void memview_pane_init(memview_pane_state *state, uint32_t source_id, bool highbit_ascii)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->views[0].view_address = 0u;
    state->views[0].cursor_address = 0u;
    state->views[0].source_id = source_id;
    state->views[0].edit_field = MEMVIEW_PANE_EDIT_HEX;
    state->views[0].columns = 16u;
    state->views[0].color_slot = 0u;
    state->views[0].highbit_ascii = highbit_ascii;
    state->views[0].initialized = true;
    state->view_count = 1;
    state->active_index = 0;
    state->color_slot_used[0] = true;
}

static bool memview_ops_available(
    const memview_pane_ops *ops, uint32_t source_id, uint16_t address)
{
    if (ops == NULL || ops->byte_available == NULL) {
        return false;
    }
    return ops->byte_available(ops->ctx, source_id, address);
}

static uint8_t memview_ops_read(
    const memview_pane_ops *ops, uint32_t source_id, uint16_t address)
{
    uint8_t value = 0u;

    if (ops != NULL && ops->read_byte != NULL) {
        (void)ops->read_byte(ops->ctx, source_id, address, &value);
    }
    return value;
}

bool memview_pane_search_run(memview_pane_state *state, bool reverse, const memview_pane_ops *ops)
{
    memview_pane_view *view = memview_active(state);
    const uint8_t *bytes;
    const uint8_t *valid = NULL;
    uint16_t found;

    if (state == NULL || view == NULL || !state->search.has_pattern) {
        return false;
    }
    if (ops == NULL || ops->search_plane == NULL) {
        snprintf(state->search.status, sizeof(state->search.status), "Memory snapshot unavailable");
        return false;
    }
    bytes = ops->search_plane(ops->ctx, view->source_id, &valid);
    if (bytes == NULL) {
        snprintf(state->search.status, sizeof(state->search.status), "Memory snapshot unavailable");
        return false;
    }
    if (!memory_search_find(
            bytes,
            valid,
            &state->search.pattern,
            state->search.last_found_address,
            reverse,
            &found)) {
        snprintf(state->search.status, sizeof(state->search.status), "Not found");
        return false;
    }
    state->search.last_found_address = found;
    view->cursor_address = found;
    memview_recenter_cursor(view);
    view->request_pending = false;
    snprintf(state->search.status, sizeof(state->search.status), "Found at %04X", found);
    return true;
}

static uint8_t memview_alloc_color(memview_pane_state *state)
{
    int i;

    for (i = 0; i < MEMVIEW_PANE_VIEW_MAX; i++) {
        if (!state->color_slot_used[i]) {
            state->color_slot_used[i] = true;
            return (uint8_t)i;
        }
    }
    return 0u;
}

static void memview_split(memview_pane_state *state, bool aligned)
{
    memview_pane_view *av;
    memview_pane_view *nv;
    int insert;
    uint8_t old_rows;
    uint8_t top_rows;

    if (state == NULL || state->view_count <= 0 || state->view_count >= MEMVIEW_PANE_VIEW_MAX) {
        return;
    }
    av = memview_active(state);
    if (av == NULL || av->rows < 2u) {
        return;
    }
    insert = state->active_index + 1;
    memmove(
        &state->views[insert + 1],
        &state->views[insert],
        (size_t)(state->view_count - insert) * sizeof(state->views[0]));
    state->view_count++;
    nv = &state->views[insert];
    *nv = *av;
    nv->color_slot = memview_alloc_color(state);
    old_rows = av->rows;
    top_rows = (uint8_t)(old_rows / 2u);
    if (top_rows < 1u) {
        top_rows = 1u;
    }
    av->rows = top_rows;
    nv->rows = (uint8_t)(old_rows - top_rows);
    if (aligned) {
        nv->view_address = av->view_address;
        nv->cursor_address = av->cursor_address;
    } else {
        nv->view_address = (uint16_t)(av->view_address + av->rows * av->columns);
        nv->cursor_address = nv->view_address;
    }
    nv->initialized = true;
    nv->request_pending = false;
}

void memview_pane_split_at(memview_pane_state *state, int view_index, bool aligned)
{
    if (state == NULL || view_index < 0 || view_index >= state->view_count) {
        return;
    }
    state->active_index = view_index;
    memview_split(state, aligned);
}

void memview_pane_join_at(memview_pane_state *state, int view_index)
{
    int i;
    int give_rows;
    int rem_count;
    int best;
    int total_remain;
    int given;
    int ri;
    int remaining;
    int shares[MEMVIEW_PANE_VIEW_MAX];

    if (state == NULL || state->view_count <= 1 ||
        view_index < 0 || view_index >= state->view_count) {
        return;
    }
    give_rows = state->views[view_index].rows;
    state->color_slot_used[state->views[view_index].color_slot] = false;
    rem_count = state->view_count - 1;
    total_remain = 0;
    for (i = 0; i < state->view_count; i++) {
        if (i != view_index) {
            total_remain += state->views[i].rows;
        }
    }
    given = 0;
    ri = 0;
    for (i = 0; i < state->view_count; i++) {
        if (i == view_index) {
            continue;
        }
        shares[ri] = (total_remain > 0) ?
            (int)((long)state->views[i].rows * give_rows / total_remain) :
            (give_rows / rem_count);
        given += shares[ri++];
    }
    remaining = give_rows - given;
    while (remaining > 0) {
        best = -1;
        for (i = 0; i < rem_count; i++) {
            if (best == -1 || shares[i] < shares[best]) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        shares[best]++;
        remaining--;
    }
    ri = 0;
    for (i = 0; i < state->view_count; i++) {
        if (i == view_index) {
            continue;
        }
        {
            int nr = state->views[i].rows + shares[ri++];
            state->views[i].rows = (uint8_t)(nr > 255 ? 255 : nr);
        }
    }
    for (i = view_index; i < state->view_count - 1; i++) {
        state->views[i] = state->views[i + 1];
    }
    state->view_count--;
    if (view_index < state->view_count) {
        state->active_index = view_index;
    } else {
        state->active_index = state->view_count - 1;
    }
}

static void memview_redistribute_rows(memview_pane_state *state, int new_total)
{
    int i;
    int old_total;
    int delta;
    int best;
    int shares[MEMVIEW_PANE_VIEW_MAX];
    int assigned;
    bool was_visible[MEMVIEW_PANE_VIEW_MAX];

    if (state->view_count <= 0 || new_total < 0) {
        return;
    }
    old_total = memview_total_rows(state);
    if (old_total == new_total) {
        return;
    }
    for (i = 0; i < state->view_count; i++) {
        was_visible[i] = memview_cursor_visible(&state->views[i]);
    }
    assigned = 0;
    for (i = 0; i < state->view_count; i++) {
        shares[i] = (old_total > 0) ?
            (int)((long)state->views[i].rows * new_total / old_total) :
            (new_total / state->view_count);
        if (shares[i] < 1) {
            shares[i] = 1;
        }
        assigned += shares[i];
    }
    delta = new_total - assigned;
    while (delta > 0) {
        best = -1;
        for (i = 0; i < state->view_count; i++) {
            if (best == -1 || shares[i] < shares[best]) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        shares[best]++;
        delta--;
    }
    while (delta < 0) {
        best = -1;
        for (i = 0; i < state->view_count; i++) {
            if (shares[i] > 1 && (best == -1 || shares[i] > shares[best])) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        shares[best]--;
        delta++;
    }
    for (i = 0; i < state->view_count; i++) {
        state->views[i].rows = (uint8_t)(shares[i] > 255 ? 255 : shares[i]);
        if (was_visible[i]) {
            memview_recenter_cursor(&state->views[i]);
        }
    }
}

static int memview_cursor_text_col(const memview_pane_view *view)
{
    uint16_t offset;
    uint8_t col;

    if (view == NULL || view->columns == 0u) {
        return 0;
    }
    if (view->edit_field == MEMVIEW_PANE_EDIT_ADDRESS) {
        return (int)view->active_address_digit;
    }
    offset = (uint16_t)(view->cursor_address - view->view_address);
    col = (uint8_t)(offset % view->columns);
    if (view->edit_field == MEMVIEW_PANE_EDIT_ASCII) {
        return 5 + (int)view->columns * 3 + (int)col;
    }
    return 5 + (int)col * 3 + (int)view->active_nibble;
}

static char memview_line_char_at(const char *line, int index)
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

static void memview_draw_cursor(
    struct nk_context *ctx,
    const memview_pane_view *view,
    struct nk_rect row_bounds,
    const char *line,
    uint16_t row_addr,
    float char_w,
    bool mem_active,
    bool paused)
{
    struct nk_command_buffer *canvas;
    uint16_t row_offset;
    int text_col;
    struct nk_rect cursor_rect;
    char text[2];

    if (ctx == NULL || view == NULL || line == NULL || !mem_active || !paused) {
        return;
    }
    row_offset = (uint16_t)(view->cursor_address - row_addr);
    if (view->edit_field == MEMVIEW_PANE_EDIT_ADDRESS &&
        (row_offset >= view->columns || view->cursor_address != row_addr)) {
        return;
    }
    if (view->edit_field != MEMVIEW_PANE_EDIT_ADDRESS && row_offset >= view->columns) {
        return;
    }

    text_col = memview_cursor_text_col(view);
    cursor_rect = nk_rect(
        row_bounds.x + char_w * (float)text_col,
        row_bounds.y + 1.0f,
        char_w,
        row_bounds.h - 2.0f);
    text[0] = memview_line_char_at(line, text_col);
    text[1] = '\0';

    canvas = nk_window_get_canvas(ctx);
    nk_fill_rect(canvas, cursor_rect, 0.0f, nk_rgb(255, 244, 120));
    nk_draw_text(
        canvas,
        cursor_rect,
        text,
        1,
        ctx->style.font,
        nk_rgb(255, 244, 120),
        nk_rgb(20, 24, 28));
}

static bool memview_row_address_at(
    const memview_pane_view *view,
    struct nk_rect row_bounds,
    uint16_t row_addr,
    float mouse_x,
    float char_w,
    uint16_t *out_address,
    memview_pane_edit_field *out_field,
    uint8_t *out_nibble,
    uint8_t *out_address_digit)
{
    float rel_x;
    int text_col;
    int hex_start = 5;
    int ascii_start;
    int hex_end;

    if (view == NULL || out_address == NULL || char_w <= 0.0f) {
        return false;
    }
    ascii_start = hex_start + (int)view->columns * 3;
    hex_end = ascii_start;

    rel_x = mouse_x - row_bounds.x;
    if (rel_x < 0.0f) {
        rel_x = 0.0f;
    }
    text_col = (int)(rel_x / char_w);

    if (text_col < 4) {
        *out_address = row_addr;
        if (out_field != NULL) {
            *out_field = MEMVIEW_PANE_EDIT_ADDRESS;
        }
        if (out_address_digit != NULL) {
            *out_address_digit = (uint8_t)text_col;
        }
        return true;
    }

    if (text_col >= hex_start && text_col < hex_end) {
        int cell = (text_col - hex_start) / 3;
        int cell_col = (text_col - hex_start) % 3;

        if (cell >= 0 && cell < (int)view->columns) {
            *out_address = (uint16_t)(row_addr + cell);
            if (out_field != NULL) {
                *out_field = MEMVIEW_PANE_EDIT_HEX;
            }
            if (out_nibble != NULL) {
                *out_nibble = (uint8_t)(cell_col == 1 ? 1 : 0);
            }
            return true;
        }
        return false;
    }

    if (text_col >= ascii_start && text_col < ascii_start + (int)view->columns) {
        int cell = text_col - ascii_start;

        *out_address = (uint16_t)(row_addr + cell);
        if (out_field != NULL) {
            *out_field = MEMVIEW_PANE_EDIT_ASCII;
        }
        return true;
    }

    return false;
}

static void memview_handle_mouse_row(
    struct nk_context *ctx,
    memview_pane_state *state,
    int view_index,
    struct nk_rect row_bounds,
    uint16_t row_addr,
    float char_w,
    const memview_pane_ops *ops)
{
    memview_pane_view *view;
    uint16_t address;
    memview_pane_edit_field field;
    uint8_t nibble = 0u;
    uint8_t address_digit = 0u;
    bool left_click;
    bool right_click;

    if (ctx == NULL || state == NULL || view_index < 0 || view_index >= state->view_count) {
        return;
    }
    if (!nk_input_is_mouse_hovering_rect(&ctx->input, row_bounds)) {
        return;
    }

    left_click = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_LEFT) != 0;
    right_click = nk_input_is_mouse_pressed(&ctx->input, NK_BUTTON_RIGHT) != 0;
    if (!left_click && !right_click) {
        return;
    }

    view = &state->views[view_index];
    if (!memview_row_address_at(
            view,
            row_bounds,
            row_addr,
            left_click ? ctx->input.mouse.pos.x :
                ctx->input.mouse.buttons[NK_BUTTON_RIGHT].clicked_pos.x,
            char_w,
            &address,
            &field,
            &nibble,
            &address_digit)) {
        return;
    }

    if (ops != NULL && ops->set_active_view != NULL) {
        ops->set_active_view(ops->ctx);
    }
    state->active_index = view_index;
    view->edit_field = field;
    view->cursor_address = address;
    if (field == MEMVIEW_PANE_EDIT_HEX) {
        view->active_nibble = nibble;
    } else if (field == MEMVIEW_PANE_EDIT_ADDRESS) {
        view->active_address_digit = address_digit;
    }
}

static void memview_draw_footer(
    struct nk_context *ctx,
    memview_pane_state *state,
    struct nk_rect content,
    float footer_h,
    bool editable,
    const memory_source *src)
{
    struct nk_command_buffer *canvas;
    struct nk_rect footer;
    struct nk_rect field_rect;
    struct nk_rect address_rect;
    struct nk_rect edit_rect;
    memview_pane_view *view = memview_active(state);
    const char *field;
    const char *status_text;
    char address[32];
    char edit_status[48];

    if (ctx == NULL || view == NULL || content.w <= 8.0f || content.h <= footer_h) {
        return;
    }
    field = view->edit_field == MEMVIEW_PANE_EDIT_ASCII ? "ASCII" :
        (view->edit_field == MEMVIEW_PANE_EDIT_ADDRESS ? "Address" : "Hex");
    snprintf(
        edit_status,
        sizeof(edit_status),
        "%s / %s",
        (editable && memview_source_writable(src)) ? "editable" : "read-only",
        view->highbit_ascii ? "hi-bit on" : "hi-bit off");
    status_text = state->search.status[0] != '\0' ? state->search.status : edit_status;
    snprintf(address, sizeof(address), "Address: %04X", view->cursor_address);
    canvas = nk_window_get_canvas(ctx);
    footer = nk_rect(content.x + 4.0f, content.y + content.h - footer_h, content.w - 8.0f, footer_h);
    field_rect = nk_rect(footer.x, footer.y + 2.0f, footer.w * 0.25f, footer.h - 4.0f);
    address_rect = nk_rect(footer.x + footer.w * 0.25f, footer.y + 2.0f, footer.w * 0.40f, footer.h - 4.0f);
    edit_rect = nk_rect(footer.x + footer.w * 0.65f, footer.y + 2.0f, footer.w * 0.35f, footer.h - 4.0f);
    if (state->search.status[0] == '\0' &&
        nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_LEFT, edit_rect)) {
        view->highbit_ascii = !view->highbit_ascii;
        view->highbit_user_set = true;
    }
    nk_draw_text(canvas, field_rect, field, (int)strlen(field), ctx->style.font,
        nk_rgb(30, 34, 38), nk_rgb(196, 214, 228));
    nk_draw_text(canvas, address_rect, address, (int)strlen(address), ctx->style.font,
        nk_rgb(30, 34, 38), nk_rgb(196, 214, 228));
    nk_draw_text(canvas, edit_rect, status_text, (int)strlen(status_text), ctx->style.font,
        nk_rgb(30, 34, 38), nk_rgb(196, 214, 228));
}

void memview_pane_draw(
    struct nk_context *ctx,
    struct nk_rect bounds,
    memview_pane_state *state,
    const memory_source *table,
    size_t count,
    bool running,
    bool paused,
    bool inspecting,
    const memview_pane_ops *ops)
{
    float row_h;
    float footer_h = 22.0f;
    float scrollbar_w = 24.0f;
    float scrollbar_margin = 8.0f;
    int total_rows;
    int max_rows_per_view;
    struct nk_style_window saved_window_style;
    memview_pane_view *active;
    const struct nk_user_font *font;
    int v;
    bool editable;
    const memory_source *active_src;

    if (ctx == NULL || state == NULL) {
        return;
    }
    if (state->view_count == 0) {
        const memory_source *src0 = (table != NULL && count > 0u) ? &table[0] : NULL;
        memview_pane_init(
            state,
            src0 != NULL ? src0->id : 0u,
            src0 != NULL && (src0->flags & MEMSRC_HIGHBIT_ASCII) != 0u);
    }
    active = memview_active(state);
    font = ctx->style.font;
    row_h = (font != NULL) ? font->height : 13.0f;
    max_rows_per_view = MEMVIEW_PANE_SNAPSHOT_MAX / 16;
    total_rows = (bounds.h > footer_h + 28.0f) ? (int)((bounds.h - footer_h - 28.0f) / row_h) : 1;
    if (total_rows > 1) {
        total_rows--;
    }
    if (total_rows < 1) {
        total_rows = 1;
    }
    for (v = 0; v < state->view_count; v++) {
        if (state->views[v].rows > (uint8_t)max_rows_per_view) {
            state->views[v].rows = (uint8_t)max_rows_per_view;
        }
        if (state->views[v].rows < 1u) {
            state->views[v].rows = 1u;
        }
    }
    if (memview_total_rows(state) != total_rows) {
        memview_redistribute_rows(state, total_rows);
    }
    if (ops != NULL && ops->request != NULL) {
        ops->request(ops->ctx);
    }
    editable = paused && !inspecting && !running;
    active_src = active != NULL ?
        memory_source_find_by_id(table, count, active->source_id) : NULL;

    if (nk_begin(ctx, "Memory", bounds, k_pane_flags)) {
        struct nk_command_buffer *canvas;
        bool any_dialog = ops != NULL && ops->any_dialog_open != NULL &&
            ops->any_dialog_open(ops->ctx);
        bool mem_active = !any_dialog && ops != NULL && ops->view_is_active != NULL &&
            ops->view_is_active(ops->ctx);
        float rows_x = 0.0f;
        float rows_w = 0.0f;
        bool have_rows_x = false;
        float char_w = (ops != NULL && ops->char_width != NULL) ? ops->char_width(ops->ctx) : 8.0f;

        saved_window_style = ctx->style.window;
        ctx->style.window.padding = nk_vec2(0.0f, 0.0f);
        ctx->style.window.spacing = nk_vec2(0.0f, 0.0f);
        ctx->style.window.group_padding = nk_vec2(0.0f, 0.0f);

        nk_layout_row_begin(ctx, NK_STATIC, row_h * (float)total_rows, 3);
        nk_layout_row_push(ctx, bounds.w - scrollbar_w - scrollbar_margin);
        if (nk_group_begin(ctx, "memory-rows", NK_WINDOW_NO_SCROLLBAR)) {
            float text_x_offset = char_w * 0.5f;
            canvas = nk_window_get_canvas(ctx);
            for (v = 0; v < state->view_count; v++) {
                memview_pane_view *mv = &state->views[v];
                struct nk_color text_c = k_view_colors[mv->color_slot].text;
                struct nk_color bg_c = k_view_colors[mv->color_slot].bg;
                uint8_t row;

                if (mv->rows == 0u) {
                    continue;
                }
                for (row = 0; row < mv->rows; row++) {
                    char line[96];
                    char *lp = line;
                    size_t remaining = sizeof(line);
                    uint8_t col;
                    uint16_t row_addr = (uint16_t)(mv->view_address + (uint16_t)row * mv->columns);
                    int written = snprintf(lp, remaining, "%04X:", row_addr);
                    struct nk_rect rb;

                    lp += written;
                    remaining -= (size_t)written;
                    for (col = 0; col < mv->columns; col++) {
                        uint16_t addr = (uint16_t)(row_addr + col);
                        if (memview_ops_available(ops, mv->source_id, addr)) {
                            written = snprintf(
                                lp, remaining, "%02X ",
                                memview_ops_read(ops, mv->source_id, addr));
                        } else {
                            written = snprintf(lp, remaining, "-- ");
                        }
                        lp += written;
                        remaining -= (size_t)written;
                    }
                    for (col = 0; col < mv->columns; col++) {
                        uint16_t addr = (uint16_t)(row_addr + col);
                        if (memview_ops_available(ops, mv->source_id, addr)) {
                            written = snprintf(
                                lp, remaining, "%c",
                                memview_pane_ascii(
                                    memview_ops_read(ops, mv->source_id, addr),
                                    mv->highbit_ascii));
                        } else {
                            written = snprintf(lp, remaining, " ");
                        }
                        lp += written;
                        remaining -= (size_t)written;
                    }
                    nk_layout_row_dynamic(ctx, row_h, 1);
                    if (nk_widget(&rb, ctx) != NK_WIDGET_INVALID) {
                        struct nk_rect text_rb = nk_rect(
                            rb.x + text_x_offset, rb.y, rb.w - text_x_offset, rb.h);
                        if (!have_rows_x) {
                            rows_x = rb.x;
                            rows_w = rb.w;
                            have_rows_x = true;
                        }
                        if (row == 0u) {
                            mv->cached_y_top = rb.y;
                        }
                        mv->cached_y_bottom = rb.y + rb.h;
                        nk_fill_rect(canvas, rb, 0.0f, bg_c);
                        nk_draw_text(canvas, text_rb, line, (int)(lp - line), font, bg_c, text_c);
                        memview_handle_mouse_row(
                            ctx, state, v, text_rb, row_addr, char_w, ops);
                        if (v == state->active_index) {
                            memview_draw_cursor(
                                ctx, mv, text_rb, line, row_addr, char_w, mem_active, paused);
                        }
                    }
                }
                if (!any_dialog && ctx->input.mouse.scroll_delta.y != 0.0f) {
                    float mx = ctx->input.mouse.pos.x;
                    float my = ctx->input.mouse.pos.y;
                    if (mx >= rows_x && mx < rows_x + rows_w &&
                        my >= mv->cached_y_top && my < mv->cached_y_bottom) {
                        int32_t lines = ctx->input.mouse.scroll_delta.y > 0.0f ? -3 : 3;
                        mv->view_address = (uint16_t)(mv->view_address + lines * mv->columns);
                        mv->request_pending = false;
                    }
                }
            }
            nk_group_end(ctx);
        }
        nk_layout_row_push(ctx, scrollbar_w);
        if (nk_group_begin(ctx, "memory-scrollbar", NK_WINDOW_NO_SCROLLBAR)) {
            state->scrollbar_bounds = nk_window_get_content_region(ctx);
            state->has_scrollbar_bounds = true;
            nk_group_end(ctx);
        }
        nk_layout_row_push(ctx, scrollbar_margin);
        nk_spacing(ctx, 1);
        nk_layout_row_end(ctx);

        memview_draw_footer(
            ctx, state, nk_window_get_content_region(ctx), footer_h, editable, active_src);

        if (nk_input_is_mouse_click_in_rect(&ctx->input, NK_BUTTON_RIGHT, bounds) &&
            ops != NULL && ops->open_context_menu != NULL) {
            int view_index = state->active_index;
            uint16_t address = 0u;
            float click_y = ctx->input.mouse.buttons[NK_BUTTON_RIGHT].clicked_pos.y;

            if (view_index >= 0 && view_index < state->view_count) {
                address = state->views[view_index].cursor_address;
            }

            for (v = 0; v < state->view_count; v++) {
                if (state->views[v].rows > 0u &&
                    click_y >= state->views[v].cached_y_top &&
                    click_y < state->views[v].cached_y_bottom) {
                    view_index = v;
                    address = state->views[v].cursor_address;
                    break;
                }
            }
            ops->open_context_menu(
                ops->ctx, view_index, address, state->view_count, running);
        }
        if (ops != NULL && ops->draw_context_menu != NULL) {
            ops->draw_context_menu(ops->ctx, ctx, state);
        }
        if (mem_active) {
            debugger_draw_active_view_border(ctx);
        }
        if (have_rows_x) {
            canvas = nk_window_get_canvas(ctx);
            for (v = 0; v < state->view_count; v++) {
                memview_pane_view *mv = &state->views[v];
                const memory_source *src = memory_source_find_by_id(table, count, mv->source_id);
                struct nk_color border;
                float thickness;
                struct nk_rect br;
                size_t idx = 0u;

                if (mv->rows == 0u || src == NULL) {
                    continue;
                }
                if (count > 0u && src->id == table[0].id &&
                    (src->flags & MEMSRC_FOREIGN_BUS) == 0u) {
                    continue;
                }
                while (idx < count && table[idx].id != src->id) {
                    idx++;
                }
                border = memview_source_border_color(src, idx);
                thickness = (mem_active && v == state->active_index) ? 4.0f : 1.0f;
                br = nk_rect(
                    rows_x + thickness * 0.5f,
                    mv->cached_y_top + 2.0f,
                    rows_w - thickness,
                    (mv->cached_y_bottom - 2.0f) - (mv->cached_y_top + 2.0f));
                if (br.h > 0.0f) {
                    nk_stroke_rect(canvas, br, 0.0f, thickness, border);
                }
            }
        }
        ctx->style.window = saved_window_style;
    }
    nk_end(ctx);
}

void memview_pane_draw_search(
    struct nk_context *ctx,
    int width,
    int height,
    memview_pane_state *state,
    bool running,
    const memview_pane_ops *ops)
{
    memview_pane_search *search;
    struct nk_rect bounds;
    nk_flags edit_result;
    bool submit = false;

    (void)running;
    if (ctx == NULL || state == NULL || !state->search.open) {
        return;
    }
    search = &state->search;
    bounds = nk_rect((float)(width - 440) * 0.5f, (float)(height - 180) * 0.5f, 440.0f, 180.0f);
    if (nk_begin(
            ctx,
            "Find Memory",
            bounds,
            NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
        nk_layout_row_dynamic(ctx, 22.0f, 3);
        if (nk_option_label(ctx, "String", search->mode == MEMORY_SEARCH_STRING)) {
            search->mode = MEMORY_SEARCH_STRING;
            search->status[0] = '\0';
        }
        if (nk_option_label(ctx, "Hex", search->mode == MEMORY_SEARCH_HEX)) {
            search->mode = MEMORY_SEARCH_HEX;
            search->status[0] = '\0';
        }
        if (search->mode != MEMORY_SEARCH_STRING) {
            nk_widget_disable_begin(ctx);
        }
        {
            nk_bool ignore = search->ignore_case ? nk_true : nk_false;
            if (nk_checkbox_label(ctx, "Ignore case", &ignore)) {
                search->ignore_case = ignore != 0;
            }
        }
        if (search->mode != MEMORY_SEARCH_STRING) {
            nk_widget_disable_end(ctx);
        }
        nk_layout_row_begin(ctx, NK_DYNAMIC, 26.0f, 2);
        nk_layout_row_push(ctx, 0.18f);
        nk_label(ctx, search->mode == MEMORY_SEARCH_STRING ? "String" : "Hex bytes", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 0.82f);
        if (search->just_opened) {
            nk_edit_focus(ctx, 0);
            search->just_opened = false;
        }
        edit_result = nk_edit_string_zero_terminated(
            ctx,
            (nk_flags)NK_EDIT_FIELD | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD | NK_EDIT_SIG_ENTER,
            search->query,
            (int)sizeof(search->query),
            nk_filter_default);
        nk_layout_row_end(ctx);
        submit = (edit_result & NK_EDIT_COMMITED) != 0;
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(
            ctx,
            search->status[0] != '\0' ? search->status :
                (search->mode == MEMORY_SEARCH_HEX ?
                    "Example: DE AD BE EF" : "Searches the active memory view"),
            NK_TEXT_LEFT);
        nk_layout_row_dynamic(ctx, 26.0f, 2);
        if (nk_button_label(ctx, "Find")) {
            submit = true;
        }
        if (nk_button_label(ctx, "Cancel")) {
            search->open = false;
        }
        if (submit) {
            if (memory_search_parse(
                    search->query,
                    search->mode,
                    search->ignore_case,
                    &search->pattern,
                    search->status,
                    sizeof(search->status))) {
                search->has_pattern = true;
                (void)memview_pane_search_run(state, false, ops);
                search->open = false;
            }
        }
    }
    nk_end(ctx);
    if (search->open && nk_window_is_hidden(ctx, "Find Memory")) {
        search->open = false;
    }
}
