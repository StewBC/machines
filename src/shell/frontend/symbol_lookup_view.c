#include "symbol_lookup_view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const frontend_symbol_lookup_entry *g_sort_entries_ptr = NULL;
static frontend_symbol_lookup_sort_col     g_sort_col_val;
static bool                                g_sort_asc_val;

void symbol_lookup_view_basename(const char *src, char *out, int max)
{
    const char *base;
    const char *ext;
    int len;

    if (src == NULL || out == NULL || max <= 0) {
        if (out && max > 0) out[0] = '\0';
        return;
    }

    base = src;
    for (const char *p = src; *p; ++p) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    ext = NULL;
    for (const char *p = base; *p; ++p) {
        if (*p == '.') ext = p;
    }

    len = ext ? (int)(ext - base) : (int)strlen(base);
    if (len >= max) len = max - 1;
    memcpy(out, base, (size_t)len);
    out[len] = '\0';
}

static void symbol_lookup_scope_str(const symbol_info *info, char *out, int max)
{
    int len;

    if (out == NULL || max <= 0) return;
    if (info == NULL || info->scope_path_length == 0) {
        out[0] = '\0';
        return;
    }

    len = (int)info->scope_path_length;
    if (len >= max) len = max - 1;
    memcpy(out, info->name, (size_t)len);
    out[len] = '\0';
}

static int symbol_lookup_compare(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    const frontend_symbol_lookup_entry *ea = &g_sort_entries_ptr[ia];
    const frontend_symbol_lookup_entry *eb = &g_sort_entries_ptr[ib];
    int cmp = 0;

    switch (g_sort_col_val) {
        case SYMBOL_LOOKUP_SORT_ADDR:
            cmp = (ea->address < eb->address) ? -1 : (ea->address > eb->address) ? 1 : 0;
            break;
        case SYMBOL_LOOKUP_SORT_SCOPE:
            cmp = strcmp(ea->scope, eb->scope);
            if (cmp == 0) cmp = (ea->address < eb->address) ? -1 : (ea->address > eb->address) ? 1 : 0;
            break;
        case SYMBOL_LOOKUP_SORT_LABEL:
            cmp = strcmp(ea->label, eb->label);
            if (cmp == 0) cmp = (ea->address < eb->address) ? -1 : (ea->address > eb->address) ? 1 : 0;
            break;
        case SYMBOL_LOOKUP_SORT_SOURCE:
            cmp = strcmp(ea->source, eb->source);
            if (cmp == 0) cmp = (ea->address < eb->address) ? -1 : (ea->address > eb->address) ? 1 : 0;
            break;
    }
    return g_sort_asc_val ? cmp : -cmp;
}

static void symbol_lookup_refilter(frontend_symbol_lookup_state *dlg)
{
    int i;
    char row[SYMBOL_LOOKUP_COL_MAX * 3 + 16];
    char addr_hex[5];

    dlg->filtered_count = 0;

    for (i = 0; i < dlg->entry_count; ++i) {
        const frontend_symbol_lookup_entry *e = &dlg->entries[i];
        if (dlg->search[0] == '\0') {
            dlg->filtered[dlg->filtered_count++] = i;
            continue;
        }
        snprintf(addr_hex, sizeof(addr_hex), "%04X", e->address);
        snprintf(row, sizeof(row), "%s %s %s %s", addr_hex, e->scope, e->label, e->source);
        if (nk_strfilter(row, dlg->search)) {
            dlg->filtered[dlg->filtered_count++] = i;
        }
    }

    if (dlg->filtered_count > 1) {
        g_sort_entries_ptr = dlg->entries;
        g_sort_col_val     = dlg->sort_col;
        g_sort_asc_val     = dlg->sort_asc;
        qsort(dlg->filtered, (size_t)dlg->filtered_count, sizeof(int), symbol_lookup_compare);
    }

    if (dlg->selected >= dlg->filtered_count) {
        dlg->selected = dlg->filtered_count > 0 ? 0 : -1;
    }
}

static void symbol_lookup_set_sort(
    frontend_symbol_lookup_state *dlg,
    frontend_symbol_lookup_sort_col col)
{
    if (dlg->sort_col == col) {
        dlg->sort_asc = !dlg->sort_asc;
    } else {
        dlg->sort_col = col;
        dlg->sort_asc = true;
    }

    if (dlg->filtered_count > 1) {
        g_sort_entries_ptr = dlg->entries;
        g_sort_col_val     = dlg->sort_col;
        g_sort_asc_val     = dlg->sort_asc;
        qsort(dlg->filtered, (size_t)dlg->filtered_count, sizeof(int), symbol_lookup_compare);
    }
    dlg->selected = dlg->filtered_count > 0 ? 0 : -1;
    dlg->scroll_to_selected = true;
}

static nk_flags symbol_lookup_edit_replace(
    struct nk_context *ctx,
    nk_flags flags,
    char *buffer,
    int max,
    nk_plugin_filter filter)
{
    nk_flags result = nk_edit_string_zero_terminated(
        ctx,
        (flags & ~(nk_flags)NK_EDIT_ALWAYS_INSERT_MODE) | NK_EDIT_SIG_ENTER,
        buffer, max, filter);
    if (result & NK_EDIT_ACTIVE) {
        ctx->current->edit.mode = NK_TEXT_EDIT_MODE_REPLACE;
    }
    if (result & NK_EDIT_COMMITED) {
        nk_edit_unfocus(ctx);
    }
    return result;
}

static void symbol_lookup_commit(
    frontend_symbol_lookup_state *dlg,
    const symbol_lookup_ops *ops)
{
    const frontend_symbol_lookup_entry *e;
    uint16_t addr;

    if (dlg == NULL) return;
    if (dlg->selected < 0 || dlg->selected >= dlg->filtered_count) {
        dlg->open = false;
        dlg->filter_open = false;
        return;
    }

    e    = &dlg->entries[dlg->filtered[dlg->selected]];
    addr = e->address;
    dlg->open = false;
    dlg->filter_open = false;

    if (ops == NULL) {
        return;
    }
    if (dlg->from_memory) {
        if (ops->jump_memory != NULL) {
            ops->jump_memory(ops->ctx, addr);
        }
    } else if (ops->jump_disasm != NULL) {
        ops->jump_disasm(ops->ctx, addr);
    }
}

void symbol_lookup_view_init(frontend_symbol_lookup_state *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->sort_col = SYMBOL_LOOKUP_SORT_ADDR;
    state->sort_asc = true;
    state->selected = -1;
}

void symbol_lookup_view_close(frontend_symbol_lookup_state *state)
{
    if (state == NULL) {
        return;
    }
    state->open = false;
    state->filter_open = false;
}

bool symbol_lookup_view_is_open(const frontend_symbol_lookup_state *state)
{
    return state != NULL && state->open;
}

bool symbol_lookup_view_filter_is_open(const frontend_symbol_lookup_state *state)
{
    return state != NULL && state->filter_open;
}

bool symbol_lookup_view_any_open(const frontend_symbol_lookup_state *state)
{
    return state != NULL && (state->open || state->filter_open);
}

void symbol_lookup_view_set_sources(
    frontend_symbol_lookup_state *state,
    const runtime_symbol_source_snapshot_entry *sources,
    size_t source_count)
{
    size_t i, n;

    if (state == NULL) {
        return;
    }

    n = source_count;
    if (n > SYMBOL_FILTER_SOURCE_MAX) {
        n = SYMBOL_FILTER_SOURCE_MAX;
    }
    state->filter_source_count = n;
    for (i = 0; i < n; ++i) {
        const runtime_symbol_source_snapshot_entry *src = &sources[i];
        symbol_filter_source_row *row = &state->filter_sources[i];
        row->source_id = src->source_id;
        row->enabled = src->enabled != 0u;
        symbol_lookup_view_basename(src->source_name, row->label, sizeof(row->label));
    }
    if (n == 0u) {
        state->filter_open = false;
    }
}

void symbol_lookup_view_rebuild_entries(
    frontend_symbol_lookup_state *state,
    const symbol_table *table)
{
    size_t i, count;
    symbol_info info;
    char search[SYMBOL_LOOKUP_SEARCH_MAX];
    frontend_symbol_lookup_sort_col sort_col;
    bool sort_asc;
    bool from_memory;
    bool table_has_kb_focus;
    int selected;

    if (state == NULL) {
        return;
    }

    memcpy(search, state->search, sizeof(search));
    sort_col = state->sort_col;
    sort_asc = state->sort_asc;
    from_memory = state->from_memory;
    table_has_kb_focus = state->table_has_kb_focus;
    selected = state->selected;

    state->entry_count = 0;
    state->filtered_count = 0;
    if (table != NULL) {
        count = symbol_table_count(table);
        if (count > SYMBOL_LOOKUP_ENTRY_MAX) {
            count = SYMBOL_LOOKUP_ENTRY_MAX;
        }
        for (i = 0; i < count; ++i) {
            frontend_symbol_lookup_entry *e = &state->entries[state->entry_count];
            if (symbol_table_get(table, i, &info) != SYMBOL_OK) {
                continue;
            }
            e->address = info.address;
            symbol_lookup_scope_str(&info, e->scope, sizeof(e->scope));
            snprintf(e->label, sizeof(e->label), "%.*s",
                SYMBOL_LOOKUP_COL_MAX, info.display_name);
            symbol_lookup_view_basename(info.source_name, e->source, sizeof(e->source));
            state->entry_count++;
        }
    }

    memcpy(state->search, search, sizeof(state->search));
    state->sort_col = sort_col;
    state->sort_asc = sort_asc;
    state->from_memory = from_memory;
    state->table_has_kb_focus = table_has_kb_focus;
    symbol_lookup_refilter(state);
    if (selected < 0 || selected >= state->filtered_count) {
        state->selected = state->filtered_count > 0 ? 0 : -1;
    } else {
        state->selected = selected;
    }
    state->scroll_to_selected = true;
}

void symbol_lookup_view_open(
    frontend_symbol_lookup_state *state,
    const symbol_table *table,
    bool from_memory)
{
    symbol_filter_source_row saved_sources[SYMBOL_FILTER_SOURCE_MAX];
    size_t saved_count = 0u;

    if (state == NULL) {
        return;
    }

    /* Preserve chrome Filter cache across reopen; Filter starts closed. */
    saved_count = state->filter_source_count;
    if (saved_count > SYMBOL_FILTER_SOURCE_MAX) {
        saved_count = SYMBOL_FILTER_SOURCE_MAX;
    }
    if (saved_count > 0u) {
        memcpy(saved_sources, state->filter_sources,
            saved_count * sizeof(saved_sources[0]));
    }

    memset(state, 0, sizeof(*state));
    state->sort_col = SYMBOL_LOOKUP_SORT_ADDR;
    state->sort_asc = true;
    state->selected = -1;
    state->from_memory = from_memory;
    state->filter_source_count = saved_count;
    if (saved_count > 0u) {
        memcpy(state->filter_sources, saved_sources,
            saved_count * sizeof(state->filter_sources[0]));
    }
    symbol_lookup_view_rebuild_entries(state, table);
    state->just_opened = true;
    state->open = true;
}

bool symbol_lookup_view_handle_key(
    frontend_symbol_lookup_state *state,
    const symbol_lookup_ops *ops,
    SDL_Keycode key)
{
    if (state == NULL || !symbol_lookup_view_any_open(state)) {
        return false;
    }

    if (key == SDLK_ESCAPE) {
        if (state->filter_open) {
            state->filter_open = false;
            return true;
        }
        if (state->open) {
            state->open = false;
            return true;
        }
        return false;
    }

    if (!state->open) {
        return false;
    }

    if (key == SDLK_TAB) {
        state->table_has_kb_focus = !state->table_has_kb_focus;
        return true;
    }
    if (!state->table_has_kb_focus) {
        return false;
    }
    if (key == SDLK_UP) {
        if (state->selected > 0) {
            state->selected--;
            state->scroll_to_selected = true;
        }
        return true;
    }
    if (key == SDLK_DOWN) {
        if (state->selected < state->filtered_count - 1) {
            state->selected++;
            state->scroll_to_selected = true;
        }
        return true;
    }
    if (key == SDLK_RETURN) {
        symbol_lookup_commit(state, ops);
        return true;
    }
    return false;
}

static void symbol_lookup_render_filter(
    struct nk_context *ctx,
    frontend_symbol_lookup_state *state,
    const symbol_lookup_ops *ops,
    int width,
    int height)
{
    struct nk_rect bounds;
    struct nk_list_view lv;
    float dw, dh, list_h;
    int i;
    static const int ROW_H = 22;

    if (ctx == NULL || state == NULL || !state->filter_open) {
        return;
    }

    dw = 360.0f;
    if (dw > (float)width - 16.0f && width > 0) dw = (float)width - 16.0f;
    dh = (float)height * 0.50f;
    if (dh < 220.0f) dh = 220.0f;
    if (dh > (float)height - 16.0f && height > 0) dh = (float)height - 16.0f;

    /* Offset from Lookup center so both stay readable when stacked. */
    bounds = nk_rect(
        ((float)width - dw) * 0.5f + 28.0f,
        ((float)height - dh) * 0.5f + 36.0f,
        dw, dh);

    if (nk_begin(ctx, "Symbol Filter", bounds,
            NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE
            | NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "Enabled sources participate in resolve", NK_TEXT_LEFT);

        {
            struct nk_rect content = nk_window_get_content_region(ctx);
            float pad_y = ctx->style.window.padding.y;
            float sp_y  = ctx->style.window.spacing.y;
            const int rows = 3; /* hint, list, close */
            float other_h = 18.0f + 24.0f;
            list_h = content.h - other_h - (float)rows * sp_y - 2.0f * pad_y;
        }
        if (list_h < 40.0f) list_h = 40.0f;
        nk_layout_row_dynamic(ctx, list_h, 1);

        if (nk_list_view_begin(ctx, &lv, "sym_filter_rows",
                NK_WINDOW_BORDER, ROW_H, (int)state->filter_source_count)) {
            for (i = lv.begin; i < lv.end; ++i) {
                symbol_filter_source_row *row;
                nk_bool active;
                bool before;

                if (i < 0 || (size_t)i >= state->filter_source_count) {
                    continue;
                }
                row = &state->filter_sources[i];
                before = row->enabled;
                active = before ? nk_true : nk_false;

                nk_layout_row_dynamic(ctx, (float)ROW_H, 1);
                nk_checkbox_label(ctx,
                    row->label[0] != '\0' ? row->label : "(unnamed)",
                    &active);
                row->enabled = active != 0;
                if (row->enabled != before &&
                        ops != NULL && ops->set_source_enabled != NULL) {
                    ops->set_source_enabled(ops->ctx, row->source_id, row->enabled);
                }
            }
            nk_list_view_end(&lv);
        }

        nk_layout_row_dynamic(ctx, 24.0f, 3);
        nk_spacing(ctx, 2);
        if (nk_button_label(ctx, "Close")) {
            state->filter_open = false;
        }

    }
    nk_end(ctx);
    if (state->filter_open && nk_window_is_hidden(ctx, "Symbol Filter")) {
        state->filter_open = false;
    }
}

void symbol_lookup_view_render(
    struct nk_context *ctx,
    frontend_symbol_lookup_state *state,
    const symbol_lookup_ops *ops,
    int width,
    int height)
{
    struct nk_rect bounds;
    struct nk_list_view lv;
    char prev_search[SYMBOL_LOOKUP_SEARCH_MAX];
    float dw, dh, table_h;
    int i;

    static const int ROW_H = 18;
    /* ADDR stays narrow; SCOPE/LABEL take most of the row; SOURCE is usually a
     * short basename. Ratios sum to 1 so the row always fits the panel width. */
    static const float COL_RATIO[4] = {0.10f, 0.30f, 0.38f, 0.22f};

    if (ctx == NULL || state == NULL) {
        return;
    }

    if (state->open) {
        dw = (float)width * 0.72f;
        if (dw < 520.0f) dw = 520.0f;
        if (dw > (float)width - 16.0f && width > 0) dw = (float)width - 16.0f;
        dh = (float)height * 0.70f;
        if (dh < 340.0f) dh = 340.0f;
        if (dh > (float)height - 16.0f && height > 0) dh = (float)height - 16.0f;

        bounds = nk_rect(((float)width - dw) * 0.5f, ((float)height - dh) * 0.5f, dw, dh);

        if (nk_begin(ctx, "Symbol Lookup", bounds,
                NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE
                | NK_WINDOW_NO_SCROLLBAR)) {

            memcpy(prev_search, state->search, sizeof(state->search));
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (state->just_opened) {
                nk_edit_focus(ctx, 0);
                state->just_opened = false;
            }
            symbol_lookup_edit_replace(
                ctx,
                (nk_flags)NK_EDIT_FIELD | NK_EDIT_SELECTABLE | NK_EDIT_CLIPBOARD,
                state->search, sizeof(state->search), nk_filter_default);
            if (memcmp(prev_search, state->search, sizeof(state->search)) != 0) {
                symbol_lookup_refilter(state);
                state->scroll_to_selected = true;
            }

            {
                char h0[12], h1[12], h2[12], h3[12];
                snprintf(h0, sizeof(h0), "ADDR%s",
                    state->sort_col == SYMBOL_LOOKUP_SORT_ADDR   ? (state->sort_asc ? "^" : "v") : "");
                snprintf(h1, sizeof(h1), "SCOPE%s",
                    state->sort_col == SYMBOL_LOOKUP_SORT_SCOPE  ? (state->sort_asc ? "^" : "v") : "");
                snprintf(h2, sizeof(h2), "LABEL%s",
                    state->sort_col == SYMBOL_LOOKUP_SORT_LABEL  ? (state->sort_asc ? "^" : "v") : "");
                snprintf(h3, sizeof(h3), "SOURCE%s",
                    state->sort_col == SYMBOL_LOOKUP_SORT_SOURCE ? (state->sort_asc ? "^" : "v") : "");

                nk_layout_row(ctx, NK_DYNAMIC, 22.0f, 4, COL_RATIO);
                if (nk_button_label(ctx, h0))
                    symbol_lookup_set_sort(state, SYMBOL_LOOKUP_SORT_ADDR);
                if (nk_button_label(ctx, h1))
                    symbol_lookup_set_sort(state, SYMBOL_LOOKUP_SORT_SCOPE);
                if (nk_button_label(ctx, h2))
                    symbol_lookup_set_sort(state, SYMBOL_LOOKUP_SORT_LABEL);
                if (nk_button_label(ctx, h3))
                    symbol_lookup_set_sort(state, SYMBOL_LOOKUP_SORT_SOURCE);
            }

            /* Size the list so search + headers + footer fit with no window-level
             * scrollbar. Same approach as the File Browser. */
            {
                struct nk_rect content = nk_window_get_content_region(ctx);
                float pad_y = ctx->style.window.padding.y;
                float sp_y  = ctx->style.window.spacing.y;
                const int rows = 4; /* search, headers, list, footer */
                float other_h = 24.0f  /* search */
                              + 22.0f  /* headers */
                              + 24.0f; /* Filter/Close */
                table_h = content.h - other_h - (float)rows * sp_y - 2.0f * pad_y;
            }
            if (table_h < 40.0f) table_h = 40.0f;
            nk_layout_row_dynamic(ctx, table_h, 1);

            if (nk_list_view_begin(ctx, &lv, "sym_rows", 0, ROW_H, state->filtered_count)) {
                struct nk_style_selectable saved_sel = ctx->style.selectable;

                if (state->scroll_to_selected && lv.scroll_pointer != NULL && state->selected >= 0) {
                    nk_uint top    = *lv.scroll_pointer;
                    nk_uint bot    = top + (nk_uint)table_h;
                    nk_uint item_y = (nk_uint)(state->selected * ROW_H);
                    if (item_y < top) {
                        *lv.scroll_pointer = item_y;
                    } else if (item_y + (nk_uint)ROW_H > bot) {
                        *lv.scroll_pointer = item_y + (nk_uint)ROW_H - (nk_uint)table_h;
                    }
                    state->scroll_to_selected = false;
                }

                for (i = lv.begin; i < lv.end; ++i) {
                    const frontend_symbol_lookup_entry *e;
                    bool sel;
                    char addr_buf[6];
                    bool clicked = false;

                    if (i < 0 || i >= state->filtered_count) continue;
                    e   = &state->entries[state->filtered[i]];
                    sel = (i == state->selected);

                    snprintf(addr_buf, sizeof(addr_buf), "%04X", e->address);

                    if (sel) {
                        ctx->style.selectable.normal  = nk_style_item_color(nk_rgb(21, 91, 116));
                        ctx->style.selectable.hover   = nk_style_item_color(nk_rgb(21, 91, 116));
                        ctx->style.selectable.pressed = nk_style_item_color(nk_rgb(21, 91, 116));
                        ctx->style.selectable.text_normal  = nk_rgb(226, 246, 255);
                        ctx->style.selectable.text_hover   = nk_rgb(226, 246, 255);
                        ctx->style.selectable.text_pressed = nk_rgb(226, 246, 255);
                    } else {
                        ctx->style.selectable = saved_sel;
                    }

                    nk_layout_row(ctx, NK_DYNAMIC, (float)ROW_H, 4, COL_RATIO);
                    /* nk_selectable_label asserts on len==0; blank cells use a space. */
                    {
                        bool s = sel;
                        if (nk_selectable_label(ctx, addr_buf, NK_TEXT_LEFT, &s)) clicked = true;
                    }
                    {
                        bool s = sel;
                        if (nk_selectable_label(ctx, e->scope[0] ? e->scope : " ",
                                NK_TEXT_LEFT, &s)) clicked = true;
                    }
                    {
                        bool s = sel;
                        if (nk_selectable_label(ctx, e->label[0] ? e->label : " ",
                                NK_TEXT_LEFT, &s)) clicked = true;
                    }
                    {
                        bool s = sel;
                        if (nk_selectable_label(ctx, e->source[0] ? e->source : " ",
                                NK_TEXT_LEFT, &s)) clicked = true;
                    }

                    if (clicked) {
                        state->selected = i;
                        symbol_lookup_commit(state, ops);
                        ctx->style.selectable = saved_sel;
                        nk_list_view_end(&lv);
                        nk_end(ctx);
                        symbol_lookup_render_filter(ctx, state, ops, width, height);
                        return;
                    }
                }

                ctx->style.selectable = saved_sel;
                nk_list_view_end(&lv);
            }

            nk_layout_row_dynamic(ctx, 24.0f, 2);
            if (state->filter_source_count == 0u) {
                nk_widget_disable_begin(ctx);
            }
            if (nk_button_label(ctx, "Filter")) {
                state->filter_open = true;
            }
            if (state->filter_source_count == 0u) {
                nk_widget_disable_end(ctx);
            }
            if (nk_button_label(ctx, "Close")) {
                state->open = false;
                state->filter_open = false;
            }

        }
        nk_end(ctx);
        if (state->open && nk_window_is_hidden(ctx, "Symbol Lookup")) {
            state->open = false;
            state->filter_open = false;
        }
    }

    /* Draw Filter after Lookup (stacking precedent: File Browser after Load). */
    symbol_lookup_render_filter(ctx, state, ops, width, height);
}
