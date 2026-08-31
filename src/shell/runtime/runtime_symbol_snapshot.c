#include "runtime_symbol_snapshot.h"

#include <stdio.h>
#include <string.h>

void runtime_symbol_snapshot_from_table(
    const symbol_table *table,
    runtime_symbol_snapshot *snap)
{
    size_t slot_count;
    size_t i;
    size_t enabled_total = 0;
    size_t entry_copied = 0;
    size_t source_copied = 0;
    size_t record_count;

    if (snap == NULL) {
        return;
    }

    memset(snap, 0, sizeof(*snap));
    if (table == NULL) {
        return;
    }

    slot_count = symbol_table_source_slot_count(table);
    for (i = 0; i < slot_count && source_copied < RUNTIME_SYMBOL_SOURCE_SNAPSHOT_MAX; ++i) {
        symbol_source_kind kind;
        const char *name = NULL;
        bool enabled = false;

        if (symbol_table_get_source_at(table, (uint32_t)i, &kind, &name, &enabled) != SYMBOL_OK) {
            continue;
        }

        snap->sources[source_copied].source_id = (uint32_t)i;
        snap->sources[source_copied].source_kind = (uint8_t)kind;
        snap->sources[source_copied].enabled = enabled ? 1u : 0u;
        snprintf(
            snap->sources[source_copied].source_name,
            sizeof(snap->sources[source_copied].source_name),
            "%s",
            name != NULL ? name : "");
        source_copied++;
    }
    snap->source_count = source_copied;

    record_count = symbol_table_count(table);
    for (i = 0; i < record_count; ++i) {
        symbol_info info;
        symbol_source_kind kind;
        const char *name = NULL;
        bool enabled = false;

        if (symbol_table_get(table, i, &info) != SYMBOL_OK) {
            continue;
        }
        if (symbol_table_get_source_at(table, info.source_id, &kind, &name, &enabled) != SYMBOL_OK ||
            !enabled) {
            continue;
        }

        enabled_total++;
        if (entry_copied >= RUNTIME_SYMBOL_SNAPSHOT_MAX) {
            continue;
        }

        snap->entries[entry_copied].address = info.address;
        snap->entries[entry_copied].source_id = info.source_id;
        snprintf(
            snap->entries[entry_copied].name,
            sizeof(snap->entries[entry_copied].name),
            "%s",
            info.name != NULL ? info.name : "");
        entry_copied++;
    }

    snap->total = enabled_total;
    snap->count = entry_copied;
}

const runtime_symbol_source_snapshot_entry *runtime_symbol_snapshot_find_source(
    const runtime_symbol_snapshot *snap,
    uint32_t source_id)
{
    size_t i;

    if (snap == NULL) {
        return NULL;
    }

    for (i = 0; i < snap->source_count; ++i) {
        if (snap->sources[i].source_id == source_id) {
            return &snap->sources[i];
        }
    }
    return NULL;
}

bool runtime_symbol_snapshot_apply_to_table(
    const runtime_symbol_snapshot *snap,
    symbol_table *table)
{
    size_t i;

    if (snap == NULL || table == NULL) {
        return false;
    }

    symbol_table_clear(table);

    for (i = 0; i < snap->count; ++i) {
        const runtime_symbol_source_snapshot_entry *src =
            runtime_symbol_snapshot_find_source(snap, snap->entries[i].source_id);
        if (src == NULL) {
            continue;
        }
        if (symbol_table_add(
                table,
                snap->entries[i].address,
                snap->entries[i].name,
                (symbol_source_kind)src->source_kind,
                src->source_name,
                true) == SYMBOL_OUT_OF_MEMORY) {
            return false;
        }
    }

    return true;
}
