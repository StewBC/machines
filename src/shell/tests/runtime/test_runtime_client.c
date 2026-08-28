#include "runtime_client_subset.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", \
                    __FILE__, __LINE__, #expr); \
            failures++; \
        } \
    } while (0)

struct runtime_client {
    uint32_t last_source_id;
    uint16_t last_address;
    uint32_t last_length;
    uint32_t session_id;
    uint64_t next_token;
    uint64_t last_token;
    int run_calls;
    int pause_calls;
    int inspector_enter_calls;
};

void runtime_client_set_command_session(runtime_client *client, uint32_t session_id)
{
    if (client != NULL) {
        client->session_id = session_id;
    }
}

uint32_t runtime_client_get_command_session(const runtime_client *client)
{
    return (client != NULL) ? client->session_id : 0u;
}

uint64_t runtime_client_alloc_request_token(runtime_client *client)
{
    if (client == NULL) {
        return 0u;
    }
    client->next_token++;
    if (client->next_token == 0u) {
        client->next_token = 1u;
    }
    return client->next_token;
}

bool runtime_client_ping(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_quit(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_reset(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_run(runtime_client *client)
{
    if (client == NULL) {
        return false;
    }
    client->run_calls++;
    return true;
}

bool runtime_client_pause(runtime_client *client)
{
    if (client == NULL) {
        return false;
    }
    client->pause_calls++;
    return true;
}

bool runtime_client_step_cycle(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_step_instruction(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_run_cycles(runtime_client *client, size_t count)
{
    (void)count;
    return client != NULL;
}

bool runtime_client_run_instructions(runtime_client *client, size_t count)
{
    (void)count;
    return client != NULL;
}

bool runtime_client_step_out(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_step_over(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_run_to_cursor(runtime_client *client, uint16_t address)
{
    (void)address;
    return client != NULL;
}

bool runtime_client_request_cpu_state(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_request_cpu_state_token(
    runtime_client *client, uint64_t request_token)
{
    if (client == NULL) {
        return false;
    }
    client->last_token = request_token;
    return true;
}

bool runtime_client_set_pc(runtime_client *client, uint16_t value)
{
    (void)value;
    return client != NULL;
}

bool runtime_client_set_sp(runtime_client *client, uint8_t value)
{
    (void)value;
    return client != NULL;
}

bool runtime_client_set_a(runtime_client *client, uint8_t value)
{
    (void)value;
    return client != NULL;
}

bool runtime_client_set_x(runtime_client *client, uint8_t value)
{
    (void)value;
    return client != NULL;
}

bool runtime_client_set_y(runtime_client *client, uint8_t value)
{
    (void)value;
    return client != NULL;
}

bool runtime_client_set_status(runtime_client *client, uint8_t value)
{
    (void)value;
    return client != NULL;
}

bool runtime_client_request_memory(
    runtime_client *client,
    uint16_t address,
    uint16_t length,
    uint32_t source_id)
{
    return runtime_client_request_memory_token(
        client, address, length, source_id, 0u);
}

bool runtime_client_request_memory_token(
    runtime_client *client,
    uint16_t address,
    uint32_t length,
    uint32_t source_id,
    uint64_t request_token)
{
    if (client == NULL) {
        return false;
    }
    client->last_address = address;
    client->last_length = length;
    client->last_source_id = source_id;
    client->last_token = request_token;
    return true;
}

bool runtime_client_request_memory_view(
    runtime_client *client,
    uint16_t address,
    uint16_t length,
    uint32_t source_id)
{
    return runtime_client_request_memory_token(
        client, address, length, source_id, 0u);
}

bool runtime_client_claim_memory_rpc(
    runtime_client *client,
    uint64_t request_token,
    uint8_t **out_bytes,
    uint32_t *out_length,
    uint16_t *out_address,
    uint32_t *out_source_id)
{
    static uint8_t k_stub_byte = 0xEA;

    if (client == NULL || out_bytes == NULL || request_token == 0u) {
        return false;
    }
    *out_bytes = &k_stub_byte;
    if (out_length != NULL) {
        *out_length = client->last_length;
    }
    if (out_address != NULL) {
        *out_address = client->last_address;
    }
    if (out_source_id != NULL) {
        *out_source_id = client->last_source_id;
    }
    return true;
}

bool runtime_client_write_memory_byte(
    runtime_client *client,
    uint16_t address,
    uint8_t value,
    uint32_t source_id)
{
    (void)value;
    if (client == NULL) {
        return false;
    }
    client->last_address = address;
    client->last_source_id = source_id;
    return true;
}

bool runtime_client_write_memory(
    runtime_client *client,
    uint16_t address,
    uint16_t length,
    uint32_t source_id,
    const uint8_t *bytes)
{
    if (client == NULL || bytes == NULL) {
        return false;
    }
    client->last_address = address;
    client->last_length = length;
    client->last_source_id = source_id;
    return true;
}

bool runtime_client_set_execute_breakpoint(runtime_client *client, uint16_t address)
{
    (void)address;
    return client != NULL;
}

bool runtime_client_duplicate_breakpoint(runtime_client *client, uint32_t id)
{
    (void)id;
    return client != NULL;
}

bool runtime_client_clear_breakpoint(runtime_client *client, uint32_t id)
{
    (void)id;
    return client != NULL;
}

bool runtime_client_clear_all_breakpoints(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_set_breakpoint_enabled(
    runtime_client *client, uint32_t id, bool enabled)
{
    (void)id;
    (void)enabled;
    return client != NULL;
}

bool runtime_client_rearm_oneshot_breakpoints(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_request_breakpoints(runtime_client *client)
{
    return client != NULL;
}

bool runtime_client_claim_history_rpc(
    runtime_client *client,
    uint64_t request_token,
    uint8_t **out_bytes,
    uint32_t *out_length,
    runtime_history_rpc_meta *out_meta)
{
    (void)request_token;
    (void)out_bytes;
    (void)out_length;
    (void)out_meta;
    return client != NULL;
}

bool runtime_client_cancel_rpc(runtime_client *client, uint64_t request_token)
{
    (void)request_token;
    return client != NULL;
}

bool runtime_client_history_info(runtime_client *client, uint64_t request_token)
{
    (void)request_token;
    return client != NULL;
}

bool runtime_client_history_record(
    runtime_client *client, bool enabled, uint64_t request_token)
{
    (void)enabled;
    (void)request_token;
    return client != NULL;
}

bool runtime_client_history_clear(runtime_client *client, uint64_t request_token)
{
    (void)request_token;
    return client != NULL;
}

bool runtime_client_history_find(
    runtime_client *client,
    uint32_t session_id,
    const runtime_history_query *query,
    runtime_history_from_kind from_kind,
    uint64_t from_id,
    uint16_t limit,
    uint64_t request_token)
{
    (void)session_id;
    (void)query;
    (void)from_kind;
    (void)from_id;
    (void)limit;
    (void)request_token;
    return client != NULL;
}

bool runtime_client_history_next(
    runtime_client *client,
    uint32_t session_id,
    uint64_t cursor,
    uint16_t limit,
    uint64_t request_token)
{
    (void)session_id;
    (void)cursor;
    (void)limit;
    (void)request_token;
    return client != NULL;
}

bool runtime_client_history_read(
    runtime_client *client,
    uint32_t session_id,
    uint64_t epoch,
    uint64_t id,
    uint16_t before,
    uint16_t after,
    uint64_t request_token)
{
    (void)session_id;
    (void)epoch;
    (void)id;
    (void)before;
    (void)after;
    (void)request_token;
    return client != NULL;
}

bool runtime_client_history_close(
    runtime_client *client,
    uint32_t session_id,
    uint64_t cursor,
    uint64_t request_token)
{
    (void)session_id;
    (void)cursor;
    (void)request_token;
    return client != NULL;
}

bool runtime_client_inspector_set_enabled(
    runtime_client *client, bool enabled, uint64_t request_token)
{
    (void)enabled;
    (void)request_token;
    return client != NULL;
}

bool runtime_client_inspector_enter(runtime_client *client, uint64_t request_token)
{
    if (client == NULL) {
        return false;
    }
    client->last_token = request_token;
    client->inspector_enter_calls++;
    return true;
}

bool runtime_client_inspector_leave(runtime_client *client, uint64_t request_token)
{
    (void)request_token;
    return client != NULL;
}

bool runtime_client_inspector_land(
    runtime_client *client, uint64_t cycle, uint64_t request_token)
{
    (void)cycle;
    (void)request_token;
    return client != NULL;
}

bool runtime_client_inspector_land_to_cycle(
    runtime_client *client, uint64_t cycle, uint64_t request_token)
{
    (void)cycle;
    (void)request_token;
    return client != NULL;
}

int main(void)
{
    runtime_client client;
    runtime_history_query query;
    uint8_t *bytes = NULL;
    uint32_t length = 0;
    uint16_t address = 0;
    uint32_t source_id = 0;
    uint8_t poke[2] = { 0xA9, 0x00 };
    uint64_t token;

    memset(&client, 0, sizeof(client));
    memset(&query, 0, sizeof(query));

    CHECK(!runtime_client_run(NULL));
    CHECK(!runtime_client_pause(NULL));
    CHECK(!runtime_client_request_memory(NULL, 0, 1, 0u));
    CHECK(runtime_client_run(&client));
    CHECK(runtime_client_pause(&client));
    CHECK(client.run_calls == 1);
    CHECK(client.pause_calls == 1);

    CHECK(runtime_client_request_cpu_state(&client));
    CHECK(runtime_client_set_pc(&client, 0xC000));
    CHECK(runtime_client_set_execute_breakpoint(&client, 0xC000));
    CHECK(runtime_client_request_breakpoints(&client));
    CHECK(runtime_client_clear_breakpoint(&client, 1u));

    CHECK(runtime_client_request_memory(&client, 0x0300, 4u, 3u));
    CHECK(client.last_source_id == 3u);
    CHECK(client.last_address == 0x0300);
    CHECK(runtime_client_write_memory_byte(&client, 0xD000, 0x60u, 0u));
    CHECK(client.last_source_id == 0u);
    CHECK(runtime_client_write_memory(&client, 0x0300, 2u, 1u, poke));
    CHECK(client.last_source_id == 1u);

    token = runtime_client_alloc_request_token(&client);
    CHECK(token != 0u);
    CHECK(runtime_client_request_memory_token(
        &client, 0x0400, 16u, 4u, token));
    CHECK(runtime_client_claim_memory_rpc(
        &client, token, &bytes, &length, &address, &source_id));
    CHECK(bytes != NULL);
    CHECK(length == 16u);
    CHECK(address == 0x0400);
    CHECK(source_id == 4u);

    CHECK(runtime_client_history_find(
        &client, 0u, &query, RUNTIME_HISTORY_FROM_DEFAULT, 0u, 1u, token));
    CHECK(runtime_client_inspector_enter(&client, token));
    CHECK(runtime_client_inspector_land(&client, 100u, token));
    CHECK(runtime_client_inspector_leave(&client, token));
    CHECK(client.inspector_enter_calls == 1);

    if (failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    return 0;
}
