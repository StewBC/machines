#include "frontend_mouse_input.h"

#include <string.h>

static bool ui_blocks_capture(const frontend_mouse_ui_flags *ui) {
    if (ui == NULL) {
        return false;
    }
    return ui->help_open || ui->forensics_open || ui->any_dialog_open ||
        ui->inspecting || ui->focus_lost;
}

static bool opt_modifier_down(void) {
    return (SDL_GetModState() & KMOD_ALT) != 0;
}

static uint8_t pot_from_counter(uint8_t counter6) {
    return (uint8_t)((counter6 & 0x3fu) << 1);
}

static int clamp_delta(int value) {
    if (value > CBM1351_MAX_DELTA) {
        return CBM1351_MAX_DELTA;
    }
    if (value < -CBM1351_MAX_DELTA) {
        return -CBM1351_MAX_DELTA;
    }
    return value;
}

static int clamp_budget(int value) {
    if (value > CBM1351_BUDGET_MAX) {
        return CBM1351_BUDGET_MAX;
    }
    if (value < -CBM1351_BUDGET_MAX) {
        return -CBM1351_BUDGET_MAX;
    }
    return value;
}

static void release_capture(frontend_mouse_input *mouse) {
    mouse->captured = false;
    mouse->opt_click_armed = false;
    mouse->buttons = 0;
    mouse->counter_x = 0;
    mouse->counter_y = 0;
    mouse->pending_x = 0;
    mouse->pending_y = 0;
    mouse->budget_ms = 0;
}

static int clamp_pending(int value) {
    if (value > CBM1351_PENDING_MAX) {
        return CBM1351_PENDING_MAX;
    }
    if (value < -CBM1351_PENDING_MAX) {
        return -CBM1351_PENDING_MAX;
    }
    return value;
}

/* Commit at most ±BUDGET_MAX from pending into counters; carry the rest
   (clamped to ±PENDING_MAX on accumulate). Returns true if counters changed. */
static bool commit_budget(frontend_mouse_input *mouse) {
    uint32_t now;
    int dx;
    int dy;

    if (mouse == NULL || !mouse->captured) {
        return false;
    }
    now = SDL_GetTicks();
    if (mouse->budget_ms == 0u) {
        mouse->budget_ms = now;
        return false;
    }
    if ((uint32_t)(now - mouse->budget_ms) < (uint32_t)CBM1351_BUDGET_MS) {
        return false;
    }
    mouse->budget_ms = now;
    dx = clamp_budget(mouse->pending_x);
    dy = clamp_budget(mouse->pending_y);
    mouse->pending_x -= dx;
    mouse->pending_y -= dy;
    if (dx == 0 && dy == 0) {
        return false;
    }
    mouse->counter_x = (uint8_t)(((int)mouse->counter_x + dx) & 63);
    mouse->counter_y = (uint8_t)(((int)mouse->counter_y + dy) & 63);
    return true;
}

void frontend_mouse_reset(frontend_mouse_input *mouse) {
    if (mouse == NULL) {
        return;
    }
    memset(mouse, 0, sizeof(*mouse));
    mouse->port = 1u;
}

void frontend_mouse_set_enabled(frontend_mouse_input *mouse, bool enabled) {
    if (mouse == NULL) {
        return;
    }
    mouse->enabled = enabled;
    if (!enabled) {
        release_capture(mouse);
    }
}

void frontend_mouse_set_port(frontend_mouse_input *mouse, unsigned port) {
    if (mouse == NULL) {
        return;
    }
    if (port != 1u && port != 2u) {
        port = 1u;
    }
    if (mouse->port != port && mouse->captured) {
        release_capture(mouse);
    }
    mouse->port = port;
}

uint8_t frontend_mouse_potx(const frontend_mouse_input *mouse) {
    if (mouse == NULL) {
        return 0xFFu;
    }
    return pot_from_counter(mouse->counter_x);
}

uint8_t frontend_mouse_poty(const frontend_mouse_input *mouse) {
    if (mouse == NULL) {
        return 0xFFu;
    }
    return pot_from_counter(mouse->counter_y);
}

bool frontend_mouse_point_in_rect(float x, float y, frontend_mouse_rect rect) {
    return x >= rect.x && x < rect.x + rect.w &&
        y >= rect.y && y < rect.y + rect.h;
}

bool frontend_mouse_poll_autorelease(
    frontend_mouse_input *mouse,
    const frontend_mouse_ui_flags *ui,
    frontend_mouse_action *out_action) {
    if (out_action != NULL) {
        *out_action = FRONTEND_MOUSE_ACTION_NONE;
    }
    if (mouse == NULL || !mouse->enabled) {
        return false;
    }
    if (mouse->captured && commit_budget(mouse)) {
        if (out_action != NULL) {
            *out_action = FRONTEND_MOUSE_ACTION_PUBLISH;
        }
    }
    if (!mouse->captured) {
        return false;
    }
    if (!ui_blocks_capture(ui)) {
        return false;
    }
    release_capture(mouse);
    if (out_action != NULL) {
        *out_action = FRONTEND_MOUSE_ACTION_LEAVE;
    }
    return true;
}

bool frontend_mouse_handle_event(
    frontend_mouse_input *mouse,
    const SDL_Event *event,
    frontend_mouse_rect crt_display,
    const frontend_mouse_ui_flags *ui,
    frontend_mouse_action *out_action) {
    frontend_mouse_action action = FRONTEND_MOUSE_ACTION_NONE;
    bool consumed = false;
    bool opt;
    float x;
    float y;

    if (out_action != NULL) {
        *out_action = FRONTEND_MOUSE_ACTION_NONE;
    }
    if (mouse == NULL || event == NULL || !mouse->enabled) {
        return false;
    }

    opt = opt_modifier_down();

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT) {
        x = (float)event->button.x;
        y = (float)event->button.y;
        if (mouse->captured) {
            if (opt) {
                mouse->opt_click_armed = true;
                consumed = true;
                action = FRONTEND_MOUSE_ACTION_CONSUME;
            } else {
                uint8_t prev = mouse->buttons;
                mouse->buttons =
                    (uint8_t)(mouse->buttons | FRONTEND_JOYSTICK_FIRE);
                consumed = true;
                action = (mouse->buttons != prev) ?
                    FRONTEND_MOUSE_ACTION_PUBLISH :
                    FRONTEND_MOUSE_ACTION_CONSUME;
            }
        } else if (opt && !ui_blocks_capture(ui) &&
                   frontend_mouse_point_in_rect(x, y, crt_display)) {
            mouse->opt_click_armed = true;
            consumed = true;
            action = FRONTEND_MOUSE_ACTION_CONSUME;
        }
    } else if (event->type == SDL_MOUSEBUTTONUP &&
               event->button.button == SDL_BUTTON_LEFT) {
        if (mouse->captured) {
            if (mouse->opt_click_armed && opt) {
                release_capture(mouse);
                consumed = true;
                action = FRONTEND_MOUSE_ACTION_LEAVE;
            } else if (mouse->opt_click_armed && !opt) {
                mouse->opt_click_armed = false;
                consumed = true;
                action = FRONTEND_MOUSE_ACTION_CONSUME;
            } else {
                uint8_t prev = mouse->buttons;
                mouse->buttons =
                    (uint8_t)(mouse->buttons & (uint8_t)~FRONTEND_JOYSTICK_FIRE);
                consumed = true;
                action = (mouse->buttons != prev) ?
                    FRONTEND_MOUSE_ACTION_PUBLISH :
                    FRONTEND_MOUSE_ACTION_CONSUME;
            }
        } else if (mouse->opt_click_armed) {
            if (opt && !ui_blocks_capture(ui)) {
                mouse->opt_click_armed = false;
                mouse->captured = true;
                mouse->buttons = 0;
                mouse->counter_x = 0;
                mouse->counter_y = 0;
                mouse->pending_x = 0;
                mouse->pending_y = 0;
                mouse->budget_ms = SDL_GetTicks();
                consumed = true;
                action = FRONTEND_MOUSE_ACTION_ENTER;
            } else {
                mouse->opt_click_armed = false;
                consumed = true;
                action = FRONTEND_MOUSE_ACTION_CONSUME;
            }
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN &&
               event->button.button == SDL_BUTTON_RIGHT && mouse->captured) {
        uint8_t prev = mouse->buttons;
        mouse->buttons = (uint8_t)(mouse->buttons | FRONTEND_JOYSTICK_UP);
        consumed = true;
        action = (mouse->buttons != prev) ?
            FRONTEND_MOUSE_ACTION_PUBLISH : FRONTEND_MOUSE_ACTION_CONSUME;
    } else if (event->type == SDL_MOUSEBUTTONUP &&
               event->button.button == SDL_BUTTON_RIGHT && mouse->captured) {
        uint8_t prev = mouse->buttons;
        mouse->buttons =
            (uint8_t)(mouse->buttons & (uint8_t)~FRONTEND_JOYSTICK_UP);
        consumed = true;
        action = (mouse->buttons != prev) ?
            FRONTEND_MOUSE_ACTION_PUBLISH : FRONTEND_MOUSE_ACTION_CONSUME;
    } else if (event->type == SDL_MOUSEMOTION && mouse->captured) {
        int dx = clamp_delta(event->motion.xrel * CBM1351_SENS);
        /* SDL +y is down; store counter-space delta (PotY grows on mouse-up). */
        int dy = clamp_delta(-(event->motion.yrel * CBM1351_SENS));
        if (dx != 0 || dy != 0) {
            mouse->pending_x = clamp_pending(mouse->pending_x + dx);
            mouse->pending_y = clamp_pending(mouse->pending_y + dy);
            consumed = true;
            if (commit_budget(mouse)) {
                action = FRONTEND_MOUSE_ACTION_PUBLISH;
            } else {
                action = FRONTEND_MOUSE_ACTION_CONSUME;
            }
        } else {
            consumed = true;
            action = FRONTEND_MOUSE_ACTION_CONSUME;
        }
    }

    if (out_action != NULL) {
        *out_action = action;
    }
    return consumed;
}
