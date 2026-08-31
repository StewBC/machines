#include "control_verbs.h"

#include <string.h>

typedef struct c64_control_verb {
    control_verb verb;
    control_command_type type;
} c64_control_verb;

static const memory_source k_c64_sources[] = {
    { 0u, "CPU map", "map", 0u, 0x10000u, MEMSRC_WRITABLE },
    { 1u, "RAM", "ram", 0u, 0x10000u, MEMSRC_WRITABLE },
    { 2u, "ROM", "rom", 0u, 0x10000u, 0u },
    { 3u, "Drive 8", "drive8", 0u, 0x10000u, MEMSRC_FOREIGN_BUS },
    { 4u, "Drive 9", "drive9", 0u, 0x10000u, MEMSRC_FOREIGN_BUS }
};

const memory_source *c64_memory_sources(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(k_c64_sources) / sizeof(k_c64_sources[0]);
    }
    return k_c64_sources;
}

static const c64_control_verb k_c64_verbs[] = {
    { { "hello", "connection", NULL, NULL }, CONTROL_COMMAND_HELLO },
    { { "version", "introspection", NULL, NULL }, CONTROL_COMMAND_VERSION },
    { { "capabilities", "introspection", NULL, NULL }, CONTROL_COMMAND_CAPABILITIES },
    { { "ping", "connection", NULL, NULL }, CONTROL_COMMAND_PING },
    { { "quit-client", "connection", NULL, NULL }, CONTROL_COMMAND_QUIT_CLIENT },
    { { "reset", "execution", NULL, NULL }, CONTROL_COMMAND_RESET },
    { { "run", "execution", NULL, NULL }, CONTROL_COMMAND_RUN },
    { { "pause", "execution", NULL, NULL }, CONTROL_COMMAND_PAUSE },
    { { "run-cycles", "execution", NULL, NULL }, CONTROL_COMMAND_RUN_CYCLES },
    { { "run-instructions", "execution", NULL, NULL }, CONTROL_COMMAND_RUN_INSTRUCTIONS },
    { { "run-to", "execution", NULL, NULL }, CONTROL_COMMAND_RUN_TO },
    { { "step-frame", "execution", NULL, NULL }, CONTROL_COMMAND_STEP_FRAME },
    { { "get-state", "state", NULL, NULL }, CONTROL_COMMAND_GET_STATE },
    { { "get-cpu", "introspection", NULL, NULL }, CONTROL_COMMAND_GET_CPU },
    { { "step-cycle", "step", NULL, NULL }, CONTROL_COMMAND_STEP_CYCLE },
    { { "step-instruction", "step", NULL, NULL }, CONTROL_COMMAND_STEP_INSTRUCTION },
    { { "step-over", "step", NULL, NULL }, CONTROL_COMMAND_STEP_OVER },
    { { "step-out", "step", NULL, NULL }, CONTROL_COMMAND_STEP_OUT },
    { { "set-turbo", "turbo", NULL, NULL }, CONTROL_COMMAND_SET_TURBO },
    { { "get-frame", "frame", NULL, NULL }, CONTROL_COMMAND_GET_FRAME },
    { { "get-frame-at", "frame", NULL, NULL }, CONTROL_COMMAND_GET_FRAME_AT },
    { { "get-memory", "memory", NULL, NULL }, CONTROL_COMMAND_GET_MEMORY },
    { { "set-memory", "memory", NULL, NULL }, CONTROL_COMMAND_SET_MEMORY },
    { { "get-debug-memory", "debug-memory", NULL, NULL }, CONTROL_COMMAND_GET_DEBUG_MEMORY },
    { { "get-call-stack", "call-stack", NULL, NULL }, CONTROL_COMMAND_GET_CALL_STACK },
    { { "key-down", "input", NULL, NULL }, CONTROL_COMMAND_KEY_DOWN },
    { { "key-up", "input", NULL, NULL }, CONTROL_COMMAND_KEY_UP },
    { { "restore", "input", NULL, NULL }, CONTROL_COMMAND_RESTORE },
    { { "joystick", "input", NULL, NULL }, CONTROL_COMMAND_JOYSTICK },
    { { "paste-text", "input", NULL, NULL }, CONTROL_COMMAND_PASTE_TEXT },
    { { "paste-events", "input", NULL, NULL }, CONTROL_COMMAND_PASTE_EVENTS },
    { { "paste-text-data", "input", NULL, NULL }, CONTROL_COMMAND_PASTE_TEXT_DATA },
    { { "paste-events-data", "input", NULL, NULL }, CONTROL_COMMAND_PASTE_EVENTS_DATA },
    { { "mount-d64", "disk", NULL, NULL }, CONTROL_COMMAND_MOUNT_D64 },
    { { "unmount-disk", "disk", NULL, NULL }, CONTROL_COMMAND_UNMOUNT_DISK },
    { { "get-disk-status", "disk", NULL, NULL }, CONTROL_COMMAND_GET_DISK_STATUS },
    { { "load-prg", "file", NULL, NULL }, CONTROL_COMMAND_LOAD_PRG },
    { { "load-bin", "file", NULL, NULL }, CONTROL_COMMAND_LOAD_BIN },
    { { "save-bin", "file", NULL, NULL }, CONTROL_COMMAND_SAVE_BIN },
    { { "save-state", "snapshot", NULL, NULL }, CONTROL_COMMAND_SAVE_STATE },
    { { "load-state", "snapshot", NULL, NULL }, CONTROL_COMMAND_LOAD_STATE },
    { { "break-exec", "breakpoints", NULL, NULL }, CONTROL_COMMAND_BREAK_EXEC },
    { { "break-clear", "breakpoints", NULL, NULL }, CONTROL_COMMAND_BREAK_CLEAR },
    { { "break-enable", "breakpoints", NULL, NULL }, CONTROL_COMMAND_BREAK_ENABLE },
    { { "break-list", "breakpoints", NULL, NULL }, CONTROL_COMMAND_BREAK_LIST },
    { { "get-breakpoints", NULL, NULL, NULL }, CONTROL_COMMAND_BREAK_LIST },
    { { "break-clear-all", "breakpoints", NULL, NULL }, CONTROL_COMMAND_BREAK_CLEAR_ALL },
    { { "break-create", "breakpoints", NULL, NULL }, CONTROL_COMMAND_BREAK_CREATE },
    { { "break-update", "breakpoints", NULL, NULL }, CONTROL_COMMAND_BREAK_UPDATE },
    { { "rearm-oneshots", "breakpoints", NULL, NULL }, CONTROL_COMMAND_REARM_ONESHOTS },
    { { "wait-paused", "wait", NULL, NULL }, CONTROL_COMMAND_WAIT_PAUSED },
    { { "wait-running", "wait", NULL, NULL }, CONTROL_COMMAND_WAIT_RUNNING },
    { { "wait-frame", "wait", NULL, NULL }, CONTROL_COMMAND_WAIT_FRAME },
    { { "wait-event", "wait", NULL, NULL }, CONTROL_COMMAND_WAIT_EVENT },
    { { "assemble", "assemble", NULL, NULL }, CONTROL_COMMAND_ASSEMBLE },
    { { "find-symbol", "symbols", NULL, NULL }, CONTROL_COMMAND_FIND_SYMBOL },
    { { "get-drive-cpu", "drive-cpu", NULL, NULL }, CONTROL_COMMAND_GET_DRIVE_CPU },
    { { "get-vic", "vic", NULL, NULL }, CONTROL_COMMAND_GET_VIC },
    { { "get-cia", "cia", NULL, NULL }, CONTROL_COMMAND_GET_CIA },
    { { "run-to-raster", "run-to-raster", NULL, NULL }, CONTROL_COMMAND_RUN_TO_RASTER },
    { { "history-info", "history", NULL, NULL }, CONTROL_COMMAND_HISTORY_INFO },
    { { "history-record", "history", NULL, NULL }, CONTROL_COMMAND_HISTORY_RECORD },
    { { "history-clear", "history", NULL, NULL }, CONTROL_COMMAND_HISTORY_CLEAR },
    { { "history-find", "history", NULL, NULL }, CONTROL_COMMAND_HISTORY_FIND },
    { { "history-next", "history", NULL, NULL }, CONTROL_COMMAND_HISTORY_NEXT },
    { { "history-read", "history", NULL, NULL }, CONTROL_COMMAND_HISTORY_READ },
    { { "history-close", "history", NULL, NULL }, CONTROL_COMMAND_HISTORY_CLOSE },
    { { "power-drive", "power-drive", NULL, NULL }, CONTROL_COMMAND_POWER_DRIVE },
    { { "frame-ring-info", "frame-ring", NULL, NULL }, CONTROL_COMMAND_FRAME_RING_INFO },
    { { "frame-ring-record", "frame-ring", NULL, NULL }, CONTROL_COMMAND_FRAME_RING_RECORD },
    { { "frame-ring-clear", "frame-ring", NULL, NULL }, CONTROL_COMMAND_FRAME_RING_CLEAR },
    { { "vic-ring-info", "vic-ring", NULL, NULL }, CONTROL_COMMAND_VIC_RING_INFO },
    { { "vic-ring-record", "vic-ring", NULL, NULL }, CONTROL_COMMAND_VIC_RING_RECORD },
    { { "vic-ring-clear", "vic-ring", NULL, NULL }, CONTROL_COMMAND_VIC_RING_CLEAR },
    { { "vic-ring-find", "vic-ring", NULL, NULL }, CONTROL_COMMAND_VIC_RING_FIND },
    { { NULL, "sessions", NULL, NULL }, CONTROL_COMMAND_NONE },
    { { NULL, "state-changed", NULL, NULL }, CONTROL_COMMAND_NONE },
    { { "leave-inspector", "inspector", NULL, NULL }, CONTROL_COMMAND_LEAVE_INSPECTOR },
    { { "enter-inspector", "inspector", NULL, NULL }, CONTROL_COMMAND_ENTER_INSPECTOR },
    { { "land-inspector", "inspector", NULL, NULL }, CONTROL_COMMAND_LAND_INSPECTOR },
    { { "land-inspector-exact", "inspector", NULL, NULL },
      CONTROL_COMMAND_LAND_INSPECTOR_EXACT }
};

control_command_type c64_control_command_from_name(const char *name, size_t length)
{
    size_t i;
    if (name == NULL || length == 0u) {
        return CONTROL_COMMAND_NONE;
    }
    for (i = 0; i < sizeof(k_c64_verbs) / sizeof(k_c64_verbs[0]); i++) {
        const char *n = k_c64_verbs[i].verb.name;
        if (n != NULL && strlen(n) == length && strncmp(n, name, length) == 0) {
            return k_c64_verbs[i].type;
        }
    }
    return CONTROL_COMMAND_NONE;
}

void c64_control_format_capabilities(char *out, size_t out_size)
{
    control_verb packed[sizeof(k_c64_verbs) / sizeof(k_c64_verbs[0])];
    size_t i;
    for (i = 0; i < sizeof(k_c64_verbs) / sizeof(k_c64_verbs[0]); i++) {
        packed[i] = k_c64_verbs[i].verb;
    }
    control_verb_format_capabilities(
        packed, sizeof(packed) / sizeof(packed[0]), out, out_size);
}
