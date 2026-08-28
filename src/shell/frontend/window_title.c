#include "window_title.h"

#include <stdio.h>
#include <string.h>

static int debugger_title_append(
    char *out,
    size_t out_size,
    int used,
    const char *part)
{
    int n;

    if (out == NULL || out_size == 0u || used < 0) {
        return used;
    }
    if (part == NULL || part[0] == '\0') {
        return used;
    }
    if ((size_t)used >= out_size) {
        return used;
    }
    if (used == 0) {
        n = snprintf(out + used, out_size - (size_t)used, "%s", part);
    } else {
        n = snprintf(out + used, out_size - (size_t)used, " - %s", part);
    }
    if (n < 0) {
        return used;
    }
    return used + n;
}

void debugger_format_window_title(
    char *out,
    size_t out_size,
    const char *product,
    const char *label,
    const char *turbo,
    const char *state)
{
    int used;

    if (out == NULL || out_size == 0u) {
        return;
    }
    out[0] = '\0';
    used = 0;
    used = debugger_title_append(out, out_size, used, product);
    used = debugger_title_append(out, out_size, used, label);
    used = debugger_title_append(out, out_size, used, turbo);
    (void)debugger_title_append(out, out_size, used, state);
}
