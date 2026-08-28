#pragma once

#include "control_command_table.h"
#include "control_framing.h"
#include "memory_source.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Product wire identity. Bump when scripts must learn new behaviour. */
#define CONTROL_PROTOCOL_VERSION "A2M/14"
#define CONTROL_PROTOCOL_APP_NAME "a2m"

typedef enum control_command_type {
    CONTROL_COMMAND_NONE = 0,
    CONTROL_COMMAND_HELLO,
    CONTROL_COMMAND_VERSION,
    CONTROL_COMMAND_CAPABILITIES,
    CONTROL_COMMAND_PING,
    CONTROL_COMMAND_QUIT_CLIENT,
    CONTROL_COMMAND_RESET,
    CONTROL_COMMAND_RUN,
    CONTROL_COMMAND_PAUSE,
    CONTROL_COMMAND_STEP_CYCLE,
    CONTROL_COMMAND_STEP_INSTRUCTION,
    CONTROL_COMMAND_STEP_OVER,
    CONTROL_COMMAND_STEP_OUT,
    CONTROL_COMMAND_GET_STATE,
    CONTROL_COMMAND_GET_CPU,
    CONTROL_COMMAND_GET_SOFTSWITCHES,
    CONTROL_COMMAND_GET_MEMORY,
    CONTROL_COMMAND_SET_MEMORY,
    CONTROL_COMMAND_GET_FRAME,
    CONTROL_COMMAND_FRAME_RING_INFO,
    CONTROL_COMMAND_FRAME_RING_RECORD,
    CONTROL_COMMAND_FRAME_RING_CLEAR,
    CONTROL_COMMAND_GET_FRAME_AT,
    CONTROL_COMMAND_SET_REG,
    CONTROL_COMMAND_SET_TURBO,
    CONTROL_COMMAND_BREAK_EXEC,
    CONTROL_COMMAND_BREAK_CLEAR,
    CONTROL_COMMAND_BREAK_CLEAR_ALL,
    CONTROL_COMMAND_BREAK_ENABLE,
    CONTROL_COMMAND_BREAK_LIST,
    CONTROL_COMMAND_BREAK_CREATE,
    CONTROL_COMMAND_BREAK_UPDATE,
    CONTROL_COMMAND_REARM_ONESHOTS,
    CONTROL_COMMAND_WAIT_PAUSED,
    CONTROL_COMMAND_WAIT_RUNNING,
    CONTROL_COMMAND_WAIT_FRAME,
    CONTROL_COMMAND_WAIT_EVENT,
    CONTROL_COMMAND_SAVE_STATE,
    CONTROL_COMMAND_LOAD_STATE,
    CONTROL_COMMAND_KEY,
    CONTROL_COMMAND_MOUNT_DISK,
    CONTROL_COMMAND_MOUNT,
    CONTROL_COMMAND_UNMOUNT,
    CONTROL_COMMAND_SELECT_DISK,
    CONTROL_COMMAND_SET_DISK_WRITABLE,
    CONTROL_COMMAND_HISTORY_INFO,
    CONTROL_COMMAND_HISTORY_RECORD,
    CONTROL_COMMAND_HISTORY_CLEAR,
    CONTROL_COMMAND_HISTORY_FIND,
    CONTROL_COMMAND_HISTORY_NEXT,
    CONTROL_COMMAND_HISTORY_READ,
    CONTROL_COMMAND_HISTORY_CLOSE,
    CONTROL_COMMAND_ASSEMBLE,
    CONTROL_COMMAND_FIND_SYMBOL,
    CONTROL_COMMAND_LEAVE_INSPECTOR,
    CONTROL_COMMAND_ENTER_INSPECTOR
} control_command_type;

/* mount/unmount card selection (0 = infer / resolve uniquely). */
typedef enum control_media_kind {
    CONTROL_MEDIA_KIND_UNSPECIFIED = 0,
    CONTROL_MEDIA_KIND_DISKII = 1,
    CONTROL_MEDIA_KIND_SMARTPORT = 2
} control_media_kind;

typedef struct control_args_memory {
    uint16_t address;
    uint32_t length;
    uint32_t source_id;
} control_args_memory;

typedef struct control_args_set_reg {
    char name[8];
    uint16_t value;
} control_args_set_reg;

typedef struct control_args_turbo {
    uint32_t milli_mhz;
} control_args_turbo;

typedef struct control_args_break {
    uint16_t address;
    uint32_t id;
    uint8_t enable;
    char text[CONTROL_LINE_MAX];
} control_args_break;

typedef struct control_args_wait {
    uint32_t timeout_ms;
    uint32_t frame_delta;
    char event_name[48];
} control_args_wait;

typedef struct control_args_path {
    char path[CONTROL_LINE_MAX];
} control_args_path;

typedef struct control_args_key {
    uint8_t value;
} control_args_key;

typedef struct control_args_media {
    uint8_t slot;
    uint8_t drive;
    uint8_t kind;
    uint32_t disk_index;
    uint8_t writable;
    char path[CONTROL_LINE_MAX];
} control_args_media;

typedef struct control_args_frame_ring {
    uint64_t target;
    bool by_cycle;
    bool record_enabled;
} control_args_frame_ring;

typedef struct control_args_history {
    bool record_enabled;
    uint64_t cursor;
    uint64_t id;
    uint64_t epoch;
    uint16_t limit;
    uint16_t before;
    uint16_t after;
    char find_text[CONTROL_LINE_MAX];
} control_args_history;

typedef struct control_args_assemble {
    uint16_t address;
    uint16_t run_address;
    bool has_run_address;
    bool auto_run;
    bool mli_launch;
    bool reset_first;
    bool auto_adjust_segments;
    char path[CONTROL_LINE_MAX];
} control_args_assemble;

typedef struct control_args_find_symbol {
    char name[CONTROL_LINE_MAX];
} control_args_find_symbol;

typedef union control_verb_args {
    control_args_memory memory;
    control_args_set_reg set_reg;
    control_args_turbo turbo;
    control_args_break brk;
    control_args_wait wait;
    control_args_path path;
    control_args_key key;
    control_args_media media;
    control_args_frame_ring frame_ring;
    control_args_history history;
    control_args_assemble assemble;
    control_args_find_symbol find_symbol;
} control_verb_args;

typedef struct control_request {
    uint32_t id;
    control_command_type type;
    const control_verb *verb;
    control_verb_args args;
    uint8_t *payload;
    size_t payload_size;
} control_request;

bool control_protocol_parse_request(
    const char *line,
    control_request *out_request,
    control_response *out_error);

bool apple2_control_parse_line(
    const char *line,
    control_request *out_request,
    control_response *out_error);

void control_request_release(control_request *request);

const char *control_protocol_memory_mode_name(uint32_t source_id);
void apple2_control_format_capabilities(char *out, size_t out_size);
const memory_source *apple2_memory_sources(size_t *count);
