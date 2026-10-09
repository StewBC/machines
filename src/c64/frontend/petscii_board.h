#pragma once

#include "keyboard.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fixed strip height. Opening the strip grows the window by this many
   pixels so the picture above it keeps its size. */
enum { PETSCII_STRIP_H = 236 };

enum { PETSCII_OP_MAX = 12 };

typedef enum petscii_op_kind {
    PETSCII_OP_KEY = 0,
    PETSCII_OP_RESTORE
} petscii_op_kind;

typedef struct petscii_key_op {
    petscii_op_kind kind;
    c64_key key;
    bool pressed;
} petscii_key_op;

/* Host keys that are already down. Sampled by the caller so a glyph click
   can force its own modifiers and put the host ones back afterwards. */
typedef struct petscii_mods {
    bool shift;
    bool control;
    bool commodore;
} petscii_mods;

typedef enum petscii_face_kind {
    PETSCII_FACE_GLYPH = 0,
    PETSCII_FACE_NAME,
    PETSCII_FACE_COLOR,
    PETSCII_FACE_SHIFT,
    PETSCII_FACE_CONTROL,
    PETSCII_FACE_COMMODORE,
    PETSCII_FACE_LOCK,
    PETSCII_FACE_RESTORE
} petscii_face_kind;

typedef struct petscii_face {
    petscii_face_kind kind;
    c64_key key;
    uint8_t screen;
    uint8_t color;
    const char *name;
    bool shift;
    bool commodore;
    bool control;
} petscii_face;

typedef struct petscii_rect {
    int x;
    int y;
    int w;
    int h;
} petscii_rect;

enum { PETSCII_BOARD_MAX_KEYS = 72 };
enum { PETSCII_BOARD_MAX_FACES = 4 };

typedef struct petscii_placed_key {
    petscii_rect rect;
    const char *label;
    int face_count;
    petscii_face faces[PETSCII_BOARD_MAX_FACES];
    petscii_rect face_rects[PETSCII_BOARD_MAX_FACES];
    bool lock_lit;
} petscii_placed_key;

typedef struct petscii_layout {
    int count;
    petscii_placed_key keys[PETSCII_BOARD_MAX_KEYS];
    int content_w;
    int content_h;
    int scroll_x;
    int scroll_max;
} petscii_layout;

typedef struct petscii_board {
    int scroll_x;
    bool shift_lock;
    bool holding;
    bool release_waiting;
    uint64_t down_frame;
    uint32_t down_ms;
    c64_key data_key;
    bool want_left;
    bool want_right;
    bool want_ctrl;
    bool want_cbm;
    bool chord_absolute;
    /* Host keys currently held. Lighting follows these, unless a face click
       is forcing its own chord. */
    bool host_down[C64_KEY_COUNT];
    bool host_restore;
} petscii_board;

void petscii_board_init(petscii_board *board);

/* view_w is the strip width in pixels. scroll_x is clamped and stored. */
void petscii_board_layout(petscii_board *board, int view_w, petscii_layout *out);

void petscii_board_scroll(petscii_board *board, int view_w, int delta_px);

bool petscii_board_hit(
    const petscii_layout *layout,
    int local_x,
    int local_y,
    int *out_key,
    int *out_face);

/* Pointer and pump emit matrix ops. A glyph click forces its modifiers.
   The chord stays down until the button is up and either the guest frame
   has advanced or 80 ms of host time has passed. */
size_t petscii_board_pointer_down(
    petscii_board *board,
    const petscii_face *face,
    uint64_t guest_frame,
    uint32_t now_ms,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap);

size_t petscii_board_pointer_up(
    petscii_board *board,
    uint64_t guest_frame,
    uint32_t now_ms,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap);

size_t petscii_board_pump(
    petscii_board *board,
    uint64_t guest_frame,
    uint32_t now_ms,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap);

size_t petscii_board_cancel(
    petscii_board *board,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap);

bool petscii_board_shift_locked(const petscii_board *board);
void petscii_board_set_host_keys(
    petscii_board *board,
    const bool down[C64_KEY_COUNT],
    bool restore);
bool petscii_board_face_held(const petscii_board *board, const petscii_face *face);

/* Screen code for a printable PETSCII byte, or -1 for a control. */
int petscii_board_screen_code(uint8_t petscii);
