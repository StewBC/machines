/*
 * Print Shop–critical ImageWriter path through SSC ACIA TX (not putc-only).
 * No Apple Print Shop disk is required; the byte stream mirrors card graphics:
 * pitch, ESC T16, ESC G/V BIM rows with CR (auto LF) between bands, FF → BMP.
 */
#include "apple2.h"
#include "imagewriter.h"
#include "softswitch.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define test_mkdir(path) _mkdir(path)
#define test_rmdir _rmdir
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(path) mkdir((path), 0777)
#define test_rmdir rmdir
#endif

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_eq_int(const char *name, int expected, int actual)
{
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected %d, got %d\n", name, expected, actual);
        exit(1);
    }
}

static void expect_true(const char *name, bool actual)
{
    if (!actual) {
        fprintf(stderr, "FAIL: %s: expected true\n", name);
        exit(1);
    }
}

/* YYYYMMDD-HHMMSSXX.bmp */
static bool is_print_page_name(const char *name)
{
    size_t i;

    if (name == NULL || strlen(name) != 21u) {
        return false;
    }
    for (i = 0; i < 8u; ++i) {
        if (!isdigit((unsigned char)name[i])) {
            return false;
        }
    }
    if (name[8] != '-') {
        return false;
    }
    for (i = 9; i < 15u; ++i) {
        if (!isdigit((unsigned char)name[i])) {
            return false;
        }
    }
    if (!isdigit((unsigned char)name[15]) || !isdigit((unsigned char)name[16])) {
        return false;
    }
    return strcmp(name + 17, ".bmp") == 0;
}

static void cleanup_print_pages(const char *dir)
{
    DIR *d;
    struct dirent *de;
    char path[1100];

    d = opendir(dir);
    if (d == NULL) {
        return;
    }
    while ((de = readdir(d)) != NULL) {
        if (is_print_page_name(de->d_name)) {
            snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
            (void)remove(path);
        }
    }
    closedir(d);
}

static int find_print_page(const char *dir, char *out, size_t out_sz)
{
    DIR *d;
    struct dirent *de;
    int n = 0;

    if (out_sz > 0u) {
        out[0] = '\0';
    }
    d = opendir(dir);
    if (d == NULL) {
        return 0;
    }
    while ((de = readdir(d)) != NULL) {
        if (!is_print_page_name(de->d_name)) {
            continue;
        }
        n++;
        if (out[0] == '\0') {
            snprintf(out, out_sz, "%s/%s", dir, de->d_name);
        }
    }
    closedir(d);
    return n;
}

static void ssc_tx_byte(apple2_t *m, uint8_t slot, uint8_t ch)
{
    uint16_t tdr = (uint16_t)(0xC080 + slot * 0x10 + 0x08);
    softswitch_c0_write(m, tdr, ch);
}

static void ssc_tx_str(apple2_t *m, uint8_t slot, const char *s)
{
    while (*s != '\0') {
        ssc_tx_byte(m, slot, (uint8_t)*s++);
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

static uint8_t bmp_pixel(const uint8_t *bmp, size_t size, int x, int y)
{
    uint32_t off;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    size_t idx;

    if (size < 54u || bmp[0] != 'B' || bmp[1] != 'M') {
        fail("bmp header");
    }
    off = read_u32_le(bmp + 10);
    width = read_u32_le(bmp + 18);
    height = read_u32_le(bmp + 22);
    if (read_u16_le(bmp + 28) != 8u) {
        fail("bmp bpp");
    }
    if (width != (uint32_t)A2_IW_WIDTH_DOTS || height != (uint32_t)A2_IW_HEIGHT_DOTS) {
        fail("bmp dimensions");
    }
    stride = (width + 3u) & ~3u;
    if (x < 0 || y < 0 || (uint32_t)x >= width || (uint32_t)y >= height) {
        fail("bmp pixel OOB");
    }
    idx = (size_t)off + (size_t)(height - 1u - (uint32_t)y) * (size_t)stride + (size_t)x;
    if (idx >= size) {
        fail("bmp pixel EOF");
    }
    return bmp[idx];
}

static int count_bmp_ink(const uint8_t *bmp, size_t size)
{
    uint32_t off;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t y;
    uint32_t x;
    int ink = 0;
    size_t need;

    off = read_u32_le(bmp + 10);
    width = read_u32_le(bmp + 18);
    height = read_u32_le(bmp + 22);
    stride = (width + 3u) & ~3u;
    need = (size_t)off + (size_t)stride * (size_t)height;
    if (need > size) {
        fail("bmp truncated");
    }
    for (y = 0; y < height; ++y) {
        const uint8_t *row = bmp + off + (size_t)y * (size_t)stride;
        for (x = 0; x < width; ++x) {
            if (row[x] == (uint8_t)A2_IW_INK) {
                ink++;
            }
        }
    }
    return ink;
}

/*
 * Card-like bands at 72 dpi with ESC T16 gapless pitch:
 *   row0: 8x top-pin via ESC G
 *   CR ($8D high-ASCII) → auto LF 8 dots
 *   row1: 8x bottom-pin via ESC G
 *   CR
 *   row2: ESC V0016 solid columns
 *   FF → one non-blank BMP
 */
static void test_ssc_printshop_card_graphics_bmp(void)
{
    apple2_t m;
    const char *dir = "ssc_iw_tmp_printshop";
    char page_path[1100];
    uint8_t *bmp;
    size_t bmp_size;
    int i;
    int ink;
    int x0;
    const uint8_t slot = 2;

    (void)test_mkdir(dir);
    cleanup_print_pages(dir);

    if (!apple2_init(&m)) {
        fail("init");
    }
    apple2_set_printer_output_dir(&m, dir);
    if (!apple2_attach_ssc(&m, (int)slot)) {
        fail("attach ssc");
    }
    expect_true("iw live", m.imagewriter_live);
    expect_true("auto LF-after-CR default", m.imagewriter.auto_lf_after_cr);

    /* Pitch + Print Shop line spacing for 8-pin bands. */
    ssc_tx_str(&m, slot, "\x1b" "n");
    ssc_tx_str(&m, slot, "\x1b" "T16");
    expect_eq_int("dpi", 72, m.imagewriter.dpi);
    expect_eq_int("lf_dots", 8, m.imagewriter.lf_dots);

    /* Row 0: top pin across 8 columns. */
    ssc_tx_str(&m, slot, "\x1b" "G0008");
    for (i = 0; i < 8; ++i) {
        ssc_tx_byte(&m, slot, 0x01u);
    }
    expect_eq_int("no auto Y after BIM", 0, m.imagewriter.cursor_y_dots);
    expect_eq_int("head after row0", 8, m.imagewriter.head_col);

    /* High-ASCII CR (Apple II style) must return + LF under default DIP. */
    ssc_tx_byte(&m, slot, 0x8Du);
    expect_eq_int("CR resets head", 0, m.imagewriter.head_col);
    expect_eq_int("T16 LF after CR", 8, m.imagewriter.cursor_y_dots);

    /* Row 1: bottom pin. */
    ssc_tx_str(&m, slot, "\x1b" "G0008");
    for (i = 0; i < 8; ++i) {
        ssc_tx_byte(&m, slot, 0x80u);
    }
    ssc_tx_byte(&m, slot, 0x0Du);
    expect_eq_int("y after row1 CR", 16, m.imagewriter.cursor_y_dots);

    /* Row 2: compact solid via ESC V (Print Shop repeat). */
    ssc_tx_str(&m, slot, "\x1b" "V0016");
    ssc_tx_byte(&m, slot, 0xFFu);
    expect_eq_int("head after V", 16, m.imagewriter.head_col);
    expect_eq_int("still y=16 until LF", 16, m.imagewriter.cursor_y_dots);

    ssc_tx_byte(&m, slot, 0x0Cu); /* FF */
    expect_true("clean after FF", !imagewriter_page_dirty(&m.imagewriter));
    expect_eq_int("pages flushed", 1, (int)imagewriter_pages_flushed(&m.imagewriter));
    expect_eq_int("one bmp", 1, find_print_page(dir, page_path, sizeof(page_path)));

    bmp = read_file_bytes(page_path, &bmp_size);
    if (bmp == NULL) {
        fail("read bmp");
    }

    x0 = imagewriter_bim_x(72, 0);
    expect_true("row0 top ink", bmp_pixel(bmp, bmp_size, x0, 0) == A2_IW_INK);
    expect_true("row0 no pin1", bmp_pixel(bmp, bmp_size, x0, 1) == A2_IW_PAPER);
    expect_true("gapless row1 bottom", bmp_pixel(bmp, bmp_size, x0, 15) == A2_IW_INK);
    expect_true("row1 no top at y8", bmp_pixel(bmp, bmp_size, x0, 8) == A2_IW_PAPER);
    expect_true("V solid top", bmp_pixel(bmp, bmp_size, x0, 16) == A2_IW_INK);
    expect_true("V solid bottom", bmp_pixel(bmp, bmp_size, x0, 23) == A2_IW_INK);
    expect_true(
        "ESC n absolute x(3)",
        bmp_pixel(bmp, bmp_size, imagewriter_bim_x(72, 3), 0) == A2_IW_INK);

    /* 8 + 8 + 16*8 = 144 ink dots. */
    ink = count_bmp_ink(bmp, bmp_size);
    expect_eq_int("ink pixel count", 144, ink);

    free(bmp);
    apple2_shutdown(&m);
    cleanup_print_pages(dir);
    (void)test_rmdir(dir);
}

/* ESC g nnn → nnn×8 columns; LF (with auto-LF off) + CR between bands. */
static void test_ssc_esc_g_and_lf_between_rows(void)
{
    apple2_t m;
    const char *dir = "ssc_iw_tmp_escg";
    char page_path[1100];
    uint8_t *bmp;
    size_t bmp_size;
    int i;
    const uint8_t slot = 1;

    (void)test_mkdir(dir);
    cleanup_print_pages(dir);

    if (!apple2_init(&m)) {
        fail("escg init");
    }
    apple2_set_printer_output_dir(&m, dir);
    if (!apple2_attach_ssc(&m, (int)slot)) {
        fail("escg attach");
    }

    m.imagewriter.auto_lf_after_cr = false;
    ssc_tx_str(&m, slot, "\x1b" "P"); /* 160 dpi: x(i)=i */
    ssc_tx_str(&m, slot, "\x1b" "T16");

    /* ESC g001 → 8 columns. */
    ssc_tx_str(&m, slot, "\x1b" "g001");
    for (i = 0; i < 8; ++i) {
        ssc_tx_byte(&m, slot, 0x01u);
    }
    expect_eq_int("esc g cols", 8, m.imagewriter.head_col);
    expect_eq_int("esc g no auto Y", 0, m.imagewriter.cursor_y_dots);

    /* Explicit CR + LF between BIM rows (auto LF off). */
    ssc_tx_byte(&m, slot, 0x0Du);
    ssc_tx_byte(&m, slot, 0x0Au);
    expect_eq_int("head after CR", 0, m.imagewriter.head_col);
    expect_eq_int("y after LF", 8, m.imagewriter.cursor_y_dots);

    ssc_tx_str(&m, slot, "\x1b" "G0004");
    for (i = 0; i < 4; ++i) {
        ssc_tx_byte(&m, slot, 0x02u);
    }
    ssc_tx_byte(&m, slot, 0x0Cu);

    expect_eq_int("escg pages", 1, find_print_page(dir, page_path, sizeof(page_path)));
    bmp = read_file_bytes(page_path, &bmp_size);
    if (bmp == NULL) {
        fail("escg bmp");
    }
    expect_true("P dens x0", bmp_pixel(bmp, bmp_size, 0, 0) == A2_IW_INK);
    expect_true("P dens x7", bmp_pixel(bmp, bmp_size, 7, 0) == A2_IW_INK);
    expect_true("second band pin1", bmp_pixel(bmp, bmp_size, 0, 9) == A2_IW_INK);
    expect_eq_int("escg ink", 8 + 4, count_bmp_ink(bmp, bmp_size));

    free(bmp);
    apple2_shutdown(&m);
    cleanup_print_pages(dir);
    (void)test_rmdir(dir);
}

int main(void)
{
    test_ssc_printshop_card_graphics_bmp();
    test_ssc_esc_g_and_lf_between_rows();
    printf("ok\n");
    return 0;
}
