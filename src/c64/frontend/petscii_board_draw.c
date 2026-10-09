#include "petscii_board_draw.h"

#include "c64_frame.h"

#include <string.h>

/* 5x7 labels. Bit 4 is the leftmost pixel. Rows are top to bottom.
   Index is ASCII 32..90. */
static const uint8_t k_font[59][7] = {
    /* 32 space */ {0, 0, 0, 0, 0, 0, 0},
    /* 33 ! */ {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},
    /* 34 " */ {0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00},
    /* 35 # */ {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},
    /* 36 $ */ {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},
    /* 37 % */ {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
    /* 38 & */ {0x08, 0x14, 0x14, 0x08, 0x15, 0x12, 0x0D},
    /* 39 ' */ {0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00},
    /* 40 ( */ {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    /* 41 ) */ {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    /* 42 * */ {0x00, 0x04, 0x15, 0x0E, 0x15, 0x04, 0x00},
    /* 43 + */ {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    /* 44 , */ {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x08},
    /* 45 - */ {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    /* 46 . */ {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04},
    /* 47 / */ {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00},
    /* 48 0 */ {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    /* 49 1 */ {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    /* 50 2 */ {0x0E, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1F},
    /* 51 3 */ {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    /* 52 4 */ {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    /* 53 5 */ {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    /* 54 6 */ {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    /* 55 7 */ {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    /* 56 8 */ {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    /* 57 9 */ {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    /* 58 : */ {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
    /* 59 ; */ {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x08},
    /* 60 < */ {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},
    /* 61 = */ {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},
    /* 62 > */ {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},
    /* 63 ? */ {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},
    /* 64 @ */ {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E},
    /* 65 A */ {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    /* 66 B */ {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    /* 67 C */ {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    /* 68 D */ {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    /* 69 E */ {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    /* 70 F */ {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    /* 71 G */ {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    /* 72 H */ {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    /* 73 I */ {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    /* 74 J */ {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
    /* 75 K */ {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    /* 76 L */ {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    /* 77 M */ {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    /* 78 N */ {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    /* 79 O */ {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 80 P */ {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    /* 81 Q */ {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    /* 82 R */ {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    /* 83 S */ {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},
    /* 84 T */ {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    /* 85 U */ {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    /* 86 V */ {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    /* 87 W */ {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    /* 88 X */ {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    /* 89 Y */ {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    /* 90 Z */ {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
};

static int text_width(const char *text)
{
    int n = 0;

    if (text == NULL) {
        return 0;
    }
    while (text[n] != '\0') {
        n += 1;
    }
    if (n <= 0) {
        return 0;
    }
    return n * 5 + (n - 1);
}

static void draw_text(SDL_Renderer *renderer, int x, int y, const char *text, int clip_r)
{
    int cx = x;
    const char *p;

    if (text == NULL) {
        return;
    }
    for (p = text; *p != '\0'; p++) {
        unsigned char ch = (unsigned char)*p;
        int row;
        if (ch >= 'a' && ch <= 'z') {
            ch = (unsigned char)(ch - 'a' + 'A');
        }
        if (ch < 32 || ch > 90) {
            cx += 6;
            continue;
        }
        for (row = 0; row < 7; row++) {
            uint8_t bits = k_font[ch - 32][row];
            int col;
            for (col = 0; col < 5; col++) {
                int px = cx + col;
                if (px >= clip_r) {
                    break;
                }
                if ((bits & (uint8_t)(0x10u >> col)) != 0) {
                    SDL_RenderDrawPoint(renderer, px, y + row);
                }
            }
        }
        cx += 6;
    }
}

static void fill_rect(SDL_Renderer *renderer, int x, int y, int w, int h)
{
    SDL_Rect rect;

    if (w <= 0 || h <= 0) {
        return;
    }
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderFillRect(renderer, &rect);
}

static void stroke_rect(SDL_Renderer *renderer, int x, int y, int w, int h)
{
    SDL_Rect rect;

    if (w <= 0 || h <= 0) {
        return;
    }
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_RenderDrawRect(renderer, &rect);
}

static void draw_glyph(
    SDL_Renderer *renderer,
    int x,
    int y,
    int w,
    int h,
    const uint8_t *charset,
    bool lowercase,
    uint8_t screen)
{
    const uint8_t *glyph;
    int ox;
    int oy;
    int row;

    if (charset == NULL || w < 8 || h < 8) {
        return;
    }
    glyph = charset + (lowercase ? 2048 : 0) + (size_t)screen * 8u;
    ox = x + (w - 8) / 2;
    oy = y + (h - 8) / 2;
    for (row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        int col;
        for (col = 0; col < 8; col++) {
            if ((bits & (uint8_t)(0x80u >> col)) != 0) {
                SDL_RenderDrawPoint(renderer, ox + col, oy + row);
            }
        }
    }
}

static void draw_centered(SDL_Renderer *renderer, const petscii_rect *rect, const char *text)
{
    int tw = text_width(text);
    int x = rect->x + (rect->w - tw) / 2;
    int y = rect->y + (rect->h - 7) / 2;

    if (x < rect->x) {
        x = rect->x;
    }
    draw_text(renderer, x, y, text, rect->x + rect->w);
}

void petscii_board_draw(
    SDL_Renderer *renderer,
    const petscii_board *board,
    const petscii_layout *layout,
    const petscii_rect *strip,
    bool open,
    const uint8_t *charset,
    bool lowercase)
{
    SDL_Rect previous_clip;
    SDL_Rect clip;
    bool had_clip;
    int i;

    if (renderer == NULL) {
        return;
    }
    if (!open || strip == NULL || layout == NULL || strip->w <= 0 || strip->h <= 0) {
        return;
    }

    had_clip = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
    if (had_clip) {
        SDL_RenderGetClipRect(renderer, &previous_clip);
    }
    clip.x = strip->x;
    clip.y = strip->y;
    clip.w = strip->w;
    clip.h = strip->h;
    SDL_RenderSetClipRect(renderer, &clip);

    SDL_SetRenderDrawColor(renderer, 16, 18, 22, 255);
    fill_rect(renderer, strip->x, strip->y, strip->w, strip->h);

    for (i = 0; i < layout->count; i++) {
        const petscii_placed_key *key = &layout->keys[i];
        petscii_rect key_rect;
        int f;

        key_rect.x = strip->x + key->rect.x;
        key_rect.y = strip->y + key->rect.y;
        key_rect.w = key->rect.w;
        key_rect.h = key->rect.h;
        if (key_rect.x + key_rect.w <= strip->x || key_rect.x >= strip->x + strip->w) {
            continue;
        }
        SDL_SetRenderDrawColor(renderer, 46, 50, 58, 255);
        fill_rect(renderer, key_rect.x, key_rect.y, key_rect.w, key_rect.h);
        if (key->lock_lit) {
            SDL_SetRenderDrawColor(renderer, 210, 170, 60, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 90, 96, 108, 255);
        }
        stroke_rect(renderer, key_rect.x, key_rect.y, key_rect.w, key_rect.h);
        SDL_SetRenderDrawColor(renderer, 176, 182, 192, 255);
        {
            petscii_rect label = key_rect;
            label.h = 12;
            draw_centered(renderer, &label, key->label);
        }

        for (f = 0; f < key->face_count; f++) {
            const petscii_face *face = &key->faces[f];
            petscii_rect face_rect = key->face_rects[f];
            bool held = petscii_board_face_held(board, face);

            face_rect.x += strip->x;
            face_rect.y += strip->y;
            if (held) {
                SDL_SetRenderDrawColor(renderer, 64, 104, 140, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 18, 20, 26, 255);
            }
            fill_rect(renderer, face_rect.x, face_rect.y, face_rect.w, face_rect.h);
            SDL_SetRenderDrawColor(renderer, 120, 128, 140, 255);
            stroke_rect(renderer, face_rect.x, face_rect.y, face_rect.w, face_rect.h);

            if (face->kind == PETSCII_FACE_COLOR) {
                uint32_t argb = c64_palette_argb[face->color & 15u];
                SDL_SetRenderDrawColor(
                    renderer,
                    (uint8_t)((argb >> 16) & 255u),
                    (uint8_t)((argb >> 8) & 255u),
                    (uint8_t)(argb & 255u),
                    255);
                fill_rect(
                    renderer,
                    face_rect.x + 2,
                    face_rect.y + 2,
                    face_rect.w - 4,
                    face_rect.h - 4);
            } else if (face->kind == PETSCII_FACE_GLYPH) {
                SDL_SetRenderDrawColor(renderer, 232, 228, 210, 255);
                draw_glyph(
                    renderer,
                    face_rect.x,
                    face_rect.y,
                    face_rect.w,
                    face_rect.h,
                    charset,
                    lowercase,
                    face->screen);
            } else {
                SDL_SetRenderDrawColor(renderer, 230, 230, 220, 255);
                draw_centered(renderer, &face_rect, face->name);
            }
        }
    }

    if (had_clip) {
        SDL_RenderSetClipRect(renderer, &previous_clip);
    } else {
        SDL_RenderSetClipRect(renderer, NULL);
    }
}
