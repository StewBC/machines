/* c64 HostFS — host directory volume + catalog / PRG+SEQ I/O + CD (+ D64 nest). */
#include "c64_hostfs.h"

#include "d64.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#define C64_HOSTFS_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#define C64_HOSTFS_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#else
#define C64_HOSTFS_ISDIR(mode) S_ISDIR(mode)
#define C64_HOSTFS_ISREG(mode) S_ISREG(mode)
#endif

typedef struct c64_hostfs_cat_entry {
    c64_drive_directory_entry dir;
    char host_path[C64_HOSTFS_PATH_MAX];
} c64_hostfs_cat_entry;

typedef enum c64_hostfs_channel_kind {
    C64_HOSTFS_CH_NONE = 0,
    C64_HOSTFS_CH_SEQ_READ,
    C64_HOSTFS_CH_SEQ_WRITE
} c64_hostfs_channel_kind;

typedef struct c64_hostfs_channel {
    bool in_use;
    uint8_t la;
    uint8_t sa;
    c64_hostfs_channel_kind kind;
    uint8_t *data;
    size_t size;
    size_t pos;
    size_t cap;
    char host_path[C64_HOSTFS_PATH_MAX];
} c64_hostfs_channel;

struct c64_hostfs_volume {
    char root_path[C64_HOSTFS_PATH_MAX];
    char cwd_path[C64_HOSTFS_PATH_MAX];
    char display_name[C64_HOSTFS_NAME_MAX];
    char status[C64_HOSTFS_STATUS_MAX];
    size_t status_length;
    bool writable;
    /* Nested .d64 sub-volume (owned). cwd_path stays the parent host folder. */
    d64_image *d64;
    char d64_host_path[C64_HOSTFS_PATH_MAX];
    c64_hostfs_cat_entry *catalog;
    size_t catalog_count;
    size_t catalog_cap;
    c64_hostfs_channel channels[C64_HOSTFS_MAX_CHANNELS];
};

static unsigned char c64_hostfs_upper(unsigned char c);
static void c64_hostfs_channel_clear(c64_hostfs_channel *ch);
static bool c64_hostfs_path_under_root(
    const c64_hostfs_volume *vol, const char *path);
static bool c64_hostfs_cd_enter_d64(
    c64_hostfs_volume *vol, const char *host_path);

static void c64_hostfs_leave_d64(c64_hostfs_volume *vol)
{
    if (vol == NULL) {
        return;
    }
    if (vol->d64 != NULL) {
        d64_image_destroy(vol->d64);
        vol->d64 = NULL;
    }
    vol->d64_host_path[0] = '\0';
}

bool c64_hostfs_in_d64(const c64_hostfs_volume *vol)
{
    return vol != NULL && vol->d64 != NULL;
}

void c64_hostfs_set_status(
    c64_hostfs_volume *vol, int code, const char *message)
{
    if (vol == NULL) {
        return;
    }
    if (message == NULL) {
        message = "OK";
    }
    snprintf(
        vol->status,
        sizeof(vol->status),
        "%02d, %s,00,00\r",
        code,
        message);
    vol->status_length = strlen(vol->status);
}

void c64_hostfs_set_status_ok(c64_hostfs_volume *vol)
{
    c64_hostfs_set_status(vol, 0, "OK");
}

void c64_hostfs_apply_drive_status(
    c64_hostfs_volume *vol, c64_drive_status_result status)
{
    if (vol == NULL) {
        return;
    }
    switch (status) {
    case C64_DRIVE_STATUS_OK:
        c64_hostfs_set_status_ok(vol);
        break;
    case C64_DRIVE_STATUS_WRITE_PROTECTED:
        c64_hostfs_set_status(vol, 26, "WRITE PROTECT");
        break;
    case C64_DRIVE_STATUS_FILE_EXISTS:
        c64_hostfs_set_status(vol, 63, "FILE EXISTS");
        break;
    case C64_DRIVE_STATUS_DISK_FULL:
        c64_hostfs_set_status(vol, 72, "DISK FULL");
        break;
    case C64_DRIVE_STATUS_NOT_MOUNTED:
    case C64_DRIVE_STATUS_IO_ERROR:
    case C64_DRIVE_STATUS_OUT_OF_MEMORY:
    case C64_DRIVE_STATUS_PARSE_ERROR:
    case C64_DRIVE_STATUS_UNSUPPORTED_IMAGE:
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        break;
    default:
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        break;
    }
}

const char *c64_hostfs_status(const c64_hostfs_volume *vol)
{
    return vol != NULL ? vol->status : NULL;
}

size_t c64_hostfs_status_length(const c64_hostfs_volume *vol)
{
    return vol != NULL ? vol->status_length : 0u;
}

bool c64_hostfs_path_is_dir(const char *path)
{
    struct stat st;

    if (path == NULL || path[0] == '\0') {
        return false;
    }
    return stat(path, &st) == 0 && C64_HOSTFS_ISDIR(st.st_mode);
}

static const char *c64_hostfs_basename(const char *path)
{
    const char *slash;
    const char *base;

    if (path == NULL || path[0] == '\0') {
        return "";
    }
    slash = strrchr(path, '/');
#if defined(_WIN32)
    {
        const char *bslash = strrchr(path, '\\');
        if (bslash != NULL && (slash == NULL || bslash > slash)) {
            slash = bslash;
        }
    }
#endif
    base = (slash != NULL) ? slash + 1 : path;
    if (base[0] == '\0' && slash != NULL && slash > path) {
        const char *p = slash;
        while (p > path && p[-1] != '/' && p[-1] != '\\') {
            p--;
        }
        return p;
    }
    return base;
}

static void c64_hostfs_mangle_cbm(
    const char *stem, char *out, size_t out_size, size_t *out_len)
{
    size_t oi = 0;
    size_t i;

    if (out == NULL || out_size == 0) {
        if (out_len != NULL) {
            *out_len = 0;
        }
        return;
    }
    out[0] = '\0';
    if (stem == NULL) {
        if (out_len != NULL) {
            *out_len = 0;
        }
        return;
    }
    for (i = 0; stem[i] != '\0' && oi + 1 < out_size && oi < 16u; i++) {
        unsigned char c = (unsigned char)stem[i];
        if (c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 'a' + 'A');
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.') {
            out[oi++] = (char)c;
        }
    }
    out[oi] = '\0';
    if (out[0] == '\0' && out_size > 1) {
        snprintf(out, out_size, "FILE");
        oi = strlen(out);
    }
    if (out_len != NULL) {
        *out_len = oi;
    }
}

static int c64_hostfs_cmp_cat(const void *a, const void *b)
{
    const c64_hostfs_cat_entry *ea = (const c64_hostfs_cat_entry *)a;
    const c64_hostfs_cat_entry *eb = (const c64_hostfs_cat_entry *)b;
    char na[17];
    char nb[17];
    size_t i;

    for (i = 0; i < ea->dir.filename_length && i < 16u; i++) {
        na[i] = (char)ea->dir.filename[i];
    }
    na[i] = '\0';
    for (i = 0; i < eb->dir.filename_length && i < 16u; i++) {
        nb[i] = (char)eb->dir.filename[i];
    }
    nb[i] = '\0';
    return strcmp(na, nb);
}

static bool c64_hostfs_catalog_has_name(
    const c64_hostfs_volume *vol, const char *cbm, size_t cbm_len)
{
    size_t i;
    size_t j;

    for (i = 0; i < vol->catalog_count; i++) {
        const c64_drive_directory_entry *e = &vol->catalog[i].dir;
        if (e->filename_length != cbm_len) {
            continue;
        }
        for (j = 0; j < cbm_len; j++) {
            unsigned char a = (unsigned char)e->filename[j];
            unsigned char b = (unsigned char)cbm[j];
            if (a >= 'a' && a <= 'z') {
                a = (unsigned char)(a - 'a' + 'A');
            }
            if (b >= 'a' && b <= 'z') {
                b = (unsigned char)(b - 'a' + 'A');
            }
            if (a != b) {
                break;
            }
        }
        if (j == cbm_len) {
            return true;
        }
    }
    return false;
}

static void c64_hostfs_unique_cbm(
    c64_hostfs_volume *vol, char *cbm, size_t cbm_cap, size_t *cbm_len)
{
    char base[17];
    size_t base_len = *cbm_len;
    unsigned alias;

    if (!c64_hostfs_catalog_has_name(vol, cbm, *cbm_len)) {
        return;
    }
    snprintf(base, sizeof(base), "%s", cbm);
    base_len = strlen(base);
    if (base_len > 13u) {
        base[13] = '\0';
        base_len = 13u;
    }
    for (alias = 1; alias <= 999u; alias++) {
        char trial[17];
        size_t trial_len;
        snprintf(trial, sizeof(trial), "%s%03u", base, alias);
        trial_len = strlen(trial);
        if (trial_len > 16u) {
            trial[16] = '\0';
            trial_len = 16u;
        }
        if (!c64_hostfs_catalog_has_name(vol, trial, trial_len)) {
            snprintf(cbm, cbm_cap, "%s", trial);
            *cbm_len = trial_len;
            return;
        }
    }
}

static bool c64_hostfs_catalog_push(
    c64_hostfs_volume *vol,
    const char *host_path,
    const char *cbm,
    size_t cbm_len,
    c64_drive_file_type type,
    uint16_t blocks)
{
    c64_hostfs_cat_entry *grown;
    c64_hostfs_cat_entry *e;
    size_t i;

    if (vol->catalog_count + 1u > vol->catalog_cap) {
        size_t ncap = vol->catalog_cap == 0 ? 16u : vol->catalog_cap * 2u;
        grown = (c64_hostfs_cat_entry *)realloc(
            vol->catalog, ncap * sizeof(*grown));
        if (grown == NULL) {
            return false;
        }
        vol->catalog = grown;
        vol->catalog_cap = ncap;
    }
    e = &vol->catalog[vol->catalog_count];
    memset(e, 0, sizeof(*e));
    e->dir.type = type;
    e->dir.raw_type = (uint8_t)type;
    e->dir.block_count = blocks;
    e->dir.filename_length = cbm_len > 16u ? 16u : cbm_len;
    for (i = 0; i < e->dir.filename_length; i++) {
        e->dir.filename[i] = (uint8_t)cbm[i];
    }
    snprintf(e->host_path, sizeof(e->host_path), "%s", host_path);
    vol->catalog_count++;
    return true;
}

static uint16_t c64_hostfs_blocks_for_size(size_t size)
{
    if (size == 0) {
        return 0;
    }
    return (uint16_t)((size + 253u) / 254u);
}

static bool c64_hostfs_ends_with_ci(
    const char *name, size_t name_len, const char *ext)
{
    size_t el;
    size_t i;

    if (name == NULL || ext == NULL) {
        return false;
    }
    el = strlen(ext);
    if (name_len < el) {
        return false;
    }
    for (i = 0; i < el; i++) {
        unsigned char a = (unsigned char)name[name_len - el + i];
        unsigned char b = (unsigned char)ext[i];
        if (a >= 'A' && a <= 'Z') {
            a = (unsigned char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (unsigned char)(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }
    return true;
}

static bool c64_hostfs_path_is_d64_file(const char *path)
{
    struct stat st;
    const char *base;
    size_t base_len;

    if (path == NULL || stat(path, &st) != 0 || !C64_HOSTFS_ISREG(st.st_mode)) {
        return false;
    }
    base = c64_hostfs_basename(path);
    base_len = strlen(base);
    return c64_hostfs_ends_with_ci(base, base_len, ".d64");
}

/* PC64 / X00: ".P00"–".P99" (PRG containers). Letter encodes type; digits disambiguate. */
enum { C64_HOSTFS_P00_HEADER_SIZE = 26 };

static bool c64_hostfs_is_pxx_name(const char *name, size_t name_len)
{
    unsigned char p;
    unsigned char d0;
    unsigned char d1;

    if (name == NULL || name_len < 4u || name[name_len - 4u] != '.') {
        return false;
    }
    p = (unsigned char)name[name_len - 3u];
    d0 = (unsigned char)name[name_len - 2u];
    d1 = (unsigned char)name[name_len - 1u];
    if (p != 'p' && p != 'P') {
        return false;
    }
    return d0 >= '0' && d0 <= '9' && d1 >= '0' && d1 <= '9';
}

/*
 * Read PC64 header: magic "C64File\\0", CBM name at +8 (16 bytes, NUL-padded).
 * On success fills cbm[0..16] and optional payload block count from file_size.
 */
static bool c64_hostfs_p00_probe(
    const char *path,
    size_t file_size,
    char *cbm,
    size_t cbm_cap,
    size_t *cbm_len,
    uint16_t *out_blocks)
{
    FILE *f;
    uint8_t hdr[C64_HOSTFS_P00_HEADER_SIZE];
    size_t n;
    size_t i;

    if (path == NULL || cbm == NULL || cbm_cap < 2u || cbm_len == NULL) {
        return false;
    }
    if (file_size < (size_t)C64_HOSTFS_P00_HEADER_SIZE + 2u) {
        return false;
    }
    f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }
    if (fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return false;
    }
    fclose(f);
    if (memcmp(hdr, "C64File", 7) != 0 || hdr[7] != 0) {
        return false;
    }
    n = 0;
    for (i = 0; i < 16u; i++) {
        if (hdr[8u + i] == 0) {
            break;
        }
        n++;
    }
    if (n == 0) {
        return false;
    }
    if (n + 1u > cbm_cap) {
        n = cbm_cap - 1u;
    }
    memcpy(cbm, &hdr[8], n);
    cbm[n] = '\0';
    *cbm_len = n;
    if (out_blocks != NULL) {
        *out_blocks = c64_hostfs_blocks_for_size(
            file_size - (size_t)C64_HOSTFS_P00_HEADER_SIZE);
    }
    return true;
}

static bool c64_hostfs_read_p00_prg(
    const char *path, uint8_t **out_bytes, size_t *out_size)
{
    uint8_t *raw = NULL;
    size_t raw_size = 0;
    uint8_t *payload;
    size_t payload_size;

    if (out_bytes == NULL || out_size == NULL) {
        return false;
    }
    *out_bytes = NULL;
    *out_size = 0;
    if (!c64_hostfs_read_file(path, &raw, &raw_size)) {
        return false;
    }
    if (raw_size < (size_t)C64_HOSTFS_P00_HEADER_SIZE + 2u ||
        memcmp(raw, "C64File", 7) != 0 || raw[7] != 0) {
        free(raw);
        return false;
    }
    payload_size = raw_size - (size_t)C64_HOSTFS_P00_HEADER_SIZE;
    payload = (uint8_t *)malloc(payload_size);
    if (payload == NULL) {
        free(raw);
        return false;
    }
    memcpy(payload, raw + C64_HOSTFS_P00_HEADER_SIZE, payload_size);
    free(raw);
    *out_bytes = payload;
    *out_size = payload_size;
    return true;
}

static bool c64_hostfs_catalog_clear(c64_hostfs_volume *vol)
{
    free(vol->catalog);
    vol->catalog = NULL;
    vol->catalog_count = 0;
    vol->catalog_cap = 0;
    return true;
}

static bool c64_hostfs_rescan_d64(c64_hostfs_volume *vol)
{
    size_t count;
    size_t i;

    if (vol == NULL || vol->d64 == NULL) {
        return false;
    }
    c64_hostfs_catalog_clear(vol);
    count = d64_image_directory_count(vol->d64);
    for (i = 0; i < count; i++) {
        d64_directory_entry d64_entry;
        char cbm[17];
        size_t cbm_len;
        c64_drive_file_type ftype;

        if (d64_image_directory_entry(vol->d64, i, &d64_entry) != D64_OK) {
            return false;
        }
        cbm_len = d64_entry.filename_length;
        if (cbm_len > 16u) {
            cbm_len = 16u;
        }
        memcpy(cbm, d64_entry.filename, cbm_len);
        cbm[cbm_len] = '\0';
        ftype = (c64_drive_file_type)d64_entry.type;
        if (ftype == C64_DRIVE_FILE_DIR) {
            ftype = C64_DRIVE_FILE_UNKNOWN;
        }
        /* Nested entries have no host path; LOAD uses extract by index. */
        if (!c64_hostfs_catalog_push(
                vol, "", cbm, cbm_len, ftype, d64_entry.block_count)) {
            return false;
        }
        vol->catalog[vol->catalog_count - 1u].dir.raw_type = d64_entry.raw_type;
        vol->catalog[vol->catalog_count - 1u].dir.first_track = d64_entry.first_track;
        vol->catalog[vol->catalog_count - 1u].dir.first_sector = d64_entry.first_sector;
    }
    /* Keep D64 directory order so catalog index matches d64_image_directory_*. */
    return true;
}

bool c64_hostfs_rescan(c64_hostfs_volume *vol)
{
    DIR *dir;
    struct dirent *de;

    if (vol == NULL) {
        return false;
    }
    if (vol->d64 != NULL) {
        return c64_hostfs_rescan_d64(vol);
    }

    c64_hostfs_catalog_clear(vol);

    dir = opendir(vol->cwd_path);
    if (dir == NULL) {
        return false;
    }
    while ((de = readdir(dir)) != NULL) {
        char full[C64_HOSTFS_PATH_MAX];
        struct stat st;
        char cbm[17];
        size_t cbm_len = 0;
        const char *name = de->d_name;
        size_t name_len;

        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            continue;
        }
        /* Dotfiles (.DS_Store, .git, …) stay host-only. */
        if (name[0] == '.') {
            continue;
        }
        name_len = strlen(name);
        if (name_len == 0) {
            continue;
        }
        if ((size_t)snprintf(full, sizeof(full), "%s/%s", vol->cwd_path, name) >=
            sizeof(full)) {
            continue;
        }
        if (stat(full, &st) != 0) {
            continue;
        }
        if (C64_HOSTFS_ISDIR(st.st_mode)) {
            c64_hostfs_mangle_cbm(name, cbm, sizeof(cbm), &cbm_len);
            c64_hostfs_unique_cbm(vol, cbm, sizeof(cbm), &cbm_len);
            if (!c64_hostfs_catalog_push(
                    vol, full, cbm, cbm_len, C64_DRIVE_FILE_DIR, 0)) {
                closedir(dir);
                return false;
            }
            continue;
        }
        if (!C64_HOSTFS_ISREG(st.st_mode)) {
            continue;
        }
        /*
         * Catalog policy: every regular non-dotfile is visible.
         *   .d64  → DIR, CBM name = full basename incl. .D64 (FB visibility)
         *   .Pxx  → PRG, CBM name from PC64 header (unwrap on LOAD)
         *   .prg  → PRG, CBM name = stem (SAVE round-trip)
         *   .seq  → SEQ, CBM name = stem (channel I/O via OPEN SA≠15)
         *   else  → PRG, CBM name = full basename (fb64, xxx.txt, .g64, …)
         */
        if (c64_hostfs_is_pxx_name(name, name_len)) {
            uint16_t blocks = 0;
            if (c64_hostfs_p00_probe(
                    full, (size_t)st.st_size, cbm, sizeof(cbm), &cbm_len, &blocks)) {
                c64_hostfs_unique_cbm(vol, cbm, sizeof(cbm), &cbm_len);
                if (!c64_hostfs_catalog_push(
                        vol, full, cbm, cbm_len, C64_DRIVE_FILE_PRG, blocks)) {
                    closedir(dir);
                    return false;
                }
                continue;
            }
            /* Invalid/truncated Pxx: fall through as ordinary PRG basename. */
        }
        {
            char stem[C64_HOSTFS_BASENAME_MAX];
            const char *mangle_src = name;
            c64_drive_file_type ftype = C64_DRIVE_FILE_PRG;
            size_t ext = 0; /* bytes to strip from name before mangle; 0 = keep */

            if (name_len >= 4u && name[name_len - 4u] == '.') {
                char e0 = name[name_len - 3u];
                char e1 = name[name_len - 2u];
                char e2 = name[name_len - 1u];
                if ((e0 == 'd' || e0 == 'D') && (e1 == '6') &&
                    (e2 == '4')) {
                    /* Keep ".D64" in the listed name so FB can tell images apart. */
                    ftype = C64_DRIVE_FILE_DIR;
                } else if (
                    (e0 == 'p' || e0 == 'P') && (e1 == 'r' || e1 == 'R') &&
                    (e2 == 'g' || e2 == 'G')) {
                    ext = 4u;
                    ftype = C64_DRIVE_FILE_PRG;
                } else if (
                    (e0 == 's' || e0 == 'S') && (e1 == 'e' || e1 == 'E') &&
                    (e2 == 'q' || e2 == 'Q')) {
                    ext = 4u;
                    ftype = C64_DRIVE_FILE_SEQ;
                }
            }
            if (ext != 0u) {
                size_t stem_len = name_len - ext;
                if (stem_len >= sizeof(stem)) {
                    stem_len = sizeof(stem) - 1u;
                }
                memcpy(stem, name, stem_len);
                stem[stem_len] = '\0';
                mangle_src = stem;
            }
            c64_hostfs_mangle_cbm(mangle_src, cbm, sizeof(cbm), &cbm_len);
            c64_hostfs_unique_cbm(vol, cbm, sizeof(cbm), &cbm_len);
            if (!c64_hostfs_catalog_push(
                    vol,
                    full,
                    cbm,
                    cbm_len,
                    ftype,
                    ftype == C64_DRIVE_FILE_DIR ?
                        0u :
                        c64_hostfs_blocks_for_size((size_t)st.st_size))) {
                closedir(dir);
                return false;
            }
        }
    }
    closedir(dir);
    if (vol->catalog_count > 1u) {
        qsort(
            vol->catalog,
            vol->catalog_count,
            sizeof(vol->catalog[0]),
            c64_hostfs_cmp_cat);
    }
    return true;
}

c64_hostfs_volume *c64_hostfs_mount(const char *root_path, bool writable)
{
    c64_hostfs_volume *vol;
    const char *base;

    if (!c64_hostfs_path_is_dir(root_path)) {
        return NULL;
    }
    if (strlen(root_path) >= (size_t)C64_HOSTFS_PATH_MAX) {
        return NULL;
    }

    vol = (c64_hostfs_volume *)calloc(1, sizeof(*vol));
    if (vol == NULL) {
        return NULL;
    }
    snprintf(vol->root_path, sizeof(vol->root_path), "%s", root_path);
    snprintf(vol->cwd_path, sizeof(vol->cwd_path), "%s", root_path);
    base = c64_hostfs_basename(vol->root_path);
    c64_hostfs_mangle_cbm(base, vol->display_name, sizeof(vol->display_name), NULL);
    if (vol->display_name[0] == '\0') {
        snprintf(vol->display_name, sizeof(vol->display_name), "HOSTFS");
    }
    vol->writable = writable;
    c64_hostfs_set_status_ok(vol);
    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_eject(vol);
        return NULL;
    }
    return vol;
}

void c64_hostfs_eject(c64_hostfs_volume *vol)
{
    if (vol == NULL) {
        return;
    }
    c64_hostfs_close_all(vol);
    c64_hostfs_leave_d64(vol);
    free(vol->catalog);
    free(vol);
}

const char *c64_hostfs_root_path(const c64_hostfs_volume *vol)
{
    return vol != NULL ? vol->root_path : NULL;
}

const char *c64_hostfs_cwd_path(const c64_hostfs_volume *vol)
{
    return vol != NULL ? vol->cwd_path : NULL;
}

const char *c64_hostfs_d64_path(const c64_hostfs_volume *vol)
{
    if (vol == NULL || vol->d64 == NULL || vol->d64_host_path[0] == '\0') {
        return NULL;
    }
    return vol->d64_host_path;
}

bool c64_hostfs_set_cwd(c64_hostfs_volume *vol, const char *cwd_path)
{
    if (vol == NULL || cwd_path == NULL || cwd_path[0] == '\0') {
        return false;
    }
    if (!c64_hostfs_path_is_dir(cwd_path) ||
        !c64_hostfs_path_under_root(vol, cwd_path)) {
        return false;
    }
    c64_hostfs_leave_d64(vol);
    snprintf(vol->cwd_path, sizeof(vol->cwd_path), "%s", cwd_path);
    if (!c64_hostfs_rescan(vol)) {
        return false;
    }
    c64_hostfs_set_status_ok(vol);
    return true;
}

bool c64_hostfs_reenter_d64(c64_hostfs_volume *vol, const char *d64_host_path)
{
    if (vol == NULL || d64_host_path == NULL || d64_host_path[0] == '\0') {
        return false;
    }
    if (!c64_hostfs_path_under_root(vol, d64_host_path)) {
        return false;
    }
    return c64_hostfs_cd_enter_d64(vol, d64_host_path);
}

const char *c64_hostfs_display_name(const c64_hostfs_volume *vol)
{
    return vol != NULL ? vol->display_name : NULL;
}

bool c64_hostfs_writable(const c64_hostfs_volume *vol)
{
    return vol != NULL && vol->writable;
}

void c64_hostfs_set_writable(c64_hostfs_volume *vol, bool writable)
{
    if (vol != NULL) {
        vol->writable = writable;
    }
}

size_t c64_hostfs_entry_count(const c64_hostfs_volume *vol)
{
    return vol != NULL ? vol->catalog_count : 0u;
}

const c64_drive_directory_entry *c64_hostfs_entries(const c64_hostfs_volume *vol)
{
    return vol != NULL && vol->catalog != NULL ? &vol->catalog[0].dir : NULL;
}

const char *c64_hostfs_entry_host_path(const c64_hostfs_volume *vol, size_t index)
{
    if (vol == NULL || index >= vol->catalog_count) {
        return NULL;
    }
    return vol->catalog[index].host_path;
}

bool c64_hostfs_apply_catalog_to_slot(c64_hostfs_volume *vol, c64_drive_slot *slot)
{
    c64_drive_directory_entry *copy = NULL;
    size_t i;
    const char *title;

    if (vol == NULL || slot == NULL) {
        return false;
    }
    if (!c64_hostfs_rescan(vol)) {
        return false;
    }
    if (vol->catalog_count > 0) {
        copy = (c64_drive_directory_entry *)calloc(
            vol->catalog_count, sizeof(*copy));
        if (copy == NULL) {
            return false;
        }
        for (i = 0; i < vol->catalog_count; i++) {
            copy[i] = vol->catalog[i].dir;
        }
    }
    free(slot->entries);
    slot->entries = copy;
    slot->entry_count = vol->catalog_count;

    if (vol->d64 != NULL) {
        const d64_disk_info *info = d64_image_disk_info(vol->d64);
        if (info != NULL) {
            d64_directory_entry title_entry;
            char disk_title[C64_DRIVE_DISK_TITLE_MAX];

            memset(&title_entry, 0, sizeof(title_entry));
            memcpy(title_entry.filename, info->title, D64_DIRECTORY_NAME_SIZE);
            title_entry.filename_length = info->title_length;
            if (d64_entry_name_ascii(
                    &title_entry, disk_title, sizeof(disk_title)) == D64_OK) {
                snprintf(
                    slot->disk_title, sizeof(slot->disk_title), "%s", disk_title);
            }
            slot->disk_id[0] = (char)info->disk_id[0];
            slot->disk_id[1] = (char)info->disk_id[1];
            slot->disk_id[2] = '\0';
            slot->dos_type[0] = (char)info->dos_type[0];
            slot->dos_type[1] = (char)info->dos_type[1];
            slot->dos_type[2] = '\0';
            slot->free_blocks = info->free_blocks;
            return true;
        }
    }

    slot->free_blocks = 65535u;
    title = c64_hostfs_display_name(vol);
    if (title != NULL && title[0] != '\0') {
        snprintf(slot->disk_title, sizeof(slot->disk_title), "%s", title);
    }
    snprintf(slot->disk_id, sizeof(slot->disk_id), "00");
    snprintf(slot->dos_type, sizeof(slot->dos_type), "2A");
    return true;
}

bool c64_hostfs_read_file(
    const char *host_path, uint8_t **out_bytes, size_t *out_size)
{
    FILE *f;
    long sz;
    uint8_t *buf;

    if (host_path == NULL || out_bytes == NULL || out_size == NULL) {
        return false;
    }
    *out_bytes = NULL;
    *out_size = 0;
    f = fopen(host_path, "rb");
    if (f == NULL) {
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    sz = ftell(f);
    if (sz < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }
    buf = (uint8_t *)malloc((size_t)sz + 1u);
    if (buf == NULL) {
        fclose(f);
        return false;
    }
    if (sz > 0 && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);
    *out_bytes = buf;
    *out_size = (size_t)sz;
    return true;
}

static c64_drive_status_result c64_hostfs_status_from_d64(d64_result result)
{
    switch (result) {
    case D64_OK:
        return C64_DRIVE_STATUS_OK;
    case D64_OUT_OF_MEMORY:
        return C64_DRIVE_STATUS_OUT_OF_MEMORY;
    case D64_DISK_FULL:
    case D64_DIRECTORY_FULL:
        return C64_DRIVE_STATUS_DISK_FULL;
    case D64_FILE_EXISTS:
        return C64_DRIVE_STATUS_FILE_EXISTS;
    case D64_FILE_NOT_FOUND:
        return C64_DRIVE_STATUS_IO_ERROR; /* caller maps to DOS 62 */
    case D64_UNSUPPORTED_IMAGE:
        return C64_DRIVE_STATUS_UNSUPPORTED_IMAGE;
    default:
        return C64_DRIVE_STATUS_IO_ERROR;
    }
}

static int c64_hostfs_find_catalog_index(
    const c64_hostfs_volume *vol, const char *cbm, size_t cbm_len)
{
    size_t i;
    size_t j;

    if (vol == NULL || cbm == NULL) {
        return -1;
    }
    for (i = 0; i < vol->catalog_count; i++) {
        const c64_drive_directory_entry *e = &vol->catalog[i].dir;
        if (e->filename_length != cbm_len) {
            continue;
        }
        for (j = 0; j < cbm_len; j++) {
            unsigned char a = (unsigned char)e->filename[j];
            unsigned char b = (unsigned char)cbm[j];
            if (a >= 'a' && a <= 'z') {
                a = (unsigned char)(a - 'a' + 'A');
            }
            if (b >= 'a' && b <= 'z') {
                b = (unsigned char)(b - 'a' + 'A');
            }
            if (a != b) {
                break;
            }
        }
        if (j == cbm_len) {
            return (int)i;
        }
    }
    return -1;
}

static void c64_hostfs_copy_upper_name(
    const uint8_t *src, size_t src_len, char *dst, size_t dst_size, size_t *out_len)
{
    size_t i;
    size_t n = src_len;

    if (dst == NULL || dst_size == 0) {
        if (out_len != NULL) {
            *out_len = 0;
        }
        return;
    }
    if (n >= dst_size) {
        n = dst_size - 1u;
    }
    for (i = 0; i < n; i++) {
        unsigned char c = src[i];
        if (c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 'a' + 'A');
        }
        dst[i] = (char)c;
    }
    dst[n] = '\0';
    if (out_len != NULL) {
        *out_len = n;
    }
}

/* Strip leading "@:" and optional ",S,R"/",S,W"/",R"/",W"/",S" suffixes. */
static bool c64_hostfs_parse_data_name(
    const uint8_t *name,
    size_t name_length,
    char *out_cbm,
    size_t out_cbm_size,
    size_t *out_cbm_len,
    bool *out_replace,
    bool *out_write,
    bool *out_seq_typed)
{
    size_t i;
    size_t len;
    bool replace = false;
    bool write_mode = false;
    bool seq_typed = false;
    bool have_mode = false;
    char tmp[48];

    if (out_cbm == NULL || out_cbm_size == 0 || out_cbm_len == NULL ||
        out_replace == NULL || out_write == NULL || out_seq_typed == NULL) {
        return false;
    }
    *out_cbm = '\0';
    *out_cbm_len = 0;
    *out_replace = false;
    *out_write = false;
    *out_seq_typed = false;
    if (name == NULL || name_length == 0 || name_length >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, name, name_length);
    tmp[name_length] = '\0';
    len = name_length;
    for (i = 0; i < len; i++) {
        tmp[i] = (char)c64_hostfs_upper((unsigned char)tmp[i]);
    }
    if (len >= 2u && tmp[0] == '@' && tmp[1] == ':') {
        replace = true;
        memmove(tmp, tmp + 2, len - 2u);
        len -= 2u;
        tmp[len] = '\0';
    }
    /* Scan commas from the end for type/mode: ,S,R / ,S,W / ,S / ,R / ,W */
    {
        char *comma = strrchr(tmp, ',');
        while (comma != NULL) {
            char *tok = comma + 1;
            if (tok[0] == 'R' && tok[1] == '\0') {
                write_mode = false;
                have_mode = true;
                *comma = '\0';
            } else if (tok[0] == 'W' && tok[1] == '\0') {
                write_mode = true;
                have_mode = true;
                *comma = '\0';
            } else if (tok[0] == 'S' && tok[1] == '\0') {
                seq_typed = true;
                *comma = '\0';
            } else if (tok[0] == 'P' && tok[1] == '\0') {
                /* PRG on data channel — not supported here. */
                return false;
            } else {
                break;
            }
            len = strlen(tmp);
            comma = strrchr(tmp, ',');
        }
        (void)have_mode;
    }
    if (len == 0u || len > 16u) {
        return false;
    }
    snprintf(out_cbm, out_cbm_size, "%s", tmp);
    *out_cbm_len = len;
    *out_replace = replace;
    *out_write = write_mode;
    *out_seq_typed = seq_typed;
    return true;
}

static void c64_hostfs_channel_clear(c64_hostfs_channel *ch)
{
    if (ch == NULL) {
        return;
    }
    free(ch->data);
    memset(ch, 0, sizeof(*ch));
}

static c64_hostfs_channel *c64_hostfs_channel_find(
    c64_hostfs_volume *vol, uint8_t la)
{
    size_t i;
    if (vol == NULL) {
        return NULL;
    }
    for (i = 0; i < C64_HOSTFS_MAX_CHANNELS; i++) {
        if (vol->channels[i].in_use && vol->channels[i].la == la) {
            return &vol->channels[i];
        }
    }
    return NULL;
}

static c64_hostfs_channel *c64_hostfs_channel_alloc(c64_hostfs_volume *vol)
{
    size_t i;
    if (vol == NULL) {
        return NULL;
    }
    for (i = 0; i < C64_HOSTFS_MAX_CHANNELS; i++) {
        if (!vol->channels[i].in_use) {
            c64_hostfs_channel_clear(&vol->channels[i]);
            vol->channels[i].in_use = true;
            return &vol->channels[i];
        }
    }
    return NULL;
}

static bool c64_hostfs_flush_seq_write(c64_hostfs_volume *vol, c64_hostfs_channel *ch)
{
    FILE *f;

    if (vol == NULL || ch == NULL || ch->kind != C64_HOSTFS_CH_SEQ_WRITE) {
        return false;
    }
    if (ch->host_path[0] == '\0') {
        return false;
    }
    f = fopen(ch->host_path, "wb");
    if (f == NULL) {
        return false;
    }
    if (ch->size > 0u &&
        fwrite(ch->data, 1, ch->size, f) != ch->size) {
        fclose(f);
        return false;
    }
    fclose(f);
    (void)c64_hostfs_rescan(vol);
    return true;
}

static bool c64_hostfs_flush_d64(c64_hostfs_volume *vol)
{
    const uint8_t *bytes;
    size_t size;
    FILE *f;

    if (vol == NULL || vol->d64 == NULL || vol->d64_host_path[0] == '\0') {
        return false;
    }
    bytes = d64_image_bytes(vol->d64, &size);
    if (bytes == NULL || size == 0) {
        return false;
    }
    f = fopen(vol->d64_host_path, "wb");
    if (f == NULL) {
        return false;
    }
    if (fwrite(bytes, 1, size, f) != size) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

bool c64_hostfs_read_entry_prg(
    c64_hostfs_volume *vol,
    size_t index,
    uint8_t **out_bytes,
    size_t *out_size)
{
    if (out_bytes == NULL || out_size == NULL) {
        return false;
    }
    *out_bytes = NULL;
    *out_size = 0;
    if (vol == NULL || index >= vol->catalog_count) {
        return false;
    }

    if (vol->d64 != NULL) {
        d64_directory_entry d64_entry;
        d64_file_data file;
        d64_result result;

        if (d64_image_directory_entry(vol->d64, index, &d64_entry) != D64_OK) {
            return false;
        }
        if (d64_entry.type != D64_FILE_PRG) {
            return false;
        }
        memset(&file, 0, sizeof(file));
        result = d64_image_extract_prg(vol->d64, &d64_entry, &file);
        if (result != D64_OK || file.bytes == NULL) {
            d64_file_data_free(&file);
            return false;
        }
        *out_bytes = file.bytes;
        *out_size = file.size;
        file.bytes = NULL;
        file.size = 0;
        d64_file_data_free(&file);
        return true;
    }

    {
        const char *host = vol->catalog[index].host_path;
        const char *base = c64_hostfs_basename(host);
        size_t base_len = base != NULL ? strlen(base) : 0u;

        if (c64_hostfs_is_pxx_name(base, base_len)) {
            return c64_hostfs_read_p00_prg(host, out_bytes, out_size);
        }
        return c64_hostfs_read_file(host, out_bytes, out_size);
    }
}

bool c64_hostfs_create_prg(
    c64_hostfs_volume *vol,
    const uint8_t *cbm_name,
    size_t cbm_name_length,
    const uint8_t *data,
    size_t data_size,
    c64_drive_status_result *out_status)
{
    char cbm[17];
    char host_name[32];
    char full[C64_HOSTFS_PATH_MAX];
    FILE *f;
    size_t n = 0;
    bool replace = false;
    const uint8_t *name = cbm_name;
    size_t name_len = cbm_name_length;
    int existing;

    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_IO_ERROR;
    }
    if (vol == NULL || cbm_name == NULL || cbm_name_length == 0 || data == NULL) {
        if (vol != NULL) {
            c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
        }
        return false;
    }
    if (name_len >= 2u && name[0] == '@' && name[1] == ':') {
        replace = true;
        name += 2;
        name_len -= 2u;
    }
    if (name_len == 0u || name_len > 16u) {
        c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
        return false;
    }
    if (!vol->writable) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_WRITE_PROTECTED;
        }
        c64_hostfs_apply_drive_status(vol, C64_DRIVE_STATUS_WRITE_PROTECTED);
        return false;
    }

    if (vol->d64 != NULL) {
        d64_result result = d64_image_write_prg(
            vol->d64, cbm_name, cbm_name_length, data, data_size, replace);
        if (result != D64_OK) {
            c64_drive_status_result st = c64_hostfs_status_from_d64(result);
            if (out_status != NULL) {
                *out_status = st;
            }
            if (result == D64_FILE_EXISTS) {
                c64_hostfs_set_status(vol, 63, "FILE EXISTS");
            } else if (result == D64_FILE_NOT_FOUND) {
                c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
            } else {
                c64_hostfs_apply_drive_status(vol, st);
            }
            return false;
        }
        if (!c64_hostfs_flush_d64(vol)) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_IO_ERROR;
            }
            c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
            return false;
        }
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_OK;
        }
        c64_hostfs_set_status_ok(vol);
        (void)c64_hostfs_rescan(vol);
        return true;
    }

    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    c64_hostfs_copy_upper_name(name, name_len, cbm, sizeof(cbm), &n);
    existing = c64_hostfs_find_catalog_index(vol, cbm, n);
    if (existing >= 0) {
        if (!replace) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_FILE_EXISTS;
            }
            c64_hostfs_set_status(vol, 63, "FILE EXISTS");
            return false;
        }
        if (vol->catalog[existing].dir.type == C64_DRIVE_FILE_DIR) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_FILE_EXISTS;
            }
            c64_hostfs_set_status(vol, 63, "FILE EXISTS");
            return false;
        }
        snprintf(
            full,
            sizeof(full),
            "%s",
            vol->catalog[existing].host_path);
    } else {
        /* Host basename: CBM name + .prg under cwd. */
        snprintf(host_name, sizeof(host_name), "%s.prg", cbm);
        if ((size_t)snprintf(
                full, sizeof(full), "%s/%s", vol->cwd_path, host_name) >=
            sizeof(full)) {
            c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
            return false;
        }
    }
    f = fopen(full, "wb");
    if (f == NULL) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    if (data_size > 0 && fwrite(data, 1, data_size, f) != data_size) {
        fclose(f);
        if (existing < 0) {
            remove(full);
        }
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    fclose(f);
    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_OK;
    }
    c64_hostfs_set_status_ok(vol);
    (void)c64_hostfs_rescan(vol);
    return true;
}

bool c64_hostfs_scratch(
    c64_hostfs_volume *vol,
    const uint8_t *cbm_name,
    size_t cbm_name_length,
    c64_drive_status_result *out_status)
{
    char cbm[17];
    size_t n = 0;
    int idx;

    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_IO_ERROR;
    }
    if (vol == NULL || cbm_name == NULL || cbm_name_length == 0 ||
        cbm_name_length > 16u) {
        if (vol != NULL) {
            c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
        }
        return false;
    }
    if (!vol->writable) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_WRITE_PROTECTED;
        }
        c64_hostfs_apply_drive_status(vol, C64_DRIVE_STATUS_WRITE_PROTECTED);
        return false;
    }

    c64_hostfs_copy_upper_name(cbm_name, cbm_name_length, cbm, sizeof(cbm), &n);

    if (vol->d64 != NULL) {
        d64_result result =
            d64_image_scratch(vol->d64, (const uint8_t *)cbm, n);
        if (result == D64_FILE_NOT_FOUND) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_IO_ERROR;
            }
            c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
            return false;
        }
        if (result != D64_OK) {
            c64_drive_status_result st = c64_hostfs_status_from_d64(result);
            if (out_status != NULL) {
                *out_status = st;
            }
            c64_hostfs_apply_drive_status(vol, st);
            return false;
        }
        if (!c64_hostfs_flush_d64(vol)) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_IO_ERROR;
            }
            c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
            return false;
        }
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_OK;
        }
        c64_hostfs_set_status_ok(vol);
        (void)c64_hostfs_rescan(vol);
        return true;
    }

    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    idx = c64_hostfs_find_catalog_index(vol, cbm, n);
    if (idx < 0) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_IO_ERROR;
        }
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    if (vol->catalog[idx].dir.type == C64_DRIVE_FILE_DIR) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_IO_ERROR;
        }
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    if (remove(vol->catalog[idx].host_path) != 0) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_IO_ERROR;
        }
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_OK;
    }
    c64_hostfs_set_status_ok(vol);
    (void)c64_hostfs_rescan(vol);
    return true;
}

bool c64_hostfs_open_seq(
    c64_hostfs_volume *vol,
    uint8_t la,
    uint8_t sa,
    const uint8_t *name,
    size_t name_length,
    c64_drive_status_result *out_status)
{
    char cbm[17];
    size_t cbm_len = 0;
    bool replace = false;
    bool write_mode = false;
    bool seq_typed = false;
    c64_hostfs_channel *ch;
    int idx;

    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_IO_ERROR;
    }
    if (vol == NULL) {
        return false;
    }
    /* Nested D64 SEQ I/O is out of scope for PR6. */
    if (vol->d64 != NULL) {
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    if (!c64_hostfs_parse_data_name(
            name,
            name_length,
            cbm,
            sizeof(cbm),
            &cbm_len,
            &replace,
            &write_mode,
            &seq_typed)) {
        c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
        return false;
    }
    if (c64_hostfs_channel_find(vol, la) != NULL) {
        c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
        return false;
    }
    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }

    if (write_mode) {
        char host_name[32];
        char full[C64_HOSTFS_PATH_MAX];

        if (!vol->writable) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_WRITE_PROTECTED;
            }
            c64_hostfs_apply_drive_status(vol, C64_DRIVE_STATUS_WRITE_PROTECTED);
            return false;
        }
        idx = c64_hostfs_find_catalog_index(vol, cbm, cbm_len);
        if (idx >= 0) {
            if (!replace) {
                if (out_status != NULL) {
                    *out_status = C64_DRIVE_STATUS_FILE_EXISTS;
                }
                c64_hostfs_set_status(vol, 63, "FILE EXISTS");
                return false;
            }
            if (vol->catalog[idx].dir.type == C64_DRIVE_FILE_DIR) {
                c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
                return false;
            }
            snprintf(full, sizeof(full), "%s", vol->catalog[idx].host_path);
        } else {
            snprintf(host_name, sizeof(host_name), "%s.seq", cbm);
            if ((size_t)snprintf(
                    full, sizeof(full), "%s/%s", vol->cwd_path, host_name) >=
                sizeof(full)) {
                c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
                return false;
            }
        }
        ch = c64_hostfs_channel_alloc(vol);
        if (ch == NULL) {
            c64_hostfs_set_status(vol, 70, "NO CHANNEL");
            return false;
        }
        ch->la = la;
        ch->sa = sa;
        ch->kind = C64_HOSTFS_CH_SEQ_WRITE;
        ch->data = NULL;
        ch->size = 0;
        ch->pos = 0;
        ch->cap = 0;
        snprintf(ch->host_path, sizeof(ch->host_path), "%s", full);
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_OK;
        }
        c64_hostfs_set_status_ok(vol);
        return true;
    }

    /* Read mode (default). */
    idx = c64_hostfs_find_catalog_index(vol, cbm, cbm_len);
    if (idx < 0 || vol->catalog[idx].dir.type != C64_DRIVE_FILE_SEQ) {
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    ch = c64_hostfs_channel_alloc(vol);
    if (ch == NULL) {
        c64_hostfs_set_status(vol, 70, "NO CHANNEL");
        return false;
    }
    if (!c64_hostfs_read_file(
            vol->catalog[idx].host_path, &ch->data, &ch->size)) {
        c64_hostfs_channel_clear(ch);
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    ch->la = la;
    ch->sa = sa;
    ch->kind = C64_HOSTFS_CH_SEQ_READ;
    ch->pos = 0;
    ch->cap = ch->size;
    snprintf(
        ch->host_path,
        sizeof(ch->host_path),
        "%s",
        vol->catalog[idx].host_path);
    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_OK;
    }
    c64_hostfs_set_status_ok(vol);
    (void)seq_typed;
    return true;
}

bool c64_hostfs_channel_is_seq_read(const c64_hostfs_volume *vol, uint8_t la)
{
    const c64_hostfs_channel *ch;
    if (vol == NULL) {
        return false;
    }
    ch = c64_hostfs_channel_find((c64_hostfs_volume *)vol, la);
    return ch != NULL && ch->kind == C64_HOSTFS_CH_SEQ_READ;
}

bool c64_hostfs_channel_is_seq_write(const c64_hostfs_volume *vol, uint8_t la)
{
    const c64_hostfs_channel *ch;
    if (vol == NULL) {
        return false;
    }
    ch = c64_hostfs_channel_find((c64_hostfs_volume *)vol, la);
    return ch != NULL && ch->kind == C64_HOSTFS_CH_SEQ_WRITE;
}

bool c64_hostfs_seq_read_byte(
    c64_hostfs_volume *vol, uint8_t la, uint8_t *out_byte, bool *out_eoi)
{
    c64_hostfs_channel *ch = c64_hostfs_channel_find(vol, la);

    if (out_byte == NULL || out_eoi == NULL) {
        return false;
    }
    *out_eoi = false;
    if (ch == NULL || ch->kind != C64_HOSTFS_CH_SEQ_READ) {
        return false;
    }
    if (ch->pos >= ch->size) {
        *out_byte = 0x0d;
        *out_eoi = true;
        return true;
    }
    *out_byte = ch->data[ch->pos++];
    if (ch->pos >= ch->size) {
        *out_eoi = true;
    }
    return true;
}

bool c64_hostfs_seq_write_byte(
    c64_hostfs_volume *vol, uint8_t la, uint8_t byte)
{
    c64_hostfs_channel *ch = c64_hostfs_channel_find(vol, la);
    uint8_t *grown;

    if (ch == NULL || ch->kind != C64_HOSTFS_CH_SEQ_WRITE) {
        return false;
    }
    if (ch->size + 1u > ch->cap) {
        size_t ncap = ch->cap == 0u ? 256u : ch->cap * 2u;
        grown = (uint8_t *)realloc(ch->data, ncap);
        if (grown == NULL) {
            return false;
        }
        ch->data = grown;
        ch->cap = ncap;
    }
    ch->data[ch->size++] = byte;
    return true;
}

bool c64_hostfs_close_la(c64_hostfs_volume *vol, uint8_t la)
{
    c64_hostfs_channel *ch = c64_hostfs_channel_find(vol, la);

    if (ch == NULL) {
        return false;
    }
    if (ch->kind == C64_HOSTFS_CH_SEQ_WRITE) {
        (void)c64_hostfs_flush_seq_write(vol, ch);
    }
    c64_hostfs_channel_clear(ch);
    return true;
}

bool c64_hostfs_discard_la(c64_hostfs_volume *vol, uint8_t la)
{
    c64_hostfs_channel *ch = c64_hostfs_channel_find(vol, la);

    if (ch == NULL) {
        return false;
    }
    c64_hostfs_channel_clear(ch);
    return true;
}

void c64_hostfs_close_all(c64_hostfs_volume *vol)
{
    size_t i;
    if (vol == NULL) {
        return;
    }
    for (i = 0; i < C64_HOSTFS_MAX_CHANNELS; i++) {
        if (vol->channels[i].in_use) {
            if (vol->channels[i].kind == C64_HOSTFS_CH_SEQ_WRITE) {
                (void)c64_hostfs_flush_seq_write(vol, &vol->channels[i]);
            }
            c64_hostfs_channel_clear(&vol->channels[i]);
        }
    }
}

static bool c64_hostfs_path_under_root(
    const c64_hostfs_volume *vol, const char *path)
{
    size_t root_len;

    if (vol == NULL || path == NULL) {
        return false;
    }
    root_len = strlen(vol->root_path);
    if (root_len == 0) {
        return false;
    }
    if (strncmp(path, vol->root_path, root_len) != 0) {
        return false;
    }
    if (path[root_len] != '\0' && path[root_len] != '/' && path[root_len] != '\\') {
        return false;
    }
    return true;
}

static bool c64_hostfs_parent_path(const char *path, char *out, size_t out_size)
{
    char tmp[C64_HOSTFS_PATH_MAX];
    char *slash;
    size_t len;

    if (path == NULL || out == NULL || out_size == 0) {
        return false;
    }
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    while (len > 1u && (tmp[len - 1u] == '/' || tmp[len - 1u] == '\\')) {
        tmp[--len] = '\0';
    }
    slash = strrchr(tmp, '/');
#if defined(_WIN32)
    {
        char *bslash = strrchr(tmp, '\\');
        if (bslash != NULL && (slash == NULL || bslash > slash)) {
            slash = bslash;
        }
    }
#endif
    if (slash == NULL) {
        return false;
    }
    if (slash == tmp) {
        snprintf(out, out_size, "/");
        return true;
    }
    *slash = '\0';
    snprintf(out, out_size, "%s", tmp);
    return true;
}

static unsigned char c64_hostfs_upper(unsigned char c)
{
    if (c >= 'a' && c <= 'z') {
        return (unsigned char)(c - 'a' + 'A');
    }
    return c;
}

static bool c64_hostfs_name_eq(
    const uint8_t *a, size_t a_len, const char *b, size_t b_len)
{
    size_t i;
    if (a_len != b_len) {
        return false;
    }
    for (i = 0; i < a_len; i++) {
        if (c64_hostfs_upper(a[i]) != c64_hostfs_upper((unsigned char)b[i])) {
            return false;
        }
    }
    return true;
}

static bool c64_hostfs_cd_enter_d64(
    c64_hostfs_volume *vol, const char *host_path)
{
    uint8_t *bytes = NULL;
    size_t size = 0;
    d64_result result = D64_OK;
    d64_image *image;

    if (vol->d64 != NULL) {
        /* Flat images only — no nested CD into another D64. */
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    if (!c64_hostfs_read_file(host_path, &bytes, &size)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    image = d64_image_create(bytes, size, &result);
    free(bytes);
    if (image == NULL) {
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    vol->d64 = image;
    snprintf(vol->d64_host_path, sizeof(vol->d64_host_path), "%s", host_path);
    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_leave_d64(vol);
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    c64_hostfs_set_status_ok(vol);
    return true;
}

static bool c64_hostfs_cd_root(c64_hostfs_volume *vol)
{
    c64_hostfs_leave_d64(vol);
    snprintf(vol->cwd_path, sizeof(vol->cwd_path), "%s", vol->root_path);
    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    c64_hostfs_set_status_ok(vol);
    return true;
}

static bool c64_hostfs_cd_parent(c64_hostfs_volume *vol)
{
    char parent[C64_HOSTFS_PATH_MAX];

    /* Leaving a nested D64 returns to the host folder that listed it. */
    if (vol->d64 != NULL) {
        c64_hostfs_leave_d64(vol);
        if (!c64_hostfs_rescan(vol)) {
            c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
            return false;
        }
        c64_hostfs_set_status_ok(vol);
        return true;
    }

    if (strcmp(vol->cwd_path, vol->root_path) == 0) {
        c64_hostfs_set_status_ok(vol);
        return true;
    }
    if (!c64_hostfs_parent_path(vol->cwd_path, parent, sizeof(parent)) ||
        !c64_hostfs_path_under_root(vol, parent)) {
        snprintf(vol->cwd_path, sizeof(vol->cwd_path), "%s", vol->root_path);
    } else {
        snprintf(vol->cwd_path, sizeof(vol->cwd_path), "%s", parent);
    }
    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    c64_hostfs_set_status_ok(vol);
    return true;
}

static bool c64_hostfs_cd_enter_name(
    c64_hostfs_volume *vol, const uint8_t *name, size_t name_len)
{
    size_t i;
    const char *host;

    if (name_len == 0 || name_len > 16u) {
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    /* Inside a D64 there are no enterable DIRs (images are flat). */
    if (vol->d64 != NULL) {
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    for (i = 0; i < vol->catalog_count; i++) {
        const c64_drive_directory_entry *e = &vol->catalog[i].dir;
        if (e->type != C64_DRIVE_FILE_DIR) {
            continue;
        }
        if (!c64_hostfs_name_eq(
                name, name_len, (const char *)e->filename, e->filename_length)) {
            continue;
        }
        host = vol->catalog[i].host_path;
        if (host == NULL || host[0] == '\0' ||
            !c64_hostfs_path_under_root(vol, host)) {
            c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
            return false;
        }
        if (c64_hostfs_path_is_dir(host)) {
            snprintf(vol->cwd_path, sizeof(vol->cwd_path), "%s", host);
            if (!c64_hostfs_rescan(vol)) {
                c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
                return false;
            }
            c64_hostfs_set_status_ok(vol);
            return true;
        }
        if (c64_hostfs_path_is_d64_file(host)) {
            return c64_hostfs_cd_enter_d64(vol, host);
        }
        c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
        return false;
    }
    c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
    return false;
}

static void c64_hostfs_trim_slashes(uint8_t *s, size_t *len)
{
    while (*len > 0u && (s[*len - 1u] == '/' || s[*len - 1u] == '\\')) {
        (*len)--;
        s[*len] = '\0';
    }
}

bool c64_hostfs_command(
    c64_hostfs_volume *vol,
    const uint8_t *name,
    size_t name_length,
    c64_drive_status_result *out_status)
{
    uint8_t buf[48];
    size_t len;
    size_t i;
    bool ok;

    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_OK;
    }
    if (vol == NULL) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_IO_ERROR;
        }
        return false;
    }

    /* Empty name: open command/status channel only. */
    if (name == NULL || name_length == 0) {
        c64_hostfs_set_status_ok(vol);
        return true;
    }
    if (name_length >= sizeof(buf)) {
        c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_IO_ERROR;
        }
        return true;
    }
    memcpy(buf, name, name_length);
    buf[name_length] = '\0';
    len = name_length;

    /* Uppercase ASCII letters for parsing; keep $5F left-arrow. */
    for (i = 0; i < len; i++) {
        buf[i] = c64_hostfs_upper(buf[i]);
    }

    /* Scratch: S:NAME or S0:NAME (exact name, no wildcards). */
    if (len >= 2u && buf[0] == 'S') {
        size_t start = 1u;
        if (start < len && buf[start] >= '0' && buf[start] <= '9') {
            start++;
        }
        if (start >= len || buf[start] != ':') {
            c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_IO_ERROR;
            }
            return true;
        }
        start++;
        if (start >= len || (len - start) > 16u) {
            c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_IO_ERROR;
            }
            return true;
        }
        ok = c64_hostfs_scratch(vol, buf + start, len - start, out_status);
        (void)ok;
        return true;
    }

    /* Require CD…, or bare // / _ shorthands. */
    {
        bool is_cd = (len >= 2u && buf[0] == 'C' && buf[1] == 'D');
        bool bare_root = (len == 2u && buf[0] == '/' && buf[1] == '/') ||
            (len == 1u && buf[0] == '/');
        bool bare_parent =
            (len == 1u && buf[0] == (uint8_t)C64_HOSTFS_PETSCII_LEFT_ARROW);
        if (!is_cd && !bare_root && !bare_parent) {
            c64_hostfs_set_status(vol, 30, "SYNTAX ERROR");
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_IO_ERROR;
            }
            return true;
        }
        if (is_cd) {
            memmove(buf, buf + 2, len - 2u);
            len -= 2u;
            buf[len] = '\0';
            if (len > 0u && buf[0] == ':') {
                memmove(buf, buf + 1, len - 1u);
                len -= 1u;
                buf[len] = '\0';
            }
        }
    }

    c64_hostfs_trim_slashes(buf, &len);

    /* Root: empty after CD, or "//", or "/" */
    if (len == 0u || (len == 1u && buf[0] == '/') ||
        (len == 2u && buf[0] == '/' && buf[1] == '/')) {
        ok = c64_hostfs_cd_root(vol);
        if (out_status != NULL) {
            *out_status = ok ? C64_DRIVE_STATUS_OK : C64_DRIVE_STATUS_IO_ERROR;
        }
        return true;
    }

    /* Parent: "_" / left-arrow, or ".." */
    if ((len == 1u && buf[0] == (uint8_t)C64_HOSTFS_PETSCII_LEFT_ARROW) ||
        (len == 2u && buf[0] == '.' && buf[1] == '.')) {
        ok = c64_hostfs_cd_parent(vol);
        if (out_status != NULL) {
            *out_status = ok ? C64_DRIVE_STATUS_OK : C64_DRIVE_STATUS_IO_ERROR;
        }
        return true;
    }

    /* Absolute-from-root: "//NAME" or "/NAME" */
    if (buf[0] == '/') {
        size_t start = 1u;
        while (start < len && buf[start] == '/') {
            start++;
        }
        ok = c64_hostfs_cd_root(vol);
        if (!ok) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_IO_ERROR;
            }
            return true;
        }
        if (start >= len) {
            if (out_status != NULL) {
                *out_status = C64_DRIVE_STATUS_OK;
            }
            return true;
        }
        /* Single path component for v1 (FB enters one level at a time). */
        ok = c64_hostfs_cd_enter_name(vol, buf + start, len - start);
        if (out_status != NULL) {
            *out_status = ok ? C64_DRIVE_STATUS_OK : C64_DRIVE_STATUS_IO_ERROR;
        }
        return true;
    }

    /* Relative NAME (FB CD:NAME). */
    ok = c64_hostfs_cd_enter_name(vol, buf, len);
    if (out_status != NULL) {
        *out_status = ok ? C64_DRIVE_STATUS_OK : C64_DRIVE_STATUS_IO_ERROR;
    }
    return true;
}
