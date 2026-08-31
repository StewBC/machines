/* c64 HostFS — host directory as an IEC-facing volume (trap-fast). */
#ifndef C64_HOSTFS_H
#define C64_HOSTFS_H

#include "c64.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    C64_HOSTFS_PATH_MAX = 1024,
    C64_HOSTFS_NAME_MAX = 17, /* CBM title + NUL */
    C64_HOSTFS_BASENAME_MAX = 256,
    C64_HOSTFS_STATUS_MAX = 64,
    C64_HOSTFS_CWD_DEPTH_MAX = 32
};

/* PETSCII left-arrow / parent marker used by FB (`CD:_`). */
enum { C64_HOSTFS_PETSCII_LEFT_ARROW = 0x5f };

typedef struct c64_hostfs_volume c64_hostfs_volume;

bool c64_hostfs_path_is_dir(const char *path);

/* Mount root_path if it is an existing directory. cwd starts at root. */
c64_hostfs_volume *c64_hostfs_mount(const char *root_path, bool writable);

void c64_hostfs_eject(c64_hostfs_volume *vol);

const char *c64_hostfs_root_path(const c64_hostfs_volume *vol);
const char *c64_hostfs_cwd_path(const c64_hostfs_volume *vol);
const char *c64_hostfs_display_name(const c64_hostfs_volume *vol);
bool c64_hostfs_writable(const c64_hostfs_volume *vol);
void c64_hostfs_set_writable(c64_hostfs_volume *vol, bool writable);
/* True while CD has entered a .d64 sub-volume (still backend=HOSTFS). */
bool c64_hostfs_in_d64(const c64_hostfs_volume *vol);

/* DOS status channel string, e.g. "00, OK,00,00\r". */
const char *c64_hostfs_status(const c64_hostfs_volume *vol);
size_t c64_hostfs_status_length(const c64_hostfs_volume *vol);
void c64_hostfs_set_status_ok(c64_hostfs_volume *vol);
void c64_hostfs_set_status(
    c64_hostfs_volume *vol, int code, const char *message);

/* Rescan cwd. Builds sorted PRG/DIR catalog. */
bool c64_hostfs_rescan(c64_hostfs_volume *vol);

size_t c64_hostfs_entry_count(const c64_hostfs_volume *vol);
const c64_drive_directory_entry *c64_hostfs_entries(const c64_hostfs_volume *vol);
/* Absolute host path for catalog index (files only; dirs may still return path). */
const char *c64_hostfs_entry_host_path(const c64_hostfs_volume *vol, size_t index);

/* Copy catalog into a drive slot's entries[] / free_blocks / title fields. */
bool c64_hostfs_apply_catalog_to_slot(c64_hostfs_volume *vol, c64_drive_slot *slot);

/*
 * Execute a command-channel name buffer (OPEN SA=15 filename).
 * Accepts FB/SD2IEC-shaped CD forms: CD//, CD:_ / CD:←, CD:NAME, CD//NAME/,
 * CD/NAME/, and bare // / _ where unambiguous. Empty name is a no-op OK
 * (open status channel). CD:NAME may enter a host directory or a .d64 listed
 * as DIR (stem); parent/root leave a nested D64 without mounting IMAGE/1541.
 * Returns true if the command was handled (including DOS errors that still
 * "handled" the OPEN); false if not a recognized cmd.
 */
bool c64_hostfs_command(
    c64_hostfs_volume *vol,
    const uint8_t *name,
    size_t name_length,
    c64_drive_status_result *out_status);

/* Read entire host file into malloc'd buffer (caller frees). */
bool c64_hostfs_read_file(
    const char *host_path, uint8_t **out_bytes, size_t *out_size);

/* Read PRG bytes for a catalog index (host file, or extract from nested D64). */
bool c64_hostfs_read_entry_prg(
    c64_hostfs_volume *vol,
    size_t index,
    uint8_t **out_bytes,
    size_t *out_size);

/* Create PRG in cwd: host `<cbm>.prg`, or into a nested D64 (flush host file).
   Fails if the CBM name already exists (no overwrite / no @:). */
bool c64_hostfs_create_prg(
    c64_hostfs_volume *vol,
    const uint8_t *cbm_name,
    size_t cbm_name_length,
    const uint8_t *data,
    size_t data_size,
    c64_drive_status_result *out_status);

#ifdef __cplusplus
}
#endif

#endif /* C64_HOSTFS_H */
