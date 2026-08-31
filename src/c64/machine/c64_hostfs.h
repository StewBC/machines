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
    C64_HOSTFS_BASENAME_MAX = 256
};

typedef struct c64_hostfs_volume c64_hostfs_volume;

bool c64_hostfs_path_is_dir(const char *path);

/* Mount root_path if it is an existing directory. */
c64_hostfs_volume *c64_hostfs_mount(const char *root_path, bool writable);

void c64_hostfs_eject(c64_hostfs_volume *vol);

const char *c64_hostfs_root_path(const c64_hostfs_volume *vol);
const char *c64_hostfs_display_name(const c64_hostfs_volume *vol);
bool c64_hostfs_writable(const c64_hostfs_volume *vol);
void c64_hostfs_set_writable(c64_hostfs_volume *vol, bool writable);

/* Rescan root (Phase 0: cwd = root). Builds sorted PRG/DIR catalog. */
bool c64_hostfs_rescan(c64_hostfs_volume *vol);

size_t c64_hostfs_entry_count(const c64_hostfs_volume *vol);
const c64_drive_directory_entry *c64_hostfs_entries(const c64_hostfs_volume *vol);
/* Absolute host path for catalog index (files only; dirs may still return path). */
const char *c64_hostfs_entry_host_path(const c64_hostfs_volume *vol, size_t index);

/* Copy catalog into a drive slot's entries[] / free_blocks / title fields. */
bool c64_hostfs_apply_catalog_to_slot(c64_hostfs_volume *vol, c64_drive_slot *slot);

/* Read entire host file into malloc'd buffer (caller frees). */
bool c64_hostfs_read_file(
    const char *host_path, uint8_t **out_bytes, size_t *out_size);

/* Create host `<cbm>.prg` under root. Fails if a catalog PRG with that CBM
   name already exists (no overwrite). cbm_name is PETSCII/ASCII bytes. */
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
