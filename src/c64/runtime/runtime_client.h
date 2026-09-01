#pragma once

#include "runtime_client_subset.h"

#include "runtime_event.h"
#include "runtime.h"
#include "runtime_frame_ring.h"
#include "runtime_vic_ring.h"

#include "c64_frame.h"
#include "c64.h"
#include "keyboard.h"
#include "paste_parser.h"

#include "runtime_symbol_snapshot.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool runtime_client_reset_ex(runtime_client *client, bool detach_cartridge);
bool runtime_client_reset_ex_with_resume(
    runtime_client *client,
    bool detach_cartridge,
    bool resume_running);
bool runtime_client_step_frame(runtime_client *client);
bool runtime_client_request_machine_state(runtime_client *client);
bool runtime_client_request_debug_memory(runtime_client *client, bool include_write_history);
bool runtime_client_request_frame(runtime_client *client);
bool runtime_client_keyboard_key(runtime_client *client, c64_key key, bool pressed);
bool runtime_client_restore(runtime_client *client);
bool runtime_client_set_joystick(runtime_client *client, unsigned port, uint8_t inputs);
/* 1351 proportional: potx/poty are encoded POT bytes; buttons = C64_JOYSTICK_*. */
bool runtime_client_set_mouse(
    runtime_client *client,
    unsigned port,
    uint8_t potx,
    uint8_t poty,
    uint8_t buttons);
bool runtime_client_clear_mouse(runtime_client *client);
bool runtime_client_create_breakpoint(
    runtime_client *client,
    const runtime_breakpoint_definition *definition);
bool runtime_client_update_breakpoint(
    runtime_client *client,
    uint32_t id,
    const runtime_breakpoint_definition *definition);
bool runtime_client_load_prg(runtime_client *client, const char *path);
bool runtime_client_load_crt(runtime_client *client, const char *path);
bool runtime_client_save_state(runtime_client *client, const char *path);
bool runtime_client_load_state(runtime_client *client, const char *path);
bool runtime_client_mount_d64(runtime_client *client, uint8_t device, const char *path);
bool runtime_client_mount_d64_ex(
    runtime_client *client,
    uint8_t device,
    const char *path,
    bool writable);
bool runtime_client_set_disk_writable(runtime_client *client, uint8_t device, bool writable);
bool runtime_client_unmount_disk(runtime_client *client, uint8_t device);
/* Soft power-on for device 8 or 9 (sticky; no-op if already powered). */
bool runtime_client_power_on_drive(runtime_client *client, uint8_t device);
/* Soft power-off: ejects media if present, then powers the unit off. */
bool runtime_client_power_off_drive(runtime_client *client, uint8_t device);
bool runtime_client_request_disk_status(runtime_client *client, uint8_t device);
bool runtime_client_assemble_file(runtime_client *client, const char *path, uint16_t address);
bool runtime_client_assemble_file_full(
    runtime_client *client,
    const char *path,
    uint16_t address,
    uint16_t run_address,
    bool auto_run,
    bool basic_run,
    bool reset_first,
    bool auto_adjust_segments);
bool runtime_client_poll_symbols(runtime_client *client, runtime_symbol_snapshot *out);
bool runtime_client_set_symbol_source_enabled(
    runtime_client *client,
    uint32_t source_id,
    bool enabled);
bool runtime_client_paste_text(runtime_client *client, const char *text, size_t length);
bool runtime_client_paste_text_buffer(runtime_client *client, const char *text, size_t length);
bool runtime_client_paste_events(runtime_client *client, const paste_event_t *events, size_t count);
bool runtime_client_cycle_turbo_speed(runtime_client *client);
/* multiplier is a turbo mode ID: 1=normal, 2=max (RUNTIME_TURBO_MODE_*). */
bool runtime_client_set_turbo_multiplier(runtime_client *client, uint32_t multiplier);
/* Soft-attach SwiftLink/Turbo232. base is 0xDE00 or 0xDF00. Conflict refuses
   enable and publishes RUNTIME_EVENT_ERROR. */
bool runtime_client_set_swiftlink(
    runtime_client *client,
    bool enabled,
    uint16_t base,
    c64_swiftlink_irq_mode irq_mode,
    bool pace_baud);
/* Soft-attach MPS-803-class printer (device 4). Format is BMP for v1.
   output_dir is required when enabling; may be NULL when disabling. */
bool runtime_client_set_printer(
    runtime_client *client,
    bool enabled,
    const char *output_dir);
/* rom_paths, when non-NULL, carries the effective ROM file paths (any member may
   be NULL/empty for "unset"); pass reload_roms=true to have the runtime re-read
   them as part of this apply (requires reset to take visible effect). */
typedef struct runtime_client_rom_paths {
    const char *system_rom_path;
    const char *basic_rom_path;
    const char *char_rom_path;
    const char *kernal_rom_path;
    const char *rom1541_path;
} runtime_client_rom_paths;

bool runtime_client_apply_machine_config(
    runtime_client *client,
    const c64_config *config,
    const runtime_config *runtime_options,
    const char *ini_path,
    const char *symbol_files,
    bool reset,
    bool save_ini,
    bool resume_running,
    const runtime_client_rom_paths *rom_paths,
    bool reload_roms);
bool runtime_client_poll_frame(runtime_client *client, c64_frame *out_frame);
bool runtime_client_poll_debug_memory(runtime_client *client, runtime_debug_memory_snapshot *out_snapshot);
bool runtime_client_poll_breakpoints(
    runtime_client *client,
    runtime_breakpoint_snapshot *out_snapshot);

/* Run until VIC raster_line matches (and optional cycle_in_line). */
bool runtime_client_run_to_raster(
    runtime_client *client,
    uint16_t raster_line,
    bool has_cycle,
    uint16_t cycle_in_line);
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
/* direction < 0 / > 0: Record checkpoint lattice walk. */
bool runtime_client_inspector_checkpoint_step(
    runtime_client *client, int direction, uint64_t request_token);
/* Local Record-index neighbor (no worker round-trip). See
   runtime_inspector_cp_index_adjacent. */
bool runtime_client_inspector_adjacent_cycle(
    runtime_client *client,
    uint64_t from_cycle,
    int direction,
    uint64_t live_cycle,
    uint64_t *out_cycle);
/* Local Snapshot-line locate (no worker round-trip). */
bool runtime_client_inspector_snapshot_slot(
    runtime_client *client,
    uint64_t cycle,
    uint64_t live_cycle,
    uint64_t *out_ordinal,
    uint64_t *out_count,
    bool *out_exact);
bool runtime_client_request_call_stack(runtime_client *client);

bool runtime_client_load_bin(
    runtime_client *client,
    const char *path,
    uint16_t address,
    bool use_file_address,
    bool reset_first,
    bool is_basic,
    bool is_basic_text);
bool runtime_client_save_bin(
    runtime_client *client,
    const char *path,
    uint16_t start_address,
    uint16_t end_address,
    bool write_file_address,
    bool is_basic,
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

/* Nearest-≤ lookup by frame number or machine_cycle. For exact machine_cycle
   match, see runtime_client_copy_frame_by_cycle_exact. */
bool runtime_client_copy_frame_at(
    runtime_client *client,
    uint64_t target,
    bool by_cycle,
    c64_frame *out_frame);

/* Exact machine_cycle match only; false on any miss (no nearest-≤ fallback). */
bool runtime_client_copy_frame_by_cycle_exact(
    runtime_client *client,
    uint64_t machine_cycle,
    c64_frame *out_frame);

/* Scrub cell-film join: quantize preview to nearest CP ≤, then exact film copy
 * at that cell's film_cycle. Local shared-index read (no worker RPC). Returns
 * false when no cell ≤ preview, the cell has film_cycle==0, or the exact film
 * slot is missing — never substitutes a nearest neighbour still. */
bool runtime_client_copy_inspector_cell_film(
    runtime_client *client,
    uint64_t preview_cycle,
    c64_frame *out_frame);

void runtime_client_set_frame_ring_recording(runtime_client *client, bool recording);

void runtime_client_clear_frame_ring(runtime_client *client);

/* Per-line VIC ring. Same threading story as the frame ring: the ring owns a
   mutex, so these are safe from the main thread while the runtime records. */
void runtime_client_get_vic_ring_info(
    runtime_client *client,
    runtime_vic_ring_info *out_info);

uint32_t runtime_client_copy_vic_lines(
    runtime_client *client,
    bool has_frame,
    uint64_t frame_number,
    uint16_t raster_first,
    uint16_t raster_last,
    uint32_t limit,
    vicii_line_record *out);

void runtime_client_set_vic_ring_recording(runtime_client *client, bool recording);

void runtime_client_clear_vic_ring(runtime_client *client);
