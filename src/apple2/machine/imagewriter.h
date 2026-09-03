#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    A2_IW_WIDTH_DOTS = 1280,  /* 160 dpi × 8" */
    A2_IW_HEIGHT_DOTS = 792,  /* 11" × 72 dpi vertical */
    A2_IW_REF_DPI = 160,
    A2_IW_PAGE_CAP = 500,
    A2_IW_PATH_MAX = 1024,
    A2_IW_INK = 0,
    A2_IW_PAPER = 255
};

typedef enum imagewriter_parse {
    A2_IW_PARSE_IDLE = 0,
    A2_IW_PARSE_ESC,
    A2_IW_PARSE_ESC_T_D1,
    A2_IW_PARSE_ESC_T_D2,
    A2_IW_PARSE_ESC_G_DIGITS, /* ESC G / ESC S: 4 digits */
    A2_IW_PARSE_BIM_DATA, /* next bim_remaining bytes are pin masks (manuals) */
    A2_IW_PARSE_ESC_V_DIGITS,
    A2_IW_PARSE_ESC_V_DATA,
    A2_IW_PARSE_ESC_F_DIGITS,
    A2_IW_PARSE_ESC_g_DIGITS, /* ESC g: 3 digits → nnn×8 bytes */
    A2_IW_PARSE_ESC_K_ARG     /* colour select: ignore one byte */
} imagewriter_parse;

typedef struct imagewriter {
    int dpi;       /* BIM / text density d */
    int lf_dots;   /* buffer dots per LF (from ESC T / A / B) */
    bool auto_lf_after_cr;

    int head_col; /* density-native column (BIM units) */
    int cursor_y_dots;

    uint8_t *raster; /* 8bpp: 0=ink, 255=paper */
    size_t raster_bytes;
    uint32_t pages_flushed;
    bool page_dirty;
    bool page_cap_hit;
    bool flush_hold;
    bool bim_clip_logged;
    /* Set after any BIM column this page; cleared on flush. Print Shop
       greeting cards separate faces with ESC T24 (no FF) — soft page break. */
    bool saw_bim;
    /* Latched after the first ESC this session; gates the Ctrl-I preamble. */
    bool seen_esc;
    /* Inside the leading Ctrl-I <cmd> SSC escape: swallow one argument byte. */
    bool in_preamble_cmd;

    char last_name_stem[16];
    uint8_t name_seq;

    imagewriter_parse parse_state;
    int parse_digits_needed;
    int parse_digits_got;
    int parse_value;
    int bim_remaining;
    int bim_declared; /* ESC G/S/g declared column count (diagnostics) */

    char output_dir[A2_IW_PATH_MAX];
} imagewriter;

/* Absolute placers (integer round-half-up). Clip is caller's job. */
int imagewriter_bim_x(int dpi, int col);
int imagewriter_text_x(int dpi, int char_i);

/* Pitch table: dpi and max nnnn for ESC pitch letter; 0 dpi if unknown. */
int imagewriter_pitch_dpi(uint8_t cmd);
int imagewriter_pitch_max_nnnn(int dpi);

void imagewriter_init(imagewriter *iw);
void imagewriter_reset(imagewriter *iw); /* discard page, no host write */
void imagewriter_shutdown(imagewriter *iw);

void imagewriter_set_output_dir(imagewriter *iw, const char *dir);

void imagewriter_putc(imagewriter *iw, uint8_t ch);
void imagewriter_force_flush(imagewriter *iw); /* write if dirty; suppress blank */

uint32_t imagewriter_pages_flushed(const imagewriter *iw);
bool imagewriter_page_dirty(const imagewriter *iw);

#ifdef __cplusplus
}
#endif
