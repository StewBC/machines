#pragma once

#include "nuklear_config.h"
#include "runtime_symbol_snapshot.h"
#include "symbol_table.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    SYMBOL_LOOKUP_ENTRY_MAX  = 4096,
    SYMBOL_LOOKUP_SEARCH_MAX = 128,
    SYMBOL_LOOKUP_COL_MAX    = 32,
    /* Match RUNTIME_SYMBOL_SOURCE_SNAPSHOT_MAX. */
    SYMBOL_FILTER_SOURCE_MAX = 64
};

typedef enum frontend_symbol_lookup_sort_col {
    SYMBOL_LOOKUP_SORT_ADDR   = 0,
    SYMBOL_LOOKUP_SORT_SCOPE  = 1,
    SYMBOL_LOOKUP_SORT_LABEL  = 2,
    SYMBOL_LOOKUP_SORT_SOURCE = 3
} frontend_symbol_lookup_sort_col;

typedef struct frontend_symbol_lookup_entry {
    uint16_t address;
    char     scope [SYMBOL_LOOKUP_COL_MAX + 1];
    char     label [SYMBOL_LOOKUP_COL_MAX + 1];
    char     source[SYMBOL_LOOKUP_COL_MAX + 1];
} frontend_symbol_lookup_entry;

/* Chrome-owned Filter row: raw source_id + basename label + enabled mirror. */
typedef struct symbol_filter_source_row {
    uint32_t source_id;
    bool     enabled;
    char     label[SYMBOL_LOOKUP_COL_MAX + 1];
} symbol_filter_source_row;

typedef struct frontend_symbol_lookup_state {
    bool                            open;
    bool                            from_memory;
    char                            search[SYMBOL_LOOKUP_SEARCH_MAX];
    frontend_symbol_lookup_entry    entries[SYMBOL_LOOKUP_ENTRY_MAX];
    int                             entry_count;
    int                             filtered[SYMBOL_LOOKUP_ENTRY_MAX];
    int                             filtered_count;
    frontend_symbol_lookup_sort_col sort_col;
    bool                            sort_asc;
    int                             selected;
    bool                            table_has_kb_focus;
    bool                            scroll_to_selected;
    bool                            just_opened;
    /* Sibling Symbol Filter window (drawn after Lookup). */
    bool                            filter_open;
    symbol_filter_source_row        filter_sources[SYMBOL_FILTER_SOURCE_MAX];
    size_t                          filter_source_count;
} frontend_symbol_lookup_state;

typedef struct symbol_lookup_ops {
    void *ctx;
    void (*jump_disasm)(void *ctx, uint16_t address);
    void (*jump_memory)(void *ctx, uint16_t address);
    void (*set_source_enabled)(void *ctx, uint32_t source_id, bool enabled);
} symbol_lookup_ops;

void symbol_lookup_view_init(frontend_symbol_lookup_state *state);
void symbol_lookup_view_open(
    frontend_symbol_lookup_state *state,
    const symbol_table *table,
    bool from_memory);
void symbol_lookup_view_close(frontend_symbol_lookup_state *state);
bool symbol_lookup_view_is_open(const frontend_symbol_lookup_state *state);
bool symbol_lookup_view_filter_is_open(const frontend_symbol_lookup_state *state);
bool symbol_lookup_view_any_open(const frontend_symbol_lookup_state *state);

void symbol_lookup_view_rebuild_entries(
    frontend_symbol_lookup_state *state,
    const symbol_table *table);

/* Refresh chrome Filter cache from a polled snapshot sources[] list.
 * Uses raw source_id values only (never UI-local table ids). */
void symbol_lookup_view_set_sources(
    frontend_symbol_lookup_state *state,
    const runtime_symbol_source_snapshot_entry *sources,
    size_t source_count);

/* Basename without extension for SOURCE / Filter labels. */
void symbol_lookup_view_basename(const char *src, char *out, int max);

/* ESC closes Filter first, then Lookup. Tab / Up / Down / Enter for Lookup.
 * Returns true if consumed. */
bool symbol_lookup_view_handle_key(
    frontend_symbol_lookup_state *state,
    const symbol_lookup_ops *ops,
    SDL_Keycode key);

/* Draws Lookup, then Filter (File Browser stacking precedent). */
void symbol_lookup_view_render(
    struct nk_context *ctx,
    frontend_symbol_lookup_state *state,
    const symbol_lookup_ops *ops,
    int width,
    int height);

#ifdef __cplusplus
}
#endif
