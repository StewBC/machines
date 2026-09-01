#include "host_page_writer.h"

#include "platform_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define host_page_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define host_page_mkdir(path) mkdir((path), 0777)
#endif

#define HOST_PAGE_BMP_FILE_HEADER_SIZE 14u
#define HOST_PAGE_BMP_INFO_HEADER_SIZE 40u
#define HOST_PAGE_PATH_MAX 1024

static uint32_t host_page_row_bytes(uint32_t width, uint8_t bpp)
{
    if (bpp == 1u) {
        return (width + 7u) / 8u;
    }
    return width;
}

static uint32_t host_page_bmp_stride(uint32_t width, uint8_t bpp)
{
    uint32_t raw = host_page_row_bytes(width, bpp);
    return (raw + 3u) & ~3u;
}

static void host_page_write_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)((value >> 8) & 0xffu);
}

static void host_page_write_u32_le(uint8_t *dst, uint32_t value)
{
    dst[0] = (uint8_t)(value & 0xffu);
    dst[1] = (uint8_t)((value >> 8) & 0xffu);
    dst[2] = (uint8_t)((value >> 16) & 0xffu);
    dst[3] = (uint8_t)((value >> 24) & 0xffu);
}

static bool host_page_mkdir_one(const char *path)
{
    if (platform_fs_is_dir(path)) {
        return true;
    }
    if (host_page_mkdir(path) == 0) {
        return true;
    }
    /* Another creator may have won the race. */
    return platform_fs_is_dir(path);
}

bool host_page_writer_ensure_dir(const char *dir_path)
{
    char path[HOST_PAGE_PATH_MAX];
    size_t len;
    size_t i;

    if (dir_path == NULL || dir_path[0] == '\0') {
        return false;
    }

    len = strlen(dir_path);
    if (len >= sizeof(path)) {
        return false;
    }
    memcpy(path, dir_path, len + 1u);

    /* Skip Windows drive prefix like "C:". */
    i = 0u;
#if defined(_WIN32)
    if (len >= 2u && path[1] == ':') {
        i = 2u;
    }
#endif
    /* Skip leading separators so absolute paths keep their root. */
    while (i < len && (path[i] == '/' || path[i] == '\\')) {
        i++;
    }

    for (; i <= len; i++) {
        char saved;
        bool at_sep = (i < len && (path[i] == '/' || path[i] == '\\'));
        bool at_end = (i == len);

        if (!at_sep && !at_end) {
            continue;
        }
        if (i == 0u) {
            continue;
        }

        saved = path[i];
        path[i] = '\0';
        if (!host_page_mkdir_one(path)) {
            path[i] = saved;
            return false;
        }
        path[i] = saved;
    }

    return platform_fs_is_dir(dir_path);
}

static bool host_page_write_bmp(const host_page_image *page, const char *tmp_path)
{
    FILE *fp;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint32_t palette_entries;
    uint32_t palette_bytes;
    uint32_t bmp_stride;
    uint32_t pixel_bytes;
    uint32_t off_bits;
    uint32_t file_size;
    uint8_t header[HOST_PAGE_BMP_FILE_HEADER_SIZE + HOST_PAGE_BMP_INFO_HEADER_SIZE];
    uint8_t *row = NULL;
    uint32_t y;
    uint32_t i;
    bool ok = false;

    width = page->width;
    height = page->height;
    bpp = page->bits_per_pixel;

    if (width == 0u || height == 0u || page->pixels == NULL) {
        return false;
    }
    if (bpp != 1u && bpp != 8u) {
        return false;
    }
    if (page->stride_bytes < host_page_row_bytes(width, bpp)) {
        return false;
    }

    palette_entries = (bpp == 1u) ? 2u : 256u;
    palette_bytes = palette_entries * 4u;
    bmp_stride = host_page_bmp_stride(width, bpp);
    pixel_bytes = bmp_stride * height;
    off_bits = HOST_PAGE_BMP_FILE_HEADER_SIZE + HOST_PAGE_BMP_INFO_HEADER_SIZE + palette_bytes;
    file_size = off_bits + pixel_bytes;

    memset(header, 0, sizeof(header));
    header[0] = 'B';
    header[1] = 'M';
    host_page_write_u32_le(header + 2, file_size);
    host_page_write_u32_le(header + 10, off_bits);
    host_page_write_u32_le(header + 14, HOST_PAGE_BMP_INFO_HEADER_SIZE);
    host_page_write_u32_le(header + 18, width);
    host_page_write_u32_le(header + 22, height);
    host_page_write_u16_le(header + 26, 1u);
    host_page_write_u16_le(header + 28, bpp);
    host_page_write_u32_le(header + 34, pixel_bytes);
    host_page_write_u32_le(header + 46, palette_entries);

    fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        return false;
    }

    if (fwrite(header, 1, sizeof(header), fp) != sizeof(header)) {
        goto done;
    }

    /* Palette: index 0 = black ink, high index = paper (white). */
    for (i = 0u; i < palette_entries; i++) {
        uint8_t entry[4];
        uint8_t gray;

        if (bpp == 1u) {
            gray = (i == 0u) ? 0u : 255u;
        } else {
            gray = (uint8_t)i;
        }
        entry[0] = gray;
        entry[1] = gray;
        entry[2] = gray;
        entry[3] = 0u;
        if (fwrite(entry, 1, sizeof(entry), fp) != sizeof(entry)) {
            goto done;
        }
    }

    row = (uint8_t *)calloc(1, bmp_stride);
    if (row == NULL) {
        goto done;
    }

    /* BMP stores rows bottom-up. */
    for (y = 0u; y < height; y++) {
        uint32_t src_y = height - 1u - y;
        const uint8_t *src = page->pixels + (size_t)src_y * (size_t)page->stride_bytes;
        uint32_t copy = host_page_row_bytes(width, bpp);

        memcpy(row, src, copy);
        if (copy < bmp_stride) {
            memset(row + copy, 0, bmp_stride - copy);
        }
        if (fwrite(row, 1, bmp_stride, fp) != bmp_stride) {
            goto done;
        }
    }

    if (fflush(fp) != 0) {
        goto done;
    }
    ok = true;

done:
    free(row);
    if (fp != NULL) {
        fclose(fp);
    }
    if (!ok) {
        remove(tmp_path);
    }
    return ok;
}

bool host_page_writer_write(
    const host_page_image *page,
    host_page_format format,
    const char *path)
{
    char tmp_path[HOST_PAGE_PATH_MAX];
    size_t path_len;

    if (page == NULL || path == NULL || path[0] == '\0') {
        return false;
    }
    if (format != HOST_PAGE_FORMAT_BMP) {
        return false;
    }

    path_len = strlen(path);
    if (path_len + 4u >= sizeof(tmp_path)) {
        return false;
    }
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    if (!host_page_write_bmp(page, tmp_path)) {
        return false;
    }

#if defined(_WIN32)
    /* Win32 rename() refuses to replace an existing destination. */
    remove(path);
#endif
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}
