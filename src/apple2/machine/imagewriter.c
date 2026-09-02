#include "imagewriter.h"

#include "host_log.h"
#include "host_page_name.h"
#include "host_page_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 7×8 draft glyphs: 8 rows, bits 6..0 = columns left→right. PD / hand-built. */
typedef uint8_t a2_iw_glyph[8];

static const a2_iw_glyph k_glyphs_ascii[95] = {
    /* 0x20 space */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* ! */ {0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x10, 0x00},
    /* " */ {0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* # */ {0x28, 0x28, 0x7C, 0x28, 0x7C, 0x28, 0x28, 0x00},
    /* $ */ {0x10, 0x3C, 0x50, 0x38, 0x14, 0x78, 0x10, 0x00},
    /* % */ {0x60, 0x64, 0x08, 0x10, 0x20, 0x4C, 0x0C, 0x00},
    /* & */ {0x30, 0x48, 0x50, 0x20, 0x54, 0x48, 0x34, 0x00},
    /* ' */ {0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* ( */ {0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00},
    /* ) */ {0x20, 0x10, 0x08, 0x08, 0x08, 0x10, 0x20, 0x00},
    /* * */ {0x00, 0x28, 0x10, 0x7C, 0x10, 0x28, 0x00, 0x00},
    /* + */ {0x00, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x00, 0x00},
    /* , */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x20},
    /* - */ {0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0x00, 0x00},
    /* . */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00},
    /* / */ {0x04, 0x04, 0x08, 0x10, 0x20, 0x40, 0x40, 0x00},
    /* 0 */ {0x38, 0x44, 0x4C, 0x54, 0x64, 0x44, 0x38, 0x00},
    /* 1 */ {0x10, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00},
    /* 2 */ {0x38, 0x44, 0x04, 0x18, 0x20, 0x40, 0x7C, 0x00},
    /* 3 */ {0x38, 0x44, 0x04, 0x18, 0x04, 0x44, 0x38, 0x00},
    /* 4 */ {0x08, 0x18, 0x28, 0x48, 0x7C, 0x08, 0x08, 0x00},
    /* 5 */ {0x7C, 0x40, 0x78, 0x04, 0x04, 0x44, 0x38, 0x00},
    /* 6 */ {0x18, 0x20, 0x40, 0x78, 0x44, 0x44, 0x38, 0x00},
    /* 7 */ {0x7C, 0x04, 0x08, 0x10, 0x20, 0x20, 0x20, 0x00},
    /* 8 */ {0x38, 0x44, 0x44, 0x38, 0x44, 0x44, 0x38, 0x00},
    /* 9 */ {0x38, 0x44, 0x44, 0x3C, 0x04, 0x08, 0x30, 0x00},
    /* : */ {0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00, 0x00},
    /* ; */ {0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x10, 0x20},
    /* < */ {0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00},
    /* = */ {0x00, 0x00, 0x7C, 0x00, 0x7C, 0x00, 0x00, 0x00},
    /* > */ {0x20, 0x10, 0x08, 0x04, 0x08, 0x10, 0x20, 0x00},
    /* ? */ {0x38, 0x44, 0x04, 0x08, 0x10, 0x00, 0x10, 0x00},
    /* @ */ {0x38, 0x44, 0x5C, 0x54, 0x5C, 0x40, 0x38, 0x00},
    /* A */ {0x38, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x44, 0x00},
    /* B */ {0x78, 0x44, 0x44, 0x78, 0x44, 0x44, 0x78, 0x00},
    /* C */ {0x38, 0x44, 0x40, 0x40, 0x40, 0x44, 0x38, 0x00},
    /* D */ {0x78, 0x44, 0x44, 0x44, 0x44, 0x44, 0x78, 0x00},
    /* E */ {0x7C, 0x40, 0x40, 0x78, 0x40, 0x40, 0x7C, 0x00},
    /* F */ {0x7C, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x00},
    /* G */ {0x38, 0x44, 0x40, 0x5C, 0x44, 0x44, 0x38, 0x00},
    /* H */ {0x44, 0x44, 0x44, 0x7C, 0x44, 0x44, 0x44, 0x00},
    /* I */ {0x38, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00},
    /* J */ {0x1C, 0x08, 0x08, 0x08, 0x08, 0x48, 0x30, 0x00},
    /* K */ {0x44, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0x00},
    /* L */ {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7C, 0x00},
    /* M */ {0x44, 0x6C, 0x54, 0x54, 0x44, 0x44, 0x44, 0x00},
    /* N */ {0x44, 0x64, 0x54, 0x4C, 0x44, 0x44, 0x44, 0x00},
    /* O */ {0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00},
    /* P */ {0x78, 0x44, 0x44, 0x78, 0x40, 0x40, 0x40, 0x00},
    /* Q */ {0x38, 0x44, 0x44, 0x44, 0x54, 0x48, 0x34, 0x00},
    /* R */ {0x78, 0x44, 0x44, 0x78, 0x50, 0x48, 0x44, 0x00},
    /* S */ {0x38, 0x44, 0x40, 0x38, 0x04, 0x44, 0x38, 0x00},
    /* T */ {0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00},
    /* U */ {0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00},
    /* V */ {0x44, 0x44, 0x44, 0x44, 0x44, 0x28, 0x10, 0x00},
    /* W */ {0x44, 0x44, 0x44, 0x54, 0x54, 0x6C, 0x44, 0x00},
    /* X */ {0x44, 0x44, 0x28, 0x10, 0x28, 0x44, 0x44, 0x00},
    /* Y */ {0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x10, 0x00},
    /* Z */ {0x7C, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7C, 0x00},
    /* [ */ {0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00},
    /* \ */ {0x40, 0x40, 0x20, 0x10, 0x08, 0x04, 0x04, 0x00},
    /* ] */ {0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38, 0x00},
    /* ^ */ {0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* _ */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00},
    /* ` */ {0x20, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* a */ {0x00, 0x00, 0x38, 0x04, 0x3C, 0x44, 0x3C, 0x00},
    /* b */ {0x40, 0x40, 0x78, 0x44, 0x44, 0x44, 0x78, 0x00},
    /* c */ {0x00, 0x00, 0x38, 0x40, 0x40, 0x40, 0x38, 0x00},
    /* d */ {0x04, 0x04, 0x3C, 0x44, 0x44, 0x44, 0x3C, 0x00},
    /* e */ {0x00, 0x00, 0x38, 0x44, 0x7C, 0x40, 0x38, 0x00},
    /* f */ {0x18, 0x20, 0x20, 0x78, 0x20, 0x20, 0x20, 0x00},
    /* g */ {0x00, 0x00, 0x3C, 0x44, 0x44, 0x3C, 0x04, 0x38},
    /* h */ {0x40, 0x40, 0x78, 0x44, 0x44, 0x44, 0x44, 0x00},
    /* i */ {0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00},
    /* j */ {0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x48, 0x30},
    /* k */ {0x40, 0x40, 0x48, 0x50, 0x60, 0x50, 0x48, 0x00},
    /* l */ {0x30, 0x10, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00},
    /* m */ {0x00, 0x00, 0x68, 0x54, 0x54, 0x44, 0x44, 0x00},
    /* n */ {0x00, 0x00, 0x78, 0x44, 0x44, 0x44, 0x44, 0x00},
    /* o */ {0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x38, 0x00},
    /* p */ {0x00, 0x00, 0x78, 0x44, 0x44, 0x78, 0x40, 0x40},
    /* q */ {0x00, 0x00, 0x3C, 0x44, 0x44, 0x3C, 0x04, 0x04},
    /* r */ {0x00, 0x00, 0x58, 0x64, 0x40, 0x40, 0x40, 0x00},
    /* s */ {0x00, 0x00, 0x3C, 0x40, 0x38, 0x04, 0x78, 0x00},
    /* t */ {0x20, 0x20, 0x78, 0x20, 0x20, 0x20, 0x18, 0x00},
    /* u */ {0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x3C, 0x00},
    /* v */ {0x00, 0x00, 0x44, 0x44, 0x44, 0x28, 0x10, 0x00},
    /* w */ {0x00, 0x00, 0x44, 0x44, 0x54, 0x54, 0x28, 0x00},
    /* x */ {0x00, 0x00, 0x44, 0x28, 0x10, 0x28, 0x44, 0x00},
    /* y */ {0x00, 0x00, 0x44, 0x44, 0x44, 0x3C, 0x04, 0x38},
    /* z */ {0x00, 0x00, 0x7C, 0x08, 0x10, 0x20, 0x7C, 0x00},
    /* { */ {0x18, 0x20, 0x20, 0x40, 0x20, 0x20, 0x18, 0x00},
    /* | */ {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00},
    /* } */ {0x30, 0x08, 0x08, 0x04, 0x08, 0x08, 0x30, 0x00},
    /* ~ */ {0x00, 0x00, 0x20, 0x54, 0x08, 0x00, 0x00, 0x00},
};

int imagewriter_bim_x(int dpi, int col)
{
    if (dpi <= 0) {
        return 0;
    }
    return (col * A2_IW_REF_DPI + dpi / 2) / dpi;
}

int imagewriter_text_x(int dpi, int char_i)
{
    if (dpi <= 0) {
        return 0;
    }
    return (char_i * 8 * A2_IW_REF_DPI + dpi / 2) / dpi;
}

int imagewriter_pitch_dpi(uint8_t cmd)
{
    switch (cmd) {
    case 'n':
        return 72;
    case 'N':
        return 80;
    case 'E':
        return 96;
    case 'e':
        return 107;
    case 'q':
        return 120;
    case 'Q':
        return 136;
    case 'p':
        return 144;
    case 'P':
        return 160;
    default:
        return 0;
    }
}

int imagewriter_pitch_max_nnnn(int dpi)
{
    switch (dpi) {
    case 72:
        return 576;
    case 80:
        return 640;
    case 96:
        return 768;
    case 107:
        return 856;
    case 120:
        return 960;
    case 136:
        return 1088;
    case 144:
        return 1152;
    case 160:
        return 1280;
    default:
        return A2_IW_WIDTH_DOTS;
    }
}

static void clear_page(imagewriter *iw)
{
    if (iw->raster != NULL && iw->raster_bytes > 0u) {
        memset(iw->raster, A2_IW_PAPER, iw->raster_bytes);
    }
    iw->head_col = 0;
    iw->cursor_y_dots = 0;
    iw->page_dirty = false;
    iw->saw_bim = false;
}

static void reset_modes(imagewriter *iw)
{
    iw->dpi = 80; /* ESC N pica default */
    iw->lf_dots = 12; /* ESC A / ESC T24 */
    /* Default off: SSC firmware / apps usually emit LF (or CR+LF). Auto-LF
       here double-spaced Print Shop BIM bands (~8 ink + ~8 blank). */
    iw->auto_lf_after_cr = false;
    iw->parse_state = A2_IW_PARSE_IDLE;
    iw->parse_digits_needed = 0;
    iw->parse_digits_got = 0;
    iw->parse_value = 0;
    iw->bim_remaining = 0;
    iw->bim_clip_logged = false;
    iw->saw_bim = false;
}

static void set_pixel(imagewriter *iw, int x, int y)
{
    size_t idx;

    if (iw->raster == NULL) {
        return;
    }
    if (x < 0 || y < 0 || x >= A2_IW_WIDTH_DOTS || y >= A2_IW_HEIGHT_DOTS) {
        return;
    }
    idx = (size_t)y * (size_t)A2_IW_WIDTH_DOTS + (size_t)x;
    iw->raster[idx] = A2_IW_INK;
    iw->page_dirty = true;
}

static bool flush_page(imagewriter *iw)
{
    host_page_image page;
    host_page_name_state name_st;
    char path[A2_IW_PATH_MAX + 32];
    char stem[16];
    uint8_t xx;
    uint32_t next;

    if (!iw->page_dirty) {
        iw->flush_hold = false;
        return true;
    }
    if (iw->page_cap_hit || iw->pages_flushed >= (uint32_t)A2_IW_PAGE_CAP) {
        if (!iw->page_cap_hit) {
            log_warn("printer: page cap (%d) reached; skipping write", A2_IW_PAGE_CAP);
            iw->page_cap_hit = true;
        }
        clear_page(iw);
        iw->flush_hold = false;
        return true;
    }
    if (iw->output_dir[0] == '\0') {
        log_error("printer: flush failed (no output_dir)");
        iw->flush_hold = true;
        return false;
    }
    if (iw->raster == NULL) {
        iw->flush_hold = true;
        return false;
    }
    if (!host_page_writer_ensure_dir(iw->output_dir)) {
        log_error("printer: ensure_dir failed for %s", iw->output_dir);
        iw->flush_hold = true;
        return false;
    }

    memset(&name_st, 0, sizeof(name_st));
    memcpy(name_st.last_stem, iw->last_name_stem, sizeof(name_st.last_stem));
    name_st.seq = iw->name_seq;
    if (!host_page_name_build_path(
            &name_st,
            iw->output_dir,
            "bmp",
            path,
            sizeof(path),
            stem,
            &xx)) {
        log_error("printer: page name build failed (clock, path, or XX exhausted)");
        iw->flush_hold = true;
        return false;
    }

    memset(&page, 0, sizeof(page));
    page.width = (uint32_t)A2_IW_WIDTH_DOTS;
    page.height = (uint32_t)A2_IW_HEIGHT_DOTS;
    page.stride_bytes = (uint32_t)A2_IW_WIDTH_DOTS;
    page.bits_per_pixel = 8;
    page.pixels = iw->raster;

    if (!host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, path)) {
        log_error("printer: write failed for %s", path);
        iw->flush_hold = true;
        return false;
    }

    log_info("printer: wrote %s (%ux%u)", path, page.width, page.height);
    next = iw->pages_flushed + 1u;
    iw->pages_flushed = next;
    memcpy(iw->last_name_stem, stem, sizeof(iw->last_name_stem));
    iw->name_seq = xx;
    clear_page(iw);
    iw->flush_hold = false;
    return true;
}

static void advance_lf(imagewriter *iw)
{
    int pitch = iw->lf_dots;

    if (pitch < 1) {
        pitch = 1;
    }
    if (iw->cursor_y_dots + pitch > A2_IW_HEIGHT_DOTS) {
        if (flush_page(iw)) {
            iw->head_col = 0;
            iw->cursor_y_dots = 0;
        }
        return;
    }
    iw->cursor_y_dots += pitch;
}

static void plot_bim_column(imagewriter *iw, uint8_t data)
{
    int x;
    int row;
    int maxn = imagewriter_pitch_max_nnnn(iw->dpi);

    if (iw->head_col >= maxn) {
        if (!iw->bim_clip_logged) {
            log_warn("printer: BIM column clipped at dpi=%d max=%d", iw->dpi, maxn);
            iw->bim_clip_logged = true;
        }
        return;
    }

    x = imagewriter_bim_x(iw->dpi, iw->head_col);
    if (x < 0) {
        x = 0;
    }
    if (x >= A2_IW_WIDTH_DOTS) {
        x = A2_IW_WIDTH_DOTS - 1;
    }
    for (row = 0; row < 8; ++row) {
        if ((data & (uint8_t)(1u << row)) != 0u) {
            set_pixel(iw, x, iw->cursor_y_dots + row);
        }
    }
    iw->head_col += 1;
    iw->saw_bim = true;
}

static void print_char(imagewriter *iw, uint8_t ch)
{
    const a2_iw_glyph *g;
    int char_i;
    int row;
    int col;
    int maxn = imagewriter_pitch_max_nnnn(iw->dpi);

    if (ch < 0x20u || ch > 0x7Eu) {
        ch = 0x20u;
    }
    g = &k_glyphs_ascii[ch - 0x20u];

    char_i = iw->head_col / 8;
    if (iw->head_col + 8 > maxn) {
        /* Soft wrap: LF then continue at left. */
        advance_lf(iw);
        if (iw->flush_hold) {
            return;
        }
        iw->head_col = 0;
        char_i = 0;
    }

    /* 7 printable columns left-aligned in the 8-dot cell; each col via BIM placer. */
    for (col = 0; col < 7; ++col) {
        int x = imagewriter_bim_x(iw->dpi, char_i * 8 + col);
        if (x < 0) {
            x = 0;
        }
        if (x >= A2_IW_WIDTH_DOTS) {
            continue;
        }
        for (row = 0; row < 8; ++row) {
            if (((*g)[row] & (uint8_t)(0x40u >> col)) != 0u) {
                set_pixel(iw, x, iw->cursor_y_dots + row);
            }
        }
    }
    iw->head_col = (char_i + 1) * 8;
}

static void begin_digit_field(imagewriter *iw, imagewriter_parse st, int n_digits)
{
    iw->parse_state = st;
    iw->parse_digits_needed = n_digits;
    iw->parse_digits_got = 0;
    iw->parse_value = 0;
}

/* -1 abort, 0 need more digits, 1 field complete. */
static int feed_digit(imagewriter *iw, uint8_t ch)
{
    int d;

    if (ch == (uint8_t)' ') {
        d = 0;
    } else if (ch >= (uint8_t)'0' && ch <= (uint8_t)'9') {
        d = (int)(ch - (uint8_t)'0');
    } else {
        iw->parse_state = A2_IW_PARSE_IDLE;
        return -1;
    }
    iw->parse_value = iw->parse_value * 10 + d;
    iw->parse_digits_got += 1;
    return (iw->parse_digits_got >= iw->parse_digits_needed) ? 1 : 0;
}

static void soft_page_break_if_needed(imagewriter *iw)
{
    /*
     * Print Shop ImageWriter greeting cards do not send FF between the
     * outside and inside faces. They re-init line spacing with ESC T24
     * (same as ESC A / 6 LPI) after BIM, then continue. Treat that as a
     * flush so each face becomes its own host page file.
     */
    if (!iw->saw_bim || !iw->page_dirty || iw->cursor_y_dots <= 0) {
        return;
    }
    if (flush_page(iw)) {
        iw->head_col = 0;
        iw->cursor_y_dots = 0;
    }
}

static void apply_esc_t(imagewriter *iw, int mm)
{
    /* mm/144 inch → buffer dots at 72 dpi vertical: mm/2 */
    if (mm < 0) {
        mm = 0;
    }
    if (mm > 99) {
        mm = 99;
    }
    if (mm == 24) {
        soft_page_break_if_needed(iw);
    }
    iw->lf_dots = mm / 2;
    if (iw->lf_dots < 1) {
        iw->lf_dots = 1;
    }
}

static void handle_esc_cmd(imagewriter *iw, uint8_t ch)
{
    int dpi;

    dpi = imagewriter_pitch_dpi(ch);
    if (dpi > 0) {
        iw->dpi = dpi;
        iw->parse_state = A2_IW_PARSE_IDLE;
        return;
    }

    switch (ch) {
    case 'T':
        begin_digit_field(iw, A2_IW_PARSE_ESC_T_D1, 2);
        break;
    case 'A':
        apply_esc_t(iw, 24); /* also soft page-break after BIM */
        iw->parse_state = A2_IW_PARSE_IDLE;
        break;
    case 'B':
        apply_esc_t(iw, 18);
        iw->parse_state = A2_IW_PARSE_IDLE;
        break;
    case '>': /* unidirectional (no args) */
    case '<': /* bidirectional (no args) */
        iw->parse_state = A2_IW_PARSE_IDLE;
        break;
    case 'G':
    case 'S':
        begin_digit_field(iw, A2_IW_PARSE_ESC_G_DIGITS, 4);
        break;
    case 'g':
        begin_digit_field(iw, A2_IW_PARSE_ESC_g_DIGITS, 3);
        break;
    case 'V':
        begin_digit_field(iw, A2_IW_PARSE_ESC_V_DIGITS, 4);
        break;
    case 'F':
        begin_digit_field(iw, A2_IW_PARSE_ESC_F_DIGITS, 4);
        break;
    case 'c':
        if (flush_page(iw)) {
            reset_modes(iw);
            iw->head_col = 0;
            iw->cursor_y_dots = 0;
        }
        iw->parse_state = A2_IW_PARSE_IDLE;
        break;
    case 'K': /* colour ribbon — ignore arg in mono v1 */
        iw->parse_state = A2_IW_PARSE_ESC_K_ARG;
        break;
    default:
        /* Unknown / deferred ESC (bold, underline, soft switches, …). */
        iw->parse_state = A2_IW_PARSE_IDLE;
        break;
    }
}

static void handle_control(imagewriter *iw, uint8_t ch)
{
    switch (ch) {
    case 0x08: /* BS: previous character cell */
        if (iw->head_col >= 8) {
            iw->head_col -= 8;
            iw->head_col = (iw->head_col / 8) * 8;
        } else {
            iw->head_col = 0;
        }
        break;
    case 0x0A: /* LF */
        /*
         * ImageWriter "CR insertion before LF" (power-on typical / Print Shop):
         * when the last BIM column is 0x0D, the real CR is consumed as pins and
         * only LF follows — without resetting head, the next band starts at the
         * previous width (e.g. 682) and draws on the right half of the page.
         */
        iw->head_col = 0;
        advance_lf(iw);
        break;
    case 0x0C: /* FF */
        if (flush_page(iw)) {
            iw->head_col = 0;
            iw->cursor_y_dots = 0;
        }
        break;
    case 0x0D: /* CR */
        iw->head_col = 0;
        if (iw->auto_lf_after_cr) {
            advance_lf(iw);
        }
        break;
    case 0x1B: /* ESC */
        iw->parse_state = A2_IW_PARSE_ESC;
        break;
    default:
        break;
    }
}

void imagewriter_init(imagewriter *iw)
{
    size_t bytes;

    if (iw == NULL) {
        return;
    }
    memset(iw, 0, sizeof(*iw));
    bytes = (size_t)A2_IW_WIDTH_DOTS * (size_t)A2_IW_HEIGHT_DOTS;
    iw->raster = (uint8_t *)malloc(bytes);
    iw->raster_bytes = (iw->raster != NULL) ? bytes : 0u;
    clear_page(iw);
    reset_modes(iw);
}

void imagewriter_reset(imagewriter *iw)
{
    if (iw == NULL) {
        return;
    }
    clear_page(iw);
    reset_modes(iw);
    iw->flush_hold = false;
}

void imagewriter_shutdown(imagewriter *iw)
{
    if (iw == NULL) {
        return;
    }
    free(iw->raster);
    iw->raster = NULL;
    iw->raster_bytes = 0;
    iw->page_dirty = false;
    iw->output_dir[0] = '\0';
}

void imagewriter_set_output_dir(imagewriter *iw, const char *dir)
{
    if (iw == NULL) {
        return;
    }
    if (dir == NULL || dir[0] == '\0') {
        iw->output_dir[0] = '\0';
        return;
    }
    /* Overlap-safe: callers may pass iw->output_dir (or an alias). */
    if (dir != iw->output_dir) {
        char tmp[sizeof(iw->output_dir)];

        strncpy(tmp, dir, sizeof(tmp) - 1u);
        tmp[sizeof(tmp) - 1u] = '\0';
        memcpy(iw->output_dir, tmp, sizeof(iw->output_dir));
    }
}

void imagewriter_putc(imagewriter *iw, uint8_t ch)
{
    int i;
    int n;

    if (iw == NULL || iw->flush_hold) {
        return;
    }

    /*
     * ImageWriter defaults to 7-bit (ignore data bit 8) until bit-image
     * data. ESC G/S/g/V payloads are 8-bit pin masks. Apple II high-ASCII
     * CR/ESC/text ($8D/$9B/$C1) must still parse as controls/glyphs.
     */
    if (iw->parse_state != A2_IW_PARSE_BIM_DATA &&
        iw->parse_state != A2_IW_PARSE_ESC_V_DATA) {
        ch = (uint8_t)(ch & 0x7Fu);
    }

    switch (iw->parse_state) {
    case A2_IW_PARSE_ESC:
        handle_esc_cmd(iw, ch);
        return;

    case A2_IW_PARSE_ESC_T_D1:
    case A2_IW_PARSE_ESC_T_D2: {
        int r = feed_digit(iw, ch);
        if (r < 0) {
            return;
        }
        if (r == 0) {
            iw->parse_state = A2_IW_PARSE_ESC_T_D2;
            return;
        }
        apply_esc_t(iw, iw->parse_value);
        iw->parse_state = A2_IW_PARSE_IDLE;
        return;
    }

    case A2_IW_PARSE_ESC_G_DIGITS: {
        int r = feed_digit(iw, ch);
        if (r <= 0) {
            return;
        }
        iw->bim_remaining = iw->parse_value;
        iw->parse_state =
            (iw->bim_remaining > 0) ? A2_IW_PARSE_BIM_DATA : A2_IW_PARSE_IDLE;
        return;
    }

    case A2_IW_PARSE_ESC_g_DIGITS: {
        int r = feed_digit(iw, ch);
        if (r <= 0) {
            return;
        }
        iw->bim_remaining = iw->parse_value * 8;
        iw->parse_state =
            (iw->bim_remaining > 0) ? A2_IW_PARSE_BIM_DATA : A2_IW_PARSE_IDLE;
        return;
    }

    case A2_IW_PARSE_BIM_DATA:
        if (iw->bim_remaining > 0) {
            plot_bim_column(iw, ch);
            iw->bim_remaining -= 1;
            return;
        }
        /*
         * Declared nnnn exhausted. Print Shop ImageWriter output sometimes
         * sends additional column bytes before CR/LF (Thank You face). Keep
         * absorbing as BIM (clipped at page width) until a line terminator.
         * Only now may 0x0D/0x0A/ESC be treated as controls — during the
         * counted payload they are valid pin masks.
         */
        {
            uint8_t c7 = (uint8_t)(ch & 0x7Fu);
            if (c7 == 0x0Du || c7 == 0x0Au || c7 == 0x0Cu || c7 == 0x1Bu) {
                iw->parse_state = A2_IW_PARSE_IDLE;
                iw->bim_remaining = 0;
                ch = c7; /* control path is 7-bit */
                break; /* fall through to control / text handling below */
            }
            plot_bim_column(iw, ch);
            return;
        }

    case A2_IW_PARSE_ESC_V_DIGITS: {
        int r = feed_digit(iw, ch);
        if (r <= 0) {
            return;
        }
        iw->bim_remaining = iw->parse_value;
        iw->parse_state = A2_IW_PARSE_ESC_V_DATA;
        return;
    }

    case A2_IW_PARSE_ESC_V_DATA:
        n = iw->bim_remaining;
        for (i = 0; i < n; ++i) {
            plot_bim_column(iw, ch);
        }
        iw->bim_remaining = 0;
        iw->parse_state = A2_IW_PARSE_IDLE;
        return;

    case A2_IW_PARSE_ESC_F_DIGITS: {
        int r = feed_digit(iw, ch);
        if (r <= 0) {
            return;
        }
        {
            int maxn = imagewriter_pitch_max_nnnn(iw->dpi);
            int col = iw->parse_value;
            if (col < 0) {
                col = 0;
            }
            if (col > maxn) {
                col = maxn;
            }
            iw->head_col = col;
        }
        iw->parse_state = A2_IW_PARSE_IDLE;
        return;
    }

    case A2_IW_PARSE_ESC_K_ARG:
        iw->parse_state = A2_IW_PARSE_IDLE;
        return;

    case A2_IW_PARSE_IDLE:
    default:
        break;
    }

    if (ch == 0x08u || ch == 0x0Au || ch == 0x0Cu || ch == 0x0Du || ch == 0x1Bu) {
        handle_control(iw, ch);
        return;
    }
    if (ch == 0x00u) {
        /* NUL: no-op (Print Shop pads; must not advance like a glyph). */
        return;
    }
    print_char(iw, ch);
}

void imagewriter_force_flush(imagewriter *iw)
{
    if (iw == NULL) {
        return;
    }
    (void)flush_page(iw);
}

uint32_t imagewriter_pages_flushed(const imagewriter *iw)
{
    return iw != NULL ? iw->pages_flushed : 0u;
}

bool imagewriter_page_dirty(const imagewriter *iw)
{
    return iw != NULL && iw->page_dirty;
}
