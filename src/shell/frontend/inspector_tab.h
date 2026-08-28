#pragma once

#include "nuklear_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct inspector_tab_row {
    const char *label;
    const char *value;
    bool wrap;
} inspector_tab_row;

typedef struct inspector_tab_view {
    bool inspecting;
    bool record_on;
    bool record_locked;
    bool can_enter;
    bool window_valid;
    bool inspector_enabled;
    const char *empty_message;
    int slider;
    int slider_max;
    bool can_previous;
    bool can_next;
    bool thumb_blocks_step;
    char snapshot_line[192];
    char cycle_line[64];
    inspector_tab_row extra[8];
    size_t extra_count;
} inspector_tab_view;

typedef struct inspector_tab_state {
    int slider;
    bool thumb_down;
} inspector_tab_state;

typedef struct inspector_tab_ops {
    void *ctx;
    void (*on_record)(void *ctx, bool enabled);
    void (*on_enter)(void *ctx);
    void (*on_leave)(void *ctx);
    void (*on_open_forensics)(void *ctx);
    void (*on_preview_tick)(void *ctx, int tick);
    void (*on_land_tick)(void *ctx, int tick);
    void (*on_step)(void *ctx, int direction);
} inspector_tab_ops;

void inspector_tab_sync_slider(
    inspector_tab_state *state,
    const inspector_tab_view *view);

bool inspector_tab_request_record(
    const inspector_tab_view *view,
    const inspector_tab_ops *ops,
    bool want_on);

void inspector_tab_request_forensics(const inspector_tab_ops *ops);

void inspector_tab_process_slider(
    inspector_tab_state *state,
    const inspector_tab_view *view,
    const inspector_tab_ops *ops,
    int tick,
    bool mouse_down_on_slider);

void inspector_tab_draw(
    struct nk_context *ctx,
    const inspector_tab_view *view,
    inspector_tab_state *state,
    const inspector_tab_ops *ops);

void inspector_chrome_begin_inspecting(
    struct nk_context *ctx,
    struct nk_style_window *saved);

void inspector_chrome_end_inspecting(
    struct nk_context *ctx,
    const struct nk_style_window *saved);

#ifdef __cplusplus
}
#endif
