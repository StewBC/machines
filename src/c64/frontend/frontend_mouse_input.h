#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#include "frontend_joystick_input.h"

enum {
    CBM1351_SENS = 1,
    /* Cap host xrel/yrel per SDL event before pending. */
    CBM1351_MAX_DELTA = 8,
    /* Pot-window budget: at most this many counts per axis commit into the
       6-bit counter each BUDGET_MS. Unused pending is carried (not dropped)
       up to PENDING_MAX so fast moves feel less laggy. */
    CBM1351_BUDGET_MAX = 8,
    CBM1351_BUDGET_MS = 16,
    CBM1351_PENDING_MAX = 48
};

/* Axis-aligned CRT hit rect (same layout as nk_rect; no nuklear dependency). */
typedef struct frontend_mouse_rect {
    float x;
    float y;
    float w;
    float h;
} frontend_mouse_rect;

typedef struct frontend_mouse_input {
    bool enabled;         /* CLI / Config */
    unsigned port;        /* 1 or 2 */
    bool captured;
    uint8_t counter_x;    /* 6-bit wrap (guest-visible) */
    uint8_t counter_y;
    uint8_t buttons;      /* FRONTEND_JOYSTICK_FIRE / UP */
    bool opt_click_armed; /* Alt+LMB down seen (enter or leave) */
    int pending_x;        /* counter-space deltas waiting for budget */
    int pending_y;
    uint32_t budget_ms;   /* SDL_GetTicks() of last budget commit */
} frontend_mouse_input;

typedef struct frontend_mouse_ui_flags {
    bool help_open;
    bool forensics_open;
    bool any_dialog_open;
    bool focus_lost;
    bool inspecting; /* treat like modal: no enter; autorelease if captured */
} frontend_mouse_ui_flags;

typedef enum frontend_mouse_action {
    FRONTEND_MOUSE_ACTION_NONE = 0,
    FRONTEND_MOUSE_ACTION_CONSUME,
    FRONTEND_MOUSE_ACTION_PUBLISH,
    FRONTEND_MOUSE_ACTION_ENTER,
    FRONTEND_MOUSE_ACTION_LEAVE
} frontend_mouse_action;

void frontend_mouse_reset(frontend_mouse_input *mouse);
void frontend_mouse_set_enabled(frontend_mouse_input *mouse, bool enabled);
void frontend_mouse_set_port(frontend_mouse_input *mouse, unsigned port);

uint8_t frontend_mouse_potx(const frontend_mouse_input *mouse);
uint8_t frontend_mouse_poty(const frontend_mouse_input *mouse);

bool frontend_mouse_point_in_rect(float x, float y, frontend_mouse_rect rect);

/* Returns true if the SDL event must not be forwarded to Nuklear/frontend.
   *out_action (optional) reports enter/leave/publish for the main loop. */
bool frontend_mouse_handle_event(
    frontend_mouse_input *mouse,
    const SDL_Event *event,
    frontend_mouse_rect crt_display,
    const frontend_mouse_ui_flags *ui,
    frontend_mouse_action *out_action);

/* Call once per main-loop iteration while enabled. Flushes the pot-window
   budget (may set *out_action to PUBLISH). Returns true if capture was
   released (*out_action LEAVE); caller should clear_mouse + ungrip. */
bool frontend_mouse_poll_autorelease(
    frontend_mouse_input *mouse,
    const frontend_mouse_ui_flags *ui,
    frontend_mouse_action *out_action);
