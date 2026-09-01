#include "frontend_mouse_input.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_true(const char *name, bool value) {
    if (!value) {
        fprintf(stderr, "%s: expected true\n", name);
        exit(1);
    }
}

static void expect_false(const char *name, bool value) {
    if (value) {
        fprintf(stderr, "%s: expected false\n", name);
        exit(1);
    }
}

static void expect_u8(const char *name, uint8_t expected, uint8_t actual) {
    if (expected != actual) {
        fprintf(stderr, "%s: expected 0x%02X, got 0x%02X\n", name, expected, actual);
        exit(1);
    }
}

static void expect_action(
    const char *name,
    frontend_mouse_action expected,
    frontend_mouse_action actual) {
    if (expected != actual) {
        fprintf(stderr, "%s: expected action %d, got %d\n", name, (int)expected, (int)actual);
        exit(1);
    }
}

static frontend_mouse_rect crt_rect(void) {
    frontend_mouse_rect r = {10.0f, 20.0f, 320.0f, 200.0f};
    return r;
}

static frontend_mouse_ui_flags ui_clear(void) {
    frontend_mouse_ui_flags ui;
    memset(&ui, 0, sizeof(ui));
    return ui;
}

static SDL_Event button_at(Uint32 type, Uint8 button, int x, int y) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.button.button = button;
    event.button.x = x;
    event.button.y = y;
    return event;
}

static SDL_Event motion(int xrel, int yrel) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEMOTION;
    event.motion.xrel = xrel;
    event.motion.yrel = yrel;
    return event;
}

static void test_pot_encode_and_wrap(void) {
    frontend_mouse_input mouse;
    frontend_mouse_ui_flags ui = ui_clear();
    frontend_mouse_action action = FRONTEND_MOUSE_ACTION_NONE;
    SDL_Event ev;

    frontend_mouse_reset(&mouse);
    frontend_mouse_set_enabled(&mouse, true);
    frontend_mouse_set_port(&mouse, 1u);

    /* Force captured; budget already elapsed so the next motion commits. */
    mouse.captured = true;
    mouse.budget_ms = SDL_GetTicks() - (uint32_t)(CBM1351_BUDGET_MS + 1);

    ev = motion(1, 0);
    expect_true("motion consumed", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_action("motion publish", FRONTEND_MOUSE_ACTION_PUBLISH, action);
    expect_u8("counter_x 1", 1, mouse.counter_x);
    expect_u8("potx <<1", 0x02, frontend_mouse_potx(&mouse));

    mouse.budget_ms = SDL_GetTicks() - (uint32_t)(CBM1351_BUDGET_MS + 1);
    ev = motion(0, 1); /* SDL +y down ⇒ counter_y decreases */
    expect_true("y motion", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_u8("counter_y wrap", 63, mouse.counter_y);
    expect_u8("poty", (uint8_t)(63u << 1), frontend_mouse_poty(&mouse));

    mouse.counter_x = 63;
    mouse.budget_ms = SDL_GetTicks() - (uint32_t)(CBM1351_BUDGET_MS + 1);
    ev = motion(2, 0);
    (void)frontend_mouse_handle_event(&mouse, &ev, crt_rect(), &ui, &action);
    expect_u8("counter_x wrap", 1, mouse.counter_x);
}

static void test_event_clamp_and_budget(void) {
    frontend_mouse_input mouse;
    frontend_mouse_ui_flags ui = ui_clear();
    frontend_mouse_action action = FRONTEND_MOUSE_ACTION_NONE;
    SDL_Event ev;
    uint8_t before;

    frontend_mouse_reset(&mouse);
    frontend_mouse_set_enabled(&mouse, true);
    mouse.captured = true;
    mouse.budget_ms = SDL_GetTicks() - (uint32_t)(CBM1351_BUDGET_MS + 1);
    mouse.counter_x = 10;

    /* One huge event: per-event clamp then budget commit (both max 8). */
    ev = motion(100, 0);
    (void)frontend_mouse_handle_event(&mouse, &ev, crt_rect(), &ui, &action);
    expect_action("spike publish", FRONTEND_MOUSE_ACTION_PUBLISH, action);
    expect_u8("spike +budget", (uint8_t)(10 + CBM1351_BUDGET_MAX), mouse.counter_x);
    expect_true("pending cleared", mouse.pending_x == 0);

    /* Same window: further motion pend but does not advance counters yet. */
    before = mouse.counter_x;
    ev = motion(8, 0);
    (void)frontend_mouse_handle_event(&mouse, &ev, crt_rect(), &ui, &action);
    expect_action("same window consume", FRONTEND_MOUSE_ACTION_CONSUME, action);
    expect_u8("counter held", before, mouse.counter_x);
    expect_true("pending held", mouse.pending_x == 8);

    /* Burst beyond budget: only ±BUDGET_MAX commits; excess dropped. */
    mouse.pending_x = 100;
    mouse.budget_ms = SDL_GetTicks() - (uint32_t)(CBM1351_BUDGET_MS + 1);
    before = mouse.counter_x;
    expect_false(
        "poll no leave",
        frontend_mouse_poll_autorelease(&mouse, &ui, &action));
    expect_action("poll publish", FRONTEND_MOUSE_ACTION_PUBLISH, action);
    expect_u8(
        "budget cap",
        (uint8_t)(before + CBM1351_BUDGET_MAX),
        mouse.counter_x);
    expect_true("excess dropped", mouse.pending_x == 0);
}

static void test_enter_leave_edges(void) {
    frontend_mouse_input mouse;
    frontend_mouse_ui_flags ui = ui_clear();
    frontend_mouse_action action = FRONTEND_MOUSE_ACTION_NONE;
    SDL_Event ev;
    SDL_Keymod saved = SDL_GetModState();

    frontend_mouse_reset(&mouse);
    frontend_mouse_set_enabled(&mouse, true);

    /* Without Opt, click on CRT is ignored by mouse module. */
    SDL_SetModState(KMOD_NONE);
    ev = button_at(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 50, 50);
    expect_false("plain down ignored", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_action("plain none", FRONTEND_MOUSE_ACTION_NONE, action);

    /* Opt+down on CRT arms. */
    SDL_SetModState(KMOD_ALT);
    ev = button_at(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 50, 50);
    expect_true("opt down arms", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_true("armed", mouse.opt_click_armed);
    expect_false("not captured yet", mouse.captured);

    /* Opt+up completes enter (may be outside CRT). */
    ev = button_at(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 0, 0);
    expect_true("opt up enters", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_action("enter", FRONTEND_MOUSE_ACTION_ENTER, action);
    expect_true("captured", mouse.captured);

    /* Opt+down/up while captured leaves (no CRT required). */
    ev = button_at(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 999, 999);
    expect_true("leave arm", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    ev = button_at(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 999, 999);
    expect_true("leave up", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_action("leave", FRONTEND_MOUSE_ACTION_LEAVE, action);
    expect_false("released", mouse.captured);

    SDL_SetModState(saved);
}

static void test_buttons_while_captured(void) {
    frontend_mouse_input mouse;
    frontend_mouse_ui_flags ui = ui_clear();
    frontend_mouse_action action = FRONTEND_MOUSE_ACTION_NONE;
    SDL_Event ev;
    SDL_Keymod saved = SDL_GetModState();

    frontend_mouse_reset(&mouse);
    frontend_mouse_set_enabled(&mouse, true);
    mouse.captured = true;
    SDL_SetModState(KMOD_NONE);

    ev = button_at(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_LEFT, 50, 50);
    expect_true("fire down", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_u8("fire", FRONTEND_JOYSTICK_FIRE, mouse.buttons);

    ev = button_at(SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT, 50, 50);
    expect_true("up down", frontend_mouse_handle_event(
        &mouse, &ev, crt_rect(), &ui, &action));
    expect_u8("fire+up", FRONTEND_JOYSTICK_FIRE | FRONTEND_JOYSTICK_UP, mouse.buttons);

    ev = button_at(SDL_MOUSEBUTTONUP, SDL_BUTTON_LEFT, 50, 50);
    (void)frontend_mouse_handle_event(&mouse, &ev, crt_rect(), &ui, &action);
    expect_u8("up remains", FRONTEND_JOYSTICK_UP, mouse.buttons);

    SDL_SetModState(saved);
}

static void test_autorelease(void) {
    frontend_mouse_input mouse;
    frontend_mouse_ui_flags ui = ui_clear();
    frontend_mouse_action action = FRONTEND_MOUSE_ACTION_NONE;

    frontend_mouse_reset(&mouse);
    frontend_mouse_set_enabled(&mouse, true);
    mouse.captured = true;
    mouse.buttons = FRONTEND_JOYSTICK_FIRE;

    expect_false(
        "no release",
        frontend_mouse_poll_autorelease(&mouse, &ui, &action));
    expect_true("still captured", mouse.captured);

    ui.help_open = true;
    expect_true(
        "help releases",
        frontend_mouse_poll_autorelease(&mouse, &ui, &action));
    expect_action("leave", FRONTEND_MOUSE_ACTION_LEAVE, action);
    expect_false("cleared", mouse.captured);
    expect_u8("buttons cleared", 0, mouse.buttons);
}

int main(void) {
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    test_pot_encode_and_wrap();
    test_event_clamp_and_budget();
    test_enter_leave_edges();
    test_buttons_while_captured();
    test_autorelease();
    SDL_Quit();
    printf("ok\n");
    return 0;
}
