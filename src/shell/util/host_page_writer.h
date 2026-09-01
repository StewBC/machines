#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum host_page_format {
    HOST_PAGE_FORMAT_BMP = 0,
    HOST_PAGE_FORMAT_PNG, /* reserved; unsupported */
    HOST_PAGE_FORMAT_PDF_IMAGES /* reserved; unsupported */
} host_page_format;

typedef struct host_page_image {
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
    /* 8bpp grayscale: 0=black ink, 255=paper; or 1bpp packed MSB-first. */
    uint8_t bits_per_pixel; /* 1 or 8 */
    const uint8_t *pixels;
} host_page_image;

bool host_page_writer_write(
    const host_page_image *page,
    host_page_format format,
    const char *path);

bool host_page_writer_ensure_dir(const char *dir_path);
