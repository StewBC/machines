#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    C64_PRINTER_COLS = 80,
    C64_PRINTER_DOTS_PER_COL = 6,
    C64_PRINTER_CHAR_LF_DOTS = 10, /* character-mode line pitch */
    C64_PRINTER_BIM_LF_DOTS = 7,   /* bit-image line pitch */
    C64_PRINTER_WIDTH_DOTS = C64_PRINTER_COLS * C64_PRINTER_DOTS_PER_COL, /* 480 */
    C64_PRINTER_HEIGHT_DOTS = 700,
    C64_PRINTER_DOT_ADDR_MAX = 639,
    C64_PRINTER_PAGE_CAP = 500,
    C64_PRINTER_PATH_MAX = 1024
};

typedef enum c64_printer_parse {
    C64_PRINTER_PARSE_IDLE = 0,
    C64_PRINTER_PARSE_HEAD_TAB_D1,
    C64_PRINTER_PARSE_HEAD_TAB_D2,
    C64_PRINTER_PARSE_REPEAT_N,
    C64_PRINTER_PARSE_REPEAT_DATA,
    C64_PRINTER_PARSE_ESC,
    C64_PRINTER_PARSE_ESC_DOT_NH,
    C64_PRINTER_PARSE_ESC_DOT_NL
} c64_printer_parse;

typedef struct c64_printer {
    bool enabled;
    uint8_t device; /* 4 */
    uint8_t sa;     /* 0 graphic, 7 business */
    bool graphic_charset;
    bool enhance;
    bool reverse;
    bool bit_image;

    int cursor_x_dots;
    int cursor_y_dots;
    uint8_t *raster; /* 8bpp: 0=ink, 255=paper; w*h bytes */
    size_t raster_bytes;
    uint32_t pages_flushed;
    bool page_dirty;
    bool page_cap_hit;
    /* Set on flush I/O failure; putc refuses mutation until a flush succeeds. */
    bool flush_hold;

    c64_printer_parse parse_state;
    uint8_t parse_buf[4];

    char output_dir[C64_PRINTER_PATH_MAX];
} c64_printer;

void c64_printer_init(c64_printer *p);
void c64_printer_reset(c64_printer *p);
void c64_printer_shutdown(c64_printer *p);

void c64_printer_set_enabled(c64_printer *p, bool on);
bool c64_printer_enabled(const c64_printer *p);

void c64_printer_set_output_dir(c64_printer *p, const char *dir);
void c64_printer_set_format_bmp(c64_printer *p); /* v1 bmp only */

void c64_printer_set_sa(c64_printer *p, uint8_t sa); /* 0 graphic, 7 business */

void c64_printer_putc(c64_printer *p, uint8_t ch);
void c64_printer_force_flush(c64_printer *p); /* no-op if !dirty; suppress blank pages */

uint32_t c64_printer_pages_flushed(const c64_printer *p);
bool c64_printer_page_dirty(const c64_printer *p);

#ifdef __cplusplus
}
#endif
