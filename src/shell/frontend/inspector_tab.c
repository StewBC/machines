#include "inspector_tab.h"

#include <string.h>

static bool inspector_nk_action_button(
    struct nk_context *ctx,
    const char *label,
    bool enabled)
{
    if (!enabled) {
        nk_label(ctx, label, NK_TEXT_CENTERED);
        return false;
    }
    return nk_button_label(ctx, label) != 0;
}

static void inspector_tab_draw_rows(
    struct nk_context *ctx,
    const inspector_tab_row *rows,
    size_t count)
{
    size_t i;

    if (ctx == NULL || rows == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        if (rows[i].wrap) {
            if (rows[i].value != NULL && rows[i].value[0] != '\0') {
                nk_layout_row_dynamic(ctx, 32.0f, 1);
                nk_label_wrap(ctx, rows[i].value);
            }
            continue;
        }
        nk_layout_row_begin(ctx, NK_DYNAMIC, 18.0f, 2);
        nk_layout_row_push(ctx, 0.48f);
        nk_label(ctx, rows[i].label != NULL ? rows[i].label : "", NK_TEXT_LEFT);
        nk_layout_row_push(ctx, 0.52f);
        nk_label(ctx, rows[i].value != NULL ? rows[i].value : "", NK_TEXT_LEFT);
        nk_layout_row_end(ctx);
    }
}

void inspector_tab_sync_slider(
    inspector_tab_state *state,
    const inspector_tab_view *view)
{
    if (state == NULL || view == NULL) {
        return;
    }
    if (!state->thumb_down) {
        state->slider = view->slider;
    }
}

bool inspector_tab_request_record(
    const inspector_tab_view *view,
    const inspector_tab_ops *ops,
    bool want_on)
{
    bool have;

    if (view == NULL || view->record_locked) {
        return false;
    }
    have = view->record_on;
    if (want_on == have) {
        return false;
    }
    if (ops != NULL && ops->on_record != NULL) {
        ops->on_record(ops->ctx, want_on);
    }
    return true;
}

void inspector_tab_request_forensics(const inspector_tab_ops *ops)
{
    if (ops != NULL && ops->on_open_forensics != NULL) {
        ops->on_open_forensics(ops->ctx);
    }
}

void inspector_tab_process_slider(
    inspector_tab_state *state,
    const inspector_tab_view *view,
    const inspector_tab_ops *ops,
    int tick,
    bool mouse_down_on_slider)
{
    int max_tick;

    if (state == NULL || view == NULL) {
        return;
    }
    max_tick = view->slider_max;
    if (max_tick < 0) {
        max_tick = 0;
    }
    if (tick < 0) {
        tick = 0;
    }
    if (tick > max_tick) {
        tick = max_tick;
    }

    if (max_tick == 0 && !state->thumb_down) {
        state->slider = 0;
        return;
    }

    if (mouse_down_on_slider) {
        state->slider = tick;
        state->thumb_down = true;
        if (ops != NULL && ops->on_preview_tick != NULL) {
            ops->on_preview_tick(ops->ctx, tick);
        }
        return;
    }

    if (state->thumb_down) {
        state->slider = tick;
        state->thumb_down = false;
        if (ops != NULL && ops->on_land_tick != NULL) {
            ops->on_land_tick(ops->ctx, tick);
        }
    }
}

void inspector_tab_draw(
    struct nk_context *ctx,
    const inspector_tab_view *view,
    inspector_tab_state *state,
    const inspector_tab_ops *ops)
{
    nk_bool rec;
    int slider;
    bool down;
    bool thumb;
    bool slider_busy;

    if (ctx == NULL || view == NULL || state == NULL) {
        return;
    }

    inspector_tab_sync_slider(state, view);

    if (!view->inspecting) {
        rec = view->record_on ? nk_true : nk_false;
        nk_layout_row_dynamic(ctx, 22.0f, 1);
        if (view->record_locked) {
            nk_widget_disable_begin(ctx);
        }
        if (nk_checkbox_label(ctx, "Record", &rec)) {
            (void)inspector_tab_request_record(view, ops, rec != nk_false);
        }
        if (view->record_locked) {
            nk_widget_disable_end(ctx);
        }
    }

    nk_layout_row_dynamic(ctx, 24.0f, 1);
    if (nk_button_label(ctx, "Forensics...")) {
        inspector_tab_request_forensics(ops);
    }

    if (!view->inspector_enabled) {
        return;
    }

    if (!view->inspecting) {
        if (view->can_enter) {
            nk_layout_row_dynamic(ctx, 24.0f, 1);
            if (nk_button_label(ctx, "Inspect")) {
                if (ops != NULL && ops->on_enter != NULL) {
                    ops->on_enter(ops->ctx);
                }
            }
            inspector_tab_draw_rows(ctx, view->extra, view->extra_count);
        } else if (!view->window_valid && view->empty_message != NULL) {
            nk_layout_row_dynamic(ctx, 36.0f, 1);
            nk_label_wrap(ctx, view->empty_message);
        }
        return;
    }

    nk_layout_row_dynamic(ctx, 24.0f, 1);
    if (nk_button_label(ctx, "Leave Inspector")) {
        state->thumb_down = false;
        if (ops != NULL && ops->on_leave != NULL) {
            ops->on_leave(ops->ctx);
        }
    }

    slider = state->slider;
    down = nk_input_is_mouse_down(&ctx->input, NK_BUTTON_LEFT);
    thumb = state->thumb_down;
    slider_busy = thumb || view->thumb_blocks_step;

    nk_layout_row_begin(ctx, NK_DYNAMIC, 22.0f, 3);
    nk_layout_row_push(ctx, 0.08f);
    if (inspector_nk_action_button(
            ctx, "-", !thumb && view->can_previous)) {
        if (ops != NULL && ops->on_step != NULL) {
            ops->on_step(ops->ctx, -1);
        }
    }
    nk_layout_row_push(ctx, 0.84f);
    {
        struct nk_rect bounds = nk_widget_bounds(ctx);
        nk_bool moved;
        bool hovered;

        if (view->slider_max == 0) {
            nk_widget_disable_begin(ctx);
        }
        moved = nk_slider_int(ctx, 0, &slider, view->slider_max, 1);
        if (view->slider_max == 0) {
            nk_widget_disable_end(ctx);
        }
        hovered = nk_input_is_mouse_hovering_rect(&ctx->input, bounds);
        if ((moved && down) || (state->thumb_down && down) ||
            ((hovered || moved) && down)) {
            inspector_tab_process_slider(state, view, ops, slider, true);
        } else if (state->thumb_down && !down) {
            inspector_tab_process_slider(state, view, ops, slider, false);
        } else if (!slider_busy) {
            state->slider = slider;
        }
    }
    nk_layout_row_push(ctx, 0.08f);
    if (inspector_nk_action_button(ctx, "+", !thumb && view->can_next)) {
        if (ops != NULL && ops->on_step != NULL) {
            ops->on_step(ops->ctx, 1);
        }
    }
    nk_layout_row_end(ctx);

    nk_layout_row_begin(ctx, NK_DYNAMIC, 18.0f, 2);
    nk_layout_row_push(ctx, 0.48f);
    nk_label(ctx, "Snapshot:", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.52f);
    nk_label(ctx, view->snapshot_line, NK_TEXT_LEFT);
    nk_layout_row_end(ctx);

    nk_layout_row_begin(ctx, NK_DYNAMIC, 18.0f, 2);
    nk_layout_row_push(ctx, 0.48f);
    nk_label(ctx, "Current cycle:", NK_TEXT_LEFT);
    nk_layout_row_push(ctx, 0.52f);
    nk_label(ctx, view->cycle_line, NK_TEXT_LEFT);
    nk_layout_row_end(ctx);

    inspector_tab_draw_rows(ctx, view->extra, view->extra_count);
}

void inspector_chrome_begin_inspecting(
    struct nk_context *ctx,
    struct nk_style_window *saved)
{
    if (ctx == NULL || saved == NULL) {
        return;
    }
    *saved = ctx->style.window;
    ctx->style.window.header.normal =
        nk_style_item_color(nk_rgb(24, 62, 118));
    ctx->style.window.header.hover =
        nk_style_item_color(nk_rgb(32, 76, 136));
    ctx->style.window.header.active =
        nk_style_item_color(nk_rgb(40, 88, 152));
}

void inspector_chrome_end_inspecting(
    struct nk_context *ctx,
    const struct nk_style_window *saved)
{
    if (ctx == NULL || saved == NULL) {
        return;
    }
    ctx->style.window = *saved;
}
