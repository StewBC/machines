#pragma once

#include "memory_search.h"
#include "memory_source.h"
#include "nuklear_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MEMVIEW_PANE_VIEW_MAX = 16
};

typedef enum memview_pane_edit_field {
    MEMVIEW_PANE_EDIT_HEX = 0,
    MEMVIEW_PANE_EDIT_ASCII,
    MEMVIEW_PANE_EDIT_ADDRESS
} memview_pane_edit_field;

typedef struct memview_pane_view {
    uint16_t view_address;
    uint16_t cursor_address;
    uint32_t source_id;
    memview_pane_edit_field edit_field;
    uint8_t active_nibble;
    uint8_t active_address_digit;
    uint8_t columns;
    uint8_t rows;
    uint8_t color_slot;
    float cached_y_top;
    float cached_y_bottom;
    bool initialized;
    bool request_pending;
    bool scrollbar_dragging;
    float scrollbar_grab_offset;
    bool highbit_ascii;
    bool highbit_user_set;
} memview_pane_view;

typedef struct memview_pane_search {
    bool open;
    bool just_opened;
    bool ignore_case;
    bool has_pattern;
    memory_search_mode mode;
    char query[MEMORY_SEARCH_QUERY_MAX + 1];
    memory_search_pattern pattern;
    uint16_t last_found_address;
    char status[96];
} memview_pane_search;

typedef struct memview_pane_state {
    memview_pane_view views[MEMVIEW_PANE_VIEW_MAX];
    memview_pane_search search;
    int view_count;
    int active_index;
    bool color_slot_used[MEMVIEW_PANE_VIEW_MAX];
    struct nk_rect scrollbar_bounds;
    bool has_scrollbar_bounds;
} memview_pane_state;

typedef struct memview_pane_ops {
    void *ctx;
    bool (*byte_available)(void *ctx, uint32_t source_id, uint16_t address);
    bool (*read_byte)(void *ctx, uint32_t source_id, uint16_t address, uint8_t *out);
    void (*request)(void *ctx);
    const uint8_t *(*search_plane)(
        void *ctx, uint32_t source_id, const uint8_t **out_valid);
    void (*set_active_view)(void *ctx);
    bool (*any_dialog_open)(void *ctx);
    bool (*view_is_active)(void *ctx);
    void (*open_context_menu)(
        void *ctx, int view_index, uint16_t address, int view_count, bool running);
    void (*draw_context_menu)(void *ctx, struct nk_context *nk, memview_pane_state *state);
    float (*char_width)(void *ctx);
} memview_pane_ops;

char memview_pane_ascii(uint8_t value, bool highbit_ascii);
void memview_pane_apply_source(
    memview_pane_view *view,
    const memory_source *table,
    size_t count,
    uint32_t source_id);

void memview_pane_init(memview_pane_state *state, uint32_t source_id, bool highbit_ascii);
void memview_pane_split_at(memview_pane_state *state, int view_index, bool aligned);
void memview_pane_join_at(memview_pane_state *state, int view_index);

void memview_pane_draw(
    struct nk_context *ctx,
    struct nk_rect bounds,
    memview_pane_state *state,
    const memory_source *table,
    size_t count,
    bool running,
    bool paused,
    bool inspecting,
    const memview_pane_ops *ops);

void memview_pane_draw_search(
    struct nk_context *ctx,
    int width,
    int height,
    memview_pane_state *state,
    bool running,
    const memview_pane_ops *ops);

/* Find next/prev in the active view using state->search.pattern. */
bool memview_pane_search_run(
    memview_pane_state *state,
    bool reverse,
    const memview_pane_ops *ops);

#ifdef __cplusplus
}
#endif
