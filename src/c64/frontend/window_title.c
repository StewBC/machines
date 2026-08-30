#include "window_title.h"

#include <stdio.h>
#include <string.h>

void debugger_format_window_title(
    char *out,
    size_t out_size,
    const char *product,
    const char *label,
    const char *turbo,
    const char *state);

static const char *window_title_stop_reason_name(runtime_stop_reason reason)
{
    switch (reason) {
        case RUNTIME_STOP_REASON_RESET:         return "reset";
        case RUNTIME_STOP_REASON_PAUSE_COMMAND: return "pause";
        case RUNTIME_STOP_REASON_STEP:          return "step";
        case RUNTIME_STOP_REASON_RUN_COMPLETE:  return "run complete";
        case RUNTIME_STOP_REASON_BREAKPOINT:    return "breakpoint";
        case RUNTIME_STOP_REASON_BRK:           return "BRK";
        case RUNTIME_STOP_REASON_ERROR:         return "error";
        case RUNTIME_STOP_REASON_NONE:
        default:                                return "none";
    }
}

void frontend_format_window_title_ex(
    char *out,
    size_t out_size,
    const char *video_standard,
    uint32_t turbo_multiplier,
    frontend_runtime_state state,
    runtime_stop_reason stop_reason,
    bool inspecting)
{
    const char *video = "?";
    char turbo[16];
    char state_text[48];

    if (out == NULL || out_size == 0) {
        return;
    }
    if (video_standard != NULL && strcmp(video_standard, "PAL") == 0) {
        video = "PAL";
    } else if (video_standard != NULL && strcmp(video_standard, "NTSC") == 0) {
        video = "NTSC";
    }
    if (turbo_multiplier == 0u) {
        turbo_multiplier = 1u;
    }
    if (turbo_multiplier == 1u) {
        snprintf(turbo, sizeof(turbo), "Normal");
    } else if (turbo_multiplier == 2u) {
        snprintf(turbo, sizeof(turbo), "Max");
    } else {
        snprintf(turbo, sizeof(turbo), "%u", (unsigned int)turbo_multiplier);
    }

    if (inspecting) {
        debugger_format_window_title(out, out_size, "c64m", video, turbo, "Inspect");
        return;
    }

    switch (state) {
        case FRONTEND_RUNTIME_STATE_RUNNING:
            snprintf(state_text, sizeof(state_text), "Running");
            break;
        case FRONTEND_RUNTIME_STATE_PAUSED:
            snprintf(state_text, sizeof(state_text), "Paused (%s)",
                window_title_stop_reason_name(stop_reason));
            break;
        case FRONTEND_RUNTIME_STATE_ERROR:
            snprintf(state_text, sizeof(state_text), "Error");
            break;
        case FRONTEND_RUNTIME_STATE_UNKNOWN:
        default:
            snprintf(state_text, sizeof(state_text), "Unknown");
            break;
    }
    debugger_format_window_title(out, out_size, "c64m", video, turbo, state_text);
}
