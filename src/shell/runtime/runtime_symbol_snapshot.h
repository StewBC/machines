#pragma once

/*
 * Shared live debugger symbol handoff (runtime thread -> UI / control).
 * Do not include product runtime_command.h from here.
 */

#include "symbol_table.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    RUNTIME_SYMBOL_NAME_MAX            = 64,
    RUNTIME_SYMBOL_SNAPSHOT_MAX        = 4096,
    RUNTIME_SYMBOL_SOURCE_SNAPSHOT_MAX = 64,
    /* Full path identity; keep equal to product RUNTIME_COMMAND_PATH_MAX. */
    RUNTIME_SYMBOL_SOURCE_NAME_MAX     = 1024
};

typedef struct runtime_symbol_snapshot_entry {
    uint16_t address;
    char name[RUNTIME_SYMBOL_NAME_MAX];
    uint32_t source_id; /* raw table->sources[] slot id (not dense index) */
} runtime_symbol_snapshot_entry;

typedef struct runtime_symbol_source_snapshot_entry {
    uint32_t source_id; /* raw table slot id */
    uint8_t source_kind; /* symbol_source_kind */
    char source_name[RUNTIME_SYMBOL_SOURCE_NAME_MAX];
    uint8_t enabled; /* 0/1 */
} runtime_symbol_source_snapshot_entry;

typedef struct runtime_symbol_snapshot {
    size_t count;  /* enabled symbols copied into entries[] (capped) */
    size_t total;  /* enabled symbol count before cap */
    runtime_symbol_snapshot_entry entries[RUNTIME_SYMBOL_SNAPSHOT_MAX];
    size_t source_count; /* non-tombstone sources copied (capped) */
    runtime_symbol_source_snapshot_entry sources[RUNTIME_SYMBOL_SOURCE_SNAPSHOT_MAX];
} runtime_symbol_snapshot;

/* Fill snap from table: dense sources[] (all live), entries[] (enabled only). */
void runtime_symbol_snapshot_from_table(
    const symbol_table *table,
    runtime_symbol_snapshot *snap);

/* Scan dense sources[] for raw source_id. NULL if missing. */
const runtime_symbol_source_snapshot_entry *runtime_symbol_snapshot_find_source(
    const runtime_symbol_snapshot *snap,
    uint32_t source_id);

/* Clear and rebuild table from enabled snapshot entries (join by source_id). */
bool runtime_symbol_snapshot_apply_to_table(
    const runtime_symbol_snapshot *snap,
    symbol_table *table);
