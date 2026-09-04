#include "frontend_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void expect_key(
    const char *name,
    frontend_input_mapper *mapper,
    SDL_Keycode symbol,
    SDL_Scancode scancode,
    host_key expected)
{
    SDL_KeyboardEvent event;
    frontend_input_action action;
    size_t count;

    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN;
    event.keysym.sym = symbol;
    event.keysym.scancode = scancode;
    count = frontend_input_map_keyboard_event(mapper, &event, &action, 1u);
    if (count != 1u || action.type != FRONTEND_INPUT_ACTION_KEY ||
        action.key != expected || !action.pressed) {
        fprintf(stderr, "FAIL: %s keydown\n", name);
        exit(1);
    }

    event.type = SDL_KEYUP;
    count = frontend_input_map_keyboard_event(mapper, &event, &action, 1u);
    if (count != 1u || action.key != expected || action.pressed) {
        fprintf(stderr, "FAIL: %s keyup\n", name);
        exit(1);
    }
}

int main(void)
{
    frontend_input_mapper mapper;

    frontend_input_mapper_reset(&mapper);
    expect_key(
        "modern Backspace",
        &mapper,
        SDLK_BACKSPACE,
        SDL_SCANCODE_BACKSPACE,
        HOST_KEY_DELETE);

    frontend_input_mapper_set_original_del(&mapper, true);
    expect_key(
        "original Backspace",
        &mapper,
        SDLK_BACKSPACE,
        SDL_SCANCODE_BACKSPACE,
        HOST_KEY_APPLE_DEL);

    frontend_input_mapper_set_original_del(&mapper, false);
    expect_key(
        "physical Delete",
        &mapper,
        SDLK_DELETE,
        SDL_SCANCODE_DELETE,
        HOST_KEY_APPLE_DEL);

    if (host_key_to_apple_strobe(HOST_KEY_DELETE, false, false) != 0x88u ||
        host_key_to_apple_strobe(HOST_KEY_APPLE_DEL, false, false) != 0xFFu) {
        fprintf(stderr, "FAIL: Apple Backspace/DEL strobe values\n");
        return 1;
    }

    expect_key(
        "Open-Apple",
        &mapper,
        SDLK_LGUI,
        SDL_SCANCODE_LGUI,
        HOST_KEY_OPEN_APPLE);
    expect_key(
        "Closed-Apple",
        &mapper,
        SDLK_RGUI,
        SDL_SCANCODE_RGUI,
        HOST_KEY_CLOSED_APPLE);

    {
        SDL_KeyboardEvent event;
        frontend_input_action action;
        size_t count;

        memset(&event, 0, sizeof(event));
        event.type = SDL_KEYDOWN;
        event.keysym.sym = SDLK_LALT;
        event.keysym.scancode = SDL_SCANCODE_LALT;
        count = frontend_input_map_keyboard_event(&mapper, &event, &action, 1u);
        if (count != 0u) {
            fprintf(stderr, "FAIL: Left-Alt must not map to the Apple II\n");
            return 1;
        }
        event.keysym.sym = SDLK_RALT;
        event.keysym.scancode = SDL_SCANCODE_RALT;
        count = frontend_input_map_keyboard_event(&mapper, &event, &action, 1u);
        if (count != 0u) {
            fprintf(stderr, "FAIL: Right-Alt must not map to the Apple II\n");
            return 1;
        }
    }

    printf("ok frontend input\n");
    return 0;
}
