#pragma once

#include "runtime_client_subset.h"

#include "runtime_event.h"
#include "runtime.h"
#include "runtime_frame_ring.h"
#include "apple2_file.h"

#include "display_frame.h"
#include "keyboard.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    RUNTIME_SYMBOL_NAME_MAX    = 64,
    RUNTIME_SYMBOL_SNAPSHOT_MAX = 4096
};

typedef struct runtime_symbol_snapshot_entry {
    uint16_t address;
    char name[RUNTIME_SYMBOL_NAME_MAX];
} runtime_symbol_snapshot_entry;

typedef struct runtime_symbol_snapshot {
    size_t count;
    size_t total;
    runtime_symbol_snapshot_entry entries[RUNTIME_SYMBOL_SNAPSHOT_MAX];
} runtime_symbol_snapshot;

/* Cold reset (CTRL+Open-Apple+RESET). Preserves run/pause. */
bool runtime_client_cold_reset(runtime_client *client);
bool runtime_client_reset_ex_with_resume(
    runtime_client *client,
    bool resume_running);
bool runtime_client_reset_with_options(
    runtime_client *client,
    bool cold,
    uint8_t resume_running);
bool runtime_client_request_machine_state(runtime_client *client);
bool runtime_client_request_debug_memory(runtime_client *client, bool include_write_history);
bool runtime_client_request_frame(runtime_client *client);
bool runtime_client_keyboard_key(runtime_client *client, host_key key, bool pressed);
/* Apple game port: axes[4] paddle units 0..255 (PDL0..PDL3), buttons bit0–2. */
bool runtime_client_set_gameport(
    runtime_client *client,
    const uint8_t axis[4],
    uint8_t button_mask);
bool runtime_client_create_breakpoint(
    runtime_client *client,
    const runtime_breakpoint_definition *definition);
bool runtime_client_update_breakpoint(
    runtime_client *client,
    uint32_t id,
    const runtime_breakpoint_definition *definition);
bool runtime_client_save_state(runtime_client *client, const char *path);
bool runtime_client_load_state(runtime_client *client, const char *path);
/* Disk II write-protect notch (slot 1–7, drive 0/1). */
bool runtime_client_set_disk_writable(
    runtime_client *client,
    uint8_t slot,
    uint8_t device,
    bool writable);
bool runtime_client_media_insert(
    runtime_client *client,
    uint8_t slot,
    uint8_t device,
    runtime_slot_card_type card_type,
    const char *path);
bool runtime_client_media_eject(runtime_client *client, uint8_t slot, uint8_t device);
bool runtime_client_media_swap(
    runtime_client *client,
    uint8_t slot,
    uint8_t device,
    int32_t param,
    bool relative);
bool runtime_client_boot_slot(runtime_client *client, uint8_t slot);
/* Debugger-only presentation override; does not modify Apple soft switches. */
bool runtime_client_set_display_override(
    runtime_client *client,
    bool enabled,
    uint32_t flags);
/* Host monitor decoder. colour=true is artefact paint; phosphor is 0..2. */
bool runtime_client_set_video_display(
    runtime_client *client,
    bool colour,
    uint8_t phosphor);
bool runtime_client_assemble_file(runtime_client *client, const char *path, uint16_t address);
bool runtime_client_assemble_file_full(
    runtime_client *client,
    const char *path,
    uint16_t address,
    uint16_t run_address,
    bool auto_run,
    bool mli_launch,
    bool reset_first,
    bool auto_adjust_segments);
bool runtime_client_poll_symbols(runtime_client *client, runtime_symbol_snapshot *out);
bool runtime_client_paste_text(runtime_client *client, const char *text, size_t length);
bool runtime_client_cycle_turbo_speed(runtime_client *client);
/* milli_mhz: RUNTIME_TURBO_MAX (0) = max free-run; else target MHz × 1000 (1000 = 1 MHz). */
bool runtime_client_set_turbo_multiplier(runtime_client *client, uint32_t milli_mhz);
bool runtime_client_apply_machine_config(
    runtime_client *client,
    const runtime_machine_config *config,
    const runtime_config *runtime_options,
    const char *ini_path,
    const char *symbol_files,
    bool reset,
    bool save_ini,
    bool resume_running);
/* Apple ARGB frame handoff. Caller provides buffer large enough for w*h. */
bool runtime_client_poll_argb_frame(
    runtime_client *client,
    uint32_t *out_pixels,
    uint32_t max_pixels,
    uint32_t *out_width,
    uint32_t *out_height,
    uint64_t *out_frame_number);
bool runtime_client_poll_debug_memory(runtime_client *client, runtime_debug_memory_snapshot *out_snapshot);
bool runtime_client_poll_breakpoints(
    runtime_client *client,
    runtime_breakpoint_snapshot *out_snapshot);

/* Pause flight-recorder automatically while turbo is max (live policy). */
bool runtime_client_set_history_off_on_max(runtime_client *client, bool enabled);
/* Wipe Inspector Record on max when enabled (live policy). */
bool runtime_client_set_inspector_off_on_max(runtime_client *client, bool enabled);
bool runtime_client_inspector_catalog_copy(
    runtime_client *client,
    runtime_inspector_catalog *out_catalog);
bool runtime_client_inspector_copy_picture(
    runtime_client *client,
    uint64_t picture_id,
    runtime_ring_frame *out_frame);
bool runtime_client_inspector_land_sample(
    runtime_client *client, uint64_t sample_id, uint64_t request_token);
bool runtime_client_inspector_step_sample(
    runtime_client *client, int direction, uint64_t request_token);
/* Worker-allocated session; reply via RUNTIME_EVENT_SESSION_RESPONSE.
   endpoint_epoch is stored for control binding (0 if unused). */
bool runtime_client_session_open(
    runtime_client *client,
    runtime_session_kind kind,
    uint64_t endpoint_epoch,
    uint64_t request_token);
bool runtime_client_session_close(
    runtime_client *client,
    uint32_t session_id,
    uint64_t request_token);
bool runtime_client_request_call_stack(runtime_client *client);

bool runtime_client_load_bin(
    runtime_client *client,
    const char *path,
    uint16_t address,
    apple2_binary_format format,
    bool reset_first,
    bool is_basic_text,
    bool run_after_load);
bool runtime_client_save_bin(
    runtime_client *client,
    const char *path,
    uint16_t start_address,
    uint16_t end_address,
    apple2_binary_format format,
    bool is_basic_text);

bool runtime_client_poll_event(
    runtime_client *client,
    runtime_event *out_event);

/* Frame ring (rolling framebuffer black box). These read the runtime's ring
   directly; it carries its own mutex, so they are safe to call from the main
   thread while the runtime thread keeps pushing frames. */
void runtime_client_get_frame_ring_info(
    runtime_client *client,
    runtime_frame_ring_info *out_info);

bool runtime_client_copy_frame_at(
    runtime_client *client,
    uint64_t target,
    bool by_cycle,
    runtime_ring_frame *out_frame);

/* index 0 = oldest retained slot (handles frame-number gaps). */
bool runtime_client_copy_frame_at_index(
    runtime_client *client,
    uint32_t index,
    runtime_ring_frame *out_frame);

/* Metadata only for span math (no pixel slab copy). */
bool runtime_client_frame_meta_at_index(
    runtime_client *client,
    uint32_t index,
    uint64_t *out_frame_number,
    uint64_t *out_machine_cycle);

void runtime_client_set_frame_ring_recording(runtime_client *client, bool recording);

void runtime_client_clear_frame_ring(runtime_client *client);
