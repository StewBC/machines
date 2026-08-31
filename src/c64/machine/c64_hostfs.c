/* c64 HostFS — host directory volume + catalog / PRG I/O + CD. */
#include "c64_hostfs.h"

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

struct c64_hostfs_volume {
    char root_path[C64_HOSTFS_PATH_MAX];
    char cwd_path[C64_HOSTFS_PATH_MAX];
    char display_name[C64_HOSTFS_NAME_MAX];
    char status[C64_HOSTFS_STATUS_MAX];
    size_t status_length;
    bool writable;
    c64_hostfs_cat_entry *catalog;
    size_t catalog_count;
    size_t catalog_cap;
};

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

bool c64_hostfs_rescan(c64_hostfs_volume *vol)
{
    DIR *dir;
    struct dirent *de;

    if (vol == NULL) {
        return false;
    }
    free(vol->catalog);
    vol->catalog = NULL;
    vol->catalog_count = 0;
    vol->catalog_cap = 0;

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
        /* Phase 0: only .prg (case-insensitive). */
        if (name_len < 5 || name[name_len - 4] != '.' ||
            (name[name_len - 3] != 'p' && name[name_len - 3] != 'P') ||
            (name[name_len - 2] != 'r' && name[name_len - 2] != 'R') ||
            (name[name_len - 1] != 'g' && name[name_len - 1] != 'G')) {
            fprintf(stderr, "HostFS: skip (not .prg): %s\n", name);
            continue;
        }
        {
            char stem[C64_HOSTFS_BASENAME_MAX];
            size_t stem_len = name_len - 4u;
            if (stem_len >= sizeof(stem)) {
                stem_len = sizeof(stem) - 1u;
            }
            memcpy(stem, name, stem_len);
            stem[stem_len] = '\0';
            c64_hostfs_mangle_cbm(stem, cbm, sizeof(cbm), &cbm_len);
            c64_hostfs_unique_cbm(vol, cbm, sizeof(cbm), &cbm_len);
            if (!c64_hostfs_catalog_push(
                    vol,
                    full,
                    cbm,
                    cbm_len,
                    C64_DRIVE_FILE_PRG,
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
    size_t i;
    size_t n;

    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_IO_ERROR;
    }
    if (vol == NULL || cbm_name == NULL || cbm_name_length == 0 ||
        cbm_name_length > 16u || data == NULL) {
        return false;
    }
    if (!vol->writable) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_WRITE_PROTECTED;
        }
        return false;
    }
    if (!c64_hostfs_rescan(vol)) {
        return false;
    }
    n = cbm_name_length;
    for (i = 0; i < n; i++) {
        unsigned char c = cbm_name[i];
        if (c >= 'a' && c <= 'z') {
            c = (unsigned char)(c - 'a' + 'A');
        }
        cbm[i] = (char)c;
    }
    cbm[n] = '\0';
    if (c64_hostfs_catalog_has_name(vol, cbm, n)) {
        if (out_status != NULL) {
            *out_status = C64_DRIVE_STATUS_FILE_EXISTS;
        }
        return false;
    }
    /* Host basename: CBM name + .prg under cwd. */
    snprintf(host_name, sizeof(host_name), "%s.prg", cbm);
    if ((size_t)snprintf(full, sizeof(full), "%s/%s", vol->cwd_path, host_name) >=
        sizeof(full)) {
        return false;
    }
    f = fopen(full, "wb");
    if (f == NULL) {
        return false;
    }
    if (data_size > 0 && fwrite(data, 1, data_size, f) != data_size) {
        fclose(f);
        remove(full);
        return false;
    }
    fclose(f);
    if (out_status != NULL) {
        *out_status = C64_DRIVE_STATUS_OK;
    }
    (void)c64_hostfs_rescan(vol);
    return true;
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

static bool c64_hostfs_cd_root(c64_hostfs_volume *vol)
{
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
    if (!c64_hostfs_rescan(vol)) {
        c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
        return false;
    }
    for (i = 0; i < vol->catalog_count; i++) {
        const c64_drive_directory_entry *e = &vol->catalog[i].dir;
        if (e->type != C64_DRIVE_FILE_DIR) {
            continue;
        }
        if (!c64_hostfs_name_eq(name, name_len, (const char *)e->filename, e->filename_length)) {
            continue;
        }
        host = vol->catalog[i].host_path;
        if (host == NULL || !c64_hostfs_path_is_dir(host) ||
            !c64_hostfs_path_under_root(vol, host)) {
            c64_hostfs_set_status(vol, 62, "FILE NOT FOUND");
            return false;
        }
        snprintf(vol->cwd_path, sizeof(vol->cwd_path), "%s", host);
        if (!c64_hostfs_rescan(vol)) {
            c64_hostfs_set_status(vol, 74, "DRIVE NOT READY");
            return false;
        }
        c64_hostfs_set_status_ok(vol);
        return true;
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
