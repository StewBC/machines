#include "breakpoint_chrome.h"

#include <stdio.h>
#include <string.h>

enum {
    BREAKPOINT_CHROME_ACCESS_EXECUTE = 1u << 0,
    BREAKPOINT_CHROME_ACCESS_READ = 1u << 1,
    BREAKPOINT_CHROME_ACCESS_WRITE = 1u << 2
};

void breakpoint_chrome_draw_list(
    struct nk_context *ctx,
    const breakpoint_chrome_row *rows,
    uint16_t count,
    const breakpoint_chrome_ops *ops)
{
    uint16_t i;

    if (ctx == NULL) {
        return;
    }

    nk_layout_row_dynamic(ctx, 18.0f, 1);
    nk_label(ctx, "Breakpoints", NK_TEXT_LEFT);
    nk_layout_row_dynamic(ctx, 24.0f, 1);
    if (nk_button_label(ctx, "New") && ops != NULL && ops->on_new != NULL) {
        ops->on_new(ops->ctx);
    }

    if (count == 0) {
        nk_layout_row_dynamic(ctx, 18.0f, 1);
        nk_label(ctx, "No breakpoints set", NK_TEXT_LEFT);
    }

    for (i = 0; i < count; ++i) {
        const breakpoint_chrome_row *entry = &rows[i];
        struct nk_style_button saved_button = ctx->style.button;
        char label[96];
        char access[4];
        size_t access_len = 0;

        if (entry->enabled == 0) {
            ctx->style.button.text_normal = nk_rgb(180, 142, 210);
            ctx->style.button.normal = nk_style_item_color(nk_rgb(40, 34, 48));
        }
        if ((entry->access & BREAKPOINT_CHROME_ACCESS_EXECUTE) != 0) {
            access[access_len++] = 'X';
        }
        if ((entry->access & BREAKPOINT_CHROME_ACCESS_READ) != 0) {
            access[access_len++] = 'R';
        }
        if ((entry->access & BREAKPOINT_CHROME_ACCESS_WRITE) != 0) {
            access[access_len++] = 'W';
        }
        access[access_len] = '\0';

        if (entry->use_counter != 0) {
            if (entry->has_end_address) {
                snprintf(
                    label, sizeof(label), "%s [%04X-%04X] (%u:%u)",
                    access, entry->start_address, entry->end_address,
                    entry->current_hits, entry->counter);
            } else {
                snprintf(
                    label, sizeof(label), "%s [%04X] (%u:%u)",
                    access, entry->start_address, entry->current_hits, entry->counter);
            }
        } else if (entry->has_end_address) {
            snprintf(
                label, sizeof(label), "%s [%04X-%04X] (%u)",
                access, entry->start_address, entry->end_address, entry->current_hits);
        } else {
            snprintf(
                label, sizeof(label), "%s [%04X] (%u)",
                access, entry->start_address, entry->current_hits);
        }
        nk_layout_row_begin(ctx, NK_DYNAMIC, 24.0f, 6);
        nk_layout_row_push(ctx, 0.28f);
        nk_label_colored(
            ctx,
            label,
            NK_TEXT_LEFT,
            entry->enabled != 0 ? nk_rgb(232, 235, 238) : nk_rgb(180, 142, 210));
        nk_layout_row_push(ctx, 0.13f);
        if (nk_button_label(ctx, "Edit") && ops != NULL && ops->on_edit != NULL) {
            ops->on_edit(ops->ctx, entry->id);
        }
        nk_layout_row_push(ctx, 0.16f);
        if (nk_button_label(ctx, "Duplicate") && ops != NULL && ops->on_duplicate != NULL) {
            ops->on_duplicate(ops->ctx, entry->id);
        }
        nk_layout_row_push(ctx, 0.14f);
        if (nk_button_label(ctx, entry->enabled != 0 ? "Disable" : "Enable") &&
            ops != NULL && ops->on_set_enabled != NULL) {
            ops->on_set_enabled(ops->ctx, entry->id, entry->enabled == 0);
        }
        nk_layout_row_push(ctx, 0.15f);
        if (nk_button_label(ctx, "View PC") && ops != NULL && ops->on_view_pc != NULL) {
            ops->on_view_pc(ops->ctx, entry->start_address);
        }
        nk_layout_row_push(ctx, 0.14f);
        if (nk_button_label(ctx, "Clear") && ops != NULL && ops->on_clear != NULL) {
            ops->on_clear(ops->ctx, entry->id);
        }
        nk_layout_row_end(ctx);
        ctx->style.button = saved_button;
    }

    if (count >= 2) {
        nk_layout_row_dynamic(ctx, 24.0f, 1);
        if (nk_button_label(ctx, "Clear All") && ops != NULL && ops->on_clear_all != NULL) {
            ops->on_clear_all(ops->ctx);
        }
    }
}
