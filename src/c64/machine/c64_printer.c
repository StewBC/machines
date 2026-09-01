#include "c64_printer.h"

#include "host_log.h"
#include "host_page_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    C64_PRINTER_INK = 0,
    C64_PRINTER_PAPER = 255
};

/* 6×7 glyphs: each row is a byte with bits 5..0 left→right (bit5 = leftmost). */
typedef uint8_t c64_printer_glyph[7];

/* Hand-built subset (PD-clean). Index by ASCII 0x20..0x7E; others blank. */
static const c64_printer_glyph k_glyphs_ascii[95] = {
    /* 0x20 space */
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* ! */ {0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00},
    /* " */ {0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* # */ {0x14, 0x3E, 0x14, 0x14, 0x3E, 0x14, 0x00},
    /* $ */ {0x08, 0x1E, 0x28, 0x1C, 0x0A, 0x3C, 0x08},
    /* % */ {0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26},
    /* & */ {0x18, 0x24, 0x18, 0x2A, 0x24, 0x1A, 0x00},
    /* ' */ {0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* ( */ {0x04, 0x08, 0x10, 0x10, 0x10, 0x08, 0x04},
    /* ) */ {0x10, 0x08, 0x04, 0x04, 0x04, 0x08, 0x10},
    /* * */ {0x00, 0x14, 0x08, 0x3E, 0x08, 0x14, 0x00},
    /* + */ {0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00},
    /* , */ {0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x10},
    /* - */ {0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00},
    /* . */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00},
    /* / */ {0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20},
    /* 0 */ {0x1C, 0x22, 0x26, 0x2A, 0x32, 0x22, 0x1C},
    /* 1 */ {0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C},
    /* 2 */ {0x1C, 0x22, 0x02, 0x0C, 0x10, 0x20, 0x3E},
    /* 3 */ {0x1C, 0x22, 0x02, 0x0C, 0x02, 0x22, 0x1C},
    /* 4 */ {0x04, 0x0C, 0x14, 0x24, 0x3E, 0x04, 0x04},
    /* 5 */ {0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C},
    /* 6 */ {0x0C, 0x10, 0x20, 0x3C, 0x22, 0x22, 0x1C},
    /* 7 */ {0x3E, 0x02, 0x04, 0x08, 0x10, 0x10, 0x10},
    /* 8 */ {0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C},
    /* 9 */ {0x1C, 0x22, 0x22, 0x1E, 0x02, 0x04, 0x18},
    /* : */ {0x00, 0x08, 0x00, 0x00, 0x08, 0x00, 0x00},
    /* ; */ {0x00, 0x08, 0x00, 0x00, 0x08, 0x08, 0x10},
    /* < */ {0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04},
    /* = */ {0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00},
    /* > */ {0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10},
    /* ? */ {0x1C, 0x22, 0x02, 0x04, 0x08, 0x00, 0x08},
    /* @ */ {0x1C, 0x22, 0x2E, 0x2A, 0x2E, 0x20, 0x1C},
    /* A */ {0x1C, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22},
    /* B */ {0x3C, 0x22, 0x22, 0x3C, 0x22, 0x22, 0x3C},
    /* C */ {0x1C, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1C},
    /* D */ {0x3C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3C},
    /* E */ {0x3E, 0x20, 0x20, 0x3C, 0x20, 0x20, 0x3E},
    /* F */ {0x3E, 0x20, 0x20, 0x3C, 0x20, 0x20, 0x20},
    /* G */ {0x1C, 0x22, 0x20, 0x2E, 0x22, 0x22, 0x1C},
    /* H */ {0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22},
    /* I */ {0x1C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C},
    /* J */ {0x0E, 0x04, 0x04, 0x04, 0x04, 0x24, 0x18},
    /* K */ {0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22},
    /* L */ {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3E},
    /* M */ {0x22, 0x36, 0x2A, 0x2A, 0x22, 0x22, 0x22},
    /* N */ {0x22, 0x32, 0x2A, 0x26, 0x22, 0x22, 0x22},
    /* O */ {0x1C, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C},
    /* P */ {0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x20},
    /* Q */ {0x1C, 0x22, 0x22, 0x22, 0x2A, 0x24, 0x1A},
    /* R */ {0x3C, 0x22, 0x22, 0x3C, 0x28, 0x24, 0x22},
    /* S */ {0x1C, 0x22, 0x20, 0x1C, 0x02, 0x22, 0x1C},
    /* T */ {0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08},
    /* U */ {0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C},
    /* V */ {0x22, 0x22, 0x22, 0x22, 0x22, 0x14, 0x08},
    /* W */ {0x22, 0x22, 0x22, 0x2A, 0x2A, 0x36, 0x22},
    /* X */ {0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22},
    /* Y */ {0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08},
    /* Z */ {0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3E},
    /* [ */ {0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1C},
    /* \ */ {0x20, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02},
    /* ] */ {0x1C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1C},
    /* ^ */ {0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00},
    /* _ */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E},
    /* ` */ {0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* a */ {0x00, 0x00, 0x1C, 0x02, 0x1E, 0x22, 0x1E},
    /* b */ {0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x3C},
    /* c */ {0x00, 0x00, 0x1C, 0x20, 0x20, 0x20, 0x1C},
    /* d */ {0x02, 0x02, 0x1E, 0x22, 0x22, 0x22, 0x1E},
    /* e */ {0x00, 0x00, 0x1C, 0x22, 0x3E, 0x20, 0x1C},
    /* f */ {0x0C, 0x10, 0x10, 0x3C, 0x10, 0x10, 0x10},
    /* g */ {0x00, 0x00, 0x1E, 0x22, 0x22, 0x1E, 0x02},
    /* h */ {0x20, 0x20, 0x3C, 0x22, 0x22, 0x22, 0x22},
    /* i */ {0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1C},
    /* j */ {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x24},
    /* k */ {0x20, 0x20, 0x24, 0x28, 0x30, 0x28, 0x24},
    /* l */ {0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C},
    /* m */ {0x00, 0x00, 0x36, 0x2A, 0x2A, 0x22, 0x22},
    /* n */ {0x00, 0x00, 0x3C, 0x22, 0x22, 0x22, 0x22},
    /* o */ {0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x1C},
    /* p */ {0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20},
    /* q */ {0x00, 0x00, 0x1E, 0x22, 0x22, 0x1E, 0x02},
    /* r */ {0x00, 0x00, 0x2C, 0x32, 0x20, 0x20, 0x20},
    /* s */ {0x00, 0x00, 0x1E, 0x20, 0x1C, 0x02, 0x3C},
    /* t */ {0x10, 0x10, 0x3C, 0x10, 0x10, 0x10, 0x0C},
    /* u */ {0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x1E},
    /* v */ {0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x08},
    /* w */ {0x00, 0x00, 0x22, 0x22, 0x2A, 0x2A, 0x14},
    /* x */ {0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22},
    /* y */ {0x00, 0x00, 0x22, 0x22, 0x22, 0x1E, 0x02},
    /* z */ {0x00, 0x00, 0x3E, 0x04, 0x08, 0x10, 0x3E},
    /* { */ {0x0C, 0x10, 0x10, 0x20, 0x10, 0x10, 0x0C},
    /* | */ {0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08},
    /* } */ {0x18, 0x04, 0x04, 0x02, 0x04, 0x04, 0x18},
    /* ~ */ {0x00, 0x00, 0x10, 0x2A, 0x04, 0x00, 0x00},
};

/* A few PETSCII graphic cells for SA=0 (checker / bars). */
static const c64_printer_glyph k_glyph_block_full = {
    0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E, 0x3E
};
static const c64_printer_glyph k_glyph_block_left = {
    0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38
};
static const c64_printer_glyph k_glyph_block_bottom = {
    0x00, 0x00, 0x00, 0x00, 0x3E, 0x3E, 0x3E
};

static void clear_page(c64_printer *p)
{
    if (p->raster != NULL && p->raster_bytes > 0u) {
        memset(p->raster, C64_PRINTER_PAPER, p->raster_bytes);
    }
    p->cursor_x_dots = 0;
    p->cursor_y_dots = 0;
    p->page_dirty = false;
}

static void reset_modes(c64_printer *p)
{
    p->enhance = false;
    p->reverse = false;
    p->bit_image = false;
    p->parse_state = C64_PRINTER_PARSE_IDLE;
    memset(p->parse_buf, 0, sizeof(p->parse_buf));
}

static int lf_pitch(const c64_printer *p)
{
    return p->bit_image ? C64_PRINTER_BIM_LF_DOTS : C64_PRINTER_CHAR_LF_DOTS;
}

static void set_pixel(c64_printer *p, int x, int y)
{
    size_t idx;

    if (p->raster == NULL) {
        return;
    }
    if (x < 0 || y < 0 || x >= C64_PRINTER_WIDTH_DOTS || y >= C64_PRINTER_HEIGHT_DOTS) {
        return;
    }
    idx = (size_t)y * (size_t)C64_PRINTER_WIDTH_DOTS + (size_t)x;
    p->raster[idx] = C64_PRINTER_INK;
    p->page_dirty = true;
}

/* true: page buffer reusable (blank, written, or cap-scratched).
   false: I/O failure — dirty page retained; flush_hold set. */
static bool flush_page(c64_printer *p)
{
    host_page_image page;
    char path[C64_PRINTER_PATH_MAX + 32];
    uint32_t next;

    if (!p->page_dirty) {
        p->flush_hold = false;
        return true;
    }
    if (p->page_cap_hit || p->pages_flushed >= (uint32_t)C64_PRINTER_PAGE_CAP) {
        if (!p->page_cap_hit) {
            log_warn("printer: page cap (%d) reached; skipping write", C64_PRINTER_PAGE_CAP);
            p->page_cap_hit = true;
        }
        clear_page(p);
        p->flush_hold = false;
        return true;
    }
    if (p->output_dir[0] == '\0') {
        log_error("printer: flush failed (no output_dir)");
        p->flush_hold = true;
        return false;
    }
    if (p->raster == NULL) {
        p->flush_hold = true;
        return false;
    }
    if (!host_page_writer_ensure_dir(p->output_dir)) {
        log_error("printer: ensure_dir failed for %s", p->output_dir);
        p->flush_hold = true;
        return false;
    }

    next = p->pages_flushed + 1u;
    if (snprintf(path, sizeof(path), "%s/page_%04u.bmp", p->output_dir, next) >= (int)sizeof(path)) {
        log_error("printer: path too long");
        p->flush_hold = true;
        return false;
    }

    memset(&page, 0, sizeof(page));
    page.width = (uint32_t)C64_PRINTER_WIDTH_DOTS;
    page.height = (uint32_t)C64_PRINTER_HEIGHT_DOTS;
    page.stride_bytes = (uint32_t)C64_PRINTER_WIDTH_DOTS;
    page.bits_per_pixel = 8;
    page.pixels = p->raster;

    if (!host_page_writer_write(&page, HOST_PAGE_FORMAT_BMP, path)) {
        log_error("printer: write failed for %s", path);
        p->flush_hold = true;
        return false;
    }

    log_info("printer: wrote %s (%ux%u)", path, page.width, page.height);
    p->pages_flushed = next;
    clear_page(p);
    p->flush_hold = false;
    return true;
}

static void advance_line(c64_printer *p)
{
    int pitch = lf_pitch(p);

    if (p->cursor_y_dots + pitch > C64_PRINTER_HEIGHT_DOTS) {
        if (flush_page(p)) {
            p->cursor_x_dots = 0;
            p->cursor_y_dots = 0;
        }
        return;
    }
    p->cursor_x_dots = 0;
    p->cursor_y_dots += pitch;
}

static const c64_printer_glyph *lookup_glyph(const c64_printer *p, uint8_t ch)
{
    uint8_t mapped = ch;

    /* Business: PETSCII $41-$5A are lowercase. */
    if (!p->graphic_charset) {
        if (ch >= 0x41u && ch <= 0x5Au) {
            mapped = (uint8_t)(ch - 0x41u + (uint8_t)'a');
        } else if (ch >= 0xC1u && ch <= 0xDAu) {
            mapped = (uint8_t)(ch - 0xC1u + (uint8_t)'A');
        }
    } else {
        if (ch >= 0xC1u && ch <= 0xDAu) {
            mapped = (uint8_t)(ch - 0xC1u + (uint8_t)'A');
        }
        if (ch == 0xA0u) {
            return &k_glyph_block_full;
        }
        if (ch == 0xA1u) {
            return &k_glyph_block_left;
        }
        if (ch == 0xA2u) {
            return &k_glyph_block_bottom;
        }
    }

    if (mapped >= 0x20u && mapped <= 0x7Eu) {
        return &k_glyphs_ascii[mapped - 0x20u];
    }
    return &k_glyphs_ascii[0]; /* space */
}

static void plot_bim_column(c64_printer *p, uint8_t data)
{
    int x;
    int row;

    if (p->cursor_x_dots >= C64_PRINTER_WIDTH_DOTS) {
        return;
    }
    x = p->cursor_x_dots;
    for (row = 0; row < 7; ++row) {
        if ((data & (uint8_t)(1u << row)) != 0u) {
            set_pixel(p, x, p->cursor_y_dots + row);
        }
    }
    p->cursor_x_dots += 1;
}

static void print_char(c64_printer *p, uint8_t ch)
{
    const c64_printer_glyph *g = lookup_glyph(p, ch);
    int width = p->enhance ? 12 : 6;
    int col;
    int row;
    int dx;

    if (p->cursor_x_dots + width > C64_PRINTER_WIDTH_DOTS) {
        advance_line(p);
        if (p->flush_hold) {
            return;
        }
    }

    for (row = 0; row < 7; ++row) {
        uint8_t bits = (*g)[row];
        for (col = 0; col < 6; ++col) {
            bool ink = (bits & (uint8_t)(0x20u >> col)) != 0u;
            if (p->reverse) {
                ink = !ink;
            }
            if (!ink) {
                continue;
            }
            if (p->enhance) {
                for (dx = 0; dx < 2; ++dx) {
                    set_pixel(p, p->cursor_x_dots + col * 2 + dx, p->cursor_y_dots + row);
                }
            } else {
                set_pixel(p, p->cursor_x_dots + col, p->cursor_y_dots + row);
            }
        }
    }
    p->cursor_x_dots += width;
    if (p->cursor_x_dots >= C64_PRINTER_WIDTH_DOTS) {
        advance_line(p);
    }
}

static void apply_head_tab(c64_printer *p, uint8_t d0, uint8_t d1)
{
    int col;

    if (d0 < (uint8_t)'0' || d0 > (uint8_t)'9' || d1 < (uint8_t)'0' || d1 > (uint8_t)'9') {
        return;
    }
    col = (int)(d0 - (uint8_t)'0') * 10 + (int)(d1 - (uint8_t)'0');
    if (col < 0) {
        col = 0;
    }
    if (col > C64_PRINTER_COLS - 1) {
        col = C64_PRINTER_COLS - 1;
    }
    p->cursor_x_dots = col * C64_PRINTER_DOTS_PER_COL;
}

static void apply_dot_address(c64_printer *p, uint8_t nh, uint8_t nl)
{
    unsigned addr = ((unsigned)nh << 8) | (unsigned)nl;

    if (addr > (unsigned)C64_PRINTER_DOT_ADDR_MAX) {
        p->cursor_x_dots = 0;
        return;
    }
    if (addr >= (unsigned)C64_PRINTER_WIDTH_DOTS) {
        p->cursor_x_dots = C64_PRINTER_WIDTH_DOTS - 1;
        return;
    }
    p->cursor_x_dots = (int)addr;
}

static void handle_control(c64_printer *p, uint8_t ch)
{
    switch (ch) {
    case 8: /* BIM enter */
        p->bit_image = true;
        p->enhance = false;
        break;
    case 10: /* LF */
        advance_line(p);
        p->reverse = false;
        break;
    case 12: /* FF — emulator convenience */
        if (flush_page(p)) {
            p->cursor_x_dots = 0;
            p->cursor_y_dots = 0;
        }
        break;
    case 13: /* CR: print line, leave BIM */
        advance_line(p);
        p->bit_image = false;
        p->reverse = false;
        break;
    case 14: /* enhance on; leave BIM */
        p->bit_image = false;
        p->enhance = true;
        break;
    case 15: /* enhance off + leave BIM */
        p->bit_image = false;
        p->enhance = false;
        break;
    case 16: /* ASCII head tab */
        p->parse_state = C64_PRINTER_PARSE_HEAD_TAB_D1;
        break;
    case 17: /* local business */
        p->graphic_charset = false;
        break;
    case 18: /* reverse on */
        p->reverse = true;
        break;
    case 26: /* repeat */
        if (p->bit_image) {
            p->parse_state = C64_PRINTER_PARSE_REPEAT_N;
        }
        break;
    case 27: /* ESC */
        p->parse_state = C64_PRINTER_PARSE_ESC;
        break;
    case 145: /* local graphic */
        p->graphic_charset = true;
        break;
    case 146: /* reverse off */
        p->reverse = false;
        break;
    default:
        break;
    }
}

static bool is_control_byte(uint8_t ch)
{
    switch (ch) {
    case 8:
    case 10:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 26:
    case 27:
    case 145:
    case 146:
        return true;
    default:
        return false;
    }
}

void c64_printer_init(c64_printer *p)
{
    size_t bytes;

    if (p == NULL) {
        return;
    }
    memset(p, 0, sizeof(*p));
    p->device = 4;
    p->graphic_charset = true;
    p->sa = 0;
    bytes = (size_t)C64_PRINTER_WIDTH_DOTS * (size_t)C64_PRINTER_HEIGHT_DOTS;
    p->raster = (uint8_t *)malloc(bytes);
    p->raster_bytes = (p->raster != NULL) ? bytes : 0u;
    clear_page(p);
    reset_modes(p);
}

void c64_printer_reset(c64_printer *p)
{
    if (p == NULL) {
        return;
    }
    clear_page(p);
    reset_modes(p);
    p->flush_hold = false;
    p->sa = 0;
    p->graphic_charset = true;
    /* Keep enabled, output_dir, pages_flushed / page_cap_hit across soft reset. */
}

void c64_printer_shutdown(c64_printer *p)
{
    if (p == NULL) {
        return;
    }
    free(p->raster);
    p->raster = NULL;
    p->raster_bytes = 0;
    p->enabled = false;
    p->page_dirty = false;
    p->output_dir[0] = '\0';
}

void c64_printer_set_enabled(c64_printer *p, bool on)
{
    if (p == NULL) {
        return;
    }
    if (on == p->enabled) {
        return;
    }
    if (!on) {
        c64_printer_force_flush(p);
        p->enabled = false;
        return;
    }
    p->enabled = true;
    p->pages_flushed = 0;
    p->page_cap_hit = false;
    p->flush_hold = false;
    clear_page(p);
    reset_modes(p);
    p->sa = 0;
    p->graphic_charset = true;
}

bool c64_printer_enabled(const c64_printer *p)
{
    return p != NULL && p->enabled;
}

void c64_printer_set_output_dir(c64_printer *p, const char *dir)
{
    if (p == NULL) {
        return;
    }
    if (dir == NULL || dir[0] == '\0') {
        p->output_dir[0] = '\0';
        return;
    }
    strncpy(p->output_dir, dir, sizeof(p->output_dir) - 1u);
    p->output_dir[sizeof(p->output_dir) - 1u] = '\0';
}

void c64_printer_set_format_bmp(c64_printer *p)
{
    (void)p; /* v1 is BMP-only */
}

void c64_printer_set_sa(c64_printer *p, uint8_t sa)
{
    if (p == NULL) {
        return;
    }
    p->sa = sa;
    if (sa == 7u) {
        p->graphic_charset = false;
    } else {
        /* SA=0 and any other → graphic (best-effort) */
        p->graphic_charset = true;
    }
}

void c64_printer_putc(c64_printer *p, uint8_t ch)
{
    unsigned n;
    unsigned i;

    if (p == NULL || !p->enabled || p->flush_hold) {
        return;
    }

    switch (p->parse_state) {
    case C64_PRINTER_PARSE_HEAD_TAB_D1:
        p->parse_buf[0] = ch;
        p->parse_state = C64_PRINTER_PARSE_HEAD_TAB_D2;
        return;
    case C64_PRINTER_PARSE_HEAD_TAB_D2:
        apply_head_tab(p, p->parse_buf[0], ch);
        p->parse_state = C64_PRINTER_PARSE_IDLE;
        return;
    case C64_PRINTER_PARSE_REPEAT_N:
        p->parse_buf[0] = ch;
        p->parse_state = C64_PRINTER_PARSE_REPEAT_DATA;
        return;
    case C64_PRINTER_PARSE_REPEAT_DATA:
        n = p->parse_buf[0] == 0u ? 256u : (unsigned)p->parse_buf[0];
        for (i = 0; i < n; ++i) {
            plot_bim_column(p, ch);
        }
        p->parse_state = C64_PRINTER_PARSE_IDLE;
        return;
    case C64_PRINTER_PARSE_ESC:
        if (ch == 16u) {
            p->parse_state = C64_PRINTER_PARSE_ESC_DOT_NH;
        } else {
            p->parse_state = C64_PRINTER_PARSE_IDLE;
        }
        return;
    case C64_PRINTER_PARSE_ESC_DOT_NH:
        p->parse_buf[0] = ch;
        p->parse_state = C64_PRINTER_PARSE_ESC_DOT_NL;
        return;
    case C64_PRINTER_PARSE_ESC_DOT_NL:
        apply_dot_address(p, p->parse_buf[0], ch);
        p->parse_state = C64_PRINTER_PARSE_IDLE;
        return;
    case C64_PRINTER_PARSE_IDLE:
    default:
        break;
    }

    if (p->bit_image) {
        if (is_control_byte(ch)) {
            handle_control(p, ch);
            return;
        }
        plot_bim_column(p, ch);
        return;
    }

    if (is_control_byte(ch)) {
        handle_control(p, ch);
        return;
    }
    print_char(p, ch);
}

void c64_printer_force_flush(c64_printer *p)
{
    if (p == NULL || !p->enabled) {
        return;
    }
    (void)flush_page(p);
}

uint32_t c64_printer_pages_flushed(const c64_printer *p)
{
    return p != NULL ? p->pages_flushed : 0u;
}

bool c64_printer_page_dirty(const c64_printer *p)
{
    return p != NULL && p->page_dirty;
}
