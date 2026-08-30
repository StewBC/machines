#pragma once

/*
 * Shared runtime_client subset (Stage 7). No apple2.h / c64.h.
 * Named *_subset.h so leftover quoted "runtime_client.h" still finds leftover extras.
 * Picture poll, keys, media, and Inspector catalog/film stay leftover.
 * Implementations live in leftover runtime_client.c (command enums diverge).
 */

#include "runtime_history.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef RUNTIME_CLIENT_DEFINED
#define RUNTIME_CLIENT_DEFINED
typedef struct runtime_client runtime_client;
#endif

void runtime_client_set_command_session(runtime_client *client, uint32_t session_id);
uint64_t runtime_client_alloc_request_token(runtime_client *client);

bool runtime_client_ping(runtime_client *client);
bool runtime_client_quit(runtime_client *client);
bool runtime_client_reset(runtime_client *client);
bool runtime_client_run(runtime_client *client);
bool runtime_client_pause(runtime_client *client);
bool runtime_client_step_cycle(runtime_client *client);
bool runtime_client_step_instruction(runtime_client *client);
bool runtime_client_run_cycles(runtime_client *client, size_t count);
bool runtime_client_run_instructions(runtime_client *client, size_t count);
bool runtime_client_step_out(runtime_client *client);
bool runtime_client_step_over(runtime_client *client);
bool runtime_client_run_to_cursor(runtime_client *client, uint16_t address);

bool runtime_client_request_cpu_state(runtime_client *client);
bool runtime_client_request_cpu_state_token(runtime_client *client, uint64_t request_token);
bool runtime_client_set_pc(runtime_client *client, uint16_t value);
bool runtime_client_set_sp(runtime_client *client, uint8_t value);
bool runtime_client_set_a(runtime_client *client, uint8_t value);
bool runtime_client_set_x(runtime_client *client, uint8_t value);
bool runtime_client_set_y(runtime_client *client, uint8_t value);
bool runtime_client_set_status(runtime_client *client, uint8_t value);

bool runtime_client_request_memory(
    runtime_client *client,
    uint16_t address,
    uint16_t length,
    uint32_t source_id);
bool runtime_client_request_memory_token(
    runtime_client *client,
    uint16_t address,
    uint32_t length,
    uint32_t source_id,
    uint64_t request_token);
bool runtime_client_request_memory_view(
    runtime_client *client,
    uint16_t address,
    uint16_t length,
    uint32_t source_id);
bool runtime_client_claim_memory_rpc(
    runtime_client *client,
    uint64_t request_token,
    uint8_t **out_bytes,
    uint32_t *out_length,
    uint16_t *out_address,
    uint32_t *out_source_id);
bool runtime_client_write_memory_byte(
    runtime_client *client,
    uint16_t address,
    uint8_t value,
    uint32_t source_id);
bool runtime_client_write_memory(
    runtime_client *client,
    uint16_t address,
    uint16_t length,
    uint32_t source_id,
    const uint8_t *bytes);

bool runtime_client_set_execute_breakpoint(runtime_client *client, uint16_t address);
bool runtime_client_duplicate_breakpoint(runtime_client *client, uint32_t id);
bool runtime_client_clear_breakpoint(runtime_client *client, uint32_t id);
bool runtime_client_clear_all_breakpoints(runtime_client *client);
bool runtime_client_set_breakpoint_enabled(runtime_client *client, uint32_t id, bool enabled);
bool runtime_client_rearm_oneshot_breakpoints(runtime_client *client);
bool runtime_client_request_breakpoints(runtime_client *client);

bool runtime_client_claim_history_rpc(
    runtime_client *client,
    uint64_t request_token,
    uint8_t **out_bytes,
    uint32_t *out_length,
    runtime_history_rpc_meta *out_meta);
bool runtime_client_cancel_rpc(runtime_client *client, uint64_t request_token);
bool runtime_client_history_info(runtime_client *client, uint64_t request_token);
bool runtime_client_history_record(
    runtime_client *client, bool enabled, uint64_t request_token);
bool runtime_client_history_clear(runtime_client *client, uint64_t request_token);
bool runtime_client_history_find(
    runtime_client *client,
    uint32_t session_id,
    const runtime_history_query *query,
    runtime_history_from_kind from_kind,
    uint64_t from_id,
    uint16_t limit,
    uint64_t request_token);
bool runtime_client_history_next(
    runtime_client *client,
    uint32_t session_id,
    uint64_t cursor,
    uint16_t limit,
    uint64_t request_token);
bool runtime_client_history_read(
    runtime_client *client,
    uint32_t session_id,
    uint64_t epoch,
    uint64_t id,
    uint16_t before,
    uint16_t after,
    uint64_t request_token);
bool runtime_client_history_close(
    runtime_client *client,
    uint32_t session_id,
    uint64_t cursor,
    uint64_t request_token);

bool runtime_client_inspector_set_enabled(
    runtime_client *client, bool enabled, uint64_t request_token);
bool runtime_client_inspector_enter(runtime_client *client, uint64_t request_token);
bool runtime_client_inspector_leave(runtime_client *client, uint64_t request_token);
bool runtime_client_inspector_land(
    runtime_client *client, uint64_t cycle, uint64_t request_token);
bool runtime_client_inspector_land_to_cycle(
    runtime_client *client, uint64_t cycle, uint64_t request_token);
bool runtime_client_inspector_step(
    runtime_client *client, int direction, uint64_t request_token);
