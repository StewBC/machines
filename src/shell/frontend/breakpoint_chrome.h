#pragma once

#include "nuklear_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct breakpoint_chrome_row {
    uint32_t id;
    uint16_t start_address;
    uint16_t end_address;
    uint8_t has_end_address;
    uint8_t enabled;
    uint8_t use_counter;
    uint32_t access;
    uint32_t current_hits;
    uint32_t counter;
} breakpoint_chrome_row;

typedef struct breakpoint_mapping_axis {
    const char *label;
    const char *const *options;
    size_t option_count;
    int *value;
} breakpoint_mapping_axis;

typedef struct breakpoint_chrome_ops {
    void *ctx;
    void (*on_new)(void *ctx);
    void (*on_edit)(void *ctx, uint32_t id);
    void (*on_duplicate)(void *ctx, uint32_t id);
    void (*on_set_enabled)(void *ctx, uint32_t id, bool enabled);
    void (*on_clear)(void *ctx, uint32_t id);
    void (*on_clear_all)(void *ctx);
    void (*on_view_pc)(void *ctx, uint16_t address);
} breakpoint_chrome_ops;

void breakpoint_chrome_draw_list(
    struct nk_context *ctx,
    const breakpoint_chrome_row *rows,
    uint16_t count,
    const breakpoint_chrome_ops *ops);

#ifdef __cplusplus
}
#endif
