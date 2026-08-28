#include "control_framing.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws(const char *s)
{
    while (s != NULL && (*s == ' ' || *s == '\t')) {
        s++;
    }
    return s;
}

control_framing_split_status control_framing_split_line(
    const char *line,
    control_framing_line *out)
{
    const char *cursor;
    char *end = NULL;
    unsigned long id;
    size_t i;

    if (out == NULL) {
        return CONTROL_FRAMING_SPLIT_EMPTY;
    }
    memset(out, 0, sizeof(*out));
    if (line == NULL) {
        return CONTROL_FRAMING_SPLIT_EMPTY;
    }

    cursor = skip_ws(line);
    if (*cursor == '\0' || *cursor == '\r' || *cursor == '\n') {
        return CONTROL_FRAMING_SPLIT_EMPTY;
    }
    if (!isdigit((unsigned char)*cursor)) {
        return CONTROL_FRAMING_SPLIT_BAD_ID;
    }
    id = strtoul(cursor, &end, 10);
    if (end == cursor || id > 0xfffffffful) {
        return CONTROL_FRAMING_SPLIT_BAD_ID;
    }
    if (*end != '\0' && *end != '\r' && *end != '\n' &&
        *end != ' ' && *end != '\t') {
        return CONTROL_FRAMING_SPLIT_BAD_ID;
    }
    cursor = skip_ws(end);
    if (*cursor == '\0' || *cursor == '\r' || *cursor == '\n') {
        out->id = (uint32_t)id;
        return CONTROL_FRAMING_SPLIT_MISSING_VERB;
    }

    i = 0;
    while (cursor[i] != '\0' && cursor[i] != '\r' && cursor[i] != '\n' &&
           cursor[i] != ' ' && cursor[i] != '\t' &&
           i + 1 < sizeof(out->verb)) {
        out->verb[i] = cursor[i];
        i++;
    }
    out->verb[i] = '\0';
    if (i == 0) {
        out->id = (uint32_t)id;
        return CONTROL_FRAMING_SPLIT_MISSING_VERB;
    }
    cursor = skip_ws(cursor + i);

    out->id = (uint32_t)id;
    out->rest = cursor;
    return CONTROL_FRAMING_SPLIT_OK;
}

void control_protocol_format_ok(
    control_response *response,
    uint32_t id,
    const char *text,
    bool close_client)
{
    if (response == NULL) {
        return;
    }
    memset(response, 0, sizeof(*response));
    response->id = id;
    response->type = CONTROL_RESPONSE_OK;
    response->close_client = close_client;
    if (text != NULL && text[0] != '\0') {
        snprintf(response->text, sizeof(response->text), "%s", text);
    }
}

void control_protocol_format_error(
    control_response *response,
    uint32_t id,
    const char *code,
    const char *message,
    bool close_client)
{
    if (response == NULL) {
        return;
    }
    memset(response, 0, sizeof(*response));
    response->id = id;
    response->type = CONTROL_RESPONSE_ERROR;
    response->close_client = close_client;
    snprintf(
        response->text,
        sizeof(response->text),
        "%s %s",
        code != NULL ? code : "error",
        message != NULL ? message : "");
}

void control_protocol_format_event(
    control_response *response,
    uint32_t id,
    const char *text)
{
    if (response == NULL) {
        return;
    }
    memset(response, 0, sizeof(*response));
    response->id = id;
    response->type = CONTROL_RESPONSE_EVENT;
    if (text != NULL) {
        snprintf(response->text, sizeof(response->text), "%s", text);
    }
}

void control_protocol_format_data(
    control_response *response,
    uint32_t id,
    const char *data_type,
    const char *metadata,
    uint8_t *payload,
    size_t payload_size,
    bool close_client)
{
    if (response == NULL) {
        return;
    }
    memset(response, 0, sizeof(*response));
    response->id = id;
    response->type = CONTROL_RESPONSE_DATA;
    response->close_client = close_client;
    if (data_type != NULL) {
        snprintf(response->data_type, sizeof(response->data_type), "%s", data_type);
    }
    if (metadata != NULL) {
        snprintf(response->metadata, sizeof(response->metadata), "%s", metadata);
    }
    response->payload = payload;
    response->payload_size = payload_size;
}

void control_protocol_format_hello(
    control_response *response,
    uint32_t id,
    const char *app_name,
    const char *protocol)
{
    char text[CONTROL_RESPONSE_TEXT_MAX];

    snprintf(
        text,
        sizeof(text),
        "name=%s protocol=%s",
        app_name != NULL ? app_name : "",
        protocol != NULL ? protocol : "");
    control_protocol_format_ok(response, id, text, false);
}

void control_protocol_format_version(
    control_response *response,
    uint32_t id,
    const char *protocol,
    const char *app_label)
{
    char text[CONTROL_RESPONSE_TEXT_MAX];

    snprintf(
        text,
        sizeof(text),
        "protocol=%s app=%s",
        protocol != NULL ? protocol : "",
        app_label != NULL ? app_label : "");
    control_protocol_format_ok(response, id, text, false);
}

bool control_protocol_write_response_line(
    char *out,
    size_t out_size,
    const control_response *response)
{
    int n;

    if (out == NULL || out_size == 0 || response == NULL) {
        return false;
    }

    if (response->type == CONTROL_RESPONSE_OK) {
        if (response->text[0] != '\0') {
            n = snprintf(out, out_size, "%u ok %s\n", response->id, response->text);
        } else {
            n = snprintf(out, out_size, "%u ok\n", response->id);
        }
        return n > 0 && (size_t)n < out_size;
    }

    if (response->type == CONTROL_RESPONSE_ERROR) {
        n = snprintf(out, out_size, "%u error %s\n", response->id, response->text);
        return n > 0 && (size_t)n < out_size;
    }

    if (response->type == CONTROL_RESPONSE_EVENT) {
        if (response->text[0] != '\0') {
            n = snprintf(out, out_size, "%u event %s\n", response->id, response->text);
        } else {
            n = snprintf(out, out_size, "%u event\n", response->id);
        }
        return n > 0 && (size_t)n < out_size;
    }

    if (response->type == CONTROL_RESPONSE_DATA) {
        if (response->metadata[0] != '\0') {
            n = snprintf(
                out,
                out_size,
                "%u data %s %zu %s\n",
                response->id,
                response->data_type,
                response->payload_size,
                response->metadata);
        } else {
            n = snprintf(
                out,
                out_size,
                "%u data %s %zu\n",
                response->id,
                response->data_type,
                response->payload_size);
        }
        return n > 0 && (size_t)n < out_size;
    }

    return false;
}

void control_framing_release_payload(uint8_t **payload, size_t *payload_size)
{
    if (payload == NULL) {
        return;
    }
    free(*payload);
    *payload = NULL;
    if (payload_size != NULL) {
        *payload_size = 0;
    }
}

void control_response_release(control_response *response)
{
    if (response == NULL) {
        return;
    }
    control_framing_release_payload(&response->payload, &response->payload_size);
}

platform_socket_listener *control_framing_listen(uint16_t port)
{
    return platform_socket_listen_localhost(port);
}

platform_socket_connection *control_framing_accept(platform_socket_listener *listener)
{
    return platform_socket_accept(listener);
}

int control_framing_read_line_nb(
    platform_socket_connection *connection,
    char *out,
    size_t out_size,
    size_t *used)
{
    if (connection == NULL || out == NULL || out_size == 0 || used == NULL) {
        return -1;
    }

    while (*used + 1 < out_size) {
        char ch;
        int n = platform_socket_read(connection, &ch, 1);
        if (n == -2) {
            return 0;
        }
        if (n <= 0) {
            return -1;
        }
        out[(*used)++] = ch;
        if (ch == '\n') {
            out[*used] = '\0';
            return 1;
        }
    }

    out[*used] = '\0';
    return -1;
}

bool control_framing_read_line(
    platform_socket_connection *connection,
    char *out,
    size_t out_size)
{
    size_t used = 0;
    int rc;

    if (connection == NULL || out == NULL || out_size == 0) {
        return false;
    }
    for (;;) {
        rc = control_framing_read_line_nb(connection, out, out_size, &used);
        if (rc == 1) {
            return true;
        }
        if (rc < 0) {
            return false;
        }
        if (platform_socket_wait_readable(connection, 1000) < 0) {
            return false;
        }
    }
}

bool control_framing_read_exact(
    platform_socket_connection *connection,
    uint8_t *out,
    size_t size)
{
    size_t used = 0;

    if (connection == NULL || (size > 0 && out == NULL)) {
        return false;
    }
    while (used < size) {
        int n = platform_socket_read(connection, out + used, size - used);
        if (n == -2) {
            if (platform_socket_wait_readable(connection, 2000u) <= 0) {
                return false;
            }
            continue;
        }
        if (n <= 0) {
            return false;
        }
        used += (size_t)n;
    }
    return true;
}

bool control_framing_read_payload(
    platform_socket_connection *connection,
    uint8_t **payload,
    size_t payload_size,
    uint32_t request_id,
    control_response *error)
{
    uint8_t newline;

    if (payload == NULL) {
        return false;
    }
    *payload = NULL;
    if (payload_size == 0) {
        return true;
    }
    *payload = (uint8_t *)malloc(payload_size);
    if (*payload == NULL) {
        control_protocol_format_error(
            error, request_id, "memory", "payload allocation failed", false);
        return false;
    }
    if (!control_framing_read_exact(connection, *payload, payload_size) ||
        !control_framing_read_exact(connection, &newline, 1) ||
        newline != '\n') {
        control_framing_release_payload(payload, NULL);
        control_protocol_format_error(
            error, request_id, "bad-payload", "payload framing error", true);
        return false;
    }
    return true;
}

bool control_framing_write_response(
    platform_socket_connection *connection,
    const control_response *response)
{
    char line[CONTROL_LINE_MAX];

    if (!control_protocol_write_response_line(line, sizeof(line), response)) {
        return false;
    }
    if (!platform_socket_write_all(connection, line, strlen(line))) {
        return false;
    }
    if (response->type == CONTROL_RESPONSE_DATA) {
        if (response->payload_size > 0) {
            if (response->payload == NULL) {
                return false;
            }
            if (!platform_socket_write_all(
                    connection, response->payload, response->payload_size)) {
                return false;
            }
        }
        if (!platform_socket_write_all(connection, "\n", 1)) {
            return false;
        }
    }
    return true;
}
