#pragma once

#include "platform_socket.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CONTROL_LINE_MAX = 512,
    CONTROL_RESPONSE_TEXT_MAX = 512,
    CONTROL_FRAMING_VERB_MAX = 64
};

typedef enum control_response_type {
    CONTROL_RESPONSE_OK = 0,
    CONTROL_RESPONSE_ERROR,
    CONTROL_RESPONSE_DATA,
    /* Unsolicited out-of-band line: "<id> event <name> [fields...]".
       Request id 0 is the reserved event channel. */
    CONTROL_RESPONSE_EVENT
} control_response_type;

typedef struct control_response {
    uint32_t id;
    control_response_type type;
    char text[CONTROL_RESPONSE_TEXT_MAX];
    char data_type[32];
    char metadata[CONTROL_RESPONSE_TEXT_MAX];
    uint8_t *payload;
    size_t payload_size;
    bool close_client;
} control_response;

typedef struct control_framing_line {
    uint32_t id;
    char verb[CONTROL_FRAMING_VERB_MAX];
    const char *rest;
} control_framing_line;

typedef enum control_framing_split_status {
    CONTROL_FRAMING_SPLIT_OK = 0,
    CONTROL_FRAMING_SPLIT_EMPTY,
    CONTROL_FRAMING_SPLIT_BAD_ID,
    CONTROL_FRAMING_SPLIT_MISSING_VERB
} control_framing_split_status;

control_framing_split_status control_framing_split_line(
    const char *line,
    control_framing_line *out);

void control_protocol_format_ok(
    control_response *response,
    uint32_t id,
    const char *text,
    bool close_client);

void control_protocol_format_error(
    control_response *response,
    uint32_t id,
    const char *code,
    const char *message,
    bool close_client);

void control_protocol_format_event(
    control_response *response,
    uint32_t id,
    const char *text);

void control_protocol_format_data(
    control_response *response,
    uint32_t id,
    const char *data_type,
    const char *metadata,
    uint8_t *payload,
    size_t payload_size,
    bool close_client);

void control_protocol_format_hello(
    control_response *response,
    uint32_t id,
    const char *app_name,
    const char *protocol);

void control_protocol_format_version(
    control_response *response,
    uint32_t id,
    const char *protocol,
    const char *app_label);

bool control_protocol_write_response_line(
    char *out,
    size_t out_size,
    const control_response *response);

void control_response_release(control_response *response);
void control_framing_release_payload(uint8_t **payload, size_t *payload_size);

platform_socket_listener *control_framing_listen(uint16_t port);
platform_socket_connection *control_framing_accept(platform_socket_listener *listener);

/* 1=complete, 0=would-block, -1=error/eof/too long. *used is partial length. */
int control_framing_read_line_nb(
    platform_socket_connection *connection,
    char *out,
    size_t out_size,
    size_t *used);

bool control_framing_read_line(
    platform_socket_connection *connection,
    char *out,
    size_t out_size);

bool control_framing_read_exact(
    platform_socket_connection *connection,
    uint8_t *out,
    size_t size);

bool control_framing_read_payload(
    platform_socket_connection *connection,
    uint8_t **payload,
    size_t payload_size,
    uint32_t request_id,
    control_response *error);

bool control_framing_write_response(
    platform_socket_connection *connection,
    const control_response *response);
