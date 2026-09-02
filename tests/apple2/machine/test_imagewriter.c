#include "imagewriter.h"

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

static uint8_t pixel_at(const imagewriter *iw, int x, int y)
{
    size_t idx = (size_t)y * (size_t)A2_IW_WIDTH_DOTS + (size_t)x;
    return iw->raster[idx];
}

static void put_str(imagewriter *iw, const char *s)
{
    while (*s != '\0') {
        imagewriter_putc(iw, (uint8_t)*s++);
    }
}

static void setup(imagewriter *iw)
{
    imagewriter_init(iw);
    expect_true("raster allocated", iw->raster != NULL);
    expect_eq_int("default dpi pica", 80, iw->dpi);
    expect_eq_int("default lf ESC A", 12, iw->lf_dots);
}

static void teardown(imagewriter *iw)
{
    imagewriter_shutdown(iw);
}

static void test_pitch_table_and_bim_x(void)
{
    const int idxs_n[] = {0, 1, 3, 288, 575}; /* mid=576/2, maxn-1 */
    const int expect_n[] = {
        (0 * 160 + 36) / 72,
        (1 * 160 + 36) / 72,
        (3 * 160 + 36) / 72,
        (288 * 160 + 36) / 72,
        (575 * 160 + 36) / 72
    };
    const int idxs_p[] = {0, 1, 3, 640, 1279};
    int i;

    expect_eq_int("ESC n dpi", 72, imagewriter_pitch_dpi((uint8_t)'n'));
    expect_eq_int("ESC N dpi", 80, imagewriter_pitch_dpi((uint8_t)'N'));
    expect_eq_int("ESC E dpi", 96, imagewriter_pitch_dpi((uint8_t)'E'));
    expect_eq_int("ESC e dpi", 107, imagewriter_pitch_dpi((uint8_t)'e'));
    expect_eq_int("ESC q dpi", 120, imagewriter_pitch_dpi((uint8_t)'q'));
    expect_eq_int("ESC Q dpi", 136, imagewriter_pitch_dpi((uint8_t)'Q'));
    expect_eq_int("ESC p dpi", 144, imagewriter_pitch_dpi((uint8_t)'p'));
    expect_eq_int("ESC P dpi", 160, imagewriter_pitch_dpi((uint8_t)'P'));

    expect_eq_int("ESC n max", 576, imagewriter_pitch_max_nnnn(72));
    expect_eq_int("ESC P max", 1280, imagewriter_pitch_max_nnnn(160));

    expect_eq_int("ESC n BIM x(3)", 7, imagewriter_bim_x(72, 3));
    expect_eq_int("ESC n Δx 0→1", 2, imagewriter_bim_x(72, 1) - imagewriter_bim_x(72, 0));

    for (i = 0; i < 5; ++i) {
        char name[64];
        snprintf(name, sizeof(name), "ESC n BIM x(%d)", idxs_n[i]);
        expect_eq_int(name, expect_n[i], imagewriter_bim_x(72, idxs_n[i]));
    }
    for (i = 0; i < 5; ++i) {
        char name[64];
        snprintf(name, sizeof(name), "ESC P BIM x(%d)", idxs_p[i]);
        expect_eq_int(name, idxs_p[i], imagewriter_bim_x(160, idxs_p[i]));
    }
}

static void test_text_x_n_and_e(void)
{
    /* ESC n: 72 dpi / 9 cpi — do not feed dpi into a cpi-only formula. */
    expect_eq_int("text n x0", 0, imagewriter_text_x(72, 0));
    expect_eq_int("text n x1", (1 * 8 * 160 + 36) / 72, imagewriter_text_x(72, 1));
    expect_eq_int("text n x2", (2 * 8 * 160 + 36) / 72, imagewriter_text_x(72, 2));

    /* ESC e: 107 dpi / 13.4 cpi */
    expect_eq_int("text e x0", 0, imagewriter_text_x(107, 0));
    expect_eq_int("text e x1", (1 * 8 * 160 + 53) / 107, imagewriter_text_x(107, 1));
    expect_eq_int("text e x2", (2 * 8 * 160 + 53) / 107, imagewriter_text_x(107, 2));

    /* Pica identity: d=80 → text x(i)=i*16 */
    expect_eq_int("text N x1", 16, imagewriter_text_x(80, 1));
    expect_eq_int("text N x2", 32, imagewriter_text_x(80, 2));
}

static void test_cr_lf_esc_t_ff(void)
{
    imagewriter iw;
    const char *dir = "iw_tmp_crlf";

    setup(&iw);
    iw.auto_lf_after_cr = false;
    (void)test_mkdir(dir);
    imagewriter_set_output_dir(&iw, dir);

    iw.head_col = 40;
    imagewriter_putc(&iw, 0x0D); /* CR */
    expect_eq_int("CR resets head", 0, iw.head_col);
    expect_eq_int("CR no Y without auto LF", 0, iw.cursor_y_dots);

    imagewriter_putc(&iw, 0x0A); /* LF default 12 */
    expect_eq_int("LF default y", 12, iw.cursor_y_dots);

    put_str(&iw, "\x1b" "T16");
    expect_eq_int("ESC T16 lf_dots", 8, iw.lf_dots);
    imagewriter_putc(&iw, 0x0A);
    expect_eq_int("LF after T16", 20, iw.cursor_y_dots);

    put_str(&iw, "\x1b" "A");
    expect_eq_int("ESC A lf", 12, iw.lf_dots);
    put_str(&iw, "\x1b" "B");
    expect_eq_int("ESC B lf", 9, iw.lf_dots);

    imagewriter_putc(&iw, (uint8_t)'A');
    expect_true("dirty before FF", imagewriter_page_dirty(&iw));
    imagewriter_putc(&iw, 0x0C);
    expect_false("clean after FF", imagewriter_page_dirty(&iw));
    expect_eq_u32("FF wrote page", 1u, imagewriter_pages_flushed(&iw));
    expect_eq_int("FF y reset", 0, iw.cursor_y_dots);
    expect_eq_int("FF x reset", 0, iw.head_col);

    cleanup_print_pages(dir);
    teardown(&iw);
    (void)test_rmdir(dir);
}

static void test_esc_g_bim_no_auto_y(void)
{
    imagewriter iw;
    int y_before;

    setup(&iw);
    put_str(&iw, "\x1b" "n"); /* 72 dpi */
    expect_eq_int("dpi after ESC n", 72, iw.dpi);

    y_before = iw.cursor_y_dots;
    put_str(&iw, "\x1b" "G0003");
    imagewriter_putc(&iw, 0x01u); /* top pin col0 */
    imagewriter_putc(&iw, 0x80u); /* bottom pin col1 */
    imagewriter_putc(&iw, 0x02u); /* bit1 col2 */

    expect_eq_int("no auto Y after BIM", y_before, iw.cursor_y_dots);
    expect_eq_int("head after 3 cols", 3, iw.head_col);
    expect_true("BIM ink x0", pixel_at(&iw, imagewriter_bim_x(72, 0), 0) == A2_IW_INK);
    expect_true("BIM ink x1 bottom", pixel_at(&iw, imagewriter_bim_x(72, 1), 7) == A2_IW_INK);
    expect_true("BIM ink x2", pixel_at(&iw, imagewriter_bim_x(72, 2), 1) == A2_IW_INK);
    expect_eq_int("BIM absolute x(3) would be 7", 7, imagewriter_bim_x(72, 3));

    /* ESC T16 between rows (Print Shop style) then another band */
    put_str(&iw, "\x1b" "T16");
    imagewriter_putc(&iw, 0x0A);
    expect_eq_int("Y after T16 LF", 8, iw.cursor_y_dots);

    teardown(&iw);
}

static void test_digit_spaces_in_nnnn(void)
{
    imagewriter iw;

    setup(&iw);

    /* ESC G with leading spaces: "  02" → 2 columns */
    put_str(&iw, "\x1b" "G  02");
    imagewriter_putc(&iw, 0x01u);
    imagewriter_putc(&iw, 0x01u);
    expect_eq_int("space-tolerant G cols", 2, iw.head_col);
    /* Stay in BIM until CR/LF/ESC (Print Shop overshoot). */
    expect_eq_int("BIM drain after count", (int)A2_IW_PARSE_BIM_DATA, (int)iw.parse_state);
    imagewriter_putc(&iw, 0x0Du);
    expect_eq_int("CR ends BIM", (int)A2_IW_PARSE_IDLE, (int)iw.parse_state);

    /* ESC T mm is two digits; leading space: " 8" → mm=8 → 4 dots */
    iw.head_col = 0;
    put_str(&iw, "\x1b" "T 8");
    expect_eq_int("ESC T spaces → 4", 4, iw.lf_dots);

    /* ESC F with spaces */
    put_str(&iw, "\x1b" "N"); /* 80 dpi */
    put_str(&iw, "\x1b" "F 100");
    expect_eq_int("ESC F spaces head", 100, iw.head_col);

    /* Abort on non-digit/non-space */
    put_str(&iw, "\x1b" "G00X0");
    expect_eq_int("abort bad digit → IDLE", (int)A2_IW_PARSE_IDLE, (int)iw.parse_state);

    teardown(&iw);
}

static void test_esc_v_and_text_pitch_cmds(void)
{
    imagewriter iw;
    int x;

    setup(&iw);
    put_str(&iw, "\x1b" "P"); /* 160 dpi */
    put_str(&iw, "\x1b" "V0004");
    imagewriter_putc(&iw, 0x01u);
    expect_eq_int("V repeat cols", 4, iw.head_col);
    for (x = 0; x < 4; ++x) {
        if (pixel_at(&iw, x, 0) != A2_IW_INK) {
            fprintf(stderr, "FAIL: V col %d missing ink\n", x);
            exit(1);
        }
    }

    imagewriter_reset(&iw);
    put_str(&iw, "\x1b" "e");
    expect_eq_int("ESC e dpi", 107, iw.dpi);
    imagewriter_putc(&iw, (uint8_t)'A');
    expect_eq_int("text head after 1 char", 8, iw.head_col);
    /* 'A' row0 uses glyph cols 1..3 → bim cols 1..3 */
    expect_true(
        "glyph ink at bim col1 row0",
        pixel_at(&iw, imagewriter_bim_x(107, 1), 0) == A2_IW_INK);

    teardown(&iw);
}

static void test_colour_esc_ignored(void)
{
    imagewriter iw;

    setup(&iw);
    put_str(&iw, "\x1b" "K3"); /* colour select */
    expect_eq_int("after colour IDLE", (int)A2_IW_PARSE_IDLE, (int)iw.parse_state);
    imagewriter_putc(&iw, (uint8_t)'B');
    expect_true("text still works", imagewriter_page_dirty(&iw));
    teardown(&iw);
}

static void test_high_ascii_7bit_mask_bim_8bit(void)
{
    imagewriter iw;

    setup(&iw);
    iw.auto_lf_after_cr = true;
    iw.head_col = 24;
    imagewriter_putc(&iw, 0x8Du); /* high-ASCII CR */
    expect_eq_int("0x8D → CR head", 0, iw.head_col);
    expect_eq_int("0x8D → CR auto LF", 12, iw.cursor_y_dots);

    imagewriter_putc(&iw, 0x9Bu); /* high-ASCII ESC */
    imagewriter_putc(&iw, (uint8_t)'T');
    imagewriter_putc(&iw, (uint8_t)'1');
    imagewriter_putc(&iw, (uint8_t)'6');
    expect_eq_int("0x9B ESC T16", 8, iw.lf_dots);

    imagewriter_reset(&iw);
    put_str(&iw, "\x1b" "n\x1b" "G0001");
    imagewriter_putc(&iw, 0x80u); /* must stay 8-bit in BIM */
    expect_true("BIM bit7 bottom pin", pixel_at(&iw, imagewriter_bim_x(72, 0), 7) == A2_IW_INK);
    expect_true("BIM bit7 not top", pixel_at(&iw, imagewriter_bim_x(72, 0), 0) == A2_IW_PAPER);

    imagewriter_reset(&iw);
    put_str(&iw, "\x1b" "N"); /* latch seen_esc (job started) */
    imagewriter_putc(&iw, 0xC1u); /* high-ASCII 'A' */
    expect_true("high-ASCII glyph", imagewriter_page_dirty(&iw));
    expect_eq_int("high-ASCII head", 8, iw.head_col);

    teardown(&iw);
}

static void test_esc_g_times_eight(void)
{
    imagewriter iw;

    setup(&iw);
    put_str(&iw, "\x1b" "P");
    put_str(&iw, "\x1b" "g002"); /* 16 columns */
    expect_eq_int("g pending", 16, iw.bim_remaining);
    while (iw.bim_remaining > 0) {
        imagewriter_putc(&iw, 0x01u);
    }
    expect_eq_int("g cols", 16, iw.head_col);
    expect_eq_int("g drain", (int)A2_IW_PARSE_BIM_DATA, (int)iw.parse_state);
    imagewriter_putc(&iw, 0x0Au);
    expect_eq_int("g idle after LF", (int)A2_IW_PARSE_IDLE, (int)iw.parse_state);
    teardown(&iw);
}

/* Print Shop greeting cards: ESC T24 after BIM soft-breaks without FF. */
static void test_esc_t24_soft_page_break_after_bim(void)
{
    imagewriter iw;
    const char *dir = "iw_tmp_t24_break";
    int i;

    (void)test_mkdir(dir);
    setup(&iw);
    imagewriter_set_output_dir(&iw, dir);

    put_str(&iw, "\x1b" "P\x1b" "T14");
    put_str(&iw, "\x1b" "G0008");
    for (i = 0; i < 8; ++i) {
        imagewriter_putc(&iw, 0xFFu);
    }
    imagewriter_putc(&iw, 0x0Du);
    imagewriter_putc(&iw, 0x0Au);
    expect_true("dirty before T24", imagewriter_page_dirty(&iw));
    expect_true("saw_bim", iw.saw_bim);
    expect_eq_u32("no page yet", 0u, imagewriter_pages_flushed(&iw));

    put_str(&iw, "\x1b" "T24"); /* soft page break */
    expect_eq_u32("flushed face 1", 1u, imagewriter_pages_flushed(&iw));
    expect_eq_int("y reset", 0, iw.cursor_y_dots);
    expect_true("saw_bim cleared", !iw.saw_bim);

    put_str(&iw, "\x1b" "T14\x1b" "G0004");
    for (i = 0; i < 4; ++i) {
        imagewriter_putc(&iw, 0x01u);
    }
    imagewriter_putc(&iw, 0x0Cu); /* FF */
    expect_eq_u32("face 2", 2u, imagewriter_pages_flushed(&iw));

    /* ESC T24 with no BIM yet must not invent a blank page. */
    put_str(&iw, "\x1b" "T24");
    expect_eq_u32("no blank on early T24", 2u, imagewriter_pages_flushed(&iw));

    teardown(&iw);
    /* cleanup bmps */
    {
        DIR *d = opendir(dir);
        struct dirent *de;
        char path[256];
        if (d != NULL) {
            while ((de = readdir(d)) != NULL) {
                if (strstr(de->d_name, ".bmp") != NULL) {
                    snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
                    (void)remove(path);
                }
            }
            closedir(d);
        }
    }
    (void)test_rmdir(dir);
}

static void test_esc_gt_lt_noop(void)
{
    imagewriter iw;

    setup(&iw);
    put_str(&iw, "\x1b" ">\x1b" "P\x1b" "<");
    expect_eq_int("dpi after >P<", 160, iw.dpi);
    expect_eq_int("IDLE", (int)A2_IW_PARSE_IDLE, (int)iw.parse_state);
    teardown(&iw);
}

/* LF must CR-reset head so a following BIM band does not start mid-line. */
static void test_lf_resets_head_after_bim(void)
{
    imagewriter iw;
    int i;

    setup(&iw);
    put_str(&iw, "\x1b" "P");
    put_str(&iw, "\x1b" "G0008");
    for (i = 0; i < 8; ++i) {
        imagewriter_putc(&iw, 0x01u);
    }
    expect_eq_int("head after BIM", 8, iw.head_col);
    imagewriter_putc(&iw, 0x0Au); /* LF only — no CR */
    expect_eq_int("LF resets head", 0, iw.head_col);
    expect_eq_int("LF advanced Y", 12, iw.cursor_y_dots);
    teardown(&iw);
}

/*
 * Print Shop sometimes ends a counted BIM run then sends more column bytes
 * before CR/LF. Those must stay BIM (not text), or the face corrupts.
 * A pin-byte 0x0A deep in overshoot must not be mistaken for a real LF.
 */
static void test_bim_overshoot_until_cr(void)
{
    imagewriter iw;
    int i;

    setup(&iw);
    put_str(&iw, "\x1b" "P\x1b" "T14");
    put_str(&iw, "\x1b" "G0008");
    for (i = 0; i < 8; ++i) {
        imagewriter_putc(&iw, 0xFFu);
    }
    expect_eq_int("count done head", 8, iw.head_col);
    /* Push head well past declared+16 so a later 0x0A is pins, not LF. */
    for (i = 0; i < 40; ++i) {
        imagewriter_putc(&iw, 0x01u);
    }
    expect_eq_int("still BIM state", (int)A2_IW_PARSE_BIM_DATA, (int)iw.parse_state);
    expect_true("head far past declared", iw.head_col > iw.bim_declared + 16);
    imagewriter_putc(&iw, 0x0Au);
    expect_eq_int("far 0x0A still BIM", (int)A2_IW_PARSE_BIM_DATA, (int)iw.parse_state);
    imagewriter_putc(&iw, 0x1Bu);
    imagewriter_putc(&iw, (uint8_t)'>');
    expect_eq_int("ESC ends overshoot", (int)A2_IW_PARSE_IDLE, (int)iw.parse_state);
    teardown(&iw);
}

static void test_esc_g_resets_head(void)
{
    imagewriter iw;

    setup(&iw);
    iw.head_col = 400;
    put_str(&iw, "\x1b" "G0001");
    expect_eq_int("G homes head before data", 0, iw.head_col);
    imagewriter_putc(&iw, 0x01u);
    expect_eq_int("one col", 1, iw.head_col);
    teardown(&iw);
}

static void test_preamble_tab_z_suppressed(void)
{
    imagewriter iw;

    setup(&iw);
    imagewriter_putc(&iw, 0x09u);
    imagewriter_putc(&iw, (uint8_t)'Z');
    imagewriter_putc(&iw, 0x0Du);
    expect_true("no ink from preamble Z", !imagewriter_page_dirty(&iw));
    put_str(&iw, "\x1b" "N");
    imagewriter_putc(&iw, (uint8_t)'A');
    expect_true("glyph after ESC ok", imagewriter_page_dirty(&iw));
    teardown(&iw);
}

/* Last counted BIM byte is 0x0D (pins); following LF must still home the head. */
static void test_bim_last_byte_cr_then_lf(void)
{
    imagewriter iw;
    int i;

    setup(&iw);
    put_str(&iw, "\x1b" "P");
    put_str(&iw, "\x1b" "G0004");
    imagewriter_putc(&iw, 0x01u);
    imagewriter_putc(&iw, 0x02u);
    imagewriter_putc(&iw, 0x04u);
    imagewriter_putc(&iw, 0x0Du); /* last pin mask happens to be CR */
    expect_eq_int("still BIM until drain", (int)A2_IW_PARSE_BIM_DATA, (int)iw.parse_state);
    expect_eq_int("head at 4", 4, iw.head_col);
    imagewriter_putc(&iw, 0x0Au); /* LF — must end BIM + home */
    expect_eq_int("IDLE after LF", (int)A2_IW_PARSE_IDLE, (int)iw.parse_state);
    expect_eq_int("head homed", 0, iw.head_col);
    teardown(&iw);
}

/* Optional: replay a captured Print Shop stream if present in-tree. */
static void test_replay_printshop_capture_if_present(void)
{
    const char *path = "assets/apple2/prints/printer-capture-20260902-141356.raw";
    const char *dir = "iw_tmp_replay_ps";
    FILE *fp;
    imagewriter iw;
    int c;
    int n;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return; /* asset not required for CI */
    }
    (void)test_mkdir(dir);
    setup(&iw);
    imagewriter_set_output_dir(&iw, dir);
    while ((c = fgetc(fp)) != EOF) {
        imagewriter_putc(&iw, (uint8_t)c);
    }
    fclose(fp);
    imagewriter_force_flush(&iw);
    n = 0;
    {
        DIR *d = opendir(dir);
        struct dirent *de;
        char pbuf[256];
        if (d != NULL) {
            while ((de = readdir(d)) != NULL) {
                if (is_print_page_name(de->d_name)) {
                    n++;
                    snprintf(pbuf, sizeof(pbuf), "%s/%s", dir, de->d_name);
                    (void)remove(pbuf);
                }
            }
            closedir(d);
        }
    }
    expect_eq_int("Print Shop capture → 2 host pages", 2, n);
    teardown(&iw);
    (void)test_rmdir(dir);
}

int main(void)
{
    test_pitch_table_and_bim_x();
    test_text_x_n_and_e();
    test_cr_lf_esc_t_ff();
    test_esc_g_bim_no_auto_y();
    test_digit_spaces_in_nnnn();
    test_esc_v_and_text_pitch_cmds();
    test_colour_esc_ignored();
    test_high_ascii_7bit_mask_bim_8bit();
    test_esc_g_times_eight();
    test_esc_t24_soft_page_break_after_bim();
    test_esc_gt_lt_noop();
    test_lf_resets_head_after_bim();
    test_bim_overshoot_until_cr();
    test_bim_last_byte_cr_then_lf();
    test_esc_g_resets_head();
    test_preamble_tab_z_suppressed();
    test_replay_printshop_capture_if_present();
    printf("ok\n");
    return 0;
}
