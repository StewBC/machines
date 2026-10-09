#include "petscii_board.h"

#include <string.h>

enum {
    PETSCII_KEY_W = 56,
    PETSCII_KEY_H = 44,
    PETSCII_GAP = 2,
    PETSCII_PAD = 4,
    PETSCII_F_GAP = 8,
    PETSCII_HOLD_MS = 80,
    /* DOWN, LEFT, and RIGHT do not fit a one-unit face. Grow the two
       cursor keys into the gap before the F column. */
    PETSCII_CRSR_EXTRA = 24
};

/* KERNAL 901227-03 keyboard tables: unshifted $EB81, shifted $EBC2,
   Commodore $EC03, control $EC78. Index is matrix row * 8 + column,
   matching c64_key_positions in machine/keyboard.c. */
static const uint8_t k_unshifted[64] = {
    0x14, 0x0D, 0x1D, 0x88, 0x85, 0x86, 0x87, 0x11,
    0x33, 0x57, 0x41, 0x34, 0x5A, 0x53, 0x45, 0x01,
    0x35, 0x52, 0x44, 0x36, 0x43, 0x46, 0x54, 0x58,
    0x37, 0x59, 0x47, 0x38, 0x42, 0x48, 0x55, 0x56,
    0x39, 0x49, 0x4A, 0x30, 0x4D, 0x4B, 0x4F, 0x4E,
    0x2B, 0x50, 0x4C, 0x2D, 0x2E, 0x3A, 0x40, 0x2C,
    0x5C, 0x2A, 0x3B, 0x13, 0x01, 0x3D, 0x5E, 0x2F,
    0x31, 0x5F, 0x04, 0x32, 0x20, 0x02, 0x51, 0x03
};

static const uint8_t k_shifted[64] = {
    0x94, 0x8D, 0x9D, 0x8C, 0x89, 0x8A, 0x8B, 0x91,
    0x23, 0xD7, 0xC1, 0x24, 0xDA, 0xD3, 0xC5, 0x01,
    0x25, 0xD2, 0xC4, 0x26, 0xC3, 0xC6, 0xD4, 0xD8,
    0x27, 0xD9, 0xC7, 0x28, 0xC2, 0xC8, 0xD5, 0xD6,
    0x29, 0xC9, 0xCA, 0x30, 0xCD, 0xCB, 0xCF, 0xCE,
    0xDB, 0xD0, 0xCC, 0xDD, 0x3E, 0x5B, 0xBA, 0x3C,
    0xA9, 0xC0, 0x5D, 0x93, 0x01, 0x3D, 0xDE, 0x3F,
    0x21, 0x5F, 0x04, 0x22, 0xA0, 0x02, 0xD1, 0x83
};

static const uint8_t k_commodore[64] = {
    0x94, 0x8D, 0x9D, 0x8C, 0x89, 0x8A, 0x8B, 0x91,
    0x96, 0xB3, 0xB0, 0x97, 0xAD, 0xAE, 0xB1, 0x01,
    0x98, 0xB2, 0xAC, 0x99, 0xBC, 0xBB, 0xA3, 0xBD,
    0x9A, 0xB7, 0xA5, 0x9B, 0xBF, 0xB4, 0xB8, 0xBE,
    0x29, 0xA2, 0xB5, 0x30, 0xA7, 0xA1, 0xB9, 0xAA,
    0xA6, 0xAF, 0xB6, 0xDC, 0x3E, 0x5B, 0xA4, 0x3C,
    0xA8, 0xDF, 0x5D, 0x93, 0x01, 0x3D, 0xDE, 0x3F,
    0x81, 0x5F, 0x04, 0x95, 0xA0, 0x02, 0xAB, 0x83
};

static const uint8_t k_control[64] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x1C, 0x17, 0x01, 0x9F, 0x1A, 0x13, 0x05, 0xFF,
    0x9C, 0x12, 0x04, 0x1E, 0x03, 0x06, 0x14, 0x18,
    0x1F, 0x19, 0x07, 0x9E, 0x02, 0x08, 0x15, 0x16,
    0x12, 0x09, 0x0A, 0x92, 0x0D, 0x0B, 0x0F, 0x0E,
    0xFF, 0x10, 0x0C, 0xFF, 0xFF, 0x1B, 0x00, 0xFF,
    0x1C, 0xFF, 0x1D, 0xFF, 0xFF, 0x1F, 0x1E, 0xFF,
    0x90, 0x06, 0xFF, 0x05, 0xFF, 0xFF, 0x11, 0xFF
};

typedef struct c64_key_position {
    uint8_t row;
    uint8_t column;
} c64_key_position;

/* Same positions as machine/keyboard.c. A drift here mis-labels glyphs. */
static const c64_key_position k_positions[C64_KEY_COUNT] = {
    [C64_KEY_A] = {1, 2},
    [C64_KEY_B] = {3, 4},
    [C64_KEY_C] = {2, 4},
    [C64_KEY_D] = {2, 2},
    [C64_KEY_E] = {1, 6},
    [C64_KEY_F] = {2, 5},
    [C64_KEY_G] = {3, 2},
    [C64_KEY_H] = {3, 5},
    [C64_KEY_I] = {4, 1},
    [C64_KEY_J] = {4, 2},
    [C64_KEY_K] = {4, 5},
    [C64_KEY_L] = {5, 2},
    [C64_KEY_M] = {4, 4},
    [C64_KEY_N] = {4, 7},
    [C64_KEY_O] = {4, 6},
    [C64_KEY_P] = {5, 1},
    [C64_KEY_Q] = {7, 6},
    [C64_KEY_R] = {2, 1},
    [C64_KEY_S] = {1, 5},
    [C64_KEY_T] = {2, 6},
    [C64_KEY_U] = {3, 6},
    [C64_KEY_V] = {3, 7},
    [C64_KEY_W] = {1, 1},
    [C64_KEY_X] = {2, 7},
    [C64_KEY_Y] = {3, 1},
    [C64_KEY_Z] = {1, 4},
    [C64_KEY_0] = {4, 3},
    [C64_KEY_1] = {7, 0},
    [C64_KEY_2] = {7, 3},
    [C64_KEY_3] = {1, 0},
    [C64_KEY_4] = {1, 3},
    [C64_KEY_5] = {2, 0},
    [C64_KEY_6] = {2, 3},
    [C64_KEY_7] = {3, 0},
    [C64_KEY_8] = {3, 3},
    [C64_KEY_9] = {4, 0},
    [C64_KEY_SPACE] = {7, 4},
    [C64_KEY_RETURN] = {0, 1},
    [C64_KEY_DELETE] = {0, 0},
    [C64_KEY_LEFT_SHIFT] = {1, 7},
    [C64_KEY_RIGHT_SHIFT] = {6, 4},
    [C64_KEY_PLUS] = {5, 0},
    [C64_KEY_MINUS] = {5, 3},
    [C64_KEY_ASTERISK] = {6, 1},
    [C64_KEY_EQUALS] = {6, 5},
    [C64_KEY_COLON] = {5, 5},
    [C64_KEY_SEMICOLON] = {6, 2},
    [C64_KEY_COMMA] = {5, 7},
    [C64_KEY_PERIOD] = {5, 4},
    [C64_KEY_SLASH] = {6, 7},
    [C64_KEY_AT] = {5, 6},
    [C64_KEY_CURSOR_RIGHT] = {0, 2},
    [C64_KEY_CURSOR_DOWN] = {0, 7},
    [C64_KEY_HOME] = {6, 3},
    [C64_KEY_RUN_STOP] = {7, 7},
    [C64_KEY_CONTROL] = {7, 2},
    [C64_KEY_COMMODORE] = {7, 5},
    [C64_KEY_LEFT_ARROW] = {7, 1},
    [C64_KEY_UP_ARROW] = {6, 6},
    [C64_KEY_POUND] = {6, 0},
    [C64_KEY_F1] = {0, 4},
    [C64_KEY_F3] = {0, 5},
    [C64_KEY_F5] = {0, 6},
    [C64_KEY_F7] = {0, 3}
};

enum {
    PETSCII_ID_LOCK = -1,
    PETSCII_ID_RESTORE = -2
};

typedef struct petscii_row_key {
    int id;
    const char *label;
    int units;
} petscii_row_key;

static const petscii_row_key k_row0[] = {
    { C64_KEY_LEFT_ARROW, "LARR", 1 },
    { C64_KEY_1, "1", 1 },
    { C64_KEY_2, "2", 1 },
    { C64_KEY_3, "3", 1 },
    { C64_KEY_4, "4", 1 },
    { C64_KEY_5, "5", 1 },
    { C64_KEY_6, "6", 1 },
    { C64_KEY_7, "7", 1 },
    { C64_KEY_8, "8", 1 },
    { C64_KEY_9, "9", 1 },
    { C64_KEY_0, "0", 1 },
    { C64_KEY_PLUS, "+", 1 },
    { C64_KEY_MINUS, "-", 1 },
    { C64_KEY_POUND, "POUND", 1 },
    { C64_KEY_HOME, "HOME", 1 },
    { C64_KEY_DELETE, "DEL", 1 }
};

static const petscii_row_key k_row1[] = {
    { C64_KEY_CONTROL, "CTRL", 1 },
    { C64_KEY_Q, "Q", 1 },
    { C64_KEY_W, "W", 1 },
    { C64_KEY_E, "E", 1 },
    { C64_KEY_R, "R", 1 },
    { C64_KEY_T, "T", 1 },
    { C64_KEY_Y, "Y", 1 },
    { C64_KEY_U, "U", 1 },
    { C64_KEY_I, "I", 1 },
    { C64_KEY_O, "O", 1 },
    { C64_KEY_P, "P", 1 },
    { C64_KEY_AT, "@", 1 },
    { C64_KEY_ASTERISK, "*", 1 },
    { C64_KEY_UP_ARROW, "UARR", 1 },
    { PETSCII_ID_RESTORE, "RSTR", 1 }
};

static const petscii_row_key k_row2[] = {
    { C64_KEY_RUN_STOP, "STOP", 1 },
    { PETSCII_ID_LOCK, "LOCK", 1 },
    { C64_KEY_A, "A", 1 },
    { C64_KEY_S, "S", 1 },
    { C64_KEY_D, "D", 1 },
    { C64_KEY_F, "F", 1 },
    { C64_KEY_G, "G", 1 },
    { C64_KEY_H, "H", 1 },
    { C64_KEY_J, "J", 1 },
    { C64_KEY_K, "K", 1 },
    { C64_KEY_L, "L", 1 },
    { C64_KEY_COLON, ":", 1 },
    { C64_KEY_SEMICOLON, ";", 1 },
    { C64_KEY_EQUALS, "=", 1 },
    { C64_KEY_RETURN, "RET", 1 }
};

static const petscii_row_key k_row3[] = {
    { C64_KEY_COMMODORE, "C=", 1 },
    { C64_KEY_LEFT_SHIFT, "SHIFT", 1 },
    { C64_KEY_Z, "Z", 1 },
    { C64_KEY_X, "X", 1 },
    { C64_KEY_C, "C", 1 },
    { C64_KEY_V, "V", 1 },
    { C64_KEY_B, "B", 1 },
    { C64_KEY_N, "N", 1 },
    { C64_KEY_M, "M", 1 },
    { C64_KEY_COMMA, ",", 1 },
    { C64_KEY_PERIOD, ".", 1 },
    { C64_KEY_SLASH, "/", 1 },
    { C64_KEY_RIGHT_SHIFT, "SHIFT", 1 },
    { C64_KEY_CURSOR_DOWN, "CRSR", 1 },
    { C64_KEY_CURSOR_RIGHT, "CRSR", 1 }
};

static const petscii_row_key k_row4[] = {
    { C64_KEY_SPACE, "SPACE", 8 }
};

static const petscii_row_key k_fkeys[] = {
    { C64_KEY_F1, "F1", 1 },
    { C64_KEY_F3, "F3", 1 },
    { C64_KEY_F5, "F5", 1 },
    { C64_KEY_F7, "F7", 1 }
};

typedef struct petscii_row {
    const petscii_row_key *keys;
    int count;
    int indent;
} petscii_row;

static const petscii_row k_rows[] = {
    { k_row0, (int)(sizeof(k_row0) / sizeof(k_row0[0])), 0 },
    { k_row1, (int)(sizeof(k_row1) / sizeof(k_row1[0])), 12 },
    { k_row2, (int)(sizeof(k_row2) / sizeof(k_row2[0])), 18 },
    { k_row3, (int)(sizeof(k_row3) / sizeof(k_row3[0])), 8 },
    { k_row4, (int)(sizeof(k_row4) / sizeof(k_row4[0])), 0 }
};

int petscii_board_screen_code(uint8_t petscii)
{
    if (petscii == 0xFFu) {
        return 0x5E;
    }
    if (petscii < 0x20u) {
        return -1;
    }
    if (petscii < 0x40u) {
        return (int)petscii;
    }
    if (petscii < 0x60u) {
        return (int)petscii - 0x40;
    }
    if (petscii < 0x80u) {
        return (int)petscii - 0x20;
    }
    if (petscii < 0xA0u) {
        return -1;
    }
    if (petscii < 0xC0u) {
        return (int)petscii - 0x40;
    }
    if (petscii < 0xFFu) {
        return (int)petscii - 0x80;
    }
    return -1;
}

static int color_of(uint8_t petscii)
{
    switch (petscii) {
        case 0x90: return 0;
        case 0x05: return 1;
        case 0x1C: return 2;
        case 0x9F: return 3;
        case 0x9C: return 4;
        case 0x1E: return 5;
        case 0x1F: return 6;
        case 0x9E: return 7;
        case 0x81: return 8;
        case 0x95: return 9;
        case 0x96: return 10;
        case 0x97: return 11;
        case 0x98: return 12;
        case 0x99: return 13;
        case 0x9A: return 14;
        case 0x9B: return 15;
        default: return -1;
    }
}

static const char *name_of(uint8_t petscii)
{
    switch (petscii) {
        case 0x03: return "STOP";
        case 0x0D: return "RET";
        case 0x8D: return "SRET";
        case 0x11: return "DOWN";
        case 0x91: return "UP";
        case 0x1D: return "RIGHT";
        case 0x9D: return "LEFT";
        case 0x13: return "HOME";
        case 0x93: return "CLR";
        case 0x14: return "DEL";
        case 0x94: return "INST";
        case 0x85: return "F1";
        case 0x89: return "F2";
        case 0x86: return "F3";
        case 0x8A: return "F4";
        case 0x87: return "F5";
        case 0x8B: return "F6";
        case 0x88: return "F7";
        case 0x8C: return "F8";
        case 0x12: return "RVS";
        case 0x92: return "OFF";
        default: return NULL;
    }
}

static bool is_modifier_flag(uint8_t petscii)
{
    return petscii == 0x00u || petscii == 0x01u || petscii == 0x02u ||
        petscii == 0x04u || petscii == 0xFFu;
}

static bool shift_follows_host(const petscii_face *face)
{
    return face != NULL &&
        (face->key == C64_KEY_SPACE || face->key == C64_KEY_RETURN) &&
        !face->shift && !face->control && !face->commodore;
}

static bool is_digit_key(c64_key key)
{
    return key >= C64_KEY_0 && key <= C64_KEY_9;
}

static int matrix_index(c64_key key)
{
    c64_key_position pos;

    if (key < 0 || key >= C64_KEY_COUNT) {
        return -1;
    }
    pos = k_positions[key];
    return (int)pos.row * 8 + (int)pos.column;
}

static bool face_same(const petscii_face *face, petscii_face_kind kind, uint8_t screen, uint8_t color, const char *name)
{
    if (face->kind != kind) {
        return false;
    }
    if (kind == PETSCII_FACE_GLYPH) {
        return face->screen == screen;
    }
    if (kind == PETSCII_FACE_COLOR) {
        return face->color == color;
    }
    if (kind == PETSCII_FACE_NAME) {
        return face->name != NULL && name != NULL && strcmp(face->name, name) == 0;
    }
    return false;
}

static void add_face(
    petscii_face *faces,
    int *count,
    const petscii_face *face)
{
    int i;

    if (*count >= PETSCII_BOARD_MAX_FACES) {
        return;
    }
    for (i = 0; i < *count; i++) {
        if (face_same(&faces[i], face->kind, face->screen, face->color, face->name)) {
            return;
        }
    }
    faces[*count] = *face;
    *count += 1;
}

static void add_petscii_face(
    petscii_face *faces,
    int *count,
    c64_key key,
    uint8_t petscii,
    bool shift,
    bool commodore,
    bool control,
    bool allow_color)
{
    petscii_face face;
    int screen;
    int color;
    const char *name;

    if (is_modifier_flag(petscii)) {
        return;
    }
    memset(&face, 0, sizeof(face));
    face.key = key;
    face.shift = shift;
    face.commodore = commodore;
    face.control = control;
    color = color_of(petscii);
    name = name_of(petscii);
    screen = petscii_board_screen_code(petscii);
    if (allow_color && color >= 0) {
        face.kind = PETSCII_FACE_COLOR;
        face.color = (uint8_t)color;
        add_face(faces, count, &face);
        return;
    }
    if (name != NULL) {
        face.kind = PETSCII_FACE_NAME;
        face.name = name;
        add_face(faces, count, &face);
        return;
    }
    if (screen >= 0) {
        face.kind = PETSCII_FACE_GLYPH;
        face.screen = (uint8_t)screen;
        add_face(faces, count, &face);
    }
}

static void build_faces(int id, petscii_face *faces, int *count)
{
    petscii_face face;
    c64_key key;
    int index;

    *count = 0;
    memset(&face, 0, sizeof(face));
    if (id == PETSCII_ID_LOCK) {
        face.kind = PETSCII_FACE_LOCK;
        face.key = C64_KEY_LEFT_SHIFT;
        face.name = "LOCK";
        add_face(faces, count, &face);
        return;
    }
    if (id == PETSCII_ID_RESTORE) {
        face.kind = PETSCII_FACE_RESTORE;
        face.key = C64_KEY_COUNT;
        face.name = "RSTR";
        add_face(faces, count, &face);
        return;
    }
    key = (c64_key)id;
    if (key == C64_KEY_LEFT_SHIFT || key == C64_KEY_RIGHT_SHIFT) {
        face.kind = PETSCII_FACE_SHIFT;
        face.key = key;
        face.name = "SHIFT";
        add_face(faces, count, &face);
        return;
    }
    if (key == C64_KEY_CONTROL) {
        face.kind = PETSCII_FACE_CONTROL;
        face.key = key;
        face.name = "CTRL";
        add_face(faces, count, &face);
        return;
    }
    if (key == C64_KEY_COMMODORE) {
        face.kind = PETSCII_FACE_COMMODORE;
        face.key = key;
        face.name = "C=";
        add_face(faces, count, &face);
        return;
    }
    index = matrix_index(key);
    if (index < 0 || index >= 64) {
        return;
    }
    /* One face. Host Shift or Shift Lock selects the shifted character. */
    if (key == C64_KEY_SPACE || key == C64_KEY_RETURN) {
        add_petscii_face(faces, count, key, k_unshifted[index], false, false, false, false);
        return;
    }
    /* LEFT sits to the left of RIGHT on the cursor key. */
    if (key == C64_KEY_CURSOR_RIGHT) {
        add_petscii_face(faces, count, key, k_shifted[index], true, false, false, false);
        add_petscii_face(faces, count, key, k_unshifted[index], false, false, false, false);
        return;
    }
    add_petscii_face(faces, count, key, k_unshifted[index], false, false, false, false);
    add_petscii_face(faces, count, key, k_shifted[index], true, false, false, false);
    add_petscii_face(
        faces, count, key, k_commodore[index], false, true, false, is_digit_key(key));
    if (is_digit_key(key)) {
        add_petscii_face(faces, count, key, k_control[index], false, false, true, true);
    }
}

static int key_width(int units)
{
    if (units < 1) {
        units = 1;
    }
    return units * PETSCII_KEY_W + (units - 1) * PETSCII_GAP;
}

static int key_span(const petscii_row_key *key)
{
    int w = key_width(key->units);

    if (key->id == C64_KEY_CURSOR_DOWN || key->id == C64_KEY_CURSOR_RIGHT) {
        w += PETSCII_CRSR_EXTRA;
    }
    return w;
}

static int row_width(const petscii_row_key *keys, int count)
{
    int i;
    int width = 0;

    for (i = 0; i < count; i++) {
        if (i > 0) {
            width += PETSCII_GAP;
        }
        width += key_span(&keys[i]);
    }
    return width;
}

static int content_width(void)
{
    int main_w = row_width(k_row0, (int)(sizeof(k_row0) / sizeof(k_row0[0])));
    return PETSCII_PAD + main_w + PETSCII_F_GAP + PETSCII_KEY_W + PETSCII_PAD;
}

static void place_faces(int x, int y, int w, int h, int count, petscii_rect *out)
{
    int cols;
    int rows;
    int gap = 2;
    int fw;
    int fh;
    int i;

    if (count <= 0) {
        return;
    }
    cols = count > 3 ? 2 : count;
    rows = count > 3 ? 2 : 1;
    if (cols < 1) {
        cols = 1;
    }
    fw = (w - gap * (cols - 1)) / cols;
    fh = (h - gap * (rows - 1)) / rows;
    if (fw < 1) {
        fw = 1;
    }
    if (fh < 1) {
        fh = 1;
    }
    for (i = 0; i < count; i++) {
        int col = i % cols;
        int row = i / cols;
        out[i].x = x + col * (fw + gap);
        out[i].y = y + row * (fh + gap);
        out[i].w = fw;
        out[i].h = fh;
    }
}

static void place_one(
    petscii_layout *out,
    const petscii_board *board,
    int id,
    const char *label,
    int x,
    int y,
    int w,
    int h,
    int scroll_x)
{
    petscii_placed_key *placed;
    int face_top;
    int face_h;

    if (out->count >= PETSCII_BOARD_MAX_KEYS) {
        return;
    }
    placed = &out->keys[out->count];
    memset(placed, 0, sizeof(*placed));
    placed->rect.x = x - scroll_x;
    placed->rect.y = y;
    placed->rect.w = w;
    placed->rect.h = h;
    placed->label = label;
    build_faces(id, placed->faces, &placed->face_count);
    placed->lock_lit = id == PETSCII_ID_LOCK && board->shift_lock;
    face_top = y + 14;
    face_h = h - 18;
    if (face_h < 8) {
        face_h = 8;
    }
    place_faces(
        placed->rect.x + 2,
        face_top,
        w - 4,
        face_h,
        placed->face_count,
        placed->face_rects);
    out->count += 1;
}

static int place_row(
    petscii_layout *out,
    const petscii_board *board,
    const petscii_row_key *keys,
    int count,
    int x0,
    int y,
    int scroll_x)
{
    int i;
    int x = x0;

    for (i = 0; i < count; i++) {
        int w = key_span(&keys[i]);
        place_one(out, board, keys[i].id, keys[i].label, x, y, w, PETSCII_KEY_H, scroll_x);
        x += w + PETSCII_GAP;
    }
    return x;
}

void petscii_board_init(petscii_board *board)
{
    if (board == NULL) {
        return;
    }
    memset(board, 0, sizeof(*board));
    board->data_key = C64_KEY_COUNT;
}

static int clamp_scroll(int scroll_x, int view_w)
{
    int max_scroll;
    int content_w = content_width();

    if (view_w >= content_w) {
        return 0;
    }
    max_scroll = content_w - view_w;
    if (scroll_x < 0) {
        return 0;
    }
    if (scroll_x > max_scroll) {
        return max_scroll;
    }
    return scroll_x;
}

void petscii_board_layout(petscii_board *board, int view_w, petscii_layout *out)
{
    int scroll_x;
    int y;
    int i;
    int main_w;
    int space_w;
    int space_x;
    int origin;

    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    if (board == NULL) {
        return;
    }
    if (view_w < 1) {
        return;
    }
    scroll_x = clamp_scroll(board->scroll_x, view_w);
    board->scroll_x = scroll_x;
    out->content_w = content_width();
    out->content_h = PETSCII_STRIP_H;
    out->scroll_x = scroll_x;
    out->scroll_max = out->content_w > view_w ? out->content_w - view_w : 0;
    y = PETSCII_PAD;
    origin = 0;
    if (view_w > out->content_w) {
        origin = (view_w - out->content_w) / 2;
    }
    for (i = 0; i < 4; i++) {
        place_row(
            out,
            board,
            k_rows[i].keys,
            k_rows[i].count,
            origin + PETSCII_PAD + k_rows[i].indent,
            y,
            scroll_x);
        y += PETSCII_KEY_H + PETSCII_GAP;
    }
    main_w = row_width(k_row0, (int)(sizeof(k_row0) / sizeof(k_row0[0])));
    space_w = key_width(k_row4[0].units);
    space_x = origin + PETSCII_PAD + (main_w - space_w) / 2;
    place_row(out, board, k_row4, 1, space_x, y, scroll_x);

    for (i = 0; i < (int)(sizeof(k_fkeys) / sizeof(k_fkeys[0])); i++) {
        int fx = origin + PETSCII_PAD + main_w + PETSCII_F_GAP;
        int fy = PETSCII_PAD + i * (PETSCII_KEY_H + PETSCII_GAP);
        place_one(
            out,
            board,
            k_fkeys[i].id,
            k_fkeys[i].label,
            fx,
            fy,
            PETSCII_KEY_W,
            PETSCII_KEY_H,
            scroll_x);
    }
}

void petscii_board_scroll(petscii_board *board, int view_w, int delta_px)
{
    if (board == NULL) {
        return;
    }
    board->scroll_x = clamp_scroll(board->scroll_x + delta_px, view_w);
}

bool petscii_board_hit(
    const petscii_layout *layout,
    int local_x,
    int local_y,
    int *out_key,
    int *out_face)
{
    int i;
    int f;

    if (layout == NULL) {
        return false;
    }
    for (i = 0; i < layout->count; i++) {
        const petscii_placed_key *key = &layout->keys[i];
        for (f = 0; f < key->face_count; f++) {
            const petscii_rect *rect = &key->face_rects[f];
            if (local_x >= rect->x && local_x < rect->x + rect->w &&
                local_y >= rect->y && local_y < rect->y + rect->h) {
                if (out_key != NULL) {
                    *out_key = i;
                }
                if (out_face != NULL) {
                    *out_face = f;
                }
                return true;
            }
        }
    }
    return false;
}

static const petscii_mods *mods_or_empty(const petscii_mods *host, petscii_mods *storage)
{
    if (host != NULL) {
        return host;
    }
    memset(storage, 0, sizeof(*storage));
    return storage;
}

static size_t push_key(petscii_key_op *ops, size_t count, size_t cap, c64_key key, bool pressed)
{
    if (ops == NULL || count >= cap || key < 0 || key >= C64_KEY_COUNT) {
        return count;
    }
    ops[count].kind = PETSCII_OP_KEY;
    ops[count].key = key;
    ops[count].pressed = pressed;
    return count + 1u;
}

static size_t push_restore(petscii_key_op *ops, size_t count, size_t cap)
{
    if (ops == NULL || count >= cap) {
        return count;
    }
    ops[count].kind = PETSCII_OP_RESTORE;
    ops[count].key = C64_KEY_COUNT;
    ops[count].pressed = true;
    return count + 1u;
}

static size_t emit_absolute_mods(
    petscii_key_op *ops,
    size_t count,
    size_t cap,
    bool left,
    bool right,
    bool ctrl,
    bool cbm)
{
    count = push_key(ops, count, cap, C64_KEY_LEFT_SHIFT, left);
    count = push_key(ops, count, cap, C64_KEY_RIGHT_SHIFT, right);
    count = push_key(ops, count, cap, C64_KEY_CONTROL, ctrl);
    count = push_key(ops, count, cap, C64_KEY_COMMODORE, cbm);
    return count;
}

static size_t release_data(petscii_board *board, petscii_key_op *ops, size_t count, size_t cap)
{
    if (board->data_key < C64_KEY_COUNT) {
        count = push_key(ops, count, cap, board->data_key, false);
        board->data_key = C64_KEY_COUNT;
    }
    return count;
}

static size_t restore_host(
    petscii_board *board,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t count,
    size_t cap)
{
    count = release_data(board, ops, count, cap);
    count = emit_absolute_mods(
        ops,
        count,
        cap,
        board->shift_lock,
        host->shift,
        host->control,
        host->commodore);
    board->holding = false;
    board->release_waiting = false;
    board->want_left = false;
    board->want_right = false;
    board->want_ctrl = false;
    board->want_cbm = false;
    board->chord_absolute = false;
    return count;
}

static bool hold_elapsed(const petscii_board *board, uint64_t guest_frame, uint32_t now_ms)
{
    if (guest_frame > board->down_frame) {
        return true;
    }
    return (uint32_t)(now_ms - board->down_ms) >= (uint32_t)PETSCII_HOLD_MS;
}

static size_t finish_hold(
    petscii_board *board,
    uint64_t guest_frame,
    uint32_t now_ms,
    const petscii_mods *host,
    bool force,
    petscii_key_op *ops,
    size_t cap)
{
    if (!board->holding) {
        return 0;
    }
    if (!force && !hold_elapsed(board, guest_frame, now_ms)) {
        board->release_waiting = true;
        return 0;
    }
    return restore_host(board, host, ops, 0, cap);
}

size_t petscii_board_pointer_down(
    petscii_board *board,
    const petscii_face *face,
    uint64_t guest_frame,
    uint32_t now_ms,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap)
{
    petscii_mods empty;
    size_t count = 0;
    bool left = false;
    bool right = false;
    bool ctrl = false;
    bool cbm = false;

    if (board == NULL || face == NULL) {
        return 0;
    }
    host = mods_or_empty(host, &empty);
    if (board->holding) {
        count = restore_host(board, host, ops, 0, cap);
    }

    if (face->kind == PETSCII_FACE_LOCK) {
        board->shift_lock = !board->shift_lock;
        count = push_key(ops, count, cap, C64_KEY_LEFT_SHIFT, board->shift_lock);
        return count;
    }
    if (face->kind == PETSCII_FACE_RESTORE) {
        return push_restore(ops, count, cap);
    }

    if (face->kind == PETSCII_FACE_SHIFT) {
        if (face->key == C64_KEY_RIGHT_SHIFT) {
            right = true;
        } else {
            left = true;
        }
    } else if (face->kind == PETSCII_FACE_CONTROL) {
        ctrl = true;
    } else if (face->kind == PETSCII_FACE_COMMODORE) {
        cbm = true;
    } else {
        left = face->shift;
        right = false;
        ctrl = face->control;
        cbm = face->commodore;
        board->chord_absolute = true;
        if (shift_follows_host(face) && (host->shift || board->shift_lock)) {
            left = true;
        }
    }

    if (board->chord_absolute) {
        count = emit_absolute_mods(ops, count, cap, left, right, ctrl, cbm);
    } else {
        if (left) {
            count = push_key(ops, count, cap, C64_KEY_LEFT_SHIFT, true);
        }
        if (right) {
            count = push_key(ops, count, cap, C64_KEY_RIGHT_SHIFT, true);
        }
        if (ctrl) {
            count = push_key(ops, count, cap, C64_KEY_CONTROL, true);
        }
        if (cbm) {
            count = push_key(ops, count, cap, C64_KEY_COMMODORE, true);
        }
    }

    board->want_left = left;
    board->want_right = right;
    board->want_ctrl = ctrl;
    board->want_cbm = cbm;
    board->data_key = C64_KEY_COUNT;
    if (face->kind != PETSCII_FACE_SHIFT &&
        face->kind != PETSCII_FACE_CONTROL &&
        face->kind != PETSCII_FACE_COMMODORE &&
        face->key < C64_KEY_COUNT) {
        board->data_key = face->key;
        count = push_key(ops, count, cap, face->key, true);
    }
    board->holding = true;
    board->release_waiting = false;
    board->down_frame = guest_frame;
    board->down_ms = now_ms;
    return count;
}

size_t petscii_board_pointer_up(
    petscii_board *board,
    uint64_t guest_frame,
    uint32_t now_ms,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap)
{
    petscii_mods empty;

    if (board == NULL || !board->holding) {
        return 0;
    }
    host = mods_or_empty(host, &empty);
    return finish_hold(board, guest_frame, now_ms, host, false, ops, cap);
}

size_t petscii_board_pump(
    petscii_board *board,
    uint64_t guest_frame,
    uint32_t now_ms,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap)
{
    petscii_mods empty;

    if (board == NULL || !board->holding || !board->release_waiting) {
        return 0;
    }
    host = mods_or_empty(host, &empty);
    if (!hold_elapsed(board, guest_frame, now_ms)) {
        return 0;
    }
    return restore_host(board, host, ops, 0, cap);
}

size_t petscii_board_cancel(
    petscii_board *board,
    const petscii_mods *host,
    petscii_key_op *ops,
    size_t cap)
{
    petscii_mods empty;

    if (board == NULL || !board->holding) {
        return 0;
    }
    host = mods_or_empty(host, &empty);
    return restore_host(board, host, ops, 0, cap);
}

bool petscii_board_shift_locked(const petscii_board *board)
{
    return board != NULL && board->shift_lock;
}

void petscii_board_set_host_keys(
    petscii_board *board,
    const bool down[C64_KEY_COUNT],
    bool restore)
{
    if (board == NULL) {
        return;
    }
    if (down == NULL) {
        memset(board->host_down, 0, sizeof(board->host_down));
    } else {
        memcpy(board->host_down, down, sizeof(board->host_down));
    }
    board->host_restore = restore;
}

static bool host_key_down(const petscii_board *board, c64_key key)
{
    return key >= 0 && key < C64_KEY_COUNT && board->host_down[key];
}

static void effective_mods(
    const petscii_board *board,
    bool *left,
    bool *right,
    bool *ctrl,
    bool *cbm,
    bool *shift)
{
    if (board->holding && board->chord_absolute) {
        *left = board->want_left;
        *right = board->want_right;
        *ctrl = board->want_ctrl;
        *cbm = board->want_cbm;
    } else {
        *left = board->shift_lock || host_key_down(board, C64_KEY_LEFT_SHIFT) ||
            (board->holding && board->want_left);
        *right = host_key_down(board, C64_KEY_RIGHT_SHIFT) ||
            (board->holding && board->want_right);
        *ctrl = host_key_down(board, C64_KEY_CONTROL) ||
            (board->holding && board->want_ctrl);
        *cbm = host_key_down(board, C64_KEY_COMMODORE) ||
            (board->holding && board->want_cbm);
    }
    *shift = *left || *right;
}

static bool data_key_down(const petscii_board *board, c64_key key)
{
    if (board->holding && board->data_key == key) {
        return true;
    }
    return host_key_down(board, key);
}

bool petscii_board_face_held(const petscii_board *board, const petscii_face *face)
{
    bool left = false;
    bool right = false;
    bool ctrl = false;
    bool cbm = false;
    bool shift = false;

    if (board == NULL || face == NULL) {
        return false;
    }
    if (face->kind == PETSCII_FACE_LOCK) {
        return board->shift_lock;
    }
    if (face->kind == PETSCII_FACE_RESTORE) {
        return board->host_restore;
    }
    effective_mods(board, &left, &right, &ctrl, &cbm, &shift);
    if (face->kind == PETSCII_FACE_SHIFT) {
        if (face->key == C64_KEY_RIGHT_SHIFT) {
            return right;
        }
        return left;
    }
    if (face->kind == PETSCII_FACE_CONTROL) {
        return ctrl;
    }
    if (face->kind == PETSCII_FACE_COMMODORE) {
        return cbm;
    }
    if (!data_key_down(board, face->key)) {
        return false;
    }
    /* Space and Return have one face, so it lights with or without Shift. */
    if (shift_follows_host(face)) {
        return true;
    }
    return face->shift == shift &&
        face->commodore == cbm &&
        face->control == ctrl;
}
