#include "host_page_writer.h"
#include "platform_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define test_rmdir _rmdir
#else
#include <unistd.h>
#define test_rmdir rmdir
#endif

static void expect_true(const char *name, bool value)
{
    if (!value) {
        fprintf(stderr, "FAIL: %s: expected true\n", name);
        exit(1);
    }
}

static void expect_false(const char *name, bool value)
{
    if (value) {
        fprintf(stderr, "FAIL: %s: expected false\n", name);
        exit(1);
    }
}

static void expect_eq_u32(const char *name, uint32_t expected, uint32_t actual)
{
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected %u, got %u\n", name, expected, actual);
        exit(1);
    }
}

static void expect_eq_u8(const char *name, uint8_t expected, uint8_t actual)
{
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected %u, got %u\n", name, expected, actual);
        exit(1);
    }
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8));
}

static uint8_t *read_file_bytes(const char *path, size_t *out_size)
{
    FILE *fp;
    long size;
    uint8_t *buf;
    size_t nread;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    rewind(fp);
    buf = (uint8_t *)malloc((size_t)size);
    if (buf == NULL) {
        fclose(fp);
        return NULL;
    }
    nread = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    if (nread != (size_t)size) {
        free(buf);
        return NULL;
    }
    *out_size = (size_t)size;
    return buf;
}

static uint32_t bmp_expected_size(uint32_t width, uint32_t height, uint8_t bpp)
{
    uint32_t row_raw = (bpp == 1u) ? ((width + 7u) / 8u) : width;
    uint32_t stride = (row_raw + 3u) & ~3u;
    uint32_t palette = (bpp == 1u) ? (2u * 4u) : (256u * 4u);
    uint32_t off_bits = 14u + 40u + palette;
    return off_bits + stride * height;
}

static uint32_t bmp_expected_off_bits(uint8_t bpp)
{
    uint32_t palette = (bpp == 1u) ? (2u * 4u) : (256u * 4u);
    return 14u + 40u + palette;
}

static void cleanup_path(const char *path)
{
    remove(path);
}

static void test_reject_unsupported_formats(void)
{
    uint8_t pixels[8];
    host_page_image page;

    memset(pixels, 0xff, sizeof(pixels));
    memset(&page, 0, sizeof(page));
    page.width = 8;
    page.height = 1;
    page.stride_bytes = 8;
    page.bits_per_pixel = 8;
    page.pixels = pixels;

    expect_false(
        "png rejected",
        host_page_writer_write(&page, HOST_PAGE_FORMAT_PNG, "hpw_reject.png"));
    expect_false(
        "pdf rejected",
        host_page_writer_write(&page, HOST_PAGE_FORMAT_PDF_IMAGES, "hpw_reject.pdf"));
    expect_false("null page rejected", host_page_writer_write(NULL, HOST_PAGE_FORMAT_BMP, "x.bmp"));
    expect_false("null path rejected", host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, NULL));
}

static void test_reject_overflow_dimensions(void)
{
    uint8_t pixel = 0;
    host_page_image page;

    memset(&page, 0, sizeof(page));
    /* 8bpp stride*height overflows uint32 (0x10000 * 0x10000). */
    page.width = 0x10000u;
    page.height = 0x10000u;
    page.stride_bytes = 0x10000u;
    page.bits_per_pixel = 8;
    page.pixels = &pixel;

    expect_false(
        "overflow dimensions rejected",
        host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, "hpw_overflow.bmp"));
}

static void test_ensure_dir(void)
{
    const char *nested = "hpw_scratch/nested/out";

    remove("hpw_scratch/nested/out/.keep");
    test_rmdir("hpw_scratch/nested/out");
    test_rmdir("hpw_scratch/nested");
    test_rmdir("hpw_scratch");

    expect_true("ensure_dir creates nested", host_page_writer_ensure_dir(nested));
    expect_true("nested is_dir", platform_fs_is_dir(nested));
    expect_true("ensure_dir idempotent", host_page_writer_ensure_dir(nested));
    expect_false("ensure_dir null", host_page_writer_ensure_dir(NULL));
    expect_false("ensure_dir empty", host_page_writer_ensure_dir(""));

    test_rmdir("hpw_scratch/nested/out");
    test_rmdir("hpw_scratch/nested");
    test_rmdir("hpw_scratch");
}

static void test_write_8bpp_bmp(void)
{
    const char *path = "hpw_scratch_8.bmp";
    const char *tmp_path = "hpw_scratch_8.bmp.tmp";
    /* width=2 => raw row 2 bytes, BMP stride 4 (2 pad bytes). */
    uint8_t pixels[2 * 3];
    host_page_image page;
    uint8_t *body;
    size_t size;
    size_t tmp_size;
    uint32_t expected;
    uint32_t off_bits;
    const uint8_t *palette;
    const uint8_t *first_row;

    cleanup_path(path);
    cleanup_path(tmp_path);

    /* Distinct top/middle/bottom rows so bottom-up order is observable. */
    pixels[0] = 0x11;
    pixels[1] = 0x12; /* top */
    pixels[2] = 0x21;
    pixels[3] = 0x22; /* middle */
    pixels[4] = 0x31;
    pixels[5] = 0x32; /* bottom */

    memset(&page, 0, sizeof(page));
    page.width = 2;
    page.height = 3;
    page.stride_bytes = 2;
    page.bits_per_pixel = 8;
    page.pixels = pixels;

    expect_true("write 8bpp bmp", host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, path));
    expect_true("final path exists", (body = read_file_bytes(path, &size)) != NULL);
    expect_false("tmp removed after success", read_file_bytes(tmp_path, &tmp_size) != NULL);

    expected = bmp_expected_size(2, 3, 8);
    off_bits = bmp_expected_off_bits(8);
    expect_eq_u32("8bpp file size", expected, (uint32_t)size);
    expect_true("BM magic", size >= 2u && body[0] == 'B' && body[1] == 'M');
    expect_eq_u32("bfSize", expected, read_u32_le(body + 2));
    expect_eq_u32("bfOffBits", off_bits, read_u32_le(body + 10));
    expect_eq_u32("biWidth", 2u, read_u32_le(body + 18));
    expect_eq_u32("biHeight", 3u, read_u32_le(body + 22));
    expect_eq_u32("biBitCount", 8u, (uint32_t)read_u16_le(body + 28));

    palette = body + 14u + 40u;
    expect_eq_u8("palette0 B", 0u, palette[0]);
    expect_eq_u8("palette0 G", 0u, palette[1]);
    expect_eq_u8("palette0 R", 0u, palette[2]);
    expect_eq_u8("palette255 B", 255u, palette[255 * 4 + 0]);
    expect_eq_u8("palette255 G", 255u, palette[255 * 4 + 1]);
    expect_eq_u8("palette255 R", 255u, palette[255 * 4 + 2]);

    first_row = body + off_bits;
    expect_eq_u8("bottom-up first pixel", 0x31u, first_row[0]);
    expect_eq_u8("bottom-up second pixel", 0x32u, first_row[1]);
    expect_eq_u8("8bpp pad0", 0u, first_row[2]);
    expect_eq_u8("8bpp pad1", 0u, first_row[3]);

    free(body);
    cleanup_path(path);
}

static void test_write_1bpp_bmp(void)
{
    const char *path = "hpw_scratch_1.bmp";
    uint8_t pixels[16 * 2];
    host_page_image page;
    uint8_t *body;
    size_t size;
    uint32_t expected;
    uint32_t off_bits;
    const uint8_t *palette;
    const uint8_t *first_row;
    int y;

    cleanup_path(path);
    cleanup_path("hpw_scratch_1.bmp.tmp");

    memset(pixels, 0, sizeof(pixels));
    for (y = 0; y < 16; y++) {
        /* Alternating black/white columns via MSB packing. */
        pixels[y * 2 + 0] = 0xAAu;
        pixels[y * 2 + 1] = 0xAAu;
    }
    /* Distinct bottom row so stored first row is identifiable. */
    pixels[15 * 2 + 0] = 0xF0u;
    pixels[15 * 2 + 1] = 0x0Fu;

    memset(&page, 0, sizeof(page));
    page.width = 16;
    page.height = 16;
    page.stride_bytes = 2;
    page.bits_per_pixel = 1;
    page.pixels = pixels;

    expect_true("write 1bpp bmp", host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, path));
    body = read_file_bytes(path, &size);
    expect_true("1bpp file readable", body != NULL);

    expected = bmp_expected_size(16, 16, 1);
    off_bits = bmp_expected_off_bits(1);
    expect_eq_u32("1bpp file size", expected, (uint32_t)size);
    expect_true("BM magic 1bpp", size >= 2u && body[0] == 'B' && body[1] == 'M');
    expect_eq_u32("1bpp bfOffBits", off_bits, read_u32_le(body + 10));
    expect_eq_u32("1bpp biBitCount", 1u, (uint32_t)read_u16_le(body + 28));

    palette = body + 14u + 40u;
    expect_eq_u8("1bpp palette0 B", 0u, palette[0]);
    expect_eq_u8("1bpp palette0 G", 0u, palette[1]);
    expect_eq_u8("1bpp palette0 R", 0u, palette[2]);
    expect_eq_u8("1bpp palette1 B", 255u, palette[4]);
    expect_eq_u8("1bpp palette1 G", 255u, palette[5]);
    expect_eq_u8("1bpp palette1 R", 255u, palette[6]);

    /* Raw row is 2 bytes; BMP stride is 4 — padding must be zero. */
    first_row = body + off_bits;
    expect_eq_u8("1bpp bottom-up byte0", 0xF0u, first_row[0]);
    expect_eq_u8("1bpp bottom-up byte1", 0x0Fu, first_row[1]);
    expect_eq_u8("1bpp pad0", 0u, first_row[2]);
    expect_eq_u8("1bpp pad1", 0u, first_row[3]);

    free(body);
    cleanup_path(path);
}

static void test_atomic_overwrite(void)
{
    const char *path = "hpw_scratch_atomic.bmp";
    uint8_t pixels_a[4];
    uint8_t pixels_b[4];
    host_page_image page;
    uint8_t *body;
    size_t size;
    FILE *tmp;

    cleanup_path(path);
    cleanup_path("hpw_scratch_atomic.bmp.tmp");

    memset(pixels_a, 0x00, sizeof(pixels_a));
    memset(pixels_b, 0xff, sizeof(pixels_b));

    memset(&page, 0, sizeof(page));
    page.width = 2;
    page.height = 2;
    page.stride_bytes = 2;
    page.bits_per_pixel = 8;
    page.pixels = pixels_a;
    expect_true("atomic first write", host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, path));

    page.pixels = pixels_b;
    expect_true("atomic second write", host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, path));

    body = read_file_bytes(path, &size);
    expect_true("atomic final readable", body != NULL);
    expect_true("atomic BM magic", size >= 2u && body[0] == 'B' && body[1] == 'M');
    expect_eq_u32("atomic size", bmp_expected_size(2, 2, 8), (uint32_t)size);
    tmp = fopen("hpw_scratch_atomic.bmp.tmp", "rb");
    expect_false("no leftover tmp", tmp != NULL);
    if (tmp != NULL) {
        fclose(tmp);
    }

    free(body);
    cleanup_path(path);
}

int main(void)
{
    test_reject_unsupported_formats();
    test_reject_overflow_dimensions();
    test_ensure_dir();
    test_write_8bpp_bmp();
    test_write_1bpp_bmp();
    test_atomic_overwrite();
    return 0;
}
