#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* `{product} - {label} - {turbo} - {state}`. Empty fields drop their separator. */
void debugger_format_window_title(
    char *out,
    size_t out_size,
    const char *product,
    const char *label,
    const char *turbo,
    const char *state);

#ifdef __cplusplus
}
#endif
