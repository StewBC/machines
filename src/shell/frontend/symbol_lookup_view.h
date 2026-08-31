#pragma once

#include "nuklear_config.h"
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
    SYMBOL_LOOKUP_COL_MAX    = 32
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
} frontend_symbol_lookup_state;

typedef struct symbol_lookup_ops {
    void *ctx;
    void (*jump_disasm)(void *ctx, uint16_t address);
    void (*jump_memory)(void *ctx, uint16_t address);
    /* Optional until Filter lands; may be NULL. */
    void (*set_source_enabled)(void *ctx, uint32_t source_id, bool enabled);
} symbol_lookup_ops;

void symbol_lookup_view_init(frontend_symbol_lookup_state *state);
void symbol_lookup_view_open(
    frontend_symbol_lookup_state *state,
    const symbol_table *table,
    bool from_memory);
void symbol_lookup_view_close(frontend_symbol_lookup_state *state);
bool symbol_lookup_view_is_open(const frontend_symbol_lookup_state *state);

void symbol_lookup_view_rebuild_entries(
    frontend_symbol_lookup_state *state,
    const symbol_table *table);

/* Basename without extension for SOURCE / Filter labels. */
void symbol_lookup_view_basename(const char *src, char *out, int max);

/* ESC / Tab / Up / Down / Enter. Returns true if consumed. */
bool symbol_lookup_view_handle_key(
    frontend_symbol_lookup_state *state,
    const symbol_lookup_ops *ops,
    SDL_Keycode key);

void symbol_lookup_view_render(
    struct nk_context *ctx,
    frontend_symbol_lookup_state *state,
    const symbol_lookup_ops *ops,
    int width,
    int height);

#ifdef __cplusplus
}
#endif
