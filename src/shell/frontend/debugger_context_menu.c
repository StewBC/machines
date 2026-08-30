#include "debugger_context_menu.h"

#include <stdio.h>

void debugger_context_popup_open(
    struct nk_context *ctx,
    debugger_context_popup *popup,
    int window_w,
    int window_h,
    float width,
    float desired_height)
{
    struct nk_rect origin;
    struct nk_rect viewport;
    struct nk_vec2 pos;
    float height;
    float x;
    float y;
    float max_height;

    if (ctx == NULL || popup == NULL) {
        return;
    }

    origin = nk_window_get_content_region(ctx);
    if (window_w > 16 && window_h > 16) {
        viewport = nk_rect(4.0f, 4.0f, (float)window_w - 8.0f, (float)window_h - 8.0f);
    } else {
        viewport = origin;
    }

    pos = ctx->input.mouse.buttons[NK_BUTTON_RIGHT].clicked_pos;
    max_height = viewport.h;
    if (max_height < 60.0f) {
        max_height = viewport.h > 0.0f ? viewport.h : 60.0f;
    }
    height = desired_height > max_height ? max_height : desired_height;
    if (height < 60.0f) {
        height = 60.0f;
    }

    x = pos.x;
    y = pos.y;
    if (x + width > viewport.x + viewport.w) {
        x = viewport.x + viewport.w - width;
    }
    if (x < viewport.x) {
        x = viewport.x;
    }
    if (y + height > viewport.y + viewport.h) {
        y = viewport.y + viewport.h - height;
    }
    if (y < viewport.y) {
        y = viewport.y;
    }

    popup->open = true;
    popup->just_opened = true;
    popup->scroll = height < desired_height;
    popup->group_open = false;
    popup->rect = nk_rect(x - origin.x, y - origin.y, width, height);
    popup->screen_rect = nk_rect(x, y, width, height);
}

bool debugger_context_popup_begin(
    struct nk_context *ctx,
    debugger_context_popup *popup,
    const char *title)
{
    const struct nk_input *input;
    bool click_outside;

    if (ctx == NULL || popup == NULL || !popup->open) {
        return false;
    }

    input = &ctx->input;
    click_outside =
        (nk_input_is_mouse_pressed(input, NK_BUTTON_LEFT) ||
         nk_input_is_mouse_pressed(input, NK_BUTTON_RIGHT)) &&
        !nk_input_is_mouse_hovering_rect(input, popup->screen_rect);
    if (!popup->just_opened && click_outside) {
        popup->open = false;
        return false;
    }
    popup->just_opened = false;

    if (!nk_popup_begin(
            ctx, NK_POPUP_STATIC, title, NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR, popup->rect)) {
        popup->open = false;
        return false;
    }

    if (popup->scroll) {
        nk_layout_row_dynamic(ctx, popup->rect.h - 8.0f, 1);
        popup->group_open = nk_group_begin(ctx, title, 0) ? true : false;
        if (!popup->group_open) {
            return true;
        }
    }
    return true;
}

void debugger_context_popup_end(
    struct nk_context *ctx,
    debugger_context_popup *popup,
    bool close_popup)
{
    if (ctx == NULL || popup == NULL) {
        return;
    }

    if (popup->scroll && popup->group_open) {
        nk_group_end(ctx);
        popup->group_open = false;
    }
    if (close_popup) {
        popup->open = false;
        nk_popup_close(ctx);
    }
    nk_popup_end(ctx);
}

void debugger_context_menu_label(struct nk_context *ctx, const char *label)
{
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label(ctx, label, NK_TEXT_LEFT);
}

void debugger_context_menu_separator(struct nk_context *ctx)
{
    struct nk_rect bounds;

    nk_layout_row_dynamic(ctx, 5.0f, 1);
    if (nk_widget(&bounds, ctx) != NK_WIDGET_INVALID) {
        struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
        float y = bounds.y + bounds.h * 0.5f;
        nk_stroke_line(
            canvas,
            bounds.x,
            y,
            bounds.x + bounds.w,
            y,
            1.0f,
            nk_rgb(90, 101, 110));
    }
}

void debugger_context_menu_heading(struct nk_context *ctx, const char *label)
{
    /* Titles stay left-aligned like items; colour is what marks them as group
     * labels (selectable rows use the default text colour). */
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    nk_label_colored(ctx, label, NK_TEXT_LEFT, nk_rgb(160, 200, 220));
    debugger_context_menu_separator(ctx);
}

bool debugger_context_menu_item(struct nk_context *ctx, const char *label)
{
    nk_bool selected = nk_false;

    nk_layout_row_dynamic(ctx, 22.0f, 1);
    return nk_selectable_label(ctx, label, NK_TEXT_LEFT, &selected) != 0;
}

bool debugger_context_menu_mode_item(
    struct nk_context *ctx, bool active, const char *label)
{
    char item[24];
    nk_bool selected = nk_false;

    snprintf(item, sizeof(item), "%c %s", active ? '*' : ' ', label);
    nk_layout_row_dynamic(ctx, 22.0f, 1);
    return nk_selectable_label(ctx, item, NK_TEXT_LEFT, &selected) != 0;
}

bool debugger_context_menu_access(
    struct nk_context *ctx, uint64_t write_history, uint16_t *out_address)
{
    int lane;
    bool selected = false;

    debugger_context_menu_heading(ctx, "Access");
    for (lane = 3; lane >= 0; lane--) {
        char item[5];
        unsigned shift = (unsigned)lane * 16u;
        uint16_t address = (uint16_t)((write_history >> shift) & 0xffffu);

        snprintf(item, sizeof(item), "%04X", (unsigned)address);
        if (debugger_context_menu_item(ctx, item)) {
            if (out_address != NULL) {
                *out_address = address;
            }
            selected = true;
        }
    }
    return selected;
}
