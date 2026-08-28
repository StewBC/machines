#pragma once

#include "nuklear_config.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct debugger_context_popup {
    bool open;
    bool just_opened;
    bool scroll;
    bool group_open;
    struct nk_rect rect;
    struct nk_rect screen_rect;
} debugger_context_popup;

void debugger_context_popup_open(
    struct nk_context *ctx,
    debugger_context_popup *popup,
    int window_w,
    int window_h,
    float width,
    float desired_height);

bool debugger_context_popup_begin(
    struct nk_context *ctx,
    debugger_context_popup *popup,
    const char *title);

void debugger_context_popup_end(
    struct nk_context *ctx,
    debugger_context_popup *popup,
    bool close_popup);

void debugger_context_menu_label(struct nk_context *ctx, const char *label);
void debugger_context_menu_separator(struct nk_context *ctx);
void debugger_context_menu_heading(struct nk_context *ctx, const char *label);
bool debugger_context_menu_item(struct nk_context *ctx, const char *label);
bool debugger_context_menu_mode_item(
    struct nk_context *ctx, bool active, const char *label);
bool debugger_context_menu_access(
    struct nk_context *ctx, uint64_t write_history, uint16_t *out_address);

#ifdef __cplusplus
}
#endif
