#include "runtime_symbol_snapshot.h"
#include "symbol_table.h"

#include <stdio.h>
#include <string.h>

static int expect_true(int cond, const char *label)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", label);
        return 1;
    }
    return 0;
}

static int test_enabled_view_and_tombstone_join(void)
{
    int failures = 0;
    symbol_table *table;
    runtime_symbol_snapshot snap;
    symbol_table *ui_table;
    symbol_info info;
    uint32_t first_id = 0;
    uint32_t second_id = 0;
    const runtime_symbol_source_snapshot_entry *found;
    const runtime_symbol_source_snapshot_entry *wrong;

    table = symbol_table_create();
    ui_table = symbol_table_create();
    if (table == NULL || ui_table == NULL) {
        fprintf(stderr, "symbol_table_create failed\n");
        symbol_table_destroy(table);
        symbol_table_destroy(ui_table);
        return 1;
    }

    failures += expect_true(
        symbol_table_add(table, 0x1000, "FIRST", SYMBOL_SOURCE_FILE, "first.sym", false) ==
            SYMBOL_OK,
        "add first");
    failures += expect_true(
        symbol_table_add(table, 0x2000, "SECOND", SYMBOL_SOURCE_FILE, "second.sym", false) ==
            SYMBOL_OK,
        "add second");
    failures += expect_true(
        symbol_table_find_source_id(table, SYMBOL_SOURCE_FILE, "first.sym", &first_id) ==
            SYMBOL_OK,
        "find first id");
    failures += expect_true(
        symbol_table_find_source_id(table, SYMBOL_SOURCE_FILE, "second.sym", &second_id) ==
            SYMBOL_OK,
        "find second id");
    failures += expect_true(first_id == 0 && second_id == 1, "initial slot ids 0 and 1");

    /* Tombstone slot 0 so dense sources[0] is table slot 1 (second), not 0. */
    failures += expect_true(
        symbol_table_remove_source(table, SYMBOL_SOURCE_FILE, "first.sym") == SYMBOL_OK,
        "tombstone first");
    failures += expect_true(
        symbol_table_add(table, 0x3000, "THIRD", SYMBOL_SOURCE_ASSEMBLER, "prog", false) ==
            SYMBOL_OK,
        "add third into tombstone slot 0");

    failures += expect_true(
        symbol_table_set_source_enabled_at(table, second_id, false) == SYMBOL_OK,
        "disable second");

    runtime_symbol_snapshot_from_table(table, &snap);

    failures += expect_true(snap.source_count == 2, "two live sources in snapshot");
    failures += expect_true(snap.total == 1, "enabled-view total is one");
    failures += expect_true(snap.count == 1, "enabled-view count is one");
    failures += expect_true(
        snap.entries[0].address == 0x3000 && strcmp(snap.entries[0].name, "THIRD") == 0,
        "only enabled THIRD published");
    failures += expect_true(snap.entries[0].source_id == 0, "THIRD reuses raw slot 0");
    failures += expect_true(
        snap.sources[0].source_id == 0 &&
            strcmp(snap.sources[0].source_name, "prog") == 0,
        "dense[0] is prog at raw id 0");
    failures += expect_true(
        snap.sources[1].source_id == 1 &&
            strcmp(snap.sources[1].source_name, "second.sym") == 0 &&
            snap.sources[1].enabled == 0,
        "dense[1] is disabled second at raw id 1");

    found = runtime_symbol_snapshot_find_source(&snap, snap.entries[0].source_id);
    failures += expect_true(found != NULL && strcmp(found->source_name, "prog") == 0,
        "join-by-id finds prog");

    /* Anti-pattern: treating raw source_id as dense subscript would still
     * happen to work for id 0 here. Disable SECOND already proved dense[1]
     * has raw id 1; find id 1 must not be &sources[0]. */
    found = runtime_symbol_snapshot_find_source(&snap, 1u);
    wrong = &snap.sources[0];
    failures += expect_true(found != NULL && found != wrong,
        "raw id 1 is not dense[0]");
    failures += expect_true(
        found != NULL && strcmp(found->source_name, "second.sym") == 0,
        "raw id 1 joins to second.sym");

    failures += expect_true(
        runtime_symbol_snapshot_apply_to_table(&snap, ui_table),
        "apply to ui table");
    failures += expect_true(
        symbol_table_find_by_name(ui_table, "THIRD", &info) == SYMBOL_OK,
        "ui resolves THIRD");
    failures += expect_true(
        symbol_table_find_by_name(ui_table, "SECOND", &info) == SYMBOL_NOT_FOUND,
        "ui omits disabled SECOND");
    failures += expect_true(
        symbol_table_find_by_name(ui_table, "FIRST", &info) == SYMBOL_NOT_FOUND,
        "ui omits tombstoned FIRST");
    failures += expect_true(
        symbol_table_get(ui_table, 0, &info) == SYMBOL_OK &&
            info.source_kind == SYMBOL_SOURCE_ASSEMBLER &&
            info.source_name != NULL &&
            strcmp(info.source_name, "prog") == 0,
        "truthful source name on ingest");

    symbol_table_destroy(table);
    symbol_table_destroy(ui_table);
    return failures;
}

int main(void)
{
    int failures = test_enabled_view_and_tombstone_join();
    return failures == 0 ? 0 : 1;
}
