#pragma once

#include "petscii_board.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/* Draw the strip when open. layout rects are strip-local. strip is in
   window coordinates. charset is the 4 KB character ROM, or NULL. */
void petscii_board_draw(
    SDL_Renderer *renderer,
    const petscii_board *board,
    const petscii_layout *layout,
    const petscii_rect *strip,
    bool open,
    const uint8_t *charset,
    bool lowercase);
