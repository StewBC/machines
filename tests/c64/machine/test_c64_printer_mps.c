#include "c64_printer.h"

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

/* YYYYMMDD-HHMMSSXX.bmp — 8 digits, '-', 6 digits, 2-digit XX, ".bmp". */
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

static int count_print_pages(const char *dir)
{
    DIR *d;
    struct dirent *de;
    int n = 0;

    d = opendir(dir);
    if (d == NULL) {
        return 0;
    }
    while ((de = readdir(d)) != NULL) {
        if (is_print_page_name(de->d_name)) {
            n++;
        }
    }
    closedir(d);
    return n;
}

static bool find_print_page(const char *dir, char *out_name, size_t out_size)
{
    DIR *d;
    struct dirent *de;

    if (out_name == NULL || out_size == 0u) {
        return false;
    }
    out_name[0] = '\0';
    d = opendir(dir);
    if (d == NULL) {
        return false;
    }
    while ((de = readdir(d)) != NULL) {
        if (is_print_page_name(de->d_name)) {
            snprintf(out_name, out_size, "%s", de->d_name);
            closedir(d);
            return true;
        }
    }
    closedir(d);
    return false;
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
        if (!is_print_page_name(de->d_name)) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        remove(path);
    }
    closedir(d);
}

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    exit(1);
}

static void expect_true(const char *name, bool actual)
{
    if (!actual) {
        fprintf(stderr, "FAIL: %s: expected true\n", name);
        exit(1);
    }
}

static void expect_false(const char *name, bool actual)
{
    if (actual) {
        fprintf(stderr, "FAIL: %s: expected false\n", name);
        exit(1);
    }
}

static void expect_eq_int(const char *name, int expected, int actual)
{
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s: expected %d, got %d\n", name, expected, actual);
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

static uint8_t pixel_at(const c64_printer *p, int x, int y)
{
    size_t idx = (size_t)y * (size_t)C64_PRINTER_WIDTH_DOTS + (size_t)x;
    return p->raster[idx];
}

static bool file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return false;
    }
    fclose(fp);
    return true;
}

static void setup_printer(c64_printer *p, const char *dir)
{
    c64_printer_init(p);
    expect_true("raster allocated", p->raster != NULL);
    c64_printer_set_output_dir(p, dir);
    c64_printer_set_format_bmp(p);
    c64_printer_set_enabled(p, true);
    if (dir != NULL && dir[0] != '\0') {
        (void)test_mkdir(dir);
    }
}

static void teardown_printer(c64_printer *p)
{
    c64_printer_shutdown(p);
}

static void test_char_lf_vs_bim_lf(void)
{
    c64_printer p;

    setup_printer(&p, "printer_mps_tmp_lf");

    expect_eq_int("start y", 0, p.cursor_y_dots);
    c64_printer_putc(&p, 10); /* LF character mode */
    expect_eq_int("char LF y", C64_PRINTER_CHAR_LF_DOTS, p.cursor_y_dots);
    expect_false("still not BIM", p.bit_image);

    c64_printer_putc(&p, 8); /* enter BIM */
    expect_true("BIM on", p.bit_image);
    c64_printer_putc(&p, 10); /* LF in BIM */
    expect_eq_int(
        "BIM LF y",
        C64_PRINTER_CHAR_LF_DOTS + C64_PRINTER_BIM_LF_DOTS,
        p.cursor_y_dots);

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_lf");
}

static void test_page_full_flush(void)
{
    c64_printer p;
    const char *dir = "printer_mps_tmp_pagefull";

    setup_printer(&p, dir);
    /* Put ink so the page is dirty before the advancing flush. */
    c64_printer_putc(&p, 8);
    c64_printer_putc(&p, 0xFFu);
    expect_true("dirty before page-full", c64_printer_page_dirty(&p));

    /* Place cursor so next BIM LF trips page-full (> 700). */
    p.cursor_y_dots = C64_PRINTER_HEIGHT_DOTS - C64_PRINTER_BIM_LF_DOTS + 1;
    c64_printer_putc(&p, 10);

    expect_eq_u32("flushed one page", 1u, c64_printer_pages_flushed(&p));
    expect_eq_int("y reset", 0, p.cursor_y_dots);
    expect_eq_int("one page file", 1, count_print_pages(dir));

    cleanup_print_pages(dir);
    teardown_printer(&p);
    (void)test_rmdir(dir);
}

static void test_bim_column_and_repeat(void)
{
    c64_printer p;
    int x;

    setup_printer(&p, "printer_mps_tmp_bim");

    c64_printer_putc(&p, 8); /* BIM */
    /* Column with only top pin (bit0) and bottom pin (bit6); bit7 set. */
    c64_printer_putc(&p, (uint8_t)(0x80u | 0x01u | 0x40u));
    expect_eq_int("x after one col", 1, p.cursor_x_dots);
    expect_true("top pin ink", pixel_at(&p, 0, 0) == 0);
    expect_true("mid clear", pixel_at(&p, 0, 3) == 255);
    expect_true("bottom pin ink", pixel_at(&p, 0, 6) == 0);

    /* CHR$(26); n=5; data=top pin → five more columns */
    c64_printer_putc(&p, 26);
    c64_printer_putc(&p, 5);
    c64_printer_putc(&p, (uint8_t)(0x80u | 0x01u));
    expect_eq_int("x after repeat", 6, p.cursor_x_dots);
    for (x = 1; x <= 5; ++x) {
        if (pixel_at(&p, x, 0) != 0) {
            fprintf(stderr, "FAIL: repeat col %d missing ink\n", x);
            exit(1);
        }
    }

    /* n==0 means 256 */
    p.cursor_x_dots = 0;
    memset(p.raster, 255, p.raster_bytes);
    p.page_dirty = false;
    c64_printer_putc(&p, 26);
    c64_printer_putc(&p, 0);
    c64_printer_putc(&p, 0x02u);
    expect_eq_int("x after 256 repeat", 256, p.cursor_x_dots);
    expect_true("repeat256 first ink", pixel_at(&p, 0, 1) == 0);
    expect_true("repeat256 last ink", pixel_at(&p, 255, 1) == 0);

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_bim");
}

static void test_dot_address_clip_and_wrap(void)
{
    c64_printer p;

    setup_printer(&p, "printer_mps_tmp_dot");

    /* ESC 16 nH nL → 100 */
    c64_printer_putc(&p, 27);
    c64_printer_putc(&p, 16);
    c64_printer_putc(&p, 0);
    c64_printer_putc(&p, 100);
    expect_eq_int("dot addr 100", 100, p.cursor_x_dots);

    /* 480 clips to 479 */
    c64_printer_putc(&p, 27);
    c64_printer_putc(&p, 16);
    c64_printer_putc(&p, 0x01u); /* 256+224=480 */
    c64_printer_putc(&p, 224);
    expect_eq_int("dot addr 480 clip", C64_PRINTER_WIDTH_DOTS - 1, p.cursor_x_dots);

    /* 639 clips to 479 */
    c64_printer_putc(&p, 27);
    c64_printer_putc(&p, 16);
    c64_printer_putc(&p, 0x02u); /* 512+127=639 */
    c64_printer_putc(&p, 127);
    expect_eq_int("dot addr 639 clip", C64_PRINTER_WIDTH_DOTS - 1, p.cursor_x_dots);

    /* >639 wraps to beginning */
    c64_printer_putc(&p, 27);
    c64_printer_putc(&p, 16);
    c64_printer_putc(&p, 0x02u); /* 512+128=640 */
    c64_printer_putc(&p, 128);
    expect_eq_int("dot addr >639 wrap", 0, p.cursor_x_dots);

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_dot");
}

static void test_bim_past_edge_no_overdraw(void)
{
    c64_printer p;
    int edge = C64_PRINTER_WIDTH_DOTS - 1;

    setup_printer(&p, "printer_mps_tmp_edge");

    c64_printer_putc(&p, 8);
    p.cursor_x_dots = edge;
    /* First column: top pin only at x=479, then X → sentinel 480. */
    c64_printer_putc(&p, (uint8_t)(0x80u | 0x01u));
    expect_eq_int("BIM after edge plot → sentinel", C64_PRINTER_WIDTH_DOTS, p.cursor_x_dots);
    expect_true("first col top ink", pixel_at(&p, edge, 0) == 0);
    expect_true("first col mid clear", pixel_at(&p, edge, 3) == 255);

    /* Second distinct column must be ignored (no overdraw on 479). */
    c64_printer_putc(&p, (uint8_t)(0x80u | 0x08u)); /* bit3 only */
    expect_eq_int("BIM past-edge x stays sentinel", C64_PRINTER_WIDTH_DOTS, p.cursor_x_dots);
    expect_true("no overdraw mid", pixel_at(&p, edge, 3) == 255);
    expect_true("first col top still only", pixel_at(&p, edge, 0) == 0);

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_edge");
}

static void test_failed_flush_retains_cursor(void)
{
    c64_printer p;
    int saved_y;

    setup_printer(&p, "printer_mps_tmp_iofail");
    c64_printer_putc(&p, (uint8_t)'A');
    expect_true("dirty before fail", c64_printer_page_dirty(&p));

    /* Force page-full with no usable output dir → I/O failure. */
    c64_printer_set_output_dir(&p, "");
    saved_y = C64_PRINTER_HEIGHT_DOTS - C64_PRINTER_CHAR_LF_DOTS + 1;
    p.cursor_y_dots = saved_y;
    c64_printer_putc(&p, 10); /* LF → page-full flush attempt */

    expect_true("still dirty after I/O fail", c64_printer_page_dirty(&p));
    expect_eq_int("Y not reset on I/O fail", saved_y, p.cursor_y_dots);
    expect_true("flush_hold set", p.flush_hold);
    expect_eq_u32("no page written", 0u, c64_printer_pages_flushed(&p));

    /* Further putc must not mutate the retained page. */
    {
        uint8_t before = pixel_at(&p, 0, 0);
        c64_printer_putc(&p, (uint8_t)'Z');
        expect_true("hold blocks putc", pixel_at(&p, 0, 0) == before);
    }

    /* Restore dir and force-flush recovers. */
    c64_printer_set_output_dir(&p, "printer_mps_tmp_iofail");
    c64_printer_force_flush(&p);
    expect_false("hold cleared", p.flush_hold);
    expect_eq_u32("retry wrote page", 1u, c64_printer_pages_flushed(&p));
    expect_false("clean after retry", c64_printer_page_dirty(&p));

    /* FF with I/O failure must not zero the cursor either. */
    c64_printer_putc(&p, (uint8_t)'B');
    p.cursor_x_dots = 33;
    p.cursor_y_dots = 44;
    c64_printer_set_output_dir(&p, "");
    c64_printer_putc(&p, 12);
    expect_eq_int("FF fail keeps x", 33, p.cursor_x_dots);
    expect_eq_int("FF fail keeps y", 44, p.cursor_y_dots);
    expect_true("FF fail dirty", c64_printer_page_dirty(&p));
    expect_true("FF fail hold", p.flush_hold);

    cleanup_print_pages("printer_mps_tmp_iofail");
    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_iofail");
}

static void test_wrap_pagefull_hold_no_draw(void)
{
    c64_printer p;
    int pre_x;
    int y;
    int x;
    uint8_t snapshot[6 * 7];
    int i = 0;

    setup_printer(&p, "printer_mps_tmp_wraphold");
    /* Seed dirty ink away from the wrap column. */
    c64_printer_putc(&p, (uint8_t)'A');
    c64_printer_set_output_dir(&p, "");

    /* Next glyph both wraps (x+6 > 480) and page-fulls (y near bottom). */
    pre_x = C64_PRINTER_WIDTH_DOTS - 4;
    p.cursor_x_dots = pre_x;
    p.cursor_y_dots = C64_PRINTER_HEIGHT_DOTS - C64_PRINTER_CHAR_LF_DOTS + 1;

    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 6; ++x) {
            snapshot[i++] = pixel_at(&p, pre_x + x, p.cursor_y_dots + y);
        }
    }

    c64_printer_putc(&p, (uint8_t)'W');

    expect_true("wrap+pagefull sets hold", p.flush_hold);
    expect_eq_int("wrap fail keeps x", pre_x, p.cursor_x_dots);
    i = 0;
    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 6; ++x) {
            if (pixel_at(&p, pre_x + x, p.cursor_y_dots + y) != snapshot[i]) {
                fprintf(stderr, "FAIL: wrap hold drew at (%d,%d)\n",
                        pre_x + x, p.cursor_y_dots + y);
                exit(1);
            }
            i++;
        }
    }

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_wraphold");
}

static void test_ascii_head_tab(void)
{
    c64_printer p;

    setup_printer(&p, "printer_mps_tmp_tab");

    c64_printer_putc(&p, 16);
    c64_printer_putc(&p, (uint8_t)'0');
    c64_printer_putc(&p, (uint8_t)'8');
    expect_eq_int("head tab 08 → x=48", 48, p.cursor_x_dots);

    c64_printer_putc(&p, 16);
    c64_printer_putc(&p, (uint8_t)'0');
    c64_printer_putc(&p, (uint8_t)'0');
    expect_eq_int("head tab 00 → x=0", 0, p.cursor_x_dots);

    c64_printer_putc(&p, 16);
    c64_printer_putc(&p, (uint8_t)'7');
    c64_printer_putc(&p, (uint8_t)'9');
    expect_eq_int("head tab 79 → x=474", 79 * 6, p.cursor_x_dots);

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_tab");
}

static void test_cr_keeps_bim(void)
{
    c64_printer p;

    setup_printer(&p, "printer_mps_tmp_cr");

    c64_printer_putc(&p, 8);
    expect_true("BIM entered", p.bit_image);
    p.cursor_x_dots = 40;
    c64_printer_putc(&p, 13); /* CR */
    expect_true("CR keeps BIM", p.bit_image);
    expect_eq_int("CR resets x", 0, p.cursor_x_dots);
    expect_eq_int("CR advances BIM pitch", C64_PRINTER_BIM_LF_DOTS, p.cursor_y_dots);

    /* Column bytes have bit7 set; 0x91 must plot, not act as CHR$(145). */
    c64_printer_putc(&p, 0x81u); /* bit7 + top pin */
    expect_true("still BIM after col", p.bit_image);
    expect_eq_int("x after pin", 1, p.cursor_x_dots);
    expect_true("top pin ink", pixel_at(&p, 0, C64_PRINTER_BIM_LF_DOTS) == 0);

    c64_printer_putc(&p, 0x91u); /* == 145 as unsigned, but bit7 marks column */
    expect_true("0x91 is column not CHR$(145)", p.bit_image);
    expect_eq_int("x after 0x91", 2, p.cursor_x_dots);
    expect_true("graphic charset unchanged", p.graphic_charset);

    c64_printer_putc(&p, 0x92u); /* == 146 */
    expect_eq_int("x after 0x92", 3, p.cursor_x_dots);

    c64_printer_putc(&p, 15); /* standard mode (bit7 clear) */
    expect_false("CHR$(15) leaves BIM", p.bit_image);

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_cr");
}

static void test_force_flush_dirty_and_blank(void)
{
    c64_printer p;
    const char *dir = "printer_mps_tmp_flush";

    setup_printer(&p, dir);

    c64_printer_force_flush(&p);
    expect_eq_u32("blank flush suppressed", 0u, c64_printer_pages_flushed(&p));
    expect_eq_int("no blank file", 0, count_print_pages(dir));

    c64_printer_putc(&p, (uint8_t)'A');
    expect_true("dirty after glyph", c64_printer_page_dirty(&p));
    c64_printer_force_flush(&p);
    expect_eq_u32("dirty flush wrote", 1u, c64_printer_pages_flushed(&p));
    expect_false("clean after flush", c64_printer_page_dirty(&p));
    expect_eq_int("one page file", 1, count_print_pages(dir));
    {
        char name[64];
        expect_true("named page", find_print_page(dir, name, sizeof(name)));
        expect_true("name pattern", is_print_page_name(name));
        expect_true("XX starts at 00", name[15] == '0' && name[16] == '0');
    }

    cleanup_print_pages(dir);
    teardown_printer(&p);
    (void)test_rmdir(dir);
}

static void test_same_second_name_counter(void)
{
    c64_printer p;
    const char *dir = "printer_mps_tmp_nameseq";
    char path[1100];
    int attempt;

    setup_printer(&p, dir);

    c64_printer_putc(&p, (uint8_t)'A');
    c64_printer_force_flush(&p);
    expect_eq_u32("first page", 1u, c64_printer_pages_flushed(&p));
    expect_eq_int("first XX", 0, (int)p.name_seq);
    expect_eq_int("one file", 1, count_print_pages(dir));

    /* Second flush in the same second must use XX=01; if the clock second
       ticks, XX resets to 00 under a new stem — still unique either way. */
    c64_printer_putc(&p, (uint8_t)'B');
    c64_printer_force_flush(&p);
    expect_eq_u32("second page", 2u, c64_printer_pages_flushed(&p));
    expect_eq_int("two files", 2, count_print_pages(dir));
    if (p.name_seq == 1u) {
        snprintf(path, sizeof(path), "%s/%s01.bmp", dir, p.last_name_stem);
        expect_true("01 file", file_exists(path));
    } else {
        expect_eq_int("new-second XX", 0, (int)p.name_seq);
    }

    /* Exhaust XX=99 while stems still match → hold + retain dirty. */
    for (attempt = 0; attempt < 5; ++attempt) {
        char stem[16];

        snprintf(stem, sizeof(stem), "%s", p.last_name_stem);
        c64_printer_putc(&p, (uint8_t)'C');
        memcpy(p.last_name_stem, stem, sizeof(p.last_name_stem));
        p.name_seq = 99u;
        c64_printer_force_flush(&p);
        if (p.flush_hold) {
            expect_true("exhausted dirty", c64_printer_page_dirty(&p));
            expect_eq_u32("no third page", 2u, c64_printer_pages_flushed(&p));
            p.flush_hold = false;
            p.page_dirty = false;
            break;
        }
        /* Second ticked: flush succeeded as XX=00 under a new stem. Retry. */
        expect_eq_int("retry after tick", 0, (int)p.name_seq);
        cleanup_print_pages(dir);
        p.pages_flushed = 2u;
    }
    expect_true("exhausted path hit", attempt < 5);

    cleanup_print_pages(dir);
    teardown_printer(&p);
    (void)test_rmdir(dir);
}

static void test_glyph_ink_and_disabled_noop(void)
{
    c64_printer p;
    int x;
    int y;
    int ink = 0;

    setup_printer(&p, "printer_mps_tmp_glyph");

    c64_printer_putc(&p, (uint8_t)'0');
    for (y = 0; y < 7; ++y) {
        for (x = 0; x < 6; ++x) {
            if (pixel_at(&p, x, y) == 0) {
                ink++;
            }
        }
    }
    expect_true("digit 0 left ink", ink > 0);

    c64_printer_set_enabled(&p, false);
    p.cursor_x_dots = 0;
    p.cursor_y_dots = 20;
    c64_printer_putc(&p, (uint8_t)'Z');
    expect_eq_int("disabled putc no move", 20, p.cursor_y_dots);

    /* Re-enable and switch business charset via SA / CHR$(17). */
    c64_printer_set_enabled(&p, true);
    c64_printer_set_sa(&p, 7);
    expect_false("SA7 business", p.graphic_charset);
    c64_printer_putc(&p, 145); /* local graphic */
    expect_true("CHR145 graphic", p.graphic_charset);
    c64_printer_putc(&p, 17); /* local business */
    expect_false("CHR17 business", p.graphic_charset);

    cleanup_print_pages("printer_mps_tmp_glyph");
    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_glyph");
}

static void test_enhance_and_reverse(void)
{
    c64_printer p;

    setup_printer(&p, "printer_mps_tmp_enh");

    c64_printer_putc(&p, 14); /* enhance */
    expect_true("enhance on", p.enhance);
    c64_printer_putc(&p, (uint8_t)'I');
    expect_eq_int("double-width advance", 12, p.cursor_x_dots);

    c64_printer_putc(&p, 8); /* BIM cancels enhance */
    expect_true("BIM", p.bit_image);
    expect_false("enhance off in BIM", p.enhance);
    c64_printer_putc(&p, 15); /* leave BIM / enhance off */
    expect_false("left BIM", p.bit_image);

    c64_printer_putc(&p, 18);
    expect_true("reverse on", p.reverse);
    c64_printer_putc(&p, 146);
    expect_false("reverse off", p.reverse);

    teardown_printer(&p);
    (void)test_rmdir("printer_mps_tmp_enh");
}

int main(void)
{
    test_char_lf_vs_bim_lf();
    test_page_full_flush();
    test_bim_column_and_repeat();
    test_dot_address_clip_and_wrap();
    test_bim_past_edge_no_overdraw();
    test_failed_flush_retains_cursor();
    test_wrap_pagefull_hold_no_draw();
    test_ascii_head_tab();
    test_cr_keeps_bim();
    test_force_flush_dirty_and_blank();
    test_same_second_name_counter();
    test_glyph_ink_and_disabled_noop();
    test_enhance_and_reverse();
    printf("ok\n");
    return 0;
}
